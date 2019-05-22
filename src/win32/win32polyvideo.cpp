
#include <assert.h>
#include "hardware.h"
#include <Windows.h>

extern HWND Window;

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
