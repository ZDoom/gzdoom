#ifndef __SDLGLVIDEO_H__
#define __SDLGLVIDEO_H__

#include "hardware.h"
#include "v_video.h"
#include <SDL.h>
#include "gl/system/gl_system.h"

EXTERN_CVAR (Float, dimamount)
EXTERN_CVAR (Color, dimcolor)

struct FRenderer;
FRenderer *gl_CreateInterface();

class SDLGLVideo : public IVideo
{
 public:
	SDLGLVideo (int parm);
	~SDLGLVideo ();

	EDisplayType GetDisplayType () { return DISPLAY_Both; }
	void SetWindowedScale (float scale);

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool SetResolution (int width, int height, int bits);

	bool SetupPixelFormat(bool allowsoftware, int multisample);
	bool InitHardware (bool allowsoftware, int multisample);

private:
	int IteratorMode;
	int IteratorBits;
};
class SDLGLFB : public DFrameBuffer
{
	DECLARE_CLASS(SDLGLFB, DFrameBuffer)
public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SDLGLFB (void *hMonitor, int width, int height, int, int, bool fullscreen); 
	~SDLGLFB ();

	void ForceBuffering (bool force);
	bool Lock(bool buffered);
	bool Lock ();
	void Unlock();
	bool IsLocked ();

	bool IsValid ();
	bool IsFullscreen ();

	virtual void SetVSync( bool vsync );
	void SwapBuffers();
	
	void NewRefreshRate ();

	friend class SDLGLVideo;

	int GetClientWidth();
	int GetClientHeight();

protected:
	bool CanUpdate();
	void SetGammaTable(WORD *tbl);
	void ResetGammaTable();
	void InitializeState();

	SDLGLFB () {}
	BYTE GammaTable[3][256];
	bool UpdatePending;

	SDL_Window *Screen;

	SDL_GLContext GLContext;

	void UpdateColors ();

	int m_Lock;
	Uint16 m_origGamma[3][256];
	bool m_supportsGamma;
};
#endif
