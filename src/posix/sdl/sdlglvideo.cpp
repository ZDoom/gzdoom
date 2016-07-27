
// HEADER FILES ------------------------------------------------------------

#include "doomtype.h"

#include "templates.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"
#include "v_pfx.h"
#include "stats.h"
#include "version.h"
#include "c_console.h"

#include "sdlglvideo.h"
#include "gl/system/gl_system.h"
#include "r_defs.h"
#include "gl/gl_functions.h"
//#include "gl/gl_intern.h"

#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/utility/gl_templates.h"
#include "gl/textures/gl_material.h"
#include "gl/system/gl_cvars.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

IMPLEMENT_ABSTRACT_CLASS(SDLGLFB)

struct MiniModeInfo
{
	WORD Width, Height;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern IVideo *Video;
// extern int vid_renderer;

EXTERN_CVAR (Float, Gamma)
EXTERN_CVAR (Int, vid_adapter)
EXTERN_CVAR (Int, vid_displaybits)
EXTERN_CVAR (Int, vid_renderer)


// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR(Int, gl_vid_multisample, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL )
{
	Printf("This won't take effect until " GAMENAME " is restarted.\n");
}

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Dummy screen sizes to pass when windowed
static MiniModeInfo WinModes[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 400, 225 },	// 16:9
	{ 400, 300 },
	{ 480, 270 },	// 16:9
	{ 480, 360 },
	{ 512, 288 },	// 16:9
	{ 512, 384 },
	{ 640, 360 },	// 16:9
	{ 640, 400 },
	{ 640, 480 },
	{ 720, 480 },	// 16:10
	{ 720, 540 },
	{ 800, 450 },	// 16:9
	{ 800, 480 },
	{ 800, 500 },	// 16:10
	{ 800, 600 },
	{ 848, 480 },	// 16:9
	{ 960, 600 },	// 16:10
	{ 960, 720 },
	{ 1024, 576 },	// 16:9
	{ 1024, 600 },	// 17:10
	{ 1024, 640 },	// 16:10
	{ 1024, 768 },
	{ 1088, 612 },	// 16:9
	{ 1152, 648 },	// 16:9
	{ 1152, 720 },	// 16:10
	{ 1152, 864 },
	{ 1280, 720 },	// 16:9
	{ 1280, 854 },
	{ 1280, 800 },	// 16:10
	{ 1280, 960 },
	{ 1280, 1024 },	// 5:4
	{ 1360, 768 },	// 16:9
	{ 1366, 768 },
	{ 1400, 787 },	// 16:9
	{ 1400, 875 },	// 16:10
	{ 1400, 1050 },
	{ 1440, 900 },
	{ 1440, 960 },
	{ 1440, 1080 },
	{ 1600, 900 },	// 16:9
	{ 1600, 1000 },	// 16:10
	{ 1600, 1200 },
	{ 1920, 1080 },
	{ 1920, 1200 },
	{ 2048, 1536 },
	{ 2560, 1440 },
	{ 2560, 1600 },
	{ 2560, 2048 },
	{ 2880, 1800 },
	{ 3200, 1800 },
	{ 3840, 2160 },
	{ 3840, 2400 },
	{ 4096, 2160 },
	{ 5120, 2880 }
};

// CODE --------------------------------------------------------------------

SDLGLVideo::SDLGLVideo (int parm)
{
	IteratorBits = 0;
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
    }
#ifndef	_WIN32
	// mouse cursor is visible by default on linux systems, we disable it by default
	SDL_ShowCursor (0);
#endif
}

SDLGLVideo::~SDLGLVideo ()
{
	if (GLRenderer != NULL) GLRenderer->FlushTextures();
}

void SDLGLVideo::StartModeIterator (int bits, bool fs)
{
	IteratorMode = 0;
	IteratorBits = bits;
}

bool SDLGLVideo::NextMode (int *width, int *height, bool *letterbox)
{
	if (IteratorBits != 8)
		return false;

	if ((unsigned)IteratorMode < sizeof(WinModes)/sizeof(WinModes[0]))
	{
		*width = WinModes[IteratorMode].Width;
		*height = WinModes[IteratorMode].Height;
		++IteratorMode;
		return true;
	}
	return false;
}

