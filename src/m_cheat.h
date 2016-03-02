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
//		Cheat code checking.
//
//-----------------------------------------------------------------------------


#ifndef __M_CHEAT_H__
#define __M_CHEAT_H__

//
// CHEAT SEQUENCE PACKAGE
//

// [RH] Functions that actually perform the cheating
class player_t;
class PClassPlayerPawn;

void cht_DoCheat (player_t *player, int cheat);
void cht_Give (player_t *player, const char *item, int amount=1);
void cht_Take (player_t *player, const char *item, int amount=1);
void cht_Suicide (player_t *player);
const char *cht_Morph (player_t *player, PClassPlayerPawn *morphclass, bool quickundo);

#endif
