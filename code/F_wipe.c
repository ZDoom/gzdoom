// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_video.h"
#include "v_video.h"
#include "m_random.h"
#include "m_alloc.h"
#include "doomdef.h"
#include "f_wipe.h"
#include "c_cvars.h"

//
//		SCREEN WIPE PACKAGE
//

enum
{
	wipe_None,			// don't bother
	wipe_Melt,			// weird screen melt
	wipe_Burn,			// fade in shape of fire
	wipe_Fade,			// crossfade from old to new
	wipe_NUMWIPES
};

cvar_t *wipetype;
static int CurrentWipeType;

static short *wipe_scr_start;
static short *wipe_scr_end;
static int *y;

// [RH] Fire Wipe
#define FIREWIDTH	64
#define FIREHEIGHT	64
static byte *burnarray;
static int density;
static int burntime;

// [RH] Crossfade
static int fade;


// Melt -------------------------------------------------------------

void wipe_shittyColMajorXform (short *array)
{
	int x, y;
	short *dest;
	int width = screen.width / 2;

	dest = (short*) Malloc (width*screen.height*2);

	for(y = 0; y < screen.height; y++)
		for(x = 0; x < width; x++)
			dest[x*screen.height+y] = array[y*width+x];

	memcpy(array, dest, screen.width*screen.height);

	free (dest);

}

int wipe_initMelt (int ticks)
{
	int i, r;
	
	// copy start screen to main screen
	V_DrawBlock (0, 0, &screen, screen.width, screen.height, (byte *)wipe_scr_start);
	
	// makes this wipe faster (in theory)
	// to have stuff in column-major format
	wipe_shittyColMajorXform (wipe_scr_start);
	wipe_shittyColMajorXform (wipe_scr_end);
	
	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y = (int *) Z_Malloc (screen.width*sizeof(int), PU_STATIC, 0);
	y[0] = -(M_Random()&0xf);
	for (i = 1; i < screen.width; i++)
	{
		r = (M_Random()%3) - 1;
		y[i] = y[i-1] + r;
		if (y[i] > 0) y[i] = 0;
		else if (y[i] == -16) y[i] = -15;
	}

	return 0;
}

int wipe_doMelt (int ticks)
{
	int 		i;
	int 		j;
	int 		dy;
	int 		idx;
	
	short*		s;
	short*		d;
	BOOL	 	done = true;

	int width = screen.width / 2;

	while (ticks--)
	{
		for (i = 0; i < width; i++)
		{
			if (y[i]<0)
			{
				y[i]++; done = false;
			} 
			else if (y[i] < screen.height)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				dy = (dy * screen.height) / 200;
				if (y[i]+dy >= screen.height)
					dy = screen.height - y[i];
				s = &wipe_scr_end[i*screen.height+y[i]];
				d = &((short *)screen.buffer)[y[i]*(screen.pitch/2)+i];
				idx = 0;
				for (j=dy;j;j--)
				{
					d[idx] = *(s++);
					idx += screen.pitch/2;
				}
				y[i] += dy;
				s = &wipe_scr_start[i*screen.height];
				d = &((short *)screen.buffer)[y[i]*(screen.pitch/2)+i];
				idx = 0;
				for (j=screen.height-y[i];j;j--)
				{
					d[idx] = *(s++);
					idx += screen.pitch/2;
				}
				done = false;
			}
		}
	}

	return done;

}

int wipe_exitMelt (int ticks)
{
	free (wipe_scr_start);
	free (wipe_scr_end);
	Z_Free (y);
	return 0;
}

// Burn -------------------------------------------------------------

int wipe_initBurn (int ticks)
{
	burnarray = Z_Malloc (FIREWIDTH * (FIREHEIGHT+4), PU_STATIC, 0);
	memset (burnarray, 0, FIREWIDTH * (FIREHEIGHT+4));
	density = 4;
	burntime = 0;
	return 0;
}

