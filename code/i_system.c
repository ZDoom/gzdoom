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
//
//-----------------------------------------------------------------------------

static const char
rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/timeb.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include "i_input.h"

#define __BYTEBOOL__

#include "doomdef.h"
#include "m_misc.h"
#include "i_video.h"
#include "i_sound.h"
#include "i_music.h"

#include "d_net.h"
#include "g_game.h"

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"

extern HWND 			Window;
extern HDC				WinDC;
extern LONG 			OemWidth, OemHeight;
extern LONG 			WinWidth, WinHeight;
extern int				ConRows, ConCols, PhysRows;
extern char 			*Lines, *Last;


int 	mb_used = 8;


void
I_Tactile
( int	on,
  int	off,
  int	total )
{
  // UNUSED.
  on = off = total = 0;
}

ticcmd_t		emptycmd;
ticcmd_t*		I_BaseTiccmd(void)
{
	return &emptycmd;
}


int  I_GetHeapSize (void)
{
	return mb_used*1024*1024;
}

byte* I_ZoneBase (int*	size)
{
	*size = mb_used*1024*1024;
	return (byte *) malloc (*size);
}



//
// I_GetTime
// returns time in 1/70th second tics
//
int  I_GetTime (void)
{
	DWORD tm;
	static DWORD basetime = 0;

	tm = timeGetTime();
	if (!basetime)
		basetime = tm;

	return ((tm-basetime)*TICRATE)/1000;
}



//
// I_Init
//
void I_Init (void)
{
	I_InitSound();

	DI_Init (Window);
	//	I_InitGraphics();
}

//
// I_Quit
//
void I_Quit (void)
{
	D_QuitNetGame ();
	I_ShutdownSound();
	I_ShutdownMusic();
	M_SaveDefaults ();
	I_ShutdownGraphics();
	exit(0);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

byte*	I_AllocLow(int length)
{
	byte*		mem;

	mem = (byte *)malloc (length);
	if (mem) {
		memset (mem,0,length);
	}
	return mem;
}


//
// I_Error
//
extern boolean demorecording;
/*
void I_Error (char *error, ...)
{
	va_list 	argptr;

	// Message first.
	va_start (argptr,error);
	fprintf (stderr, "Error: ");
	vfprintf (stderr,error,argptr);
	fprintf (stderr, "\n");
	va_end (argptr);

	fflush( stderr );

	// Shutdown. Here might be other errors.
	if (demorecording)
		G_CheckDemoStatus();

	D_QuitNetGame ();
	I_ShutdownGraphics();
	
	exit(-1);
}
*/
void I_FatalError (char *error, ...)
{
	va_list 	argptr;
	char errtext[1024];
	int index;

	// Message first.
	va_start (argptr,error);
	index = sprintf (errtext, "Error: ");
	index += vsprintf (&errtext[index],error,argptr);
	sprintf (&errtext[index], "\nGetLastError() = 0x%08X", GetLastError());
	va_end (argptr);

	// Shutdown. Here might be other errors.
	if (demorecording)
		G_CheckDemoStatus();

	D_QuitNetGame ();
	I_ShutdownGraphics();

	if (Window)
		ShowWindow (Window, SW_HIDE);
	MessageBox (Window, errtext, "DOOM Fatal Error", MB_OK|MB_ICONSTOP|MB_TASKMODAL);

	exit(-1);
}


void I_PaintConsole (void)
{
	PAINTSTRUCT paint;
	HDC dc;
	char *row;
	int line, last;

	if (dc = BeginPaint (Window, &paint)) {
		if (Last) {
			line = paint.rcPaint.top / OemHeight;
			last = (paint.rcPaint.bottom - 1) / OemHeight;
			for (row = Last - (PhysRows - 1 - line) * (ConCols + 2); line <= last; line++) {
				TextOut (dc, 0, line * OemHeight, row + 2, row[1]);
				row += ConCols + 2;
			}
		}
		EndPaint (Window, &paint);
	}
}

void I_PrintStr (int xp, const char *cp, int count, boolean scroll) {
	MSG mess;

	if (count)
		TextOut (WinDC, xp * OemWidth, WinHeight - OemHeight, cp, count);
	if (scroll) {
		ScrollWindowEx (Window, 0, -OemHeight, NULL, NULL, NULL, NULL, SW_ERASE|SW_INVALIDATE);
		UpdateWindow (Window);
	}
	while (PeekMessage (&mess, Window, 0, 0, PM_REMOVE)) {
		TranslateMessage (&mess);
		DispatchMessage (&mess);
	}
}