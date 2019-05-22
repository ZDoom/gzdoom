
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

	HDC dc = GetDC(Window);
	SetDIBitsToDevice(dc, 0, 0, w, h, 0, 0, 0, h, pixels, (const BITMAPINFO *)&info, DIB_RGB_COLORS);
	ReleaseDC(Window, dc);
}
