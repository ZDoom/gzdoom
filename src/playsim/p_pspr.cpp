//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2016 Leonard2
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

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "doomdef.h"
#include "d_event.h"
#include "c_cvars.h"
#include "m_random.h"
#include "p_enemy.h"
#include "p_local.h"
#include "s_sound.h"
#include "doomstat.h"
#include "p_pspr.h"
#include "templates.h"
#include "g_level.h"
#include "d_player.h"
#include "serializer_doom.h"
#include "serialize_obj.h"
#include "v_text.h"
#include "cmdlib.h"
#include "g_levellocals.h"
#include "vm.h"
#include "sbar.h"


// MACROS ------------------------------------------------------------------

#define LOWERSPEED				6.

// TYPES -------------------------------------------------------------------

struct FGenericButtons
{
	int ReadyFlag;			// Flag passed to A_WeaponReady
	int StateFlag;			// Flag set in WeaponState
	int ButtonFlag;			// Button to press
	ENamedName StateName;	// Name of the button/state
};

enum EWRF_Options
{
	WRF_NoBob			= 1,
	WRF_NoSwitch		= 1 << 1,
	WRF_NoPrimary		= 1 << 2,
	WRF_NoSecondary		= 1 << 3,
	WRF_NoFire = WRF_NoPrimary | WRF_NoSecondary,
	WRF_AllowReload		= 1 << 4,
	WRF_AllowZoom		= 1 << 5,
	WRF_DisableSwitch	= 1 << 6,
	WRF_AllowUser1		= 1 << 7,
	WRF_AllowUser2		= 1 << 8,
	WRF_AllowUser3		= 1 << 9,
	WRF_AllowUser4		= 1 << 10,
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// [SO] 1=Weapons states are all 1 tick
//		2=states with a function 1 tick, others 0 ticks.
CVAR(Int, sv_fastweapons, false, CVAR_SERVERINFO);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const FGenericButtons ButtonChecks[] =
{
	{ WRF_AllowZoom,	WF_WEAPONZOOMOK,	BT_ZOOM,	NAME_Zoom },
	{ WRF_AllowReload,	WF_WEAPONRELOADOK,	BT_RELOAD,	NAME_Reload },
	{ WRF_AllowUser1,	WF_USER1OK,			BT_USER1,	NAME_User1 },
	{ WRF_AllowUser2,	WF_USER2OK,			BT_USER2,	NAME_User2 },
	{ WRF_AllowUser3,	WF_USER3OK,			BT_USER3,	NAME_User3 },
	{ WRF_AllowUser4,	WF_USER4OK,			BT_USER4,	NAME_User4 },
};

// CODE --------------------------------------------------------------------

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

IMPLEMENT_CLASS(DPSprite, false, true)

IMPLEMENT_POINTERS_START(DPSprite)
	IMPLEMENT_POINTER(Caller)
	IMPLEMENT_POINTER(Next)
IMPLEMENT_POINTERS_END

DEFINE_FIELD_NAMED(DPSprite, State, CurState)	// deconflict with same named type
DEFINE_FIELD(DPSprite, Caller)
DEFINE_FIELD(DPSprite, Next)
DEFINE_FIELD(DPSprite, Owner)
DEFINE_FIELD(DPSprite, Sprite)
DEFINE_FIELD(DPSprite, Frame)
DEFINE_FIELD(DPSprite, Flags)
DEFINE_FIELD(DPSprite, ID)
DEFINE_FIELD(DPSprite, processPending)
DEFINE_FIELD(DPSprite, x)
DEFINE_FIELD(DPSprite, y)
DEFINE_FIELD(DPSprite, oldx)
DEFINE_FIELD(DPSprite, oldy)
DEFINE_FIELD(DPSprite, pivot)
DEFINE_FIELD(DPSprite, scale)
DEFINE_FIELD(DPSprite, rotation)
DEFINE_FIELD_NAMED(DPSprite, Coord[0], Coord0)
DEFINE_FIELD_NAMED(DPSprite, Coord[1], Coord1)
DEFINE_FIELD_NAMED(DPSprite, Coord[2], Coord2)
DEFINE_FIELD_NAMED(DPSprite, Coord[3], Coord3)
DEFINE_FIELD(DPSprite, firstTic)
DEFINE_FIELD(DPSprite, Tics)
DEFINE_FIELD(DPSprite, Translation)
DEFINE_FIELD(DPSprite, HAlign)
DEFINE_FIELD(DPSprite, VAlign)
DEFINE_FIELD(DPSprite, alpha)
DEFINE_FIELD(DPSprite, InterpolateTic)
DEFINE_FIELD_BIT(DPSprite, Flags, bAddWeapon, PSPF_ADDWEAPON)
DEFINE_FIELD_BIT(DPSprite, Flags, bAddBob, PSPF_ADDBOB)
DEFINE_FIELD_BIT(DPSprite, Flags, bPowDouble, PSPF_POWDOUBLE)
DEFINE_FIELD_BIT(DPSprite, Flags, bCVarFast, PSPF_CVARFAST)
DEFINE_FIELD_BIT(DPSprite, Flags, bFlip, PSPF_FLIP)
DEFINE_FIELD_BIT(DPSprite, Flags, bMirror, PSPF_MIRROR)
DEFINE_FIELD_BIT(DPSprite, Flags, bPlayerTranslated, PSPF_PLAYERTRANSLATED)
DEFINE_FIELD_BIT(DPSprite, Flags, bPivotPercent, PSPF_PIVOTPERCENT)
DEFINE_FIELD_BIT(DPSprite, Flags, bInterpolate, PSPF_INTERPOLATE)

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

DPSprite::DPSprite(player_t *owner, AActor *caller, int id)
: HAlign(0),
  VAlign(0),
  x(.0), y(.0),
  oldx(.0), oldy(.0),
  InterpolateTic(false),
  firstTic(true),
  Tics(0),
  Translation(0),
  Flags(0),
  Caller(caller),
  Owner(owner),
  State(nullptr),
  Sprite(0),
  Frame(0),
  ID(id),
  processPending(true)
{
	rotation = 0.;
	scale = {1.0, 1.0};
	pivot = {0.0, 0.0};
	for (int i = 0; i < 4; i++)
	{
		Coord[i] = DVector2(0, 0);
		Prev.v[i] = Vert.v[i] = FVector2(0,0);
	}
	
	alpha = 1;
	Renderstyle = STYLE_Normal;

	DPSprite *prev = nullptr;
	DPSprite *next = Owner->psprites;
	while (next != nullptr && next->ID < ID)
	{
		prev = next;
		next = next->Next;
	}
	Next = next;
	GC::WriteBarrier(this, next);
	if (prev == nullptr)
	{
		Owner->psprites = this;
		GC::WriteBarrier(this);
	}
	else
	{
		prev->Next = this;
		GC::WriteBarrier(prev, this);
	}

	if (Next && Next->ID == ID && ID != 0)
		Next->Destroy(); // Replace it.

	if (Caller->IsKindOf(NAME_Weapon) || Caller->IsKindOf(NAME_PlayerPawn))
		Flags = (PSPF_ADDWEAPON|PSPF_ADDBOB|PSPF_POWDOUBLE|PSPF_CVARFAST|PSPF_PIVOTPERCENT);
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

DPSprite *player_t::FindPSprite(int layer)
{
	if (layer == 0)
		return nullptr;

	DPSprite *pspr = psprites;
	while (pspr)
	{
		if (pspr->ID == layer)
			break;

		pspr = pspr->Next;
	}

	return pspr;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, FindPSprite)	// the underscore is needed to get past the name mangler which removes the first clas name character to match the class representation (needs to be fixed in a later commit)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(id);
	ACTION_RETURN_OBJECT(self->FindPSprite((PSPLayers)id));
}


//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

void P_SetPsprite(player_t *player, PSPLayers id, FState *state, bool pending)
{
	if (player == nullptr) return;
	auto psp = player->GetPSprite(id);
	if (psp) psp->SetState(state, pending);
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetPSprite)	// the underscore is needed to get past the name mangler which removes the first clas name character to match the class representation (needs to be fixed in a later commit)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(id);
	PARAM_POINTER(state, FState);
	PARAM_BOOL(pending);
	P_SetPsprite(self, (PSPLayers)id, state, pending);
	return 0;
}

