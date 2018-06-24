#pragma once

#include "v_video.h"

//==========================================================================
//
// 
//
//==========================================================================

class Win32GLVideo : public IVideo
{
public:
	Win32GLVideo(int parm);
	virtual ~Win32GLVideo();

	DFrameBuffer *CreateFrameBuffer();
	void DumpAdapters();
	bool InitHardware(HWND Window, int multisample);
	void Shutdown();

	HDC m_hDC;

protected:
	HMODULE hmRender;

	char m_DisplayDeviceBuffer[CCHDEVICENAME];
	char *m_DisplayDeviceName;
	HMONITOR m_hMonitor;

	HWND m_Window;
	HGLRC m_hRC;

	HWND InitDummy();
	void ShutdownDummy(HWND dummy);
	bool SetPixelFormat();
	bool SetupPixelFormat(int multisample);

	void GetDisplayDeviceName();
public:

};
