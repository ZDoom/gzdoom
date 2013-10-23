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
//	Sprite animation.
//
//-----------------------------------------------------------------------------


#ifndef __P_PSPR_H__
#define __P_PSPR_H__

// Basic data types.
// Needs fixed point, and BAM angles.
#include "tables.h"
#include "thingdef/thingdef.h"

#define WEAPONBOTTOM			128*FRACUNIT

// [RH] +0x6000 helps it meet the screen bottom
//		at higher resolutions while still being in
//		the right spot at 320x200.
#define WEAPONTOP				(32*FRACUNIT+0x6000)


//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
typedef enum
{
	ps_weapon,
	ps_flash,
	ps_targetcenter,
	ps_targetleft,
	ps_targetright,
	NUMPSPRITES

} psprnum_t;

/*
inline FArchive &operator<< (FArchive &arc, psprnum_t &i)
{
	BYTE val = (BYTE)i;
	arc << val;
	i = (psprnum_t)val;
	return arc;
}
*/

struct pspdef_t
{
	FState*		state;	// a NULL state means not active
	int 		tics;
	fixed_t 	sx;
	fixed_t 	sy;
	int			sprite;
	int			frame;
	bool		processPending; // true: waiting for periodic processing on this tick
};

class FArchive;

FArchive &operator<< (FArchive &, pspdef_t &);

class player_t;
class AActor;
struct FState;

void P_NewPspriteTick();
void P_SetPsprite (player_t *player, int position, FState *state, bool nofunction=false);
void P_CalcSwing (player_t *player);
void P_BringUpWeapon (player_t *player);
void P_FireWeapon (player_t *player);
void P_DropWeapon (player_t *player);
void P_BobWeapon (player_t *player, pspdef_t *psp, fixed_t *x, fixed_t *y);
angle_t P_BulletSlope (AActor *mo, AActor **pLineTarget = NULL);
void P_GunShot (AActor *mo, bool accurate, const PClass *pufftype, angle_t pitch);

void DoReadyWeapon(AActor * self);
void DoReadyWeaponToBob(AActor * self);
void DoReadyWeaponToFire(AActor * self, bool primary = true, bool secondary = true);
void DoReadyWeaponToSwitch(AActor * self, bool switchable = true);

DECLARE_ACTION(A_Raise)
void A_ReFire(AActor *self, FState *state = NULL);

#endif	// __P_PSPR_H__