DPSprite *player_t::GetPSprite(PSPLayers layer)
{
	AActor *oldcaller = nullptr;
	AActor *newcaller = nullptr;

	if (layer >= PSP_TARGETCENTER)
	{
		if (mo != nullptr)
		{
			newcaller = mo->FindInventory(NAME_PowerTargeter, true);
		}
	}
	else if (layer == PSP_STRIFEHANDS)
	{
		newcaller = mo;
	}
	else
	{
		newcaller = ReadyWeapon;
	}

	if (newcaller == nullptr) return nullptr; // Error case was not handled properly. This function cannot give a guarantee to always succeed!
	
	DPSprite *pspr = FindPSprite(layer);
	if (pspr == nullptr)
	{
		pspr = Create<DPSprite>(this, newcaller, layer);
	}
	else
	{
		oldcaller = pspr->Caller;
	}

	// Always update the caller here in case we switched weapon
	// or if the layer was being used by something else before.
	pspr->Caller = newcaller;

	if (newcaller != oldcaller)
	{ // Only reset stuff if this layer was created now or if it was being used before.
		if (layer >= PSP_TARGETCENTER)
		{ // The targeter layers were affected by those.
			pspr->Flags = (PSPF_CVARFAST|PSPF_POWDOUBLE);
		}
		else
		{
			pspr->Flags = (PSPF_ADDWEAPON|PSPF_ADDBOB|PSPF_CVARFAST|PSPF_POWDOUBLE|PSPF_PIVOTPERCENT);
		}
		if (layer == PSP_STRIFEHANDS)
		{
			// Some of the old hacks rely on this layer coming from the FireHands state.
			// This is the ONLY time a psprite's state is actually null.
			pspr->State = nullptr;
			pspr->y = WEAPONTOP;
		}

		pspr->firstTic = true;
	}

	return pspr;
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, GetPSprite)	// the underscore is needed to get past the name mangler which removes the first clas name character to match the class representation (needs to be fixed in a later commit)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_INT(id);
	ACTION_RETURN_OBJECT(self->GetPSprite((PSPLayers)id));
}



