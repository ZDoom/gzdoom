#include "hardware.h"
#undef MINCHAR
#undef MAXCHAR
#undef MINSHORT
#undef MAXSHORT
#undef MINLONG
#undef MAXLONG
#include "win32iface.h"
#include "i_video.h"
#include "i_system.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"

extern constate_e ConsoleState;

static bool MouseShouldBeGrabbed ();

CVAR (vid_fps, "0", 0)
CVAR (ticker, "0", 0)
EXTERN_CVAR (fullscreen)
EXTERN_CVAR (vid_winscale)

static IVideo *Video;
static IKeyboard *Keyboard;
static IMouse *Mouse;
static IJoystick *Joystick;

void STACK_ARGS I_ShutdownHardware ()
{
	if (Video)
		delete Video, Video = NULL;
}

void I_InitHardware ()
{
	char num[4];
	num[0] = '1' - !Args.CheckParm ("-devparm");
	num[1] = 0;
	ticker.SetDefault (num);

	Video = new Win32Video (0);
	if (Video == NULL)
		I_FatalError ("Failed to initialize display");

	atterm (I_ShutdownHardware);

	Video->SetWindowedScale (vid_winscale.value);
}

/** Remaining code is common to Win32 and Linux **/

// VIDEO WRAPPERS ---------------------------------------------------------

void I_BeginUpdate ()
{
	screen->Lock ();
}

void I_FinishUpdateNoBlit ()
{
	screen->Unlock ();
}

void I_FinishUpdate ()
{
	// Draws frame time and cumulative fps
	if (vid_fps.value)
	{
		static DWORD lastms = 0, lastsec = 0;
		static int framecount = 0, lastcount = 0;
		char fpsbuff[40];
		int chars;

		QWORD ms = I_MSTime ();
		QWORD howlong = ms - lastms;
		if (howlong > 0)
		{
			chars = sprintf (fpsbuff, "%I64d ms (%d fps)",
							 howlong, lastcount);
			screen->Clear (0, screen->height - 8, chars * 8, screen->height, 0);
			screen->PrintStr (0, screen->height - 8, (char *)&fpsbuff[0], chars);
			if (lastsec < ms / 1000)
			{
				lastcount = framecount / (ms/1000 - lastsec);
				lastsec = ms / 1000;
				framecount = 0;
			}
			framecount++;
		}
		lastms = ms;
	}

    // draws little dots on the bottom of the screen
    if (ticker.value)
    {
		static int lasttic = 0;
		int i = I_GetTime();
		int tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;
		
		for (i=0 ; i<tics*2 ; i+=2)
			screen->buffer[(screen->height-1)*screen->pitch + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
			screen->buffer[(screen->height-1)*screen->pitch + i] = 0x0;
    }

	Video->UpdateScreen (screen);
}

void I_ReadScreen (byte *block)
{
	Video->ReadScreen (block);
}

void I_SetPalette (DWORD *pal)
{
	Video->SetPalette (pal);
}

EDisplayType I_DisplayType ()
{
	return Video->GetDisplayType ();
}

void I_SetMode (int &width, int &height, int &bits)
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
		fs = fullscreen.value ? true : false;
		break;
	}
	bool res = Video->SetMode (width, height, bits, fs);

	if (!res)
	{
		I_ClosestResolution (&width, &height, bits);
		if (!Video->SetMode (width, height, bits, fs))
			I_FatalError ("Mode %dx%dx%d is unavailable\n",
						  width, height, bits);
	}
}

