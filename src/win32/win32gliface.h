#ifndef __WIN32GLIFACE_H__
#define __WIN32GLIFACE_H__

#include "hardware.h"
#include "win32iface.h"
#include "v_video.h"
#include "tarray.h"

extern IVideo *Video;



EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

EXTERN_CVAR(Int, vid_defwidth);
EXTERN_CVAR(Int, vid_defheight);
EXTERN_CVAR(Int, vid_renderer);
EXTERN_CVAR(Int, vid_adapter);

extern IVideo *Video;

struct FRenderer;
FRenderer *gl_CreateInterface();



class Win32GLFrameBuffer : public BaseWinFB
{
	typedef BaseWinFB Super;

public:
	Win32GLFrameBuffer() {}
	// Actually, hMonitor is a HMONITOR, but it's passed as a void * as there
    // look to be some cross-platform bits in the way.
	Win32GLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen, bool bgra);
	virtual ~Win32GLFrameBuffer();


	// unused but must be defined
	virtual void Blank ();
	virtual bool PaintToWindow ();
	virtual long/*HRESULT*/ GetHR();	// windows.h pollution prevention.

	virtual bool CreateResources ();
	virtual void ReleaseResources ();

	void SetVSync (bool vsync);
	void SwapBuffers();
	void NewRefreshRate ();

	int GetClientWidth();
	int GetClientHeight();

	int GetTrueHeight();

	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();


	bool IsFullscreen();
	void PaletteChanged();
	int QueryNewPalette();

	void InitializeState();

protected:

	bool CanUpdate();
	void ResetGammaTable();
	void SetGammaTable(uint16_t * tbl);

	float m_Gamma, m_Brightness, m_Contrast;
	uint16_t m_origGamma[768];
	bool m_supportsGamma;
	bool m_Fullscreen, m_Bgra;
	int m_Width, m_Height, m_Bits, m_RefreshHz;
	int m_Lock;
	char m_displayDeviceNameBuffer[32/*CCHDEVICENAME*/];	// do not use windows.h constants here!
	char *m_displayDeviceName;
	int SwapInterval;

	friend class Win32GLVideo;

};

#endif //__WIN32GLIFACE_H__