std::pair<FRenderStyle, float> DPSprite::GetRenderStyle(FRenderStyle ownerstyle, double owneralpha)
{
	FRenderStyle returnstyle, mystyle;
	double returnalpha;

	ownerstyle.CheckFuzz();
	mystyle = Renderstyle;
	mystyle.CheckFuzz();
	if (Flags & PSPF_RENDERSTYLE)
	{
		if (Flags & PSPF_FORCESTYLE)
		{
			returnstyle = mystyle;
		}
		else if (ownerstyle.BlendOp != STYLEOP_Add)
		{
			// all styles that do more than simple blending need to be fully preserved.
			returnstyle = ownerstyle;
		}
		else
		{
			returnstyle = mystyle;
			if (ownerstyle.DestAlpha == STYLEALPHA_One)
			{
				// If the owner is additive and the overlay translucent, force additive result.
				returnstyle.DestAlpha = STYLEALPHA_One;
			}
			if (ownerstyle.Flags & (STYLEF_ColorIsFixed|STYLEF_RedIsAlpha))
			{
				// If the owner's style is a stencil type, this must be preserved.
				returnstyle.Flags = ownerstyle.Flags;
			}
		}
	}
	else
	{
		returnstyle = ownerstyle;
	}

	if ((Flags & PSPF_FORCEALPHA) && returnstyle.BlendOp != STYLEOP_Fuzz && returnstyle.BlendOp != STYLEOP_Shadow)
	{
		// ALWAYS take priority if asked for, except fuzz. Fuzz does absolutely nothing
		// no matter what way it's changed.
		returnalpha = alpha;
		returnstyle.Flags &= ~(STYLEF_Alpha1 | STYLEF_TransSoulsAlpha);
	}
	else if (Flags & PSPF_ALPHA)
	{
		// Set the alpha based on if using the overlay's own or not. Also adjust
		// and override the alpha if not forced.
		if (returnstyle.BlendOp != STYLEOP_Add)
		{
			returnalpha = owneralpha;
		}
		else
		{
			returnalpha = owneralpha * alpha;
		}
	}
	else
	{
		returnalpha = owneralpha;
	}

	// Should normal renderstyle come out on top at the end and we desire alpha,
	// switch it to translucent. Normal never applies any sort of alpha.
	if (returnstyle.BlendOp == STYLEOP_Add && returnstyle.SrcAlpha == STYLEALPHA_One && returnstyle.DestAlpha == STYLEALPHA_Zero && returnalpha < 1.)
	{
		returnstyle = LegacyRenderStyles[STYLE_Translucent];
		returnalpha = owneralpha * alpha;
	}

	return{ returnstyle, clamp<float>(float(returnalpha), 0.f, 1.f) };
}

//---------------------------------------------------------------------------
//
// PROC P_NewPspriteTick
//
//---------------------------------------------------------------------------

