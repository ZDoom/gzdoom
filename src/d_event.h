//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
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


#ifndef __D_EVENT_H__
#define __D_EVENT_H__


#include "basictypes.h"
#include <functional>


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
	EV_GUI_Event,	// subtype specifies actual event
	EV_DeviceChange,// a device has been connected or removed
};

// Event structure.
struct event_t
{
	uint8_t		type;
	uint8_t		subtype;
	int16_t 		data1;		// keys / mouse/joystick buttons
	int16_t		data2;
	int16_t		data3;
	int 		x;			// mouse/joystick x move
	int 		y;			// mouse/joystick y move
};

 
enum gameaction_t : int
{
	ga_nothing,
	ga_loadlevel, // not used.
	ga_newgame,
	ga_newgame2,
	ga_recordgame,
	ga_loadgame,
	ga_loadgamehidecon,
	ga_loadgameplaydemo,
	ga_autoloadgame,
	ga_savegame,
	ga_autosave,
	ga_playdemo,
	ga_completed,
	ga_slideshow,
	ga_worlddone,
	ga_screenshot,
	ga_togglemap,
	ga_fullconsole,
	ga_resumeconversation,
};



//
// Button/action code definitions.
// The net code supports up to 29 buttons, so don't make this longer than that.
//
typedef enum
{
	BT_ATTACK		= 1<<0,	// Press "Fire".
	BT_USE			= 1<<1,	// Use button, to open doors, activate switches.
    BT_JUMP			= 1<<2,
    BT_CROUCH		= 1<<3,
	BT_TURN180		= 1<<4,
	BT_ALTATTACK	= 1<<5,	// Press your other "Fire".
	BT_RELOAD		= 1<<6,	// [XA] Reload key. Causes state jump in A_WeaponReady.
	BT_ZOOM			= 1<<7,	// [XA] Zoom key. Ditto.

	// The rest are all ignored by the play simulation and are for scripts.
	BT_SPEED		= 1<<8,
	BT_STRAFE		= 1<<9,

	BT_MOVERIGHT	= 1<<10,
	BT_MOVELEFT		= 1<<11,
	BT_BACK			= 1<<12,
	BT_FORWARD		= 1<<13,
	BT_RIGHT		= 1<<14,
	BT_LEFT			= 1<<15,
	BT_LOOKUP		= 1<<16,
	BT_LOOKDOWN		= 1<<17,
	BT_MOVEUP		= 1<<18,
	BT_MOVEDOWN		= 1<<19,
	BT_SHOWSCORES	= 1<<20,

	BT_USER1		= 1<<21,
	BT_USER2		= 1<<22,
	BT_USER3		= 1<<23,
	BT_USER4		= 1<<24,
} buttoncode_t;

// Called by IO functions when input is detected.
void D_PostEvent (const event_t* ev);
void D_RemoveNextCharEvent();
void D_Render(std::function<void()> action, bool interpolate);


//
// GLOBAL VARIABLES
//
#define MAXEVENTS		128

extern	event_t 		events[MAXEVENTS];

extern	gameaction_t	gameaction;


#endif
