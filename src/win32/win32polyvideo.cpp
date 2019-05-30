
#include <assert.h>
#include "hardware.h"
#include "doomerrors.h"
#include <Windows.h>

extern HWND Window;

#if 1

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

struct D3D9Present
{
	D3D9Present()
	{
		d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
		if (!d3d9)
		{
			I_FatalError("Direct3DCreate9 failed");
		}

		bool vsync = false;
		RECT rect = {};
		GetClientRect(Window, &rect);
		ClientWidth = rect.right;
		ClientHeight = rect.bottom;

		D3DPRESENT_PARAMETERS pp = {};
		pp.Windowed = true;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferWidth = ClientWidth;
		pp.BackBufferHeight = ClientHeight;
		pp.BackBufferCount = 1;
		pp.hDeviceWindow = Window;
		pp.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

		HRESULT result = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
		if (FAILED(result))
		{
			I_FatalError("IDirect3D9.CreateDevice failed");
		}
	}

	void Present(int w, int h, const void *pixels)
	{
		HRESULT result;
		if (SrcWidth != w || SrcHeight != h || !surface)
		{
			if (surface)
			{
				surface->Release();
				surface = nullptr;
			}

			SrcWidth = w;
			SrcHeight = h;
			result = device->CreateOffscreenPlainSurface(SrcWidth, SrcHeight, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &surface, 0);
			if (FAILED(result))
			{
				I_FatalError("IDirect3DDevice9.CreateOffscreenPlainSurface failed");
			}
		}

		D3DLOCKED_RECT lockrect = {};
		result = surface->LockRect(&lockrect, nullptr, D3DLOCK_DISCARD);
		if (FAILED(result))
			return;

		int pixelsize = 4;
		const uint8_t *src = (const uint8_t*)pixels;
		uint8_t *dst = (uint8_t*)lockrect.pBits;
		for (int y = 0; y < h; y++)
		{
			memcpy(dst + y * lockrect.Pitch, src + y * w * pixelsize, w * pixelsize);
		}

		surface->UnlockRect();

		IDirect3DSurface9 *backbuffer = nullptr;
		device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
		if (FAILED(result))
			return;

		result = device->BeginScene();
		if (SUCCEEDED(result))
		{
			RECT srcrect = {}, dstrect = {};
			srcrect.right = SrcWidth;
			srcrect.bottom = SrcHeight;
			dstrect.right = ClientWidth;
			dstrect.bottom = ClientHeight;
			device->StretchRect(surface, &srcrect, backbuffer, &dstrect, D3DTEXF_LINEAR);

			result = device->EndScene();
			if (SUCCEEDED(result))
				device->Present(nullptr, nullptr, 0, nullptr);
		}

		backbuffer->Release();
	}

	~D3D9Present()
	{
		if (surface) surface->Release();
		if (device) device->Release();
		if (d3d9) d3d9->Release();
	}

private:
	int SrcWidth = 0;
	int SrcHeight = 0;
	int ClientWidth = 0;
	int ClientHeight = 0;

	IDirect3D9 *d3d9 = nullptr;
	IDirect3DDevice9 *device = nullptr;
	IDirect3DSurface9 *surface = nullptr;
};

void I_PresentPolyImage(int w, int h, const void *pixels)
{
	static D3D9Present present;
	present.Present(w, h, pixels);
}

#else

void I_PresentPolyImage(int w, int h, const void *pixels)
{
	BITMAPV5HEADER info = {};
	info.bV5Size = sizeof(BITMAPV5HEADER);
	info.bV5Width = w;
	info.bV5Height = -h;
	info.bV5Planes = 1;
	info.bV5BitCount = 32;
	info.bV5Compression = BI_RGB;
	info.bV5SizeImage = 0;
	info.bV5CSType = LCS_WINDOWS_COLOR_SPACE;

	RECT box = {};
	GetClientRect(Window, &box);

	HDC dc = GetDC(Window);
	if (box.right == w && box.bottom == h)
		SetDIBitsToDevice(dc, 0, 0, w, h, 0, 0, 0, h, pixels, (const BITMAPINFO *)&info, DIB_RGB_COLORS);
	else
		StretchDIBits(dc, 0, 0, box.right, box.bottom, 0, 0, w, h, pixels, (const BITMAPINFO *)&info, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(Window, dc);
}

#endif
