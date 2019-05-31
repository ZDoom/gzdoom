
#include <assert.h>
#include "hardware.h"
#include "doomerrors.h"
#include <Windows.h>

EXTERN_CVAR(Bool, vid_vsync)

extern HWND Window;

#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")

namespace
{
	int SrcWidth = 0;
	int SrcHeight = 0;
	int ClientWidth = 0;
	int ClientHeight = 0;
	bool CurrentVSync = false;

	IDirect3D9 *d3d9 = nullptr;
	IDirect3DDevice9 *device = nullptr;
	IDirect3DSurface9 *surface = nullptr;
}

void I_PolyPresentInit()
{
	d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d9)
	{
		I_FatalError("Direct3DCreate9 failed");
	}

	RECT rect = {};
	GetClientRect(Window, &rect);

	CurrentVSync = vid_vsync;
	ClientWidth = rect.right;
	ClientHeight = rect.bottom;

	D3DPRESENT_PARAMETERS pp = {};
	pp.Windowed = true;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.BackBufferWidth = ClientWidth;
	pp.BackBufferHeight = ClientHeight;
	pp.BackBufferCount = 1;
	pp.hDeviceWindow = Window;
	pp.PresentationInterval = CurrentVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

	HRESULT result = d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, Window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device);
	if (FAILED(result))
	{
		I_FatalError("IDirect3D9.CreateDevice failed");
	}
}

uint8_t *I_PolyPresentLock(int w, int h, int &pitch)
{
	HRESULT result;

	RECT rect = {};
	GetClientRect(Window, &rect);
	if (rect.right != ClientWidth || rect.bottom != ClientHeight || CurrentVSync != vid_vsync)
	{
		if (surface)
		{
			surface->Release();
			surface = nullptr;
		}

		CurrentVSync = vid_vsync;
		ClientWidth = rect.right;
		ClientHeight = rect.bottom;

		D3DPRESENT_PARAMETERS pp = {};
		pp.Windowed = true;
		pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pp.BackBufferWidth = ClientWidth;
		pp.BackBufferHeight = ClientHeight;
		pp.BackBufferCount = 1;
		pp.hDeviceWindow = Window;
		pp.PresentationInterval = CurrentVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
		device->Reset(&pp);
	}

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
	{
		pitch = 0;
		return nullptr;
	}

	pitch = lockrect.Pitch;
	return (uint8_t*)lockrect.pBits;
}

void I_PolyPresentUnlock(int x, int y, int width, int height)
{
	surface->UnlockRect();

	IDirect3DSurface9 *backbuffer = nullptr;
	HRESULT result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
	if (FAILED(result))
		return;

	result = device->BeginScene();
	if (SUCCEEDED(result))
	{
		int count = 0;
		D3DRECT clearrects[4];
		if (y > 0)
		{
			clearrects[count].x1 = 0;
			clearrects[count].y1 = 0;
			clearrects[count].x2 = ClientWidth;
			clearrects[count].y2 = y;
			count++;
		}
		if (y + height < ClientHeight)
		{
			clearrects[count].x1 = 0;
			clearrects[count].y1 = y + height;
			clearrects[count].x2 = ClientWidth;
			clearrects[count].y2 = ClientHeight;
			count++;
		}
		if (x > 0)
		{
			clearrects[count].x1 = 0;
			clearrects[count].y1 = y;
			clearrects[count].x2 = x;
			clearrects[count].y2 = y + height;
			count++;
		}
		if (x + width < ClientWidth)
		{
			clearrects[count].x1 = x + width;
			clearrects[count].y1 = y;
			clearrects[count].x2 = ClientWidth;
			clearrects[count].y2 = y + height;
			count++;
		}
		if (count > 0)
			device->Clear(count, clearrects, D3DCLEAR_TARGET, 0, 0.0f, 0);

		RECT srcrect = {}, dstrect = {};
		srcrect.right = SrcWidth;
		srcrect.bottom = SrcHeight;
		dstrect.left = x;
		dstrect.top = y;
		dstrect.right = x + width;
		dstrect.bottom = y + height;
		device->StretchRect(surface, &srcrect, backbuffer, &dstrect, D3DTEXF_LINEAR);

		result = device->EndScene();
		if (SUCCEEDED(result))
			device->Present(nullptr, nullptr, 0, nullptr);
	}

	backbuffer->Release();
}

void I_PolyPresentDeinit()
{
	if (surface) surface->Release();
	if (device) device->Release();
	if (d3d9) d3d9->Release();
}
