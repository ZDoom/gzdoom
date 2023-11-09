//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
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
//	Sprite animation.
//
//-----------------------------------------------------------------------------


#ifndef __P_PSPR_H__
#define __P_PSPR_H__

#include "renderstyle.h"
#include "palettecontainer.h"

// Basic data types.
// Needs fixed point, and BAM angles.

#define WEAPONBOTTOM			128.

#define WEAPONTOP				32.
#define WEAPON_FUDGE_Y			0.375
struct FTranslatedLineTarget;
struct FState;
class player_t;

//
// Overlay psprites are scaled shapes
// drawn directly on the view screen,
// coordinates are given for a 320*200 view screen.
//
enum PSPLayers
{
	PSP_STRIFEHANDS = -1,
	PSP_CALLERID = 0,
	PSP_WEAPON = 1,
	PSP_FLASH = 1000,
	PSP_TARGETCENTER = INT_MAX - 2,
	PSP_TARGETLEFT,
	PSP_TARGETRIGHT,
};

enum PSPFlags
{
	PSPF_ADDWEAPON		= 1 << 0,
	PSPF_ADDBOB			= 1 << 1,
	PSPF_POWDOUBLE		= 1 << 2,
	PSPF_CVARFAST		= 1 << 3,
	PSPF_ALPHA			= 1 << 4,
	PSPF_RENDERSTYLE	= 1 << 5,
	PSPF_FLIP			= 1 << 6,
	PSPF_FORCEALPHA		= 1 << 7,
	PSPF_FORCESTYLE		= 1 << 8,
	PSPF_MIRROR			= 1 << 9,
	PSPF_PLAYERTRANSLATED = 1 << 10,
	PSPF_PIVOTPERCENT	= 1 << 11,
	PSPF_INTERPOLATE	= 1 << 12,
};

enum PSPAlign
{
	PSPA_TOP = 0,
	PSPA_CENTER,
	PSPA_BOTTOM,
	PSPA_LEFT = PSPA_TOP,
	PSPA_RIGHT = 2
};

struct WeaponInterp
{
	FVector2 v[4];
};

class DPSprite : public DObject
{
	DECLARE_CLASS (DPSprite, DObject)
	HAS_OBJECT_POINTERS
public:
	DPSprite(player_t *owner, AActor *caller, int id);

	static void NewTick();
	void SetState(FState *newstate, bool pending = false);

	int			GetID()							const { return ID; }
	int			GetSprite()						const { return Sprite; }
	int			GetFrame()						const { return Frame; }
	int			GetTics()						const {	return Tics; }
	FTranslationID	GetTranslation()					  { return Translation; }
	FState*		GetState()						const { return State; }
	DPSprite*	GetNext()							  { return Next; }
	AActor*		GetCaller()							  { return Caller; }
	void		SetCaller(AActor *newcaller)		  { Caller = newcaller; }
	void		ResetInterpolation()				  { oldx = x; oldy = y; Prev = Vert; InterpolateTic = false; }
	void OnDestroy() override;
	std::pair<FRenderStyle, float> GetRenderStyle(FRenderStyle ownerstyle, double owneralpha);
	float GetYAdjust(bool fullscreen);

	int HAlign, VAlign;		// Horizontal and vertical alignment
	DVector2 baseScale;		// Base scale (set by weapon); defaults to (1.0, 1.2) since that's Doom's native aspect ratio
	DAngle rotation;		// How much rotation to apply.
	DVector2 pivot;			// pivot points
	DVector2 scale;			// Dynamic scale (set by A_Overlay functions)
	double x, y, alpha;
	double oldx, oldy;
	bool InterpolateTic;	// One tic interpolation (WOF_INTERPOLATE)
	DVector2 Coord[4];		// Offsets
	WeaponInterp Prev;		// Interpolation
	WeaponInterp Vert;		// Current Position
	bool firstTic;
	int Tics;
	FTranslationID Translation;
	int Flags;
	FRenderStyle Renderstyle;

private:
	DPSprite () {}

	void Serialize(FSerializer &arc);

public:	// must be public to be able to generate the field export tables. Grrr...
	TObjPtr<AActor*> Caller;
	TObjPtr<DPSprite*> Next;
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
void P_SetPsprite(player_t *player, PSPLayers id, FState *state, bool pending = false);
void P_BringUpWeapon (player_t *player);
void P_FireWeapon (player_t *player);
void P_BobWeapon (player_t *player, float *x, float *y, double ticfrac);
void P_BobWeapon3D (player_t *player, FVector3 *translation, FVector3 *rotation, double ticfrac);
DAngle P_BulletSlope (AActor *mo, FTranslatedLineTarget *pLineTarget = NULL, int aimflags = 0);
AActor *P_AimTarget(AActor *mo);

void DoReadyWeaponToBob(AActor *self);
void DoReadyWeaponToFire(AActor *self, bool primary = true, bool secondary = true);
void DoReadyWeaponToSwitch(AActor *self, bool switchable = true);

void A_ReFire(AActor *self, FState *state = NULL);

#endif	// __P_PSPR_H__
