/*
** i_video.cpp: Low level DOOM graphics routines.
**				This version uses PTC. (Prometheus True Color)
**
** There are two reasons why I switched from DirectDraw to PTC:
**	  1. PTC makes windowed modes easy without any special work.
**	  2. PTC makes it easy to support *all* hi- and true-color modes.
**
** If you wish to use this file, you need to be sure that the
** PTC include files are in the include path and that you link
** with the PTC library. If not, the old DirectDraw code can be found
** in the file "oldvideo.c". Using it is a bit more complicating than
** just building ZDoom with that file instead of this one, since all
** new changes I've made since the switch to PTC have only been added
** here and not oldvideo.c.
*/

#include "ptc.h"

extern "C" {

#define __BYTEBOOL__
#include "doomtype.h"
#include "c_dispch.h"
#include "c_consol.h"

#include "doomstat.h"
#include "cmdlib.h"
#include "i_system.h"
#include "i_input.h"
#include "i_video.h"
#include "v_video.h"
#include "v_text.h"
#include "m_argv.h"
#include "d_main.h"
#include "m_alloc.h"

#include "c_consol.h"
#include "c_cvars.h"
#include "c_dispch.h"
#include "st_stuff.h"

#include "doomdef.h"
#include "z_zone.h"


extern HWND		Window;

extern BOOL setmodeneeded;
extern int	NewWidth, NewHeight, NewBpp;


cvar_t *I_ShowFPS;
cvar_t *win_stretchx;
cvar_t *win_stretchy;
cvar_t *fullscreen;


static void Cmd_Vid_DescribeModes ();
static void Cmd_Vid_DescribeCurrentMode ();

static char WindowedIFace[8];
static char FullscreenIFace[8];

BOOL Fullscreen;

static struct CmdDispatcher VidCommands[] = {
	"vid_describecurrentmode",	Cmd_Vid_DescribeCurrentMode,
	"vid_describemodes",		Cmd_Vid_DescribeModes,
	NULL
};

}


#define true TRUE
#define false FALSE

inline int ModeFromBPP (int bpp)
{
	if (bpp == 8)
		return INDEX8;
	else if (bpp == 16)
		return VIRTUAL16;
	else
		return VIRTUAL32;
}


struct screenchain {
	screen_t *scr;
	struct screenchain *next;
} *chain;

typedef struct modelist_s {
	modelist_s *next;
	int width, height, bpp;
	int id;
} modelist_t;


static modelist_t *Modes = NULL;

// Number of bits-per-pixel of output device
static int DisplayBPP;
// Number of bits-per-pixel desired (might not match DisplayBPP)
static int WantedBPP;
// Display size
static int DisplayWidth, DisplayHeight;

static int nummodes = 0;



/* The PTC interface object */
PTC ptc;

/* The display palette */
Palette *DisPal;

static void addmode (int x, int y, int bpp, int id, modelist_t **lastmode)
{
	modelist_t *newmode = (modelist_t *)Z_Malloc (sizeof (modelist_t), PU_STATIC, 0);

	nummodes++;
	newmode->next = NULL;
	newmode->width = x;
	newmode->height = y;
	newmode->bpp = bpp;
	newmode->id = id;

	(*lastmode)->next = newmode;
	*lastmode = newmode;
}

