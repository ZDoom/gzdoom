#ifndef __WIN32_GL_SYSFB_H__
#define __WIN32_GL_SYSFB_H__

#include "v_video.h"

class SystemFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;

public:
	SystemFrameBuffer() {}
	// Actually, hMonitor is a HMONITOR, but it's passed as a void * as there
    // look to be some cross-platform bits in the way.
	SystemFrameBuffer(void *hMonitor, bool fullscreen);
	virtual ~SystemFrameBuffer();

	void SetVSync (bool vsync);
	void SwapBuffers();

	int GetClientWidth();
	int GetClientHeight();

	bool IsFullscreen();

	void InitializeState();

protected:

	void PositionWindow();

	void ResetGammaTable();
	void SetGammaTable(uint16_t * tbl);

	float m_Gamma, m_Brightness, m_Contrast;
	uint16_t m_origGamma[768];
	bool m_supportsGamma;
	bool m_Fullscreen;
	char m_displayDeviceNameBuffer[32/*CCHDEVICENAME*/];	// do not use windows.h constants here!
	char *m_displayDeviceName;
	void *m_Monitor;
	int SwapInterval;

	friend class Win32GLVideo;

};

#endif // __WIN32_GL_SYSFB_H__
