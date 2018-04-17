//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
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
//		Mission start screen wipe/melt, special effects.
//		
//-----------------------------------------------------------------------------


#ifndef __F_WIPE_H__
#define __F_WIPE_H__

//
//						 SCREEN WIPE PACKAGE
//

#if 0
bool wipe_StartScreen (int type);
void wipe_EndScreen (void);
bool wipe_ScreenWipe (int ticks);
void wipe_Cleanup ();

// The buffer must have an additional 5 rows not included in height
// to use for a seeding area.
#endif
int wipe_CalcBurn(uint8_t *buffer, int width, int height, int density);

enum
{
	wipe_None,			// don't bother
	wipe_Melt,			// weird screen melt
	wipe_Burn,			// fade in shape of fire
	wipe_Fade,			// crossfade from old to new
	wipe_NUMWIPES
};

#endif