static void MakeModesList (void)
{
	modelist_t *lastmode = (modelist_t *)&Modes;
	BOOL have320x200x8 = false, have320x240x8 = false;

	// get ptc modelist
	List<MODE> list;
	ptc.GetModeList(list);

	// modelist iterator
	List<MODE>::Iterator iterator = list.first();
	MODE *ptcmode = iterator.current();

	while (ptcmode) {
		if (ptcmode->output == WINDOWED) {
			if (!WindowedIFace[0])
				strncpy (WindowedIFace, ptcmode->i, 8);
		} else {
			if (!FullscreenIFace[0])
				strncpy (FullscreenIFace, ptcmode->i, 8);

			// Filter out modes taller than 1024 pixels since the
			// assembly routines will choke on them. (Besides, they
			// are really slow so why use them?)
			if (ptcmode->y <= 1024) {
				nummodes++;

				addmode (ptcmode->x, ptcmode->y, ptcmode->format.bits, ptcmode->format.id, &lastmode);
				if (ptcmode->x == 320 && ptcmode->format.bits == 8) {
					if (ptcmode->y == 200)
						have320x200x8 = true;
					else if (ptcmode->y == 240)
						have320x240x8 = true;
				}
			}
		}
		ptcmode = iterator.next ();
	}

	if (OSPlatform == os_Win95) {
		// Windows 95 will let us use Mode X (assuming my PTC patch has been
		// applied). If we didn't find any linear modes in the loop above,
		// add the Mode X modes here.

		if (!have320x200x8)
			addmode (320, 200, 8, INDEX8, &lastmode);
		if (!have320x240x8)
			addmode (320, 240, 8, INDEX8, &lastmode);
	}
}

static char *GetFormatName (const int id)
{
	switch (id)
	{
		case ARGB8888:		return "ARGB8888";
		case ABGR8888:		return "ABGR8888";
		case BGRA8888:		return "BGRA8888";
		case RGBA8888:		return "RGBA8888";
		case RGB888:		return "RGB888";
		case BGR888:		return "BGR888";
		case RGB565:		return "RGB565";
		case BGR565:		return "BGR565";
		case ARGB1555:		return "ARGB1555";
		case ABGR1555:		return "ABGR1555";
		case INDEX8:		return "INDEX8";
		
		// Will the Win32 version of PTC support fakemodes?
		// It doesn't on my machine.
		case FAKEMODE1A:	return "FAKEMODE1A";
		case FAKEMODE1B:	return "FAKEMODE1B";
		case FAKEMODE1C:	return "FAKEMODE1C";
		case FAKEMODE2A:	return "FAKEMODE2A";
		case FAKEMODE2B:	return "FAKEMODE2B";
		case FAKEMODE2C:	return "FAKEMODE2C";
		case FAKEMODE3A:	return "FAKEMODE3A";
		case FAKEMODE3B:	return "FAKEMODE3B";
		case FAKEMODE3C:	return "FAKEMODE3C";
		case GREY8:			return "GREY8";
		case RGB332:		return "RGB332";
		default:			return "(custom)";
	}
}


extern "C" {

void I_ShutdownGraphics (void)
{
	I_ShutdownInput ();

	{
		modelist_t *mode = Modes, *tempmode;

		while (mode) {
			tempmode = mode;
			mode = mode->next;
			Z_Free (tempmode);
		}
		Modes = NULL;
	}

	while (chain) {
		I_FreeScreen (chain->scr);
	}

	if (DisPal) {
		delete DisPal;
		DisPal = NULL;
	}

	ptc.Close ();
}

//
// I_StartFrame
//
void I_StartFrame (void)
{
	// er?

}


//
// [RH] I_BeginUpdate
//
void I_BeginUpdate (void)
{
	V_LockScreen (&screens[0]);
}

//
// I_UpdateNoBlit
//
void I_UpdateNoBlit (void)
{
	// what is this?
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
	static int	lasttic, lastms = 0;
	int		tics, ms;
	int		i;

	ms = timeGetTime ();
	if ((ms - lastms) && I_ShowFPS->value) {
		char fpsbuff[40];
		int chars;

		sprintf (fpsbuff, "%d ms (%d fps)",
				 ms - lastms,
				 1000 / (ms - lastms));
		chars = strlen (fpsbuff);
		V_Clear (0, screens[0].height - 8, chars * 8, screens[0].height, &screens[0], 0);
		V_PrintStr (0, screens[0].height - 8, (byte *)&fpsbuff[0], chars, 0);
	}
	lastms = ms;

	// draws little dots on the bottom of the screen
	if (devparm) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i=0 ; i<tics*2 ; i+=2)
			screens[0].buffer[ (screens[0].height-1)*screens[0].pitch + i] = 0xff;
		for ( ; i<20*2 ; i+=2)
			screens[0].buffer[ (screens[0].height-1)*screens[0].pitch + i] = 0x0;
	}

	V_UnlockScreen (&screens[0]);

	((Surface *)(screens[0].impdata))->Update ();
}


