#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hardware.h"
#include "win32iface.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"

extern constate_e ConsoleState;

EXTERN_CVAR (Bool, ticker)
EXTERN_CVAR (Bool, fullscreen)
EXTERN_CVAR (Float, vid_winscale)

IVideo *Video;
static IKeyboard *Keyboard;
static IMouse *Mouse;
static IJoystick *Joystick;

void STACK_ARGS I_ShutdownHardware ()
{
	if (screen)
		delete screen, screen = NULL;
	if (Video)
		delete Video, Video = NULL;
}

void I_InitHardware ()
{
	UCVarValue val;

	val.Bool = !!Args.CheckParm ("-devparm");
	ticker.SetGenericRepDefault (val, CVAR_Bool);

	Video = new Win32Video (0);
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	atterm (I_ShutdownHardware);

	Video->SetWindowedScale (vid_winscale);
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

EDisplayType I_DisplayType ()
{
	return Video->GetDisplayType ();
}

DFrameBuffer *I_SetMode (int &width, int &height, DFrameBuffer *old)
{
	bool fs = false;
	switch (Video->GetDisplayType ())
	{
	case DISPLAY_WindowOnly:
		fs = false;
		break;
	case DISPLAY_FullscreenOnly:
		fs = true;
		break;
	case DISPLAY_Both:
		fs = fullscreen;
		break;
	}
	DFrameBuffer *res = Video->CreateFrameBuffer (width, height, fs, old);

	/* Right now, CreateFrameBuffer cannot return NULL
	if (res == NULL)
	{
		I_FatalError ("Mode %dx%d is unavailable\n", width, height);
	}
	*/
	return res;
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->FullscreenChanged (screen ? screen->IsFullscreen() : fullscreen);
	Video->StartModeIterator (bits);
	while (Video->NextMode (&twidth, &theight))
	{
		if (width == twidth && height == theight)
			return true;
	}
	return false;
}

void I_ClosestResolution (int *width, int *height, int bits)
{
	int twidth, theight;
	int cwidth = 0, cheight = 0;
	int iteration;
	DWORD closest = 4294967295u;
	bool fs;

	if (screen != NULL)
	{
		fs = screen->IsFullscreen ();
	}
	else
	{
		fs = fullscreen;
	}
	Video->FullscreenChanged (screen ? screen->IsFullscreen() : fullscreen);
	for (iteration = 0; iteration < 2; iteration++)
	{
		Video->StartModeIterator (bits);
		while (Video->NextMode (&twidth, &theight))
		{
			if (twidth == *width && theight == *height)
				return;

			if (iteration == 0 && (twidth < *width || theight < *height))
				continue;

			DWORD dist = (twidth - *width) * (twidth - *width)
				+ (theight - *height) * (theight - *height);

			if (dist < closest)
			{
				closest = dist;
				cwidth = twidth;
				cheight = theight;
			}
		}
		if (closest != 4294967295u)
		{
			*width = cwidth;
			*height = cheight;
			return;
		}
	}
}	

void I_StartModeIterator (int bits)
{
	Video->StartModeIterator (bits);
}

bool I_NextMode (int *width, int *height)
{
	return Video->NextMode (width, height);
}

DCanvas *I_NewStaticCanvas (int width, int height)
{
	return new DSimpleCanvas (width, height);
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;

CUSTOM_CVAR (Bool, fullscreen, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (Video->FullscreenChanged (self))
	{
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CUSTOM_CVAR (Float, vid_winscale, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 1.f)
	{
		self = 1.f;
	}
	else if (Video)
	{
		Video->SetWindowedScale (self);
		NewWidth = screen->GetWidth();
		NewHeight = screen->GetHeight();
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}

CCMD (vid_listmodes)
{
	int width, height, bits;

	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits);
		while (Video->NextMode (&width, &height))
			if (width == DisplayWidth && height == DisplayHeight && bits == DisplayBits)
				Printf_Bold ("%4d x%5d x%3d\n", width, height, bits);
			else
				Printf ("%4d x%5d x%3d\n", width, height, bits);
	}
}

CCMD (vid_currentmode)
{
	Printf ("%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}