int wipe_doBurn (int ticks)
{
	static int voop;
	BOOL done;

	// This is a modified version of the fire from the player
	// setup menu.
	burntime += ticks;
	ticks *= 2;

	// Make the fire burn
	while (ticks--)
	{
		int a, b;
		byte *from;

		// generator
		from = burnarray + FIREHEIGHT * FIREWIDTH;
		b = voop;
		voop += density / 3;
		for (a = 0; a < density/8; a++)
		{
			unsigned int offs = (a+b) % FIREWIDTH;
			unsigned int v = M_Random();
			v = from[offs] + 4 + (v & 15) + (v >> 3) + (M_Random() & 31);
			if (v > 255)
				v = 255;
			from[offs] = from[FIREWIDTH*2 + (offs + FIREWIDTH*3/2)%FIREWIDTH] = v;
		}

		density += 10;
		if (density > FIREWIDTH*7)
			density = FIREWIDTH*7;

		from = burnarray;
		for (b = 0; b <= FIREHEIGHT; b += 2)
		{
			byte *pixel = from;

			// special case: first pixel on line
			byte *p = pixel + (FIREWIDTH << 1);
			unsigned int top = *p + *(p + FIREWIDTH - 1) + *(p + 1);
			unsigned int bottom = *(pixel + (FIREWIDTH << 2));
			unsigned int c1 = (top + bottom) >> 2;
			if (c1 > 1) c1--;
			*pixel = c1;
			*(pixel + FIREWIDTH) = (c1 + bottom) >> 1;
			pixel++;

			// main line loop
			for (a = 1; a < FIREWIDTH-1; a++)
			{
				// sum top pixels
				p = pixel + (FIREWIDTH << 1);
				top = *p + *(p - 1) + *(p + 1);

				// bottom pixel
				bottom = *(pixel + (FIREWIDTH << 2));

				// combine pixels
				c1 = (top + bottom) >> 2;
				if (c1 > 1) c1--;

				// store pixels
				*pixel = c1;
				*(pixel + FIREWIDTH) = (c1 + bottom) >> 1;		// interpolate

				// next pixel
				pixel++;
			}

			// special case: last pixel on line
			p = pixel + (FIREWIDTH << 1);
			top = *p + *(p - 1) + *(p - FIREWIDTH + 1);
			bottom = *(pixel + (FIREWIDTH << 2));
			c1 = (top + bottom) >> 2;
			if (c1 > 1) c1--;
			*pixel = c1;
			*(pixel + FIREWIDTH) = (c1 + bottom) >> 1;

			// next line
			from += FIREWIDTH << 1;
		}
	}

	// Draw the screen
	{
		fixed_t xstep, ystep, firex, firey;
		int x, y;
		byte *to, *fromold, *fromnew;

		xstep = (FIREWIDTH * FRACUNIT) / screen.width;
		ystep = (FIREHEIGHT * FRACUNIT) / screen.height;
		to = screen.buffer;
		fromold = (byte *)wipe_scr_start;
		fromnew = (byte *)wipe_scr_end;
		done = true;

		for (y = 0, firey = 0; y < screen.height; y++, firey += ystep)
		{
			for (x = 0, firex = 0; x < screen.width; x++, firex += xstep)
			{
				int fglevel;

				fglevel = burnarray[(firex>>FRACBITS)+(firey>>FRACBITS)*FIREWIDTH] / 2;
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
					unsigned int *fg2rgb = Col2RGB8[fglevel];
					unsigned int *bg2rgb = Col2RGB8[bglevel];
					unsigned int fg = fg2rgb[fromnew[x]];
					unsigned int bg = bg2rgb[fromold[x]];
					fg = (fg+bg) | 0x1f07c1f;
					to[x] = RGB32k[0][0][fg & (fg>>15)];
					done = false;
				}
			}
			fromold += screen.width;
			fromnew += screen.width;
			to += screen.pitch;
		}
	}

	return done || (burntime > 40);
}

int wipe_exitBurn (int ticks)
{
	free (wipe_scr_start);
	free (wipe_scr_end);
	Z_Free (burnarray);
	return 0;
}

// Crossfade --------------------------------------------------------

int wipe_initFade (int ticks)
{
	fade = 0;
	return 0;
}

int wipe_doFade (int ticks)
{
	fade += ticks;
	if (fade > 64)
	{
		V_DrawBlock (0, 0, &screen, screen.width, screen.height, (byte *)wipe_scr_end);
		return 1;
	}
	else
	{
		int x, y;
		fixed_t bglevel = 64 - fade;
		unsigned int *fg2rgb = Col2RGB8[fade];
		unsigned int *bg2rgb = Col2RGB8[bglevel];
		byte *fromnew = (byte *)wipe_scr_end;
		byte *fromold = (byte *)wipe_scr_start;
		byte *to = screen.buffer;

		for (y = 0; y < screen.height; y++)
		{
			for (x = 0; x < screen.width; x++)
			{
				unsigned int fg = fg2rgb[fromnew[x]];
				unsigned int bg = bg2rgb[fromold[x]];
				fg = (fg+bg) | 0x1f07c1f;
				to[x] = RGB32k[0][0][fg & (fg>>15)];
			}
			fromnew += screen.width;
			fromold += screen.width;
			to += screen.pitch;
		}
	}
	fade++;
	return 0;
}

int wipe_exitFade (int ticks)
{
	return 0;
}

// General Wipe Functions -------------------------------------------

int wipe_StartScreen (void)
{
	CurrentWipeType = (int)wipetype->value;
	if (CurrentWipeType < 0)
		CurrentWipeType = 0;
	else if (CurrentWipeType >= wipe_NUMWIPES)
		CurrentWipeType = wipe_NUMWIPES-1;

	if (CurrentWipeType)
	{
		if (screen.is8bit)
			wipe_scr_start = (short *)Malloc (screen.width * screen.height);
		else
			wipe_scr_start = (short *)Malloc (screen.width * screen.height * 4);

		V_GetBlock (0, 0, &screen, screen.width, screen.height, (byte *)wipe_scr_start);
	}

	return 0;
}

int wipe_EndScreen (void)
{
	if (CurrentWipeType)
	{
		if (screen.is8bit)
			wipe_scr_end = (short *)Malloc (screen.width * screen.height);
		else
			wipe_scr_end = (short *)Malloc (screen.width * screen.height * 4);

		V_GetBlock (0, 0, &screen, screen.width, screen.height, (byte *)wipe_scr_end);
		V_DrawBlock (0, 0, &screen, screen.width, screen.height, (byte *)wipe_scr_start); // restore start scr.
	}

	return 0;
}

int wipe_ScreenWipe (int ticks)
{
	static BOOL	go = 0;		// when zero, stop the wipe
	static int (*wipes[])(int) =
	{
		wipe_initMelt, wipe_doMelt, wipe_exitMelt,
		wipe_initBurn, wipe_doBurn, wipe_exitBurn,
		wipe_initFade, wipe_doFade, wipe_exitFade
	};
	int rc;

	if (CurrentWipeType == wipe_None)
		return true;

	// initial stuff
	if (!go)
	{
		go = 1;
		(*wipes[(CurrentWipeType-1)*3])(ticks);
	}

	// do a piece of wipe-in
	V_MarkRect(0, 0, screen.width, screen.height);
	rc = (*wipes[(CurrentWipeType-1)*3+1])(ticks);

	// final stuff
	if (rc)
	{
		go = 0;
		(*wipes[(CurrentWipeType-1)*3+2])(ticks);
	}

	return !go;
}