void DPSprite::NewTick()
{
	// This function should be called after the beginning of a tick, before any possible
	// prprite-event, or near the end, after any possible psprite event.
	// Because data is reset for every tick (which it must be) this has no impact on savegames.
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			DPSprite *pspr = players[i].psprites;
			while (pspr)
			{
				pspr->processPending = true;
				pspr->ResetInterpolation();

				pspr = pspr->Next;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// PROC P_SetPsprite
//
//---------------------------------------------------------------------------

void DPSprite::SetState(FState *newstate, bool pending)
{
	if (ID == PSP_WEAPON)
	{ // A_WeaponReady will re-set these as needed
		Owner->WeaponState &= ~(WF_WEAPONREADY | WF_WEAPONREADYALT | WF_WEAPONBOBBING | WF_WEAPONSWITCHOK | WF_WEAPONRELOADOK | WF_WEAPONZOOMOK |
								WF_USER1OK | WF_USER2OK | WF_USER3OK | WF_USER4OK);
	}

	processPending = pending;

	do
	{
		if (newstate == nullptr)
		{ // Object removed itself.
			Destroy();
			return;
		}

		if (!(newstate->UseFlags & (SUF_OVERLAY|SUF_WEAPON)))	// Weapon and overlay are mostly the same, the main difference is that weapon states restrict the self pointer to class Actor.
		{
			Printf(TEXTCOLOR_RED "State %s not flagged for use in overlays or weapons\n", FState::StaticGetStateName(newstate).GetChars());
			State = nullptr;
			Destroy();
			return;
		}
		else if (!(newstate->UseFlags & SUF_WEAPON))
		{
			if (Caller->IsKindOf(NAME_Weapon))
			{
				Printf(TEXTCOLOR_RED "State %s not flagged for use in weapons\n", FState::StaticGetStateName(newstate).GetChars());
				State = nullptr;
				Destroy();
				return;
			}
		}

		State = newstate;

		if (newstate->sprite != SPR_FIXED)
		{ // okay to change sprite and/or frame
			if (!newstate->GetSameFrame())
			{ // okay to change frame
				Frame = newstate->GetFrame();
			}
			if (newstate->sprite != SPR_NOCHANGE)
			{ // okay to change sprite
				Sprite = newstate->sprite;
			}
		}

		Tics = newstate->GetTics(); // could be 0

		if (Flags & PSPF_CVARFAST)
		{
			if (sv_fastweapons == 2 && ID == PSP_WEAPON)
				Tics = newstate->ActionFunc == nullptr ? 0 : 1;
			else if (sv_fastweapons == 3)
				Tics = (newstate->GetTics() != 0);
			else if (sv_fastweapons)
				Tics = 1;		// great for producing decals :)
		}

		if (ID != PSP_FLASH)
		{ // It's still possible to set the flash layer's offsets with the action function.
			// Anything going through here cannot be reliably interpolated so this has to reset the interpolation coordinates if it changes the values.
			if (newstate->GetMisc1())
			{ // Set coordinates.
				oldx = x = newstate->GetMisc1();
			}
			if (newstate->GetMisc2())
			{
				oldy = y = newstate->GetMisc2();
			}
		}

		if (Owner->mo != nullptr)
		{
			FState *nextstate;
			FStateParamInfo stp = { newstate, STATE_Psprite, ID };
			if (newstate->ActionFunc != nullptr && newstate->ActionFunc->Unsafe)
			{
				// If an unsafe function (i.e. one that accesses user variables) is being detected, print a warning once and remove the bogus function. We may not call it because that would inevitably crash.
				Printf(TEXTCOLOR_RED "Unsafe state call in state %sd to %s which accesses user variables. The action function has been removed from this state\n", 
					FState::StaticGetStateName(newstate).GetChars(), newstate->ActionFunc->PrintableName.GetChars());
				newstate->ActionFunc = nullptr;
			}
			if (newstate->CallAction(Owner->mo, Caller, &stp, &nextstate))
			{
				// It's possible this call resulted in this very layer being replaced.
				if (ObjectFlags & OF_EuthanizeMe)
				{
					return;
				}
				if (nextstate != nullptr)
				{
					newstate = nextstate;
					Tics = 0;
					continue;
				}
				if (State == nullptr)
				{
					Destroy();
					return;
				}
			}
		}

		newstate = State->GetNextState();
	} while (!Tics); // An initial state of 0 could cycle through.

	return;
}

DEFINE_ACTION_FUNCTION(DPSprite, SetState)
{
	PARAM_SELF_PROLOGUE(DPSprite);
	PARAM_POINTER(state, FState);
	PARAM_BOOL(pending);
	self->SetState(state, pending);
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC P_BringUpWeapon
//
// Starts bringing the pending weapon up from the bottom of the screen.
// This is only called to start the rising, not throughout it.
//
//---------------------------------------------------------------------------

void P_BringUpWeapon (player_t *player)
{
	IFVM(PlayerPawn, BringUpWeapon)
	{
		VMValue param = player->mo;
		VMCall(func, &param, 1, nullptr, 0);
	}
}

//============================================================================
//
// P_BobWeapon
//
// [RH] Moved this out of A_WeaponReady so that the weapon can bob every
// tic and not just when A_WeaponReady is called. Not all weapons execute
// A_WeaponReady every tic, and it looks bad if they don't bob smoothly.
//
// [XA] Added new bob styles and exposed bob properties. Thanks, Ryan Cordell!
// [SP] Added new user option for bob speed
//
//============================================================================

void P_BobWeapon (player_t *player, float *x, float *y, double ticfrac)
{
	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, BobWeapon)
	{
		VMValue param[] = { player->mo, ticfrac };
		DVector2 result;
		VMReturn ret(&result);
		VMCall(func, param, 2, &ret, 1);
		*x = (float)result.X;
		*y = (float)result.Y;
		return;
	}
	*x = *y = 0;
}

//---------------------------------------------------------------------------
//
// PROC P_CheckWeaponButtons
//
// Check extra button presses for weapons.
//
//---------------------------------------------------------------------------

static void P_CheckWeaponButtons (player_t *player)
{
	auto weapon = player->ReadyWeapon;
	if (weapon == nullptr)
	{
		return;
	}
	// The button checks are ordered by precedence. The first one to match a
	// button press and affect a state change wins.
	for (size_t i = 0; i < countof(ButtonChecks); ++i)
	{
		if ((player->WeaponState & ButtonChecks[i].StateFlag) &&
			(player->cmd.ucmd.buttons & ButtonChecks[i].ButtonFlag))
		{
			FState *state = weapon->FindState(ButtonChecks[i].StateName);
			// [XA] don't change state if still null, so if the modder
			// sets WRF_xxx to true but forgets to define the corresponding
			// state, the weapon won't disappear. ;)
			if (state != nullptr)
			{
				P_SetPsprite(player, PSP_WEAPON, state);
				return;
			}
		}
	}
}

DEFINE_ACTION_FUNCTION(APlayerPawn, CheckWeaponButtons)
{
	PARAM_SELF_PROLOGUE(AActor);
	P_CheckWeaponButtons(self->player);
	return 0;
}



enum WOFFlags
{
	WOF_KEEPX = 1,
	WOF_KEEPY = 1 << 1,
	WOF_ADD = 1 << 2,
	WOF_INTERPOLATE = 1 << 3,
	WOF_RELATIVE = 1 << 4,
	WOF_ZEROY = 1 << 5,
};

void HandleOverlayRelative(DPSprite *psp, double *x, double *y)
{
	double wx = *x;
	double wy = *y;

	double c = psp->rotation.Cos(), s = psp->rotation.Sin();
	double nx = wx * c + wy * s;
	double ny = wx * s - wy * c;
	*x = nx; *y = ny;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayVertexOffset
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayVertexOffset)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer)
	PARAM_INT(index)
	PARAM_FLOAT(x)
	PARAM_FLOAT(y)
	PARAM_INT(flags)

	if (index < 0 || index > 3 || ((flags & WOF_KEEPX) && (flags & WOF_KEEPY)) || !ACTION_CALL_FROM_PSPRITE())
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr == nullptr)
		return 0;

	if (!(flags & WOF_KEEPX))	pspr->Coord[index].X = (flags & WOF_ADD) ? pspr->Coord[index].X + x : x;
	if (!(flags & WOF_KEEPY))	pspr->Coord[index].Y = (flags & WOF_ADD) ? pspr->Coord[index].Y + y : y;

	if (flags & (WOF_ADD | WOF_INTERPOLATE))	pspr->InterpolateTic = true;

	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayScale
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayScale)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer)
	PARAM_FLOAT(wx)
	PARAM_FLOAT(wy)
	PARAM_INT(flags)
	
	if (!ACTION_CALL_FROM_PSPRITE() || ((flags & WOF_KEEPX) && (flags & WOF_KEEPY)))
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr == nullptr)
		return 0;

	if (!(flags & WOF_ZEROY) && wy == 0.0)
		wy = wx;

	if (!(flags & WOF_KEEPX))	pspr->scale.X = (flags & WOF_ADD) ? pspr->scale.X + wx : wx;
	if (!(flags & WOF_KEEPY))	pspr->scale.Y = (flags & WOF_ADD) ? pspr->scale.Y + wy : wy;

	if (flags & (WOF_ADD | WOF_INTERPOLATE))	pspr->InterpolateTic = true;

	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayRotate
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayRotate)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer)
	PARAM_ANGLE(degrees)
	PARAM_INT(flags)

	if (!ACTION_CALL_FROM_PSPRITE())
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr != nullptr)
	{
		pspr->rotation = (flags & WOF_ADD) ? pspr->rotation + degrees : degrees;
		if (flags & (WOF_ADD | WOF_INTERPOLATE))	pspr->InterpolateTic = true;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayPivot
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayPivot)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer)
	PARAM_FLOAT(wx)
	PARAM_FLOAT(wy)
	PARAM_INT(flags)

	if (!ACTION_CALL_FROM_PSPRITE() || ((flags & WOF_KEEPX) && (flags & WOF_KEEPY)))
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr == nullptr)
		return 0;

	if (flags & WOF_RELATIVE)
	{
		HandleOverlayRelative(pspr, &wx, &wy);
	}

	if (!(flags & WOF_KEEPX))	pspr->pivot.X = (flags & WOF_ADD) ? pspr->pivot.X + wx : wx;
	if (!(flags & WOF_KEEPY))	pspr->pivot.Y = (flags & WOF_ADD) ? pspr->pivot.Y + wy : wy;

	if (flags & (WOF_ADD | WOF_INTERPOLATE))	pspr->InterpolateTic = true;

	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayOffset
