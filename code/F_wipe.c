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

//
//		SCREEN WIPE PACKAGE
//

// when zero, stop the wipe
static BOOL	go = 0;

short		*wipe_scr_start;
short		*wipe_scr_end;
screen_t	*wipe_scr;


void wipe_shittyColMajorXform (short *array, int width, int height)
{
	int 		x;
	int 		y;
	short*		dest;

	dest = (short*) Z_Malloc(width*height*2, PU_STATIC, 0);

	for(y=0;y<height;y++)
		for(x=0;x<width;x++)
			dest[x*height+y] = array[y*width+x];

	memcpy(array, dest, width*height*2);

	Z_Free(dest);

}

int wipe_initColorXForm (int width, int height, int ticks)
{
	V_DrawBlock (0, 0, wipe_scr, width, height, (byte *)wipe_scr_start);
	return 0;
}

int wipe_doColorXForm (int width, int height, int ticks)
{
	BOOL	 	changed;
	byte*		w;
	byte*		wend;
	byte*		e;
	int 		newval;
	int			y;

	changed = false;
	e = (byte *)wipe_scr_end;
	
	for (y = 0; y < height; y++) {
		w = wipe_scr->buffer + wipe_scr->pitch * y;
		wend = w + width;
		while (w!=wend)
		{
			if (*w != *e)
			{
				if (*w > *e)
				{
					newval = *w - ticks;
					if (newval < *e)
						*w = *e;
					else
						*w = (byte)newval;
					changed = true;
				}
				else if (*w < *e)
				{
					newval = *w + ticks;
					if (newval > *e)
						*w = *e;
					else
						*w = (byte)newval;
					changed = true;
				}
			}
			w++;
			e++;
		}
	}

	return !changed;

}

int wipe_exitColorXForm (int width, int height, int ticks)
{
	return 0;
}


static int *y;

int wipe_initMelt (int width, int height, int ticks)
{
	int i, r;
	
	// copy start screen to main screen
	V_DrawBlock (0, 0, wipe_scr, width, height, (byte *)wipe_scr_start);
	
	// makes this wipe faster (in theory)
	// to have stuff in column-major format
	wipe_shittyColMajorXform (wipe_scr_start, width/2, height);
	wipe_shittyColMajorXform (wipe_scr_end, width/2, height);
	
	// setup initial column positions
	// (y<0 => not ready to scroll yet)
	y = (int *) Z_Malloc(width*sizeof(int), PU_STATIC, 0);
	y[0] = -(M_Random()&0xf);
	for (i=1;i<width;i++)
	{
		r = (M_Random()%3) - 1;
		y[i] = y[i-1] + r;
		if (y[i] > 0) y[i] = 0;
		else if (y[i] == -16) y[i] = -15;
	}

	return 0;
}

int wipe_doMelt (int width, int height, int ticks)
{
	int 		i;
	int 		j;
	int 		dy;
	int 		idx;
	
	short*		s;
	short*		d;
	BOOL	 	done = true;

	width/=2;

	while (ticks--)
	{
		for (i=0;i<width;i++)
		{
			if (y[i]<0)
			{
				y[i]++; done = false;
			} 
			else if (y[i] < height)
			{
				dy = (y[i] < 16) ? y[i]+1 : 8;
				dy = (dy * screen.height) / 200;
				if (y[i]+dy >= height) dy = height - y[i];
				s = &wipe_scr_end[i*height+y[i]];
				d = &((short *)wipe_scr->buffer)[y[i]*(wipe_scr->pitch/2)+i];
				idx = 0;
				for (j=dy;j;j--)
				{
					d[idx] = *(s++);
					idx += wipe_scr->pitch/2;
				}
				y[i] += dy;
				s = &wipe_scr_start[i*height];
				d = &((short *)wipe_scr->buffer)[y[i]*(wipe_scr->pitch/2)+i];
				idx = 0;
				for (j=height-y[i];j;j--)
				{
					d[idx] = *(s++);
					idx += wipe_scr->pitch/2;
				}
				done = false;
			}
		}
	}

	return done;

}

int wipe_exitMelt (int width, int height, int ticks)
{
	free (wipe_scr_start);
	free (wipe_scr_end);
	Z_Free(y);
	return 0;
}

int wipe_StartScreen (int x, int y, int width, int height)
{
	wipe_scr = &screen;

	if (wipe_scr->is8bit)
		wipe_scr_start = (short *)Malloc (width * height);
	else
		wipe_scr_start = (short *)Malloc (width * height * 4);

	V_GetBlock (0, 0, wipe_scr, width, height, (byte *)wipe_scr_start);
	return 0;
}

int wipe_EndScreen (int x, int y, int width, int height)
{
	if (wipe_scr->is8bit)
		wipe_scr_end = (short *)Malloc (width * height);
	else
		wipe_scr_end = (short *)Malloc (width * height * 4);

	V_GetBlock (0, 0, wipe_scr, width, height, (byte *)wipe_scr_end);
	V_DrawBlock (0, 0, wipe_scr, width, height, (byte *)wipe_scr_start); // restore start scr.

	return 0;
}

int wipe_ScreenWipe (int wipeno, int x, int y, int width, int height, int ticks)
{
	int rc;
	static int (*wipes[])(int, int, int) =
	{
		wipe_initColorXForm, wipe_doColorXForm, wipe_exitColorXForm,
		wipe_initMelt, wipe_doMelt, wipe_exitMelt
	};

	void V_MarkRect(int, int, int, int);

	// initial stuff
	if (!go)
	{
		go = 1;
		// wipe_scr = (byte *) Z_Malloc(width*height, PU_STATIC, 0); // DEBUG
		wipe_scr = &screen;
		(*wipes[wipeno*3])(width, height, ticks);
	}

	// do a piece of wipe-in
	V_MarkRect(0, 0, width, height);
	rc = (*wipes[wipeno*3+1])(width, height, ticks);
	//	V_DrawBlock(x, y, 0, width, height, wipe_scr); // DEBUG

	// final stuff
	if (rc)
	{
		go = 0;
		(*wipes[wipeno*3+2])(width, height, ticks);
	}

	return !go;

}
