// For license, see https://github.com/IMQS/tsf
#pragma once
#ifndef TSF_H_INCLUDED
#define TSF_H_INCLUDED

/*

tsf: A small, typesafe, cross-platform printf replacement (tested on Windows & linux).

We use snprintf as a backend, so all of the regular formatting
commands that you expect from the printf family of functions work.
This makes the code much smaller than other implementations.

We do however implement some of the common operations ourselves,
such as emitting integers or plain strings, because most snprintf
implementations are actually very slow, and we can gain a lot of
speed by doing these common operations ourselves.

Usage:

tsf::fmt("%v %v", "abc", 123)        -->  "abc 123"     <== Use %v as a generic value type
tsf::fmt("%s %d", "abc", 123)        -->  "abc 123"     <== Specific value types are fine too, unless they conflict with the provided type, in which case they are overridden
tsf::fmt("%v", std::string("abc"))   -->  "abc"         <== std::string
tsf::fmt("%v", std::wstring("abc"))  -->  "abc"         <== std::wstring
tsf::fmt("%.3f", 25.5)               -->  "25.500"      <== Use format strings as usual
tsf::print("%v", "Hello world")      -->  "Hello world" <== Print to stdout
tsf::print(stderr, "err %v", 5)      -->  "err 5"       <== Print to stderr (or any other FILE*)

Known unsupported features:
* Positional arguments
* %*s (integer width parameter)	-- wouldn't be hard to add. Presently ignored.

API:

fmt           returns std::string.
fmt_buf       is useful if you want to provide your own buffer to avoid memory allocations.
print         prints to stdout
print(FILE*)  prints to any FILE*

By providing a cast operator to fmtarg, you can get an arbitrary type
supported as an argument, provided it fits into one of the molds of the
printf family of arguments.

We also support two custom types: %Q and %q. In order to use these, you need to provide your own
implementation that wraps one of the lower level functions, and provides a 'context' object with
custom functions defined for Escape_Q and Escape_q. These were originally added in order to provide 
quoting and escaping for SQL identifiers and SQL string literals.

*/

#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifndef TSF_FMT_API
#define TSF_FMT_API
#endif

#include <stdarg.h>
#include <stdint.h>
#include <string>

namespace tsf {

class fmtarg
{
public:
	enum Types
	{
		TNull,	// Used as a sentinel to indicate that no parameter was passed
		TPtr,
		TCStr,
		TWStr,
		TI32,
		TU32,
		TI64,
		TU64,
		TDbl,
	};
	union
	{
		const void*		Ptr;
		const char*		CStr;
		const wchar_t*	WStr;
		int32_t			I32;
		uint32_t		UI32;
		int64_t			I64;
		uint64_t		UI64;
		double			Dbl;
	};
	Types Type;

	fmtarg()								: Type(TNull), CStr(NULL) {}
	fmtarg(const void* v)					: Type(TPtr), Ptr(v) {}
	fmtarg(const char* v)					: Type(TCStr), CStr(v) {}
	fmtarg(const wchar_t* v)				: Type(TWStr), WStr(v) {}
	fmtarg(const std::string& v)			: Type(TCStr), CStr(v.c_str()) {}
	fmtarg(const std::wstring& v)			: Type(TWStr), WStr(v.c_str()) {}
#ifdef _MSC_VER
	fmtarg(long v)							: Type(TI32), I32(v) {}
	fmtarg(unsigned long v)					: Type(TU32), UI32(v) {}
	fmtarg(__int32 v)						: Type(TI32), I32(v) {}
	fmtarg(unsigned __int32 v)				: Type(TU32), UI32(v) {}
	fmtarg(__int64 v)						: Type(TI64), I64(v) {}
	fmtarg(unsigned __int64 v)				: Type(TU64), UI64(v) {}
#else
	// clang/gcc
	fmtarg(int v)							: Type(TI32), I32(v) {}
	fmtarg(unsigned int v)					: Type(TU32), UI32(v) {}
#if LONG_MAX == 0x7fffffff
	// 32-bit arch
	fmtarg(long v)							: Type(TI32), I32(v) {}
	fmtarg(unsigned long v)					: Type(TU32), UI32(v) {}
#else
	// 64-bit arch
	fmtarg(long v)							: Type(TI64), I64(v) {}
	fmtarg(unsigned long v)					: Type(TU64), UI64(v) {}
#endif
	fmtarg(long long v)						: Type(TI64), I64(v) {}
	fmtarg(unsigned long long v)			: Type(TU64), UI64(v) {}
#endif
	fmtarg(double v)						: Type(TDbl), Dbl(v) {}
};

/* This can be used to add custom formatting tokens.
The only supported characters that you can use are "Q" and "q".
These were added for escaping SQL identifiers and SQL strings.
*/
struct context
{
	// Return the number of characters written, or -1 if outBufSize is not large enough to hold
	// the number of characters that you need to write. Do not write a null terminator.
	typedef size_t(*WriteSpecialFunc)(char* outBuf, size_t outBufSize, const fmtarg& val);

