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
// DESCRIPTION:
//		Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------


#ifndef __R_STATE_H__
#define __R_STATE_H__

// Need data structure definitions.
#include "doomtype.h"
#include "r_defs.h"
#include "r_data/sprites.h"

//
// Refresh internal data structures,
//	for rendering.
//

extern int				viewwindowx;
extern int				viewwindowy;
extern int				viewwidth;
extern int				viewheight;

//
// Lookup tables for map data.
//
extern TArray<spritedef_t> sprites;
extern uint32_t NumStdSprites;

//
// POV data.
//

int R_FindSkin (const char *name, int pclass);	// [RH] Find a skin

#endif // __R_STATE_H__