//
//---------------------------------------------------------------------------

void A_OverlayOffset(AActor *self, int layer, double wx, double wy, int flags)
{
	if ((flags & WOF_KEEPX) && (flags & WOF_KEEPY))
	{
		return;
	}

	player_t *player = self->player;
	DPSprite *psp;

	if (player)
	{
		psp = player->FindPSprite(layer);

		if (psp == nullptr)
			return;

		if (flags & WOF_RELATIVE)
		{
			HandleOverlayRelative(psp, &wx, &wy);
		}

		if (!(flags & WOF_KEEPX))		psp->x = (flags & WOF_ADD) ? psp->x + wx : wx;
		if (!(flags & WOF_KEEPY))		psp->y = (flags & WOF_ADD) ? psp->y + wy : wy;
		
		if (!(flags & (WOF_ADD | WOF_INTERPOLATE)))
			psp->ResetInterpolation();
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_OverlayOffset)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer)
	PARAM_FLOAT(wx)	
	PARAM_FLOAT(wy)	
	PARAM_INT(flags)
	A_OverlayOffset(self, ((layer != 0) ? layer : stateinfo->mPSPIndex), wx, wy, flags);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_WeaponOffset)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(wx)	
	PARAM_FLOAT(wy)	
	PARAM_INT(flags)
	A_OverlayOffset(self, PSP_WEAPON, wx, wy, flags);
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayFlags
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayFlags)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);
	PARAM_INT(flags);
	PARAM_BOOL(set);

	if (!ACTION_CALL_FROM_PSPRITE())
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr == nullptr)
		return 0;

	if (set)
		pspr->Flags |= flags;
	else
	{
		pspr->Flags &= ~flags;

		// This is the only way to shut off the temporary interpolation tic
		// in the event another mod is causing potential interference
		if (flags & PSPF_INTERPOLATE)
			pspr->ResetInterpolation();
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_OverlayPivotAlign)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);
	PARAM_INT(halign);
	PARAM_INT(valign);

	if (!ACTION_CALL_FROM_PSPRITE())
		return 0;

	DPSprite *pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));

	if (pspr != nullptr)
	{
		if (halign >= PSPA_LEFT && halign <= PSPA_RIGHT)
			pspr->HAlign |= halign;
		if (valign >= PSPA_TOP && valign <= PSPA_BOTTOM)
			pspr->VAlign |= valign;
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayTranslation
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayTranslation)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);
	PARAM_NAME(trname);

	if (!ACTION_CALL_FROM_PSPRITE())
		return 0;

	DPSprite* pspr = self->player->FindPSprite(((layer != 0) ? layer : stateinfo->mPSPIndex));
	if (pspr != nullptr)
	{
		// There is no constant for the empty name...
		if (trname.GetChars()[0] == 0)
		{
			// an empty string resets to the default
			// (unlike AActor::SetTranslation, there is no Default block for PSprites, so just set the translation to 0)
			pspr->Translation = 0;
			return 0;
		}

		int tnum = R_FindCustomTranslation(trname);
		if (tnum >= 0)
		{
			pspr->Translation = tnum;
		}
		// silently ignore if the name does not exist, this would create some insane message spam otherwise.
	}

	return 0;
}