	WriteSpecialFunc Escape_Q = nullptr;
	WriteSpecialFunc Escape_q = nullptr;
};

struct StrLenPair
{
	char*  Str;
	size_t Len;
};

TSF_FMT_API std::string fmt_core(const context& context, const char* fmt, ssize_t nargs, const fmtarg* args);
TSF_FMT_API StrLenPair  fmt_core(const context& context, const char* fmt, ssize_t nargs, const fmtarg* args, char* staticbuf, size_t staticbuf_size);

namespace internal {

inline void fmt_pack(fmtarg* pack)
{
}

inline void fmt_pack(fmtarg* pack, const fmtarg& arg)
{
	*pack = arg;
}

template<typename... Args>
void fmt_pack(fmtarg* pack, const fmtarg& arg, const Args&... args)
{
	*pack = arg;
	fmt_pack(pack + 1, args...);
}

}

// Format and return std::string
template<typename... Args>
std::string fmt(const char* fs, const Args&... args)
{
	const auto num_args = sizeof...(Args);
	fmtarg pack_array[num_args + 1]; // +1 for zero args case
	internal::fmt_pack(pack_array, args...);
	context cx;
	return fmt_core(cx, fs, (ssize_t) num_args, pack_array);
}

// If the formatted string, with null terminator, fits inside staticbuf_len, then the returned pointer is staticbuf,
// and no memory allocation takes place.
// However, if the formatted string is too large to fit inside staticbuf_len, then the returned pointer must
// be deleted with "delete[] ptr".
template<typename... Args>
StrLenPair fmt_buf(const context& cx, char* buf, size_t buf_len, const char* fs, const Args&... args)
{
	const auto num_args = sizeof...(Args);
	fmtarg pack_array[num_args + 1]; // +1 for zero args case
	internal::fmt_pack(pack_array, args...);
	return fmt_core(cx, fs, (ssize_t) num_args, pack_array, buf, buf_len);
}

// If the formatted string, with null terminator, fits inside staticbuf_len, then the returned pointer is staticbuf,
// and no memory allocation takes place.
// However, if the formatted string is too large to fit inside staticbuf_len, then the returned pointer must
// be deleted with "delete[] ptr".
template<typename... Args>
StrLenPair fmt_buf(char* buf, size_t buf_len, const char* fs, const Args&... args)
{
	context cx;
	return fmt_buf(cx, buf, buf_len, fs, args...);
}

// Format and write to FILE*
template<typename... Args>
size_t print(FILE* file, const char* fs, const Args&... args)
{
	auto res = fmt(fs, args...);
	if (res.size() == 0)
		return 0;
	return fwrite(res.c_str(), 1, res.length(), file);
}

// Format and write to stdout
template<typename... Args>
size_t print(const char* fs, const Args&... args)
{
	return print(stdout, fs, args...);
}

/*  cross-platform "snprintf"

	destination			Destination buffer
	count				Number of characters available in 'destination'. This must include space for the null terminating character.
	format_str			The format string
	return
		-1				Not enough space
		0..count-1		Number of characters written, excluding the null terminator. The null terminator was written though.
*/
TSF_FMT_API int fmt_snprintf(char* destination, size_t count, const char* format_str, ...);

} // namespace tsf

#endif