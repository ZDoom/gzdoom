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
#include "thingdef/thingdef.h"

#define WEAPONBOTTOM			128.

// [RH] +0x6000 helps it meet the screen bottom
//		at higher resolutions while still being in
//		the right spot at 320x200.
#define WEAPONTOP				(32+6./16)

class AInventory;
class FArchive;

//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
enum psprnum_t // These are all called by the owner's ReadyWeapon.
{
	ps_weapon = 1,
	ps_flash,
	ps_targetcenter,
	ps_targetleft,
	ps_targetright,
	NUMPSPRITES
};

class DPSprite : public DObject
{
	DECLARE_CLASS (DPSprite, DObject)
	HAS_OBJECT_POINTERS
public:
	DPSprite(player_t *owner, AInventory *caller, int id);

	static void NewTick();
	void SetState(FState *newstate, bool pending = false);

	int					GetID()		const { return ID; }
	int					GetSprite()	const { return Sprite; }
	int					GetFrame()	const { return Frame; }
	FState*				GetState()	const { return State; }
	DPSprite*			GetNext()	      { return Next; }
	AInventory*			GetCaller()	      { return Caller; }

	double x, y;
	double oldx, oldy;
	bool firstTic;
	int Tics;

private:
	DPSprite () {}

	void Serialize(FArchive &arc);
	void Tick();
	void Destroy();

	TObjPtr<AInventory> Caller;
	TObjPtr<DPSprite> Next;
	player_t *Owner;
	FState *State;
	int Sprite;
	int Frame;
	int ID;
	bool processPending; // true: waiting for periodic processing on this tick

	friend class player_t;
	friend void CopyPlayer(player_t *dst, player_t *src, const char *name);
};

void P_NewPspriteTick();
void P_CalcSwing (player_t *player);
void P_SetPsprite(player_t *player, psprnum_t id, FState *state, bool pending = false);
void P_BringUpWeapon (player_t *player);
void P_FireWeapon (player_t *player);
void P_DropWeapon (player_t *player);
void P_BobWeapon (player_t *player, float *x, float *y, double ticfrac);
DAngle P_BulletSlope (AActor *mo, FTranslatedLineTarget *pLineTarget = NULL, int aimflags = 0);

void P_GunShot (AActor *mo, bool accurate, PClassActor *pufftype, DAngle pitch);

void DoReadyWeapon(AActor *self);
void DoReadyWeaponToBob(AActor *self);
void DoReadyWeaponToFire(AActor *self, bool primary = true, bool secondary = true);
void DoReadyWeaponToSwitch(AActor *self, bool switchable = true);

DECLARE_ACTION(A_Raise)
void A_ReFire(AActor *self, FState *state = NULL);

#endif	// __P_PSPR_H__
