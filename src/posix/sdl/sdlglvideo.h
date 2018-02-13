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

	DFrameBuffer *CreateFrameBuffer (int width, int height, bool bgra, bool fs, DFrameBuffer *old);

	void StartModeIterator (int bits, bool fs);
	bool NextMode (int *width, int *height, bool *letterbox);
	bool SetResolution (int width, int height, int bits);

	void SetupPixelFormat(bool allowsoftware, int multisample, const int *glver);

private:
	int IteratorMode;
	int IteratorBits;
};

class SDLBaseFB : public DFrameBuffer
{
	typedef DFrameBuffer Super;
public:
	using DFrameBuffer::DFrameBuffer;
	virtual SDL_Window *GetSDLWindow() = 0;
	
	friend class SDLGLVideo;
};

class SDLGLFB : public SDLBaseFB
{
	typedef SDLBaseFB Super;
public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SDLGLFB (void *hMonitor, int width, int height, int, int, bool fullscreen, bool bgra); 
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

	virtual void ScaleCoordsFromWindow(int16_t &x, int16_t &y);

	SDL_Window *GetSDLWindow() override { return Screen; }

	virtual int GetTrueHeight() { return GetClientHeight(); }
protected:
	bool CanUpdate();
	void SetGammaTable(uint16_t *tbl);
	void ResetGammaTable();
	void InitializeState();

	SDLGLFB () {}
	uint8_t GammaTable[3][256];
	bool UpdatePending;

	SDL_Window *Screen;

	SDL_GLContext GLContext;

	void UpdateColors ();

	int m_Lock;
	Uint16 m_origGamma[3][256];
	bool m_supportsGamma;
};
#endif
