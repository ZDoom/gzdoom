/*
** sdlglvideo.cpp
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Christoph Oelckers et.al.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "i_module.h"
#include "i_system.h"
#include "i_video.h"
#include "m_argv.h"
#include "v_video.h"
#include "version.h"
#include "c_console.h"
#include "s_sound.h"

#include "hardware.h"
#include "gl_sysfb.h"
#include "gl_load/gl_system.h"
#include "r_defs.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_adapter)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_maxfps)
EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Bool, cl_capfps)

// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Bool, gl_debug, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}
CUSTOM_CVAR(Bool, gl_es, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

CVAR(Bool, i_soundinbackground, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

CVAR (Int, vid_adapter, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

// PRIVATE DATA DEFINITIONS ------------------------------------------------

class SDLGLVideo : public IVideo
{
public:
	SDLGLVideo (int parm);
	~SDLGLVideo ();

	DFrameBuffer *CreateFrameBuffer ();

	void SetupPixelFormat(bool allowsoftware, int multisample, const int *glver);
};

// CODE --------------------------------------------------------------------

SDLGLVideo::SDLGLVideo (int parm)
{
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
    }
}

SDLGLVideo::~SDLGLVideo ()
{
}

DFrameBuffer *SDLGLVideo::CreateFrameBuffer ()
{
	SystemGLFrameBuffer *fb = new OpenGLRenderer::OpenGLFrameBuffer(0, fullscreen);

	return fb;
}

//==========================================================================
//
// 
//
//==========================================================================

void SDLGLVideo::SetupPixelFormat(bool allowsoftware, int multisample, const int *glver)
{
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,  24 );
	SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,  8 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,  1 );
	if (multisample > 0) {
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, multisample );
	}
	if (gl_debug)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		
	if (gl_es)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	}
	else if (glver[0] > 2)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, glver[0]);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, glver[1]);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	}
}


IVideo *gl_CreateVideo()
{
	return new SDLGLVideo(0);
}


// FrameBuffer implementation -----------------------------------------------

FModule sdl_lib("SDL2");

typedef int (*SDL_GetWindowBordersSizePtr)(SDL_Window *, int *, int *, int *, int *);
static TOptProc<sdl_lib, SDL_GetWindowBordersSizePtr> SDL_GetWindowBordersSize_("SDL_GetWindowBordersSize");

SystemGLFrameBuffer::SystemGLFrameBuffer (void *, bool fullscreen)
	: DFrameBuffer (vid_defwidth, vid_defheight)
{
	m_fsswitch = false;

	// SDL_GetWindowBorderSize() is only available since 2.0.5, but because
	// GZDoom supports platforms with older SDL2 versions, this function
	// has to be dynamically loaded
	if (!sdl_lib.IsLoaded())
	{
		sdl_lib.Load({ "libSDL2.so", "libSDL2-2.0.so" });
	}

	// NOTE: Core profiles were added with GL 3.2, so there's no sense trying
	// to set core 3.1 or 3.0. We could try a forward-compatible context
	// instead, but that would be too restrictive (w.r.t. shaders).
	static const int glvers[][2] = {
		{ 4, 5 }, { 4, 4 }, { 4, 3 }, { 4, 2 }, { 4, 1 }, { 4, 0 },
		{ 3, 3 }, { 3, 2 }, { 2, 0 },
		{ 0, 0 },
	};
	int glveridx = 0;
	int i;

	UpdatePending = false;

	const char *version = Args->CheckValue("-glversion");
	if (version != NULL)
	{
		double gl_version = strtod(version, NULL) + 0.01;
		int vermaj = (int)gl_version;
		int vermin = (int)(gl_version*10.0) % 10;

		while (glvers[glveridx][0] > vermaj || (glvers[glveridx][0] == vermaj &&
		        glvers[glveridx][1] > vermin))
		{
			glveridx++;
			if (glvers[glveridx][0] == 0)
			{
				glveridx = 0;
				break;
			}
		}
	}

	FString caption;
	caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());

	for ( ; glvers[glveridx][0] > 0; ++glveridx)
	{
		static_cast<SDLGLVideo*>(Video)->SetupPixelFormat(false, 0, glvers[glveridx]);

		SDL_Rect bounds;
		SDL_GetDisplayBounds(vid_adapter,&bounds);
		// set default size
		if ( win_w <= 0 || win_h <= 0 )
		{
			win_w = bounds.w * 8 / 10;
			win_h = bounds.h * 8 / 10;
		}

		Screen = SDL_CreateWindow(caption,
			(win_x <= 0) ? SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter) : win_x,
			(win_y <= 0) ? SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter) : win_y,
			win_w, win_h, (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) | (win_maximized ? SDL_WINDOW_MAXIMIZED : 0) | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		if (Screen != NULL)
		{
			// enforce minimum size limit
			SDL_SetWindowMinimumSize(Screen, MIN_WIDTH, MIN_HEIGHT);

			GLContext = SDL_GL_CreateContext(Screen);
			if (GLContext != NULL)
			{
				m_supportsGamma = -1 != SDL_GetWindowGammaRamp(Screen,
					 m_origGamma[0], m_origGamma[1], m_origGamma[2]
				);

				return;
			}

			SDL_DestroyWindow(Screen);
			Screen = NULL;
		}
	}
}

SystemGLFrameBuffer::~SystemGLFrameBuffer ()
{
	if (Screen)
	{
		ResetGammaTable();

		if (GLContext)
		{
			SDL_GL_DeleteContext(GLContext);
		}

		SDL_DestroyWindow(Screen);
	}
}


void SystemGLFrameBuffer::SetGammaTable(uint16_t *tbl)
{
	if (m_supportsGamma)
	{
		SDL_SetWindowGammaRamp(Screen, &tbl[0], &tbl[256], &tbl[512]);
	}
}

void SystemGLFrameBuffer::ResetGammaTable()
{
	if (m_supportsGamma)
	{
		SDL_SetWindowGammaRamp(Screen, m_origGamma[0], m_origGamma[1], m_origGamma[2]);
	}
}

bool SystemGLFrameBuffer::IsFullscreen ()
{
	return (SDL_GetWindowFlags (Screen) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

void SystemGLFrameBuffer::SetVSync( bool vsync )
{
#if defined (__APPLE__)
	const GLint value = vsync ? 1 : 0;
	CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval, &value );
#else
	if (vsync)
	{
		if (SDL_GL_SetSwapInterval(-1) == -1)
			SDL_GL_SetSwapInterval(1);
	}
	else
	{
		SDL_GL_SetSwapInterval(0);
	}
#endif
}

void SystemGLFrameBuffer::SwapBuffers()
{
#if !defined(__APPLE__) && !defined(__OpenBSD__)
	if (vid_maxfps && !cl_capfps)
	{
		SEMAPHORE_WAIT(FPSLimitSemaphore)
	}
#endif

	SDL_GL_SwapWindow (Screen);
}

void SystemGLFrameBuffer::ToggleFullscreen(bool yes)
{
	SDL_SetWindowFullscreen(Screen, yes ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if ( !yes )
	{
		if ( !m_fsswitch )
		{
			m_fsswitch = true;
			fullscreen = false;
		}
		else
		{
			m_fsswitch = false;
			SetWindowSize(win_w, win_h);
		}
	}
}

int SystemGLFrameBuffer::GetClientWidth()
{
	int width = 0;
	SDL_GL_GetDrawableSize(Screen, &width, nullptr);
	return width;
}

int SystemGLFrameBuffer::GetClientHeight()
{
	int height = 0;
	SDL_GL_GetDrawableSize(Screen, nullptr, &height);
	return height;
}

void SystemGLFrameBuffer::SetWindowSize(int w, int h)
{
	if (w < MIN_WIDTH || h < MIN_HEIGHT)
	{
		w = MIN_WIDTH;
		h = MIN_HEIGHT;
	}
	win_w = w;
	win_h = h;
	if ( fullscreen )
	{
		fullscreen = false;
	}
	else
	{
		win_maximized = false;
		SDL_SetWindowSize(Screen, w, h);
		SDL_SetWindowPosition(Screen, SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter), SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter));
		SetSize(GetClientWidth(), GetClientHeight());
		int x, y;
		SDL_GetWindowPosition(Screen, &x, &y);
		win_x = x;
		win_y = y;
	}
}

void SystemGLFrameBuffer::GetWindowBordersSize(int &top, int &left)
{
	if (SDL_GetWindowBordersSize_)
	{
		SDL_GetWindowBordersSize_(Screen, &top, &left, nullptr, nullptr);
	}
}

void ProcessSDLWindowEvent(const SDL_WindowEvent &event)
{
	switch (event.event)
	{
	extern bool AppActive;

	case SDL_WINDOWEVENT_FOCUS_GAINED:
		S_SetSoundPaused(1);
		AppActive = true;
		break;

	case SDL_WINDOWEVENT_FOCUS_LOST:
		S_SetSoundPaused(i_soundinbackground);
		AppActive = false;
		break;

	case SDL_WINDOWEVENT_MOVED:
		if (!fullscreen)
		{
			int top = 0, left = 0;
			static_cast<SystemGLFrameBuffer *>(screen)->GetWindowBordersSize(top,left);
			win_x = event.data1-left;
			win_y = event.data2-top;
		}
		break;

	case SDL_WINDOWEVENT_RESIZED:
		if (!fullscreen && !(static_cast<SystemGLFrameBuffer *>(screen)->m_fsswitch))
		{
			win_w = event.data1;
			win_h = event.data2;
		}
		break;

	case SDL_WINDOWEVENT_MAXIMIZED:
		win_maximized = true;
		break;

	case SDL_WINDOWEVENT_RESTORED:
		win_maximized = false;
		break;
	}
}


// each platform has its own specific version of this function.
void I_SetWindowTitle(const char* caption)
{
	auto window = static_cast<SystemGLFrameBuffer *>(screen)->GetSDLWindow();
	if (caption)
		SDL_SetWindowTitle(window, caption);
	else
	{
		FString default_caption;
		default_caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());
		SDL_SetWindowTitle(window, default_caption);
	}
}

