/*
** i_video.cpp: Low level DOOM graphics routines.
**				This version uses PTC. (Prometheus True Color)
*/

#include "ptc.h"

extern "C" {

#include <dos.h>
#include <sys/nearptr.h>

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

#define STATUS_REGISTER_1	0x3da
#define PEL_WRITE_ADR		0x3c8
#define PEL_DATA			0x3c9
#define _outbyte(x,y) (outp(x,y))

extern BOOL vidactive;	// Checked to avoid annoying flashes on console
						// during startup if developer is true.
extern BOOL setmodeneeded;
extern int	NewWidth, NewHeight, NewID;


cvar_t *I_ShowFPS;
cvar_t *ticker;
cvar_t *win_stretchx;
cvar_t *win_stretchy;

struct player_s;
static void Cmd_Vid_DescribeModes (struct player_s*,int,char**);
static void Cmd_Vid_DescribeCurrentMode (struct player_s*,int,char**);

BOOL Fullscreen;

static struct CmdDispatcher VidCommands[] = {
	{ "vid_currentmode",	Cmd_Vid_DescribeCurrentMode },
	{ "vid_listmodes",		Cmd_Vid_DescribeModes },
	{ NULL, }
};

char *IdStrings[22] = {
	"ARGB8888",
	"ABGR8888",
	"BGRA8888",
	"RGBA8888",
	"RGB888",
	"BGR888",
	"RGB565",
	"BGR565",
	"ARGB1555",
	"ABGR1555",
	"INDEX8",
	"FAKEMODE1A",
	"FAKEMODE1B",
	"FAKEMODE1C",
	"FAKEMODE2A",
	"FAKEMODE2B",
	"FAKEMODE2C",
	"FAKEMODE3A",
	"FAKEMODE3B",
	"FAKEMODE3C",
	"GREY8",
	"RGB332"
};

byte IdTobpp[22] = {
	32, 32, 32, 32, 24, 24, 16, 16, 15, 15, 8,
	18, 18, 18, 14, 14, 14, 12, 12, 12, 8, 8
};


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

int DisplayID;
// Number of bits-per-pixel of output device
int DisplayBPP;

// Display size
int DisplayWidth, DisplayHeight;

}

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

	// get ptc modelist
	List<MODE> list;
	ptc.GetModeList(list);

	// modelist iterator
	List<MODE>::Iterator iterator = list.first();
	MODE *ptcmode = iterator.current();

	while (ptcmode) {
		// Filter out modes taller than 1024 pixels since the
		// assembly routines will choke on them. (Besides, they
		// are really slow so why use them?)
		if (ptcmode->y <= 1024 && ptcmode->format.id == INDEX8) {
			nummodes++;

			addmode (ptcmode->x, ptcmode->y, ptcmode->format.bits, ptcmode->format.id, &lastmode);
		}
		ptcmode = iterator.next ();
	}
}

static char *GetFormatName (const int id)
{
	return IdStrings[id-1000];
}

