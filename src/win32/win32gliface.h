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


class Win32GLVideo : public IVideo
{
public:
	Win32GLVideo(int parm);
	virtual ~Win32GLVideo();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);
	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool GoFullscreen(bool yes);
	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);
	virtual bool SetResolution (int width, int height, int bits);
	void DumpAdapters();
	bool InitHardware (HWND Window, int multisample);
	void Shutdown();
	bool SetFullscreen(const char *devicename, int w, int h, int bits, int hz);

	HDC m_hDC;

protected:
	struct ModeInfo
	{
		ModeInfo (int inX, int inY, int inBits, int inRealY, int inRefresh)
			: next (NULL),
			width (inX),
			height (inY),
			bits (inBits),
			refreshHz (inRefresh),
			realheight (inRealY)
		{}
		ModeInfo *next;
		int width, height, bits, refreshHz, realheight;
	} *m_Modes;

	ModeInfo *m_IteratorMode;
	int m_IteratorBits;
	bool m_IteratorFS;
	bool m_IsFullscreen;
	int m_trueHeight;
	int m_DisplayWidth, m_DisplayHeight, m_DisplayBits, m_DisplayHz;
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
	void MakeModesList();
	void AddMode(int x, int y, int bits, int baseHeight, int refreshHz);
	void FreeModes();
	bool checkCoreUsability();
public:
	int GetTrueHeight() { return m_trueHeight; }

};



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

	int GetTrueHeight() { return static_cast<Win32GLVideo *>(Video)->GetTrueHeight(); }

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
	void SetGammaTable(WORD * tbl);

	float m_Gamma, m_Brightness, m_Contrast;
	WORD m_origGamma[768];
	BOOL m_supportsGamma;
	bool m_Fullscreen;
	int m_Width, m_Height, m_Bits, m_RefreshHz;
	int m_Lock;
	char m_displayDeviceNameBuffer[CCHDEVICENAME];
	char *m_displayDeviceName;

	friend class Win32GLVideo;

};

#endif //__WIN32GLIFACE_H__
