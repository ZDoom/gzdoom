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


#ifndef __P_INTER__
#define __P_INTER__


BOOL P_GiveAmmo(player_t*, ammotype_t, int);
BOOL P_GiveWeapon(player_t*, weapontype_t, BOOL);
BOOL P_GiveArmor(player_t*, int);
void P_GiveCard(player_t*, card_t);
BOOL P_GivePower(player_t*, int);


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