extern "C" {

void I_ShutdownGraphics (void)
{
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
// [RH] I_BeginUpdate
//
void I_BeginUpdate (void)
{
	V_LockScreen (&screen);
}

//
// I_FinishUpdateNoBlit
//
void I_FinishUpdateNoBlit (void)
{
	V_UnlockScreen (&screen);
}

//
// I_FinishUpdate
//
void I_FinishUpdate (void)
{
	static int lasttic ;
	int	tics;
	int	i;

	// draws little dots on the bottom of the screen
	if (ticker->value) {
		i = I_GetTime();
		tics = i - lasttic;
		lasttic = i;
		if (tics > 20) tics = 20;

		for (i = 0; i < tics * 2; i += 2)
			screen.buffer[ (screen.height-1)*screen.pitch + i] = 0xff;
		for (; i < 20*2; i +=2 )
			screen.buffer[ (screen.height-1)*screen.pitch + i] = 0x0;
	}

	V_UnlockScreen (&screen);

	((Surface *)(screen.impdata))->Update ();
}


//
// I_ReadScreen
//
void I_ReadScreen (byte *scr)
{
	int x, y;
	byte *source;

	V_LockScreen (&screen);

	for (y = 0; y < screen.height; y++) {
		source = screen.buffer + y * screen.pitch;
		for (x = 0; x < screen.width; x++) {
			*scr++ = *source++;
		}
	}

	V_UnlockScreen (&screen);
}


//
// I_SetPalette
//
void I_SetPalette (unsigned int *pal)
{
	if (!pal)
		return;

	uint *palentries;

	if (!DisPal)
		return;

	palentries = (uint *)DisPal->Lock ();
	if (!palentries) {
		DPrintf ("Failed to set palette\n");
		return;
	}

	memcpy (palentries, pal, 256*sizeof(uint));

	DisPal->Unlock ();

	if (screen.is8bit)
		((Surface *)(screen.impdata))->SetPalette (*DisPal);

	// Only set the display palette if it is indexed color
	FORMAT format = ptc.GetFormat ();

	if (format.type == INDEXED && format.model != GREYSCALE)
		ptc.SetPalette (*DisPal);
}

void I_SetMode (int width, int height, int id)
{
	/*
	if (((id >= FAKEMODE1A && id <= FAKEMODE3C) ||
		 (DisplayID < FAKEMODE1A || DisplayID > FAKEMODE3C))
		||
		 (DisplayID >= FAKEMODE1A && DisplayID <= FAKEMODE3C) &&
		 (id < FAKEMODE1A || id > FAKEMODE3C))
		ptc.Init (width, height, id);
	else
	*/
	DisplayWidth = width;
	DisplayHeight = height;
	DisplayID = id;

	ptc.SetMode (width, height, id);

	{
		MODE mode = ptc.GetMode();

		DisplayBPP = mode.format.bits;
		if (developer->value && vidactive)
			Printf (PRINT_HIGH, "Mode set: %dx%d (%s)\n",
					mode.x, mode.y, IdStrings[DisplayID-1000]);
	}

	// cheapo hack to make sure current display BPP is stored in screen
	screen.Bpp = IdTobpp[DisplayID-1000] >> 3;
}

static void refreshDisplay (void) {
	NewWidth = DisplayWidth;
	NewHeight = DisplayHeight;
	NewID = DisplayID;
	setmodeneeded = true;
}

void I_InitGraphics(void)
{
	char num[8];

	I_ShowFPS = cvar ("vid_fps", "0", 0);	// unused for DOS
	num[0] = '0' + !!M_CheckParm ("-devparm");
	num[1] = 0;
	ticker = cvar ("ticker", num, 0);
	Fullscreen = true;

	C_RegisterCommands (VidCommands);

	MakeModesList ();
	sprintf (num, "%d", nummodes);
	cvar ("vid_nummodes", num, CVAR_NOSET);

	if (!ptc.Init (320,200,INDEX8))
		I_FatalError ("Could not initialize PTC");

	if (!(DisPal = new Palette))
		I_FatalError ("Could not create palette");

	atexit (I_ShutdownGraphics);
}

BOOL I_CheckResolution (int width, int height, int id)
{
	modelist_t *mode = Modes;

	while (mode) {
		if (mode->width == width && mode->height == height && mode->id == id)
			return true;

		mode = mode->next;
	}
	return false;
}

static modelist_t *IteratorMode;
static int IteratorID;

void I_StartModeIterator (int id)
{
	IteratorMode = Modes;
	IteratorID = id;
}

BOOL I_NextMode (int *width, int *height)
{
	if (IteratorMode) {
		while (IteratorMode && IteratorMode->id != IteratorID)
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

void Cmd_Vid_DescribeModes (struct player_s *p, int argc, char **argv)
{
	modelist_t *mode = Modes;

	while (mode)
	{
		// print accumulated mode info
		Printf (PRINT_HIGH, "%4dx%-4d  %2dbit %10s\n",
				mode->width,
				mode->height,
				mode->bpp,
				IdStrings[mode->id - 1000]);

		// go to next mode
		mode = mode->next;
	}
}

void Cmd_Vid_DescribeCurrentMode (struct player_s *p, int argc, char **argv)
{
	Printf (PRINT_HIGH, "%dx%d (%s)\n", screen.width, screen.height, IdStrings[DisplayID-1000]);
}



// [RH] New routines to handle screens as surfaces.

BOOL I_AllocateScreen (screen_t *scrn, int width, int height, int Bpp)
{
	Surface *surface;

	if (scrn->impdata) {
		I_FreeScreen (scrn);
	}
	scrn->width = width;
	scrn->height = height;
	scrn->is8bit = (Bpp == 8) ? true : false;
	scrn->lockcount = 0;
	scrn->Bpp = IdTobpp[DisplayID-1000] >> 3;
	scrn->palette = NULL;

	if (scrn == &screen) {
		surface = new Surface (ptc, width, height, (Bpp == 8) ? INDEX8 : ARGB8888);
	} else {
		surface = new Surface (width, height, (Bpp == 8) ? INDEX8 : ARGB8888);
	}

	if (surface) {
		screenchain *tracer = (screenchain *)Malloc (sizeof(struct screenchain));

		scrn->impdata = surface;
		tracer->scr = scrn;
		tracer->next = chain;
		chain = tracer;

		if (developer->value && vidactive) {
			Printf_Bold ("Allocated new surface. (%dx%dx%d)\n",
					surface->GetWidth (),
					surface->GetHeight (),
					surface->GetBitsPerPixel ());
			Printf (PRINT_HIGH, "Orientation: %d\n", surface->GetOrientation ());
			Printf (PRINT_HIGH, "Layout: %d\n", surface->GetLayout ());
			Printf (PRINT_HIGH, "Advance: %d\n", surface->GetAdvance ());
			Printf (PRINT_HIGH, "Pitch: %d\n", surface->GetPitch ());
			Printf (PRINT_HIGH, "Format: %s\n\n", GetFormatName ((surface->GetFormat ()).id));
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
	if (!src->impdata || !dest->impdata)
		return;
/*
	Funny problem with calling BitBlt() here. The more I do, the slower it gets.
	It also doesn't give me quite the right dimensions. (?) For now, I just do
	the conversion and copying myself.
*/
#if 0
	RECTANGLE srcrect (srcx, srcy, srcx + srcwidth, srcy + srcheight);
	RECTANGLE destrect (destx, desty, destx + destwidth, desty + destheight);

	((Surface *)(src->impdata))->BitBlt (*(Surface *)(dest->impdata),
										 srcrect, destrect);
#else
	fixed_t fracxstep, fracystep;
	fixed_t fracx, fracy;
	int x, y;

	if (!src->lockcount)
		I_LockScreen (src);
	if (!dest->lockcount)
		I_LockScreen (dest);

	fracy = srcy << FRACBITS;
	fracystep = (srcheight << FRACBITS) / destheight;
	fracxstep = (srcwidth << FRACBITS) / destwidth;

	if (src->is8bit == dest->is8bit) {
		// INDEX8 -> INDEX8 or ARGB8888 -> ARGB8888

		byte *destline, *srcline;

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
	} else if (!src->is8bit && dest->is8bit) {
		// ARGB8888 -> INDEX8
		I_FatalError ("Can't I_Blit() an ARGB8888 source to\nan INDEX8 destination");
	} else {
		// INDEX8 -> ARGB8888 (Palette set in V_Palette)
		uint *destline;
		byte *srcline;

		if (fracxstep == FRACUNIT) {
			// No X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (uint *)(dest->buffer + y * dest->pitch) + destx;
				for (x = 0; x < destwidth; x++) {
					destline[x] = V_Palette[srcline[x]];
				}
			}
		} else {
			// With X-scaling
			for (y = desty; y < desty + destheight; y++, fracy += fracystep) {
				srcline = src->buffer + (fracy >> FRACBITS) * src->pitch + srcx;
				destline = (uint *)(dest->buffer + y * dest->pitch) + destx;
				for (x = fracx = 0; x < destwidth; x++, fracx += fracxstep) {
					destline[x] = V_Palette[srcline[fracx >> FRACBITS]];
				}
			}
		}
	}

	if (!src->lockcount)
		I_UnlockScreen (src);
	if (!dest->lockcount)
		I_UnlockScreen (dest);
#endif
}

}	// extern "C"
