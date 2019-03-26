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

#ifdef HAVE_VULKAN
#include "rendering/vulkan/system/vk_framebuffer.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;

EXTERN_CVAR (Int, vid_adapter)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Int, vid_backend)
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

namespace Priv
{
	FModule library("SDL2");

#define SDL2_OPTIONAL_FUNCTION(RESULT, NAME, ...) \
	static TOptProc<library, RESULT(*)(__VA_ARGS__)> NAME("SDL_" #NAME)

	SDL2_OPTIONAL_FUNCTION(int,      GetWindowBordersSize,         SDL_Window *window, int *top, int *left, int *bottom, int *right);
#ifdef HAVE_VULKAN
	SDL2_OPTIONAL_FUNCTION(void,     Vulkan_GetDrawableSize,       SDL_Window *window, int *width, int *height);
	SDL2_OPTIONAL_FUNCTION(SDL_bool, Vulkan_GetInstanceExtensions, SDL_Window *window, unsigned int *count, const char **names);
	SDL2_OPTIONAL_FUNCTION(SDL_bool, Vulkan_CreateSurface,         SDL_Window *window, VkInstance instance, VkSurfaceKHR *surface);
#endif

#undef SDL2_OPTIONAL_FUNCTION

	static const uint32_t VulkanWindowFlag = 0x1000'0000;

	static const int MIN_WIDTH = 320;
	static const int MIN_HEIGHT = 200;

	SDL_Window *window;
	bool vulkanEnabled;
	bool fullscreenSwitch;

	void CreateWindow(uint32_t extraFlags)
	{
		assert(Priv::window == nullptr);

		// Set default size
		SDL_Rect bounds;
		SDL_GetDisplayBounds(vid_adapter, &bounds);

		if (win_w <= 0 || win_h <= 0)
		{
			win_w = bounds.w * 8 / 10;
			win_h = bounds.h * 8 / 10;
		}

		FString caption;
		caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());

		const uint32_t windowFlags = (win_maximized ? SDL_WINDOW_MAXIMIZED : 0) | SDL_WINDOW_RESIZABLE | extraFlags;
		Priv::window = SDL_CreateWindow(caption,
			(win_x <= 0) ? SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter) : win_x,
			(win_y <= 0) ? SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter) : win_y,
			win_w, win_h, windowFlags);

		if (Priv::window != nullptr)
		{
			// Enforce minimum size limit
			SDL_SetWindowMinimumSize(Priv::window, Priv::MIN_WIDTH, Priv::MIN_HEIGHT);
		}
	}

	void SetupPixelFormat(int multisample, const int *glver)
	{
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		if (multisample > 0) {
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
			SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, multisample);
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
}

class SDLVideo : public IVideo
{
public:
	SDLVideo ();
	~SDLVideo ();

	DFrameBuffer *CreateFrameBuffer ();

private:
#ifdef HAVE_VULKAN
	VulkanDevice *device = nullptr;
#endif
};

// CODE --------------------------------------------------------------------

#ifdef HAVE_VULKAN
void I_GetVulkanDrawableSize(int *width, int *height)
{
	assert(Priv::vulkanEnabled);
	assert(Priv::window != nullptr);
	assert(Priv::Vulkan_GetDrawableSize);
	Priv::Vulkan_GetDrawableSize(Priv::window, width, height);
}

bool I_GetVulkanPlatformExtensions(unsigned int *count, const char **names)
{
	assert(Priv::vulkanEnabled);
	assert(Priv::window != nullptr);
	return Priv::Vulkan_GetInstanceExtensions(Priv::window, count, names) == SDL_TRUE;
}

bool I_CreateVulkanSurface(VkInstance instance, VkSurfaceKHR *surface)
{
	assert(Priv::vulkanEnabled);
	assert(Priv::window != nullptr);
	return Priv::Vulkan_CreateSurface(Priv::window, instance, surface) == SDL_TRUE;
}
#endif


SDLVideo::SDLVideo ()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		fprintf(stderr, "Video initialization failed: %s\n", SDL_GetError());
		return;
	}

	// Load optional SDL functions
	if (!Priv::library.IsLoaded())
	{
		Priv::library.Load({ "libSDL2.so", "libSDL2-2.0.so" });
	}

#ifdef HAVE_VULKAN
	Priv::vulkanEnabled = vid_backend == 0
		&& Priv::Vulkan_GetDrawableSize && Priv::Vulkan_GetInstanceExtensions && Priv::Vulkan_CreateSurface;

	if (Priv::vulkanEnabled)
	{
		Priv::CreateWindow(Priv::VulkanWindowFlag | SDL_WINDOW_HIDDEN);

		if (Priv::window == nullptr)
		{
			Priv::vulkanEnabled = false;
		}
	}
#endif
}

SDLVideo::~SDLVideo ()
{
	delete device;
}

DFrameBuffer *SDLVideo::CreateFrameBuffer ()
{
	SystemBaseFrameBuffer *fb = nullptr;

	// first try Vulkan, if that fails OpenGL
#ifdef HAVE_VULKAN
	if (Priv::vulkanEnabled)
	{
		try
		{
			assert(device == nullptr);
			device = new VulkanDevice();
			fb = new VulkanFrameBuffer(nullptr, fullscreen, device);
		}
		catch (CRecoverableError const&)
		{
			Priv::vulkanEnabled = false;
		}
	}
#endif

	if (fb == nullptr)
	{
		fb = new OpenGLRenderer::OpenGLFrameBuffer(0, fullscreen);
	}

	return fb;
}