//
// I_ReadScreen
//
void I_ReadScreen (byte *scr)
{
	int x, y;
	byte *source;

	V_LockScreen (&screens[0]);

	for (y = 0; y < screens[0].height; y++) {
		source = screens[0].buffer + y * screens[0].pitch;
		for (x = 0; x < screens[0].width; x++) {
			*scr++ = *source++;
		}
	}

	V_UnlockScreen (&screens[0]);
}


//
// I_SetPalette
//
void I_SetPalette (unsigned int *pal)
{
	uint *palentries;

	if (!DisPal || !pal)
		return;

	palentries = (uint *)DisPal->Lock ();
	if (!palentries) {
		DPrintf ("Failed to set palette\n");
		return;
	}

	memcpy (palentries, pal, 256*(sizeof uint));

	DisPal->Unlock ();

	if (screens[0].is8bit)
		((Surface *)(screens[0].impdata))->SetPalette (*DisPal);
	if (screens[1].is8bit)
		((Surface *)(screens[1].impdata))->SetPalette (*DisPal);

	// Only set the display palette if it is indexed color
	FORMAT format = ptc.GetFormat ();

	if (format.type == INDEXED && format.model != GREYSCALE)
		ptc.SetPalette (*DisPal);
}

static void ReinitPTC (char *iface)
{
	if (!ptc.Init (iface, Window))
		I_FatalError ("Could not reinitialize PTC");
	// We don't want our priority class bumped up!
	SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	DPrintf ("PTC reinitialized to use %s\n\n", iface);
}

void I_SetMode (int width, int height, int Bpp)
{
	static BOOL firsttime = true;

	DisplayWidth = width;
	DisplayHeight = height;
	WantedBPP = Bpp;

	if (firsttime) {
		firsttime = false;
	} else {
		if (Fullscreen) {
			if (!fullscreen->value) {
				Fullscreen = FALSE;
				// Get out of fullscreen mode
				ptc.Close ();
				//ReinitPTC (WindowedIFace);
			}
		} else {
			if (fullscreen->value) {
				Fullscreen = TRUE;
				ReinitPTC (FullscreenIFace);
			}
		}

		if (Fullscreen) {
			ptc.SetMode (width, height, ModeFromBPP (Bpp));
		} else {
			width *= win_stretchx->value;
			height *= win_stretchy->value;

			height += GetSystemMetrics (SM_CYFIXEDFRAME) * 2 +
					  GetSystemMetrics (SM_CYCAPTION);
			width  += GetSystemMetrics (SM_CXFIXEDFRAME) * 2;

			SetWindowPos (Window, NULL, 0, 0, width, height, SWP_DRAWFRAME|
															 SWP_NOACTIVATE|
															 SWP_NOCOPYBITS|
															 SWP_NOMOVE|
															 SWP_NOZORDER);

			// Let PTC know about the new window size
			ReinitPTC (WindowedIFace);

			// If the menu is active, make sure the mouse is released.
			if (menuactive)
				I_PauseMouse ();
		}
	}
	{
		MODE mode = ptc.GetMode();

		DisplayBPP = mode.format.bits;
		if (developer->value) {
			Printf ("Mode set: %dx%dx%d\n", mode.x, mode.y, DisplayBPP);
		}
	}

	// cheapo hack to make sure current display BPP is stored in screens[0]
	screens[0].Bpp = WantedBPP >> 3;
}

static void refreshDisplay (void) {
	NewWidth = DisplayWidth;
	NewHeight = DisplayHeight;
	NewBpp = WantedBPP;
	setmodeneeded = true;
}

static void stretchChanged (cvar_t *var)
{
	if (!Fullscreen)
		refreshDisplay ();
}

static void fullChanged (cvar_t *var)
{
	if (Fullscreen != var->value)
		refreshDisplay ();
}

