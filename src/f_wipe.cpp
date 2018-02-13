//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "doomdef.h"
#include "f_wipe.h"
#include "c_cvars.h"
#include "templates.h"
#include "v_palette.h"

EXTERN_CVAR(Bool, r_blendmethod)

//
//		SCREEN WIPE PACKAGE
//

static int CurrentWipeType;

static short *wipe_scr_start;
static short *wipe_scr_end;
static int *y;

// [RH] Fire Wipe
#define FIREWIDTH	64
#define FIREHEIGHT	64
static uint8_t *burnarray;
static int density;
static int burntime;

// [RH] Crossfade
static int fade;


// Melt -------------------------------------------------------------

// Match the strip sizes that oldschool Doom used on a 320x200 screen.
#define MELT_WIDTH		160
#define MELT_HEIGHT		200

void wipe_shittyColMajorXform (short *array)
{
	int x, y;
	short *dest;
	int width = SCREENWIDTH / 2;

	dest = new short[width*SCREENHEIGHT*2];

	for(y = 0; y < SCREENHEIGHT; y++)
		for(x = 0; x < width; x++)
			dest[x*SCREENHEIGHT+y] = array[y*width+x];

	memcpy(array, dest, SCREENWIDTH*SCREENHEIGHT);

	delete[] dest;
}

bool wipe_initMelt (int ticks)
{
	int i, r;
	
	// copy start screen to main screen
	screen->DrawBlock (0, 0, SCREENWIDTH, SCREENHEIGHT, (uint8_t *)wipe_scr_start);
	
	// makes this wipe faster (in theory)
	// to have stuff in column-major format
	wipe_shittyColMajorXform (wipe_scr_start);
	wipe_shittyColMajorXform (wipe_scr_end);
	
	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y = new int[MELT_WIDTH];
	y[0] = -(M_Random() & 15);
	for (i = 1; i < MELT_WIDTH; i++)
	{
		r = (M_Random()%3) - 1;
		y[i] = clamp(y[i-1] + r, -15, 0);
	}

	return 0;
}

bool wipe_doMelt (int ticks)
{
	int i, j, dy, x;
	const short *s;
	short *d;
	bool done = true;

	while (ticks--)
	{
		done = true;
		for (i = 0; i < MELT_WIDTH; i++)
		{
			if (y[i] < 0)
			{
				y[i]++;
				done = false;
			} 
			else if (y[i] < MELT_HEIGHT)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				y[i] = MIN(y[i] + dy, MELT_HEIGHT);
				done = false;
			}
			if (ticks == 0 && y[i] >= 0)
			{ // Only draw for the final tick.
				const int pitch = screen->GetPitch() / 2;
				int sy = y[i] * SCREENHEIGHT / MELT_HEIGHT;

				for (x = i * (SCREENWIDTH/2) / MELT_WIDTH; x < (i + 1) * (SCREENWIDTH/2) / MELT_WIDTH; ++x)
				{
					s = &wipe_scr_end[x*SCREENHEIGHT];
					d = &((short *)screen->GetBuffer())[x];

					for (j = sy; j != 0; --j)
					{
						*d = *(s++);
						d += pitch;
					}

					s = &wipe_scr_start[x*SCREENHEIGHT];

					for (j = SCREENHEIGHT - sy; j != 0; --j)
					{
						*d = *(s++);
						d += pitch;
					}
				}
			}
		}
	}

	return done;
}

bool wipe_exitMelt (int ticks)
{
	delete[] y;
	return 0;
}

// Burn -------------------------------------------------------------

bool wipe_initBurn (int ticks)
{
	burnarray = new uint8_t[FIREWIDTH * (FIREHEIGHT+5)];
	memset (burnarray, 0, FIREWIDTH * (FIREHEIGHT+5));
	density = 4;
	burntime = 0;
	return 0;
}