DFrameBuffer *SDLGLVideo::CreateFrameBuffer (int width, int height, bool fullscreen, DFrameBuffer *old)
{
	static int retry = 0;
	static int owidth, oheight;
	
	PalEntry flashColor;
//	int flashAmount;

	if (old != NULL)
	{ // Reuse the old framebuffer if its attributes are the same
		SDLGLFB *fb = static_cast<SDLGLFB *> (old);
		if (fb->Width == width &&
			fb->Height == height)
		{
			bool fsnow = (SDL_GetWindowFlags (fb->Screen) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
	
			if (fsnow != fullscreen)
			{
				SDL_SetWindowFullscreen (fb->Screen, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
			}
			return old;
		}
//		old->GetFlash (flashColor, flashAmount);
		delete old;
	}
	else
	{
		flashColor = 0;
//		flashAmount = 0;
	}
	
	SDLGLFB *fb = new OpenGLFrameBuffer (0, width, height, 32, 60, fullscreen);
	retry = 0;
	
	// If we could not create the framebuffer, try again with slightly
	// different parameters in this order:
	// 1. Try with the closest size
	// 2. Try in the opposite screen mode with the original size
	// 3. Try in the opposite screen mode with the closest size
	// This is a somewhat confusing mass of recursion here.

	while (fb == NULL || !fb->IsValid ())
	{
		if (fb != NULL)
		{
			delete fb;
		}

		switch (retry)
		{
		case 0:
			owidth = width;
			oheight = height;
		case 2:
			// Try a different resolution. Hopefully that will work.
			I_ClosestResolution (&width, &height, 8);
			break;

		case 1:
			// Try changing fullscreen mode. Maybe that will work.
			width = owidth;
			height = oheight;
			fullscreen = !fullscreen;
			break;

		default:
			// I give up!
			I_FatalError ("Could not create new screen (%d x %d)", owidth, oheight);

			fprintf( stderr, "!!! [SDLGLVideo::CreateFrameBuffer] Got beyond I_FatalError !!!" );
			return NULL;	//[C] actually this shouldn't be reached; probably should be replaced with an ASSERT
		}

		++retry;
		fb = static_cast<SDLGLFB *>(CreateFrameBuffer (width, height, fullscreen, NULL));
	}

//	fb->SetFlash (flashColor, flashAmount);
	return fb;
}

void SDLGLVideo::SetWindowedScale (float scale)
{
}

bool SDLGLVideo::SetResolution (int width, int height, int bits)
{
	// FIXME: Is it possible to do this without completely destroying the old
	// interface?
#ifndef NO_GL

	if (GLRenderer != NULL) GLRenderer->FlushTextures();
	I_ShutdownGraphics();

	Video = new SDLGLVideo(0);
	if (Video == NULL) I_FatalError ("Failed to initialize display");

#if (defined(WINDOWS)) || defined(WIN32)
	bits=32;
#else
	bits=24;
#endif
	
	V_DoModeSetup(width, height, bits);
#endif
	return true;	// We must return true because the old video context no longer exists.
}

//==========================================================================
//
// 
//
//==========================================================================

bool SDLGLVideo::SetupPixelFormat(bool allowsoftware, int multisample)
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
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================

bool SDLGLVideo::InitHardware (bool allowsoftware, int multisample)
{
	if (!SetupPixelFormat(allowsoftware, multisample))
	{
		Printf ("R_OPENGL: Reverting to software mode...\n");
		return false;
	}
	return true;
}


// FrameBuffer implementation -----------------------------------------------

SDLGLFB::SDLGLFB (void *, int width, int height, int, int, bool fullscreen)
	: DFrameBuffer (width, height)
{
	static int localmultisample=-1;

	if (localmultisample<0) localmultisample=gl_vid_multisample;

	int i;
	
	m_Lock=0;

	UpdatePending = false;
	
	if (!static_cast<SDLGLVideo*>(Video)->InitHardware(false, localmultisample))
	{
		vid_renderer = 0;
		return;
	}

	FString caption;
	caption.Format(GAMESIG " %s (%s)", GetVersionString(), GetGitTime());
	Screen = SDL_CreateWindow (caption,
		SDL_WINDOWPOS_UNDEFINED_DISPLAY(vid_adapter), SDL_WINDOWPOS_UNDEFINED_DISPLAY(vid_adapter),
		width, height, (fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0)|SDL_WINDOW_OPENGL);

	if (Screen == NULL)
		return;

	GLContext = SDL_GL_CreateContext(Screen);
	if (GLContext == NULL)
		return;

	m_supportsGamma = -1 != SDL_GetWindowGammaRamp(Screen, m_origGamma[0], m_origGamma[1], m_origGamma[2]);
}

SDLGLFB::~SDLGLFB ()
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




void SDLGLFB::InitializeState() 
{
}

bool SDLGLFB::CanUpdate ()
{
	if (m_Lock != 1)
	{
		if (m_Lock > 0)
		{
			UpdatePending = true;
			--m_Lock;
		}
		return false;
	}
	return true;
}

void SDLGLFB::SetGammaTable(WORD *tbl)
{
	if (m_supportsGamma)
	{
		SDL_SetWindowGammaRamp(Screen, &tbl[0], &tbl[256], &tbl[512]);
	}
}

void SDLGLFB::ResetGammaTable()
{
	if (m_supportsGamma)
	{
		SDL_SetWindowGammaRamp(Screen, m_origGamma[0], m_origGamma[1], m_origGamma[2]);
	}
}

bool SDLGLFB::Lock(bool buffered)
{
	m_Lock++;
	Buffer = MemBuffer;
	return true;
}

bool SDLGLFB::Lock () 
{ 	
	return Lock(false); 
}

void SDLGLFB::Unlock () 	
{ 
	if (UpdatePending && m_Lock == 1)
	{
		Update ();
	}
	else if (--m_Lock <= 0)
	{
		m_Lock = 0;
	}
}

bool SDLGLFB::IsLocked () 
{ 
	return m_Lock>0;// true;
}

bool SDLGLFB::IsFullscreen ()
{
	return (SDL_GetWindowFlags (Screen) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}


bool SDLGLFB::IsValid ()
{
	return DFrameBuffer::IsValid() && Screen != NULL;
}

void SDLGLFB::SetVSync( bool vsync )
{
#if defined (__APPLE__)
	const GLint value = vsync ? 1 : 0;
	CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval, &value );
#endif
}

void SDLGLFB::NewRefreshRate ()
{
}

void SDLGLFB::SwapBuffers()
{
	SDL_GL_SwapWindow (Screen);
}

int SDLGLFB::GetClientWidth()
{
	int width = 0;
	SDL_GL_GetDrawableSize(Screen, &width, nullptr);
	return width;
}

int SDLGLFB::GetClientHeight()
{
	int height = 0;
	SDL_GL_GetDrawableSize(Screen, nullptr, &height);
	return height;
}
