#ifndef __POSIX_SDL_GL_SYSFB_H__
#define __POSIX_SDL_GL_SYSFB_H__

#include <SDL.h>

#include "v_video.h"

class SystemBaseFrameBuffer : public DFrameBuffer
{
	typedef DFrameBuffer Super;

public:
	// this must have the same parameters as the Windows version, even if they are not used!
	SystemBaseFrameBuffer (void *hMonitor, bool fullscreen);
	~SystemBaseFrameBuffer ();

	void ForceBuffering (bool force);

	bool IsFullscreen ();

	virtual void SetVSync( bool vsync );
	void SwapBuffers();
	
	friend class SDLGLVideo;

	int GetClientWidth() override;
	int GetClientHeight() override;
	void ToggleFullscreen(bool yes) override;
	void SetWindowSize(int client_w, int client_h);

	SDL_Window *GetSDLWindow() { return Screen; }
	void GetWindowBordersSize(int &top, int &left);

	bool m_fsswitch;

protected:
	void SetGammaTable(uint16_t *tbl);
	void ResetGammaTable();

	SystemBaseFrameBuffer () {}
	uint8_t GammaTable[3][256];
	bool UpdatePending;

	SDL_Window *Screen;

	SDL_GLContext GLContext;

	void UpdateColors ();

	static const int MIN_WIDTH = 320;
	static const int MIN_HEIGHT = 200;
};

class SystemGLFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;

public:
	SystemGLFrameBuffer(void *hMonitor, bool fullscreen)
	: SystemBaseFrameBuffer(hMonitor, fullscreen)
	{}

protected:
	SystemGLFrameBuffer() {}
};

#endif // __POSIX_SDL_GL_SYSFB_H__