int wipe_CalcBurn (uint8_t *burnarray, int width, int height, int density)
{
	// This is a modified version of the fire that was once used
	// on the player setup menu.
	static int voop;

	int a, b;
	uint8_t *from;

	// generator
	from = &burnarray[width * height];
	b = voop;
	voop += density / 3;
	for (a = 0; a < density/8; a++)
	{
		unsigned int offs = (a+b) & (width - 1);
		unsigned int v = M_Random();
		v = MIN(from[offs] + 4 + (v & 15) + (v >> 3) + (M_Random() & 31), 255u);
		from[offs] = from[width*2 + ((offs + width*3/2) & (width - 1))] = v;
	}

	density = MIN(density + 10, width * 7);

	from = burnarray;
	for (b = 0; b <= height; b += 2)
	{
		uint8_t *pixel = from;

		// special case: first pixel on line
		uint8_t *p = pixel + (width << 1);
		unsigned int top = *p + *(p + width - 1) + *(p + 1);
		unsigned int bottom = *(pixel + (width << 2));
		unsigned int c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;
		pixel++;

		// main line loop
		for (a = 1; a < width-1; a++)
		{
			// sum top pixels
			p = pixel + (width << 1);
			top = *p + *(p - 1) + *(p + 1);

			// bottom pixel
			bottom = *(pixel + (width << 2));

			// combine pixels
			c1 = (top + bottom) >> 2;
			if (c1 > 1) c1--;

			// store pixels
			*pixel = c1;
			*(pixel + width) = (c1 + bottom) >> 1;		// interpolate

			// next pixel
			pixel++;
		}

		// special case: last pixel on line
		p = pixel + (width << 1);
		top = *p + *(p - 1) + *(p - width + 1);
		bottom = *(pixel + (width << 2));
		c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;

		// next line
		from += width << 1;
	}

	// Check for done-ness. (Every pixel with level 126 or higher counts as done.)
	for (a = width * height, from = burnarray; a != 0; --a, ++from)
	{
		if (*from < 126)
		{
			return density;
		}
	}
	return -1;
}

bool wipe_doBurn (int ticks)
{
	bool done;

	burntime += ticks;
	ticks *= 2;

	// Make the fire burn
	done = false;
	while (!done && ticks--)
	{
		density = wipe_CalcBurn(burnarray, FIREWIDTH, FIREHEIGHT, density);
		done = (density < 0);
	}

	// Draw the screen
	int xstep, ystep, firex, firey;
	int x, y;
	uint8_t *to, *fromold, *fromnew;
	const int SHIFT = 16;

	xstep = (FIREWIDTH << SHIFT) / SCREENWIDTH;
	ystep = (FIREHEIGHT << SHIFT) / SCREENHEIGHT;
	to = screen->GetBuffer();
	fromold = (uint8_t *)wipe_scr_start;
	fromnew = (uint8_t *)wipe_scr_end;

	if (!r_blendmethod)
	{
		for (y = 0, firey = 0; y < SCREENHEIGHT; y++, firey += ystep)
		{
			for (x = 0, firex = 0; x < SCREENWIDTH; x++, firex += xstep)
			{
				int fglevel;

				fglevel = burnarray[(firex>>SHIFT)+(firey>>SHIFT)*FIREWIDTH] / 2;
				if (fglevel >= 63)
				{
					to[x] = fromnew[x];
				}
				else if (fglevel == 0)
				{
					to[x] = fromold[x];
					done = false;
				}
				else
				{
					int bglevel = 64-fglevel;
					uint32_t *fg2rgb = Col2RGB8[fglevel];
					uint32_t *bg2rgb = Col2RGB8[bglevel];
					uint32_t fg = fg2rgb[fromnew[x]];
					uint32_t bg = bg2rgb[fromold[x]];
					fg = (fg+bg) | 0x1f07c1f;
					to[x] = RGB32k.All[fg & (fg>>15)];
					done = false;
				}
			}
			fromold += SCREENWIDTH;
			fromnew += SCREENWIDTH;
			to += SCREENPITCH;
		}

	}
	else
	{
		for (y = 0, firey = 0; y < SCREENHEIGHT; y++, firey += ystep)
		{
			for (x = 0, firex = 0; x < SCREENWIDTH; x++, firex += xstep)
			{
				int fglevel;

				fglevel = burnarray[(firex>>SHIFT)+(firey>>SHIFT)*FIREWIDTH] / 2;
				if (fglevel >= 63)
				{
					to[x] = fromnew[x];
				}
				else if (fglevel == 0)
				{
					to[x] = fromold[x];
					done = false;
				}
				else
				{
					int bglevel = 64-fglevel;

					const PalEntry* pal = GPalette.BaseColors;

					uint32_t fg = fromnew[x];
					uint32_t bg = fromold[x];
					int r = MIN((pal[fg].r * fglevel + pal[bg].r * bglevel) >> 8, 63);
					int g = MIN((pal[fg].g * fglevel + pal[bg].g * bglevel) >> 8, 63);
					int b = MIN((pal[fg].b * fglevel + pal[bg].b * bglevel) >> 8, 63);
					to[x] = RGB256k.RGB[r][g][b];
					done = false;
				}
			}
			fromold += SCREENWIDTH;
			fromnew += SCREENWIDTH;
			to += SCREENPITCH;
		}
	}
	return done || (burntime > 40);
}

bool wipe_exitBurn (int ticks)
{
	delete[] burnarray;
	return 0;
}