//---------------------------------------------------------------------------
//
// PROC OverlayX/Y
// Action function to return the X/Y of an overlay.
//---------------------------------------------------------------------------

static double GetOverlayPosition(AActor *self, int layer, bool gety)
{
	if (layer)
	{
		DPSprite *pspr = self->player->FindPSprite(layer);

		if (pspr != nullptr)
		{
			return gety ? (pspr->y) : (pspr->x);
		}
	}
	return 0.;
}

DEFINE_ACTION_FUNCTION(AActor, OverlayX)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		double res = GetOverlayPosition(self, ((layer != 0) ? layer : stateinfo->mPSPIndex), false);
		ACTION_RETURN_FLOAT(res);	
	}
	ACTION_RETURN_FLOAT(0.);
}

DEFINE_ACTION_FUNCTION(AActor, OverlayY)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		double res = GetOverlayPosition(self, ((layer != 0) ? layer : stateinfo->mPSPIndex), true);
		ACTION_RETURN_FLOAT(res);
	}
	ACTION_RETURN_FLOAT(0.);
}

//---------------------------------------------------------------------------
//
// PROC OverlayID
// Because non-action functions cannot acquire the ID of the overlay...
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, OverlayID)
{
	PARAM_ACTION_PROLOGUE(AActor);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		ACTION_RETURN_INT(stateinfo->mPSPIndex);
	}
	ACTION_RETURN_INT(0);
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayAlpha
// Sets the alpha of an overlay.
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayAlpha)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);
	PARAM_FLOAT(alph);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		DPSprite *pspr = self->player->FindPSprite((layer != 0) ? layer : stateinfo->mPSPIndex);

		if (pspr != nullptr)
			pspr->alpha = clamp<double>(alph, 0.0, 1.0);
	}
	return 0;
}

