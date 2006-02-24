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
// DESCRIPTION:
//
//	  
//-----------------------------------------------------------------------------


#ifndef __D_EVENT_H__
#define __D_EVENT_H__


#include "doomtype.h"


//
// Event handling.
//

// Input event types.
enum EGenericEvent
{
	EV_None,
	EV_KeyDown,		// data1: scan code, data2: Qwerty ASCII code
	EV_KeyUp,		// same
	EV_Mouse,		// x, y: mouse movement deltas
	EV_GUI_Event	// subtype specifies actual event
};

// Event structure.
struct event_t
{
	BYTE		type;
	BYTE		subtype;
	SWORD 		data1;		// keys / mouse/joystick buttons
	SWORD		data2;
	SWORD		data3;
	int 		x;			// mouse/joystick x move
	int 		y;			// mouse/joystick y move
};

 
typedef enum
{
	ga_nothing,
	ga_loadlevel,
	ga_newgame,
	ga_newgame2,
	ga_loadgame,
	ga_savegame,
	ga_autosave,
	ga_playdemo,
	ga_completed,
	ga_slideshow,
	ga_worlddone,
	ga_screenshot,
	ga_togglemap,
	ga_fullconsole,
} gameaction_t;



//
// Button/action code definitions.
//
typedef enum
{
	BT_ATTACK		= 1,	// Press "Fire".
	BT_USE			= 2,	// Use button, to open doors, activate switches.
    BT_JUMP			= 4,
    BT_DUCK			= 8,	// Unimplemented
	BT_TURN180		= 16,
	BT_ALTATTACK	= 32,	// Press your other "Fire".
} buttoncode_t;

#define IMP_WEAPONSLOT


//
// GLOBAL VARIABLES
//
#define MAXEVENTS		128

extern	event_t 		events[MAXEVENTS];
extern	int 			eventhead;
extern	int 			eventtail;

extern	gameaction_t	gameaction;


#endif
