#pragma once

typedef struct HWND__* HWND;

class Win32NativeHandle
{
public:
	HWND hwnd = {};
};
