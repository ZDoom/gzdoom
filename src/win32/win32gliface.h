#ifndef __WIN32GLIFACE_H__
#define __WIN32GLIFACE_H__

#include "hardware.h"
#include "win32iface.h"
#include "v_video.h"
#include "tarray.h"

extern IVideo *Video;


extern BOOL AppActive;

EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

EXTERN_CVAR(Int, vid_defwidth);
EXTERN_CVAR(Int, vid_defheight);
EXTERN_CVAR(Int, vid_renderer);
EXTERN_CVAR(Int, vid_adapter);

extern HINSTANCE g_hInst;
extern HWND Window;
extern IVideo *Video;

struct FRenderer;
FRenderer *gl_CreateInterface();



class Win32GLFrameBuffer : public BaseWinFB
{
	DECLARE_CLASS(Win32GLFrameBuffer, BaseWinFB)

public:
	Win32GLFrameBuffer() {}
	// Actually, hMonitor is a HMONITOR, but it's passed as a void * as there
    // look to be some cross-platform bits in the way.
	Win32GLFrameBuffer(void *hMonitor, int width, int height, int bits, int refreshHz, bool fullscreen);
	virtual ~Win32GLFrameBuffer();


	// unused but must be defined
	virtual void Blank ();
	virtual bool PaintToWindow ();
	virtual HRESULT GetHR();

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
	WORD m_origGamma[768];
	BOOL m_supportsGamma;
	bool m_Fullscreen;
	int m_Width, m_Height, m_Bits, m_RefreshHz;
	int m_Lock;
	char m_displayDeviceNameBuffer[CCHDEVICENAME];
	char *m_displayDeviceName;
	int SwapInterval;

	friend class Win32GLVideo;

};

#endif //__WIN32GLIFACE_H__
