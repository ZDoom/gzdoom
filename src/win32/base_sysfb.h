#pragma once

#include "v_video.h"

class SystemBaseFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;

	void SaveWindowedPos();
	void RestoreWindowedPos();

public:
	SystemBaseFrameBuffer();
	// Actually, hMonitor is a HMONITOR, but it's passed as a void * as there
    // look to be some cross-platform bits in the way.
	SystemBaseFrameBuffer(void *hMonitor, bool fullscreen);
	virtual ~SystemBaseFrameBuffer();

	int GetClientWidth() override;
	int GetClientHeight() override;

	bool IsFullscreen() override;
	void ToggleFullscreen(bool yes) override;
	void SetWindowSize(int client_w, int client_h);

protected:

	void GetCenteredPos(int in_w, int in_h, int &winx, int &winy, int &winw, int &winh, int &scrwidth, int &scrheight);
	void KeepWindowOnScreen(int &winx, int &winy, int winw, int winh, int scrwidth, int scrheight);

	void PositionWindow(bool fullscreen, bool initialcall = false);

	void ResetGammaTable();
	void SetGammaTable(uint16_t * tbl);

	float m_Gamma, m_Brightness, m_Contrast;
	uint16_t m_origGamma[768];
	bool m_supportsGamma;
	bool m_Fullscreen = false;
	char m_displayDeviceNameBuffer[32/*CCHDEVICENAME*/];	// do not use windows.h constants here!
	char *m_displayDeviceName;
	void *m_Monitor;
};