bool I_CheckResolution (int width, int height, int bits)
{
	int twidth, theight;

	Video->FullscreenChanged (fullscreen.value ? true : false);
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

	Video->FullscreenChanged (fullscreen.value ? true : false);
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

bool I_AllocateScreen (DCanvas *canvas, int width, int height, int bits)
{
	if (!Video->AllocateSurface (canvas, width, height, bits))
	{
		I_FatalError ("Failed to allocate a %dx%dx%d surface",
					  width, height, bits);
	}
	return true;
}

void I_FreeScreen (DCanvas *canvas)
{
	Video->ReleaseSurface (canvas);
}

void I_LockScreen (DCanvas *canvas)
{
	Video->LockSurface (canvas);
}

void I_UnlockScreen (DCanvas *canvas)
{
	Video->UnlockSurface (canvas);
}

void I_Blit (DCanvas *src, int srcx, int srcy, int srcwidth, int srcheight,
			 DCanvas *dest, int destx, int desty, int destwidth, int destheight)
{
    if (!src->m_Private || !dest->m_Private)
		return;

    if (!src->m_LockCount)
		src->Lock ();
    if (!dest->m_LockCount)
		dest->Lock ();

	if (!Video->CanBlit() ||
		!Video->Blit (src, srcx, srcy, srcwidth, srcheight,
					  dest, destx, desty, destwidth, destheight))
	{
		fixed_t fracxstep, fracystep;
		fixed_t fracx, fracy;
		int x, y;

		fracy = srcy << FRACBITS;
		fracystep = (srcheight << FRACBITS) / destheight;
		fracxstep = (srcwidth << FRACBITS) / destwidth;

		if (src->is8bit == dest->is8bit)
		{
			// INDEX8 -> INDEX8 or ARGB8888 -> ARGB8888

			byte *destline, *srcline;

			if (!dest->is8bit)
			{
				destwidth <<= 2;
				srcwidth <<= 2;
				srcx <<= 2;
				destx <<= 2;
			}

			if (fracxstep == FRACUNIT)
			{
				for (y = desty; y < desty + destheight; y++, fracy += fracystep)
				{
					memcpy (dest->buffer + y * dest->pitch + destx,
							src->buffer + (fracy >> FRACBITS) * src->pitch + srcx,
							destwidth);
				}
			}
			else
			{
				for (y = desty; y < desty + destheight; y++, fracy += fracystep)
				{
					srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
					destline = dest->buffer + y * dest->pitch + destx;
					for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
					{
						destline[x] = srcline[fracx >> FRACBITS];
					}
				}
			}
		}
		else if (!src->is8bit && dest->is8bit)
		{
			// ARGB8888 -> INDEX8
			I_FatalError ("Can't I_Blit() an ARGB8888 source to\nan INDEX8 destination");
		}
		else
		{
			// INDEX8 -> ARGB8888 (Palette set in V_Palette)
			DWORD *destline;
			byte *srcline;

			if (fracxstep == FRACUNIT)
			{
				// No X-scaling
				for (y = desty; y < desty + destheight; y++, fracy += fracystep)
				{
					srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
					destline = (DWORD *)(dest->buffer + y * dest->pitch) + destx;
					for (x = 0; x < destwidth; x++)
					{
						destline[x] = V_Palette[srcline[x]];
					}
				}
			}
			else
			{
				// With X-scaling
				for (y = desty; y < desty + destheight; y++, fracy += fracystep)
				{
					srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
					destline = (DWORD *)(dest->buffer + y * dest->pitch) + destx;
					for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep)
					{
						destline[x] = V_Palette[srcline[fracx >> FRACBITS]];
					}
				}
			}
		}
	}

    if (!src->m_LockCount)
		I_UnlockScreen (src);
    if (!dest->m_LockCount)
		I_UnlockScreen (dest);
}

extern int NewWidth, NewHeight, NewBits, DisplayBits;

BEGIN_CUSTOM_CVAR (fullscreen, "0", CVAR_ARCHIVE)
{
	if (Video->FullscreenChanged (var.value ? true : false))
	{
		NewWidth = screen->width;
		NewHeight = screen->height;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}
END_CUSTOM_CVAR (fullscreen)

BEGIN_CUSTOM_CVAR (vid_winscale, "1.0", CVAR_ARCHIVE)
{
	if (var.value < 1.f)
	{
		var.Set (1.f);
	}
	else if (Video)
	{
		Video->SetWindowedScale (var.value);
		NewWidth = screen->width;
		NewHeight = screen->height;
		NewBits = DisplayBits;
		setmodeneeded = true;
	}
}
END_CUSTOM_CVAR (vid_winscale)

BEGIN_COMMAND (vid_listmodes)
{
	int width, height, bits;

	for (bits = 1; bits <= 32; bits++)
	{
		Video->StartModeIterator (bits);
		while (Video->NextMode (&width, &height))
			if (width == DisplayWidth && height == DisplayHeight && bits == DisplayBits)
				Printf_Bold ("%4d x%5d x%3d\n", width, height, bits);
			else
				Printf (PRINT_HIGH, "%4d x%5d x%3d\n", width, height, bits);
	}
}
END_COMMAND (vid_listmodes)

BEGIN_COMMAND (vid_currentmode)
{
	Printf (PRINT_HIGH, "%dx%dx%d\n", DisplayWidth, DisplayHeight, DisplayBits);
}
END_COMMAND (vid_currentmode)
