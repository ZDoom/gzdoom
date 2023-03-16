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

	bool IsFullscreen() override;

	int GetClientWidth() override;
	int GetClientHeight() override;

	void ToggleFullscreen(bool yes) override;
	void SetWindowSize(int client_w, int client_h) override;

protected:
	SystemBaseFrameBuffer () {}
};

#endif // __POSIX_SDL_GL_SYSFB_H__