// NON-ACTION function to get the overlay alpha of a layer.
DEFINE_ACTION_FUNCTION(AActor, OverlayAlpha)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		DPSprite *pspr = self->player->FindPSprite((layer != 0) ? layer : stateinfo->mPSPIndex);

		if (pspr != nullptr)
		{
			ACTION_RETURN_FLOAT(pspr->alpha);
		}
	}
	ACTION_RETURN_FLOAT(0.0);
}

//---------------------------------------------------------------------------
//
// PROC A_OverlayRenderStyle
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_OverlayRenderStyle)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(layer);
	PARAM_INT(style);

	if (ACTION_CALL_FROM_PSPRITE())
	{
		DPSprite *pspr = self->player->FindPSprite((layer != 0) ? layer : stateinfo->mPSPIndex);

		if (pspr == nullptr || style >= STYLE_Count || style < 0)
			return 0;

		pspr->Renderstyle = ERenderStyle(style);
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_Overlay
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_Overlay)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT		(layer);
	PARAM_STATE_ACTION(state);
	PARAM_BOOL(dontoverride);

	player_t *player = self->player;

	if (player == nullptr || (dontoverride && (player->FindPSprite(layer) != nullptr)))
	{
		ACTION_RETURN_BOOL(false);
	}

	DPSprite *pspr;
	pspr = Create<DPSprite>(player, stateowner, layer);
	pspr->SetState(state);
	ACTION_RETURN_BOOL(true);
}

DEFINE_ACTION_FUNCTION(AActor, A_ClearOverlays)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_INT(start);
	PARAM_INT(stop);
	PARAM_BOOL(safety)

	if (self->player == nullptr)
		ACTION_RETURN_INT(0);

	if (!start && !stop)
	{
		start = INT_MIN;
		stop = safety ? PSP_TARGETCENTER - 1 : INT_MAX;
	}

	unsigned int count = 0;
	int id;

	for (DPSprite *pspr = self->player->psprites; pspr != nullptr; pspr = pspr->GetNext())
	{
		id = pspr->GetID();

		if (id < start || id == 0)
			continue;
		else if (id > stop)
			break;

		if (safety)
		{
			if (id >= PSP_TARGETCENTER)
				break;
			else if (id == PSP_STRIFEHANDS || id == PSP_WEAPON || id == PSP_FLASH)
				continue;
		}

		pspr->SetState(nullptr);
		count++;
	}

	ACTION_RETURN_INT(count);
}

//
// WEAPON ATTACKS
//

//
// P_BulletSlope
// Sets a slope so a near miss is at aproximately
// the height of the intended target
//

DAngle P_BulletSlope (AActor *mo, FTranslatedLineTarget *pLineTarget, int aimflags)
{
	static const double angdiff[3] = { -5.625f, 5.625f, 0 };
	int i;
	DAngle an;
	DAngle pitch;
	FTranslatedLineTarget scratch;

	if (pLineTarget == NULL) pLineTarget = &scratch;
	// see which target is to be aimed at
	i = 2;
	do
	{
		an = mo->Angles.Yaw + angdiff[i];
		pitch = P_AimLineAttack (mo, an, 16.*64, pLineTarget, 0., aimflags);

		if (mo->player != nullptr &&
			mo->Level->IsFreelookAllowed() &&
			mo->player->userinfo.GetAimDist() <= 0.5)
		{
			break;
		}
	} while (pLineTarget->linetarget == NULL && --i >= 0);

	return pitch;
}

DEFINE_ACTION_FUNCTION(AActor, BulletSlope)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_POINTER(t, FTranslatedLineTarget);
	PARAM_INT(aimflags);
	ACTION_RETURN_FLOAT(P_BulletSlope(self, t, aimflags).Degrees);
}

//------------------------------------------------------------------------
//
// PROC P_SetupPsprites
//
// Called at start of level for each player
//
//------------------------------------------------------------------------

