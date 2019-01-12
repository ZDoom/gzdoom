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
//
#ifndef __AMMAP_H__
#define __AMMAP_H__

struct event_t;
class FSerializer;
struct FLevelLocals;


void AM_StaticInit();
void AM_ClearColorsets();	// reset data for a restart.

// Called by main loop.
bool AM_Responder (event_t* ev, bool last);

// Called by main loop.
void AM_Ticker (FLevelLocals *Level);

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer (FLevelLocals *Level, int bottom);

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop (void);

void AM_NewResolution (FLevelLocals *Level);
void AM_ToggleMap (FLevelLocals *Level);
void AM_LevelInit (FLevelLocals *Level);


#endif
