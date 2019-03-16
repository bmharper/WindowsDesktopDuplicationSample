#pragma once

typedef std::string Error;

// BGRA U8 Bitmap
struct Bitmap {
	int                  Width  = 0;
	int                  Height = 0;
	std::vector<uint8_t> Buf;
};

// WinDesktopDup hides the gory details of capturing the screen using the
// Windows Desktop Duplication API
class WinDesktopDup {
public:
	Bitmap Latest;
	int    OutputNumber = 0;

	~WinDesktopDup();

	Error Initialize();
	void  Close();
	bool  CaptureNext();

private:
	ID3D11Device*           D3DDevice        = nullptr;
	ID3D11DeviceContext*    D3DDeviceContext = nullptr;
	IDXGIOutputDuplication* DeskDupl         = nullptr;
	DXGI_OUTPUT_DESC        OutputDesc;
	bool                    HaveFrameLock = false;
};