void P_SetupPsprites(player_t *player, bool startweaponup)
{
	// Remove all psprites
	player->DestroyPSprites();

	// Spawn the ready weapon
	player->PendingWeapon = !startweaponup ? player->ReadyWeapon : (AActor*)WP_NOCHANGE;
	P_BringUpWeapon (player);
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

void DPSprite::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);

	arc("next", Next)
		("caller", Caller)
		("owner", Owner)
		("flags", Flags)
		("state", State)
		("tics", Tics)
		("translation", Translation)
		.Sprite("sprite", Sprite, nullptr)
		("frame", Frame)
		("id", ID)
		("x", x)
		("y", y)
		("oldx", oldx)
		("oldy", oldy)
		("alpha", alpha)
		("pivot", pivot)
		("scale", scale)
		("rotation", rotation)
		("halign", HAlign)
		("valign", VAlign)
		("renderstyle_", Renderstyle);	// The underscore is intentional to avoid problems with old savegames which had this as an ERenderStyle (which is not future proof.)
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

void player_t::DestroyPSprites()
{
	DPSprite *pspr = psprites;
	psprites = nullptr;
	while (pspr)
	{
		DPSprite *next = pspr->Next;
		pspr->Next = nullptr;
		pspr->Destroy();
		pspr = next;
	}
}

//------------------------------------------------------------------------------------
//
// Setting a random flash like some of Doom's weapons can easily crash when the
// definition is overridden incorrectly so let's check that the state actually exists.
// Be aware though that this will not catch all DEHACKED related problems. But it will
// find all DECORATE related ones.
//
//------------------------------------------------------------------------------------

void P_SetSafeFlash(AActor *weapon, player_t *player, FState *flashstate, int index)
{
	auto wcls = PClass::FindActor(NAME_Weapon);
	if (flashstate != nullptr)
	{
		PClassActor *cls = weapon->GetClass();
		while (cls != wcls)
		{
			if (cls->OwnsState(flashstate))
			{
				// The flash state belongs to this class.
				// Now let's check if the actually wanted state does also
				if (cls->OwnsState(flashstate + index))
				{
					// we're ok so set the state
					P_SetPsprite(player, PSP_FLASH, flashstate + index, true);
					return;
				}
				else
				{
					// oh, no! The state is beyond the end of the state table so use the original flash state.
					P_SetPsprite(player, PSP_FLASH, flashstate, true);
					return;
				}
			}
			// try again with parent class
			cls = static_cast<PClassActor *>(cls->ParentClass);
		}
		// if we get here the state doesn't seem to belong to any class in the inheritance chain
		// This can happen with Dehacked if the flash states are remapped. 
		// The only way to check this would be to go through all Dehacked modifiable actors, convert
		// their states into a single flat array and find the correct one.
		// Rather than that, just check to make sure it belongs to something.
		if (FState::StaticFindStateOwner(flashstate + index) == NULL)
		{ // Invalid state. With no index offset, it should at least be valid.
			index = 0;
		}
	}
	P_SetPsprite(player, PSP_FLASH, flashstate + index, true);
}

DEFINE_ACTION_FUNCTION(_PlayerInfo, SetSafeFlash)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_OBJECT_NOT_NULL(weapon, AActor);
	PARAM_POINTER(state, FState);
	PARAM_INT(index);
	P_SetSafeFlash(weapon, self, state, index);
	return 0;
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

void DPSprite::OnDestroy()
{
	// Do not crash if this gets called on partially initialized objects.
	if (Owner != nullptr && Owner->psprites != nullptr)
	{
		if (Owner->psprites != this)
		{
			DPSprite *prev = Owner->psprites;
			while (prev != nullptr && prev->Next != this)
				prev = prev->Next;

			if (prev != nullptr && prev->Next == this)
			{
				prev->Next = Next;
				GC::WriteBarrier(prev, Next);
			}
		}
		else
		{
			Owner->psprites = Next;
			GC::WriteBarrier(Next);
		}
	}
	Super::OnDestroy();
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

float DPSprite::GetYAdjust(bool fullscreen)
{
	auto weapon = GetCaller();
	if (weapon != nullptr && weapon->IsKindOf(NAME_Weapon))
	{
		auto fYAd = weapon->FloatVar(NAME_YAdjust);
		if (fYAd != 0)
		{
			if (fullscreen)
			{
				return (float)fYAd;
			}
			else
			{
				return (float)(StatusBar->GetDisplacement() * fYAd);
			}
		}
	}
	return 0;
}

//------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------

ADD_STAT(psprites)
{
	FString out;
	DPSprite *pspr;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		out.AppendFormat("[psprites] player: %d | layers: ", i);

		pspr = players[i].psprites;
		while (pspr)
		{
			out.AppendFormat("%d, ", pspr->GetID());

			pspr = pspr->GetNext();
		}

		out.AppendFormat("\n");
	}

	return out;
}