void I_InitGraphics(void)
{

	DisPal = new Palette;
	if (!DisPal)
		I_FatalError ("Could not create palette");

	NewWidth = DisplayWidth;
	NewHeight = DisplayHeight;
	NewBpp = WantedBPP;
	setmodeneeded = true;
}

void I_StartGraphics (void)
{
	char num[8], *iface;
	modelist_t *mode = (modelist_t *)&Modes;

	I_DetectOS ();

	I_ShowFPS = cvar ("vid_fps", "0", 0);
	win_stretchx = cvar ("win_stretchx", "1.0", CVAR_ARCHIVE|CVAR_CALLBACK);
	win_stretchy = cvar ("win_stretchy", "1.0", CVAR_ARCHIVE|CVAR_CALLBACK);
	fullscreen = cvar ("fullscreen", "1", CVAR_ARCHIVE|CVAR_CALLBACK);

	Fullscreen = (BOOL)fullscreen->value;

	win_stretchx->u.callback =
	win_stretchy->u.callback = stretchChanged;
	fullscreen->u.callback = fullChanged;

	C_RegisterCommands (VidCommands);

	MakeModesList ();
	sprintf (num, "%d", nummodes);
	cvar ("vid_nummodes", num, CVAR_NOSET);

	if (Fullscreen)
		iface = FullscreenIFace;
	else
		iface = WindowedIFace;

	if (!ptc.Init (iface, Window))
		I_FatalError ("Could not initialize PTC");

	// We don't want our priority class bumped up!
	SetPriorityClass (GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
}

BOOL I_CheckResolution (int width, int height, int bpp)
{
	modelist_t *mode = Modes;

	while (mode) {
		if (mode->width == width && mode->height == height && mode->bpp == bpp)
			return true;

		mode = mode->next;
	}
	return false;
}

static modelist_t *IteratorMode;
static int IteratorBPP;

void I_StartModeIterator (int bpp)
{
	IteratorMode = Modes;
	IteratorBPP = bpp;
}

BOOL I_NextMode (int *width, int *height)
{
	if (IteratorMode) {
		while (IteratorMode && IteratorMode->bpp != IteratorBPP)
			IteratorMode = IteratorMode->next;

		if (IteratorMode) {
			*width = IteratorMode->width;
			*height = IteratorMode->height;
			IteratorMode = IteratorMode->next;
			return true;
		}
	}
	return false;
}

void Cmd_Vid_DescribeModes ()
{
	modelist_t *mode = Modes;

	while (mode)
	{
		// print accumulated mode info
		Printf ("%4dx%-4d  %2dbit %10s\n",
				mode->width,
				mode->height,
				mode->bpp,
				GetFormatName (mode->id));

		// go to next mode
		mode = mode->next;
	}
}

void Cmd_Vid_DescribeCurrentMode ()
{
	Printf ("%dx%dx%d\n", screens[0].width, screens[0].height, screens[0].Bpp << 3);
}



// [RH] New routines to handle screens as surfaces with
//		possible hardware acceleration.

BOOL I_AllocateScreen (screen_t *scrn, int width, int height, int Bpp)
{
	Surface *surface;

	if (scrn->impdata)
		I_FreeScreen (scrn);

	scrn->width = width;
	scrn->height = height;
	scrn->is8bit = (Bpp == 8) ? true : false;
	scrn->lockcount = 0;
	scrn->Bpp = WantedBPP >> 3;
	scrn->palette = NULL;

	if (scrn == &screens[0])
		surface = new Surface (ptc, width, height, (Bpp == 8) ? INDEX8 : ARGB8888);
	else
		surface = new Surface (width, height, (Bpp == 8) ? INDEX8 : ARGB8888);

	if (surface) {
		screenchain *tracer = (screenchain *)Malloc (sizeof(struct screenchain));

		scrn->impdata = surface;
		tracer->scr = scrn;
		tracer->next = chain;
		chain = tracer;

		if (developer->value) {
			Printf_Bold ("Allocated new surface. (%dx%dx%d)\n",
					surface->GetWidth (),
					surface->GetHeight (),
					surface->GetBitsPerPixel ());
			Printf ("Orientation: %d\n", surface->GetOrientation ());
			Printf ("Layout: %d\n", surface->GetLayout ());
			Printf ("Advance: %d\n", surface->GetAdvance ());
			Printf ("Pitch: %d\n", surface->GetPitch ());
			Printf ("Format: %s\n\n", GetFormatName ((surface->GetFormat ()).id));
		}

		return true;
	} else {
		return false;
	}
}

void I_FreeScreen (screen_t *scrn)
{
	struct screenchain *thisone, *prev;

	if (scrn->impdata) {
		// Make sure the surface is unlocked
		if (scrn->lockcount)
			((Surface *)(scrn->impdata))->Unlock ();

		// Release it
		delete (Surface *)(scrn->impdata);
		scrn->impdata = NULL;
	}

	V_DetachPalette (scrn);

	thisone = chain;
	prev = NULL;
	while (thisone) {
		if (thisone->scr == scrn) {
			if (prev) {
				prev->next = thisone->next;
			} else {
				chain = thisone->next;
			}
			free (thisone);
			break;
		} else {
			prev = thisone;
			thisone = thisone->next;
		}
	}
}

void I_LockScreen (screen_t *scrn)
{
	if (scrn->impdata) {
		scrn->buffer = (byte *)((Surface *)(scrn->impdata))->Lock ();

		if (scrn->buffer == NULL)
			I_FatalError ("Could not lock surface");

		scrn->pitch = ((Surface *)(scrn->impdata))->GetPitch ();
	} else {
		scrn->lockcount--;
	}
}

void I_UnlockScreen (screen_t *scrn)
{
	if (scrn->impdata) {
		((Surface *)(scrn->impdata))->Unlock ();
		scrn->buffer = NULL;
	}
}

void I_Blit (screen_t *src, int srcx, int srcy, int srcwidth, int srcheight,
			 screen_t *dest, int destx, int desty, int destwidth, int destheight)
{
	if (src->impdata && dest->impdata) {
/*
	Funny problem with calling BitBlt() here. The more I do, the slower it gets.
	It also doesn't give me quite the right dimensions. (?) For now, I just do
	the conversion and copying myself.

			RECTANGLE srcrect (srcx, srcy, srcx + srcwidth, srcy + srcheight);
			RECTANGLE destrect (destx, desty, destx + destwidth, desty + destheight);

			((Surface *)(src->impdata))->BitBlt (*(Surface *)(dest->impdata),
												 srcrect, destrect);

*/
			fixed_t fracxstep, fracystep;
			fixed_t fracx, fracy;
			int x, y;
			byte *destline, *srcline;

			if (!src->lockcount)
				I_LockScreen (src);
			if (!dest->lockcount)
				I_LockScreen (dest);

			fracy = srcy << FRACBITS;
			fracystep = (srcheight << FRACBITS) / destheight;
			fracxstep = (srcwidth << FRACBITS) / destwidth;

//			if (src->is8bit == dest->is8bit) {
				if (!dest->is8bit) {
					destwidth <<= 2;
					srcwidth <<= 2;
					srcx <<= 2;
					destx <<= 2;
				}

				if (fracxstep == FRACUNIT) {
					for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
						memcpy (dest->buffer + y * dest->pitch + destx,
								src->buffer + (fracy >> FRACBITS) * src->pitch + srcx,
								destwidth);
					}
				} else {
					for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
						srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
						destline = dest->buffer + y * dest->pitch + destx;
						for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep) {
							destline[x] = srcline[fracx >> FRACBITS];
						}
					}
				}
/*			} else if (!src->is8bit && dest->is8bit) {
				I_FatalError ("Can't I_Blit() an ARGB8888 source to\nan INDEX8 destination");
			} else {
				RASTER raster;
				uint templine[4096];		// Support lines as long as 4096 pixels

				if (src->is8bit && dest->is8bit)


			if (!src->lockcount)
				I_UnlockScreen (src);
			if (!dest->lockcount)
				I_UnlockScreen (dest);
		}
*/	}
}
}