IVideo *gl_CreateVideo()
{
	return new SDLVideo();
}


// FrameBuffer Implementation -----------------------------------------------

SystemBaseFrameBuffer::SystemBaseFrameBuffer (void *, bool fullscreen)
: DFrameBuffer (vid_defwidth, vid_defheight)
{
	if (Priv::window != nullptr)
	{
		SDL_SetWindowFullscreen(Priv::window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
		SDL_ShowWindow(Priv::window);
	}
}

int SystemBaseFrameBuffer::GetClientWidth()
{
	int width = 0;

#ifdef HAVE_VULKAN
	assert(Priv::vulkanEnabled);
	Priv::Vulkan_GetDrawableSize(Priv::window, &width, nullptr);
#endif

	return width;
}

int SystemBaseFrameBuffer::GetClientHeight()
{
	int height = 0;

#ifdef HAVE_VULKAN
	assert(Priv::vulkanEnabled);
	Priv::Vulkan_GetDrawableSize(Priv::window, nullptr, &height);
#endif

	return height;
}

bool SystemBaseFrameBuffer::IsFullscreen ()
{
	return (SDL_GetWindowFlags(Priv::window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

void SystemBaseFrameBuffer::ToggleFullscreen(bool yes)
{
	SDL_SetWindowFullscreen(Priv::window, yes ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	if ( !yes )
	{
		if ( !Priv::fullscreenSwitch )
		{
			Priv::fullscreenSwitch = true;
			fullscreen = false;
		}
		else
		{
			Priv::fullscreenSwitch = false;
			SetWindowSize(win_w, win_h);
		}
	}
}

void SystemBaseFrameBuffer::SetWindowSize(int w, int h)
{
	if (w < Priv::MIN_WIDTH || h < Priv::MIN_HEIGHT)
	{
		w = Priv::MIN_WIDTH;
		h = Priv::MIN_HEIGHT;
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
		SDL_SetWindowSize(Priv::window, w, h);
		SDL_SetWindowPosition(Priv::window, SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter), SDL_WINDOWPOS_CENTERED_DISPLAY(vid_adapter));
		SetSize(GetClientWidth(), GetClientHeight());
		int x, y;
		SDL_GetWindowPosition(Priv::window, &x, &y);
		win_x = x;
		win_y = y;
	}
}


SystemGLFrameBuffer::SystemGLFrameBuffer(void *hMonitor, bool fullscreen)
: SystemBaseFrameBuffer(hMonitor, fullscreen)
{
	// NOTE: Core profiles were added with GL 3.2, so there's no sense trying
	// to set core 3.1 or 3.0. We could try a forward-compatible context
	// instead, but that would be too restrictive (w.r.t. shaders).
	static const int glvers[][2] = {
		{ 4, 6 }, { 4, 5 }, { 4, 4 }, { 4, 3 }, { 4, 2 }, { 4, 1 }, { 4, 0 },
		{ 3, 3 }, { 3, 2 }, { 2, 0 },
		{ 0, 0 },
	};
	int glveridx = 0;
	int i;

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

	for ( ; glvers[glveridx][0] > 0; ++glveridx)
	{
		Priv::SetupPixelFormat(0, glvers[glveridx]);
		Priv::CreateWindow(SDL_WINDOW_OPENGL | (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));

		if (Priv::window == nullptr)
		{
			continue;
		}

		GLContext = SDL_GL_CreateContext(Priv::window);
		if (GLContext == nullptr)
		{
			SDL_DestroyWindow(Priv::window);
			Priv::window = nullptr;
		}
		else
		{
			break;
		}
	}
}

SystemGLFrameBuffer::~SystemGLFrameBuffer ()
{
	if (Priv::window)
	{
		if (GLContext)
		{
			SDL_GL_DeleteContext(GLContext);
		}

		SDL_DestroyWindow(Priv::window);
		Priv::window = nullptr;
	}
}

int SystemGLFrameBuffer::GetClientWidth()
{
	int width = 0;
	SDL_GL_GetDrawableSize(Priv::window, &width, nullptr);
	return width;
}

int SystemGLFrameBuffer::GetClientHeight()
{
	int height = 0;
	SDL_GL_GetDrawableSize(Priv::window, nullptr, &height);
	return height;
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
	SDL_GL_SwapWindow(Priv::window);
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
		if (!fullscreen && Priv::GetWindowBordersSize)
		{
			int top = 0, left = 0;
			Priv::GetWindowBordersSize(Priv::window, &top, &left, nullptr, nullptr);
			win_x = event.data1-left;
			win_y = event.data2-top;
		}
		break;

	case SDL_WINDOWEVENT_RESIZED:
		if (!fullscreen && !Priv::fullscreenSwitch)
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
	if (caption)
	{
		SDL_SetWindowTitle(Priv::window, caption);
	}
	else
	{
		FString default_caption;
		default_caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());
		SDL_SetWindowTitle(Priv::window, default_caption);
	}
}