// Crossfade --------------------------------------------------------

bool wipe_initFade (int ticks)
{
	fade = 0;
	return 0;
}

bool wipe_doFade (int ticks)
{
	fade += ticks * 2;
	if (fade > 64)
	{
		screen->DrawBlock (0, 0, SCREENWIDTH, SCREENHEIGHT, (uint8_t *)wipe_scr_end);
		return true;
	}
	else
	{
		int x, y;
		int bglevel = 64 - fade;
		uint32_t *fg2rgb = Col2RGB8[fade];
		uint32_t *bg2rgb = Col2RGB8[bglevel];
		uint8_t *fromnew = (uint8_t *)wipe_scr_end;
		uint8_t *fromold = (uint8_t *)wipe_scr_start;
		uint8_t *to = screen->GetBuffer();
		const PalEntry *pal = GPalette.BaseColors;

		if (!r_blendmethod)
		{
			for (y = 0; y < SCREENHEIGHT; y++)
			{
				for (x = 0; x < SCREENWIDTH; x++)
				{
					uint32_t fg = fg2rgb[fromnew[x]];
					uint32_t bg = bg2rgb[fromold[x]];
					fg = (fg+bg) | 0x1f07c1f;
					to[x] = RGB32k.All[fg & (fg>>15)];
				}
				fromnew += SCREENWIDTH;
				fromold += SCREENWIDTH;
				to += SCREENPITCH;
			}
		}
		else
		{
			for (y = 0; y < SCREENHEIGHT; y++)
			{
				for (x = 0; x < SCREENWIDTH; x++)
				{
					uint32_t fg = fromnew[x];
					uint32_t bg = fromold[x];
					int r = MIN((pal[fg].r * (64-bglevel) + pal[bg].r * bglevel) >> 8, 63);
					int g = MIN((pal[fg].g * (64-bglevel) + pal[bg].g * bglevel) >> 8, 63);
					int b = MIN((pal[fg].b * (64-bglevel) + pal[bg].b * bglevel) >> 8, 63);
					to[x] = RGB256k.RGB[r][g][b];
				}
				fromnew += SCREENWIDTH;
				fromold += SCREENWIDTH;
				to += SCREENPITCH;
			}
		}
	}
	return false;
}

bool wipe_exitFade (int ticks)
{
	return 0;
}

// General Wipe Functions -------------------------------------------

static bool (*wipes[])(int) =
{
	wipe_initMelt, wipe_doMelt, wipe_exitMelt,
	wipe_initBurn, wipe_doBurn, wipe_exitBurn,
	wipe_initFade, wipe_doFade, wipe_exitFade
};

// Returns true if the wipe should be performed.
bool wipe_StartScreen (int type)
{
	if (screen->IsBgra())
		return false;

	CurrentWipeType = clamp(type, 0, wipe_NUMWIPES - 1);

	if (CurrentWipeType)
	{
		wipe_scr_start = new short[SCREENWIDTH * SCREENHEIGHT / 2];
		screen->GetBlock (0, 0, SCREENWIDTH, SCREENHEIGHT, (uint8_t *)wipe_scr_start);
		return true;
	}
	return false;
}

void wipe_EndScreen (void)
{
	if (screen->IsBgra())
		return;

	if (CurrentWipeType)
	{
		wipe_scr_end = new short[SCREENWIDTH * SCREENHEIGHT / 2];
		screen->GetBlock (0, 0, SCREENWIDTH, SCREENHEIGHT, (uint8_t *)wipe_scr_end);
		screen->DrawBlock (0, 0, SCREENWIDTH, SCREENHEIGHT, (uint8_t *)wipe_scr_start); // restore start scr.

		// Initialize the wipe
		(*wipes[(CurrentWipeType-1)*3])(0);
	}
}

// Returns true if the wipe is done.
bool wipe_ScreenWipe (int ticks)
{
	bool rc;

	if (screen->IsBgra())
		return true;

	if (CurrentWipeType == wipe_None)
		return true;

	// do a piece of wipe-in
	rc = (*wipes[(CurrentWipeType-1)*3+1])(ticks);

	return rc;
}

// Final things for the wipe
void wipe_Cleanup()
{
	if (screen->IsBgra())
		return;

	if (wipe_scr_start != NULL)
	{
		delete[] wipe_scr_start;
		wipe_scr_start = NULL;
	}
	if (wipe_scr_end != NULL)
	{
		delete[] wipe_scr_end;
		wipe_scr_end = NULL;
	}
	if (CurrentWipeType > 0)
	{
		(*wipes[(CurrentWipeType-1)*3+2])(0);
	}
}
