#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "p_pspr.h"
#include "gstrings.h"
#include "a_hexenglobal.h"

static FRandom pr_smoke ("MWandSmoke");

void A_MWandAttack (AActor *actor);

// The Mage's Wand ----------------------------------------------------------

class AMWeapWand : public AMageWeapon
{
	DECLARE_ACTOR (AMWeapWand, AMageWeapon)
};

FState AMWeapWand::States[] =
{
#define S_MWANDREADY 0
	S_NORMAL (MWND, 'A',	1, A_WeaponReady		    , &States[S_MWANDREADY]),

#define S_MWANDDOWN (S_MWANDREADY+1)
	S_NORMAL (MWND, 'A',	1, A_Lower				    , &States[S_MWANDDOWN]),

#define S_MWANDUP (S_MWANDDOWN+1)
	S_NORMAL (MWND, 'A',	1, A_Raise				    , &States[S_MWANDUP]),

#define S_MWANDATK (S_MWANDUP+1)
	S_NORMAL (MWND, 'A',	6, NULL					    , &States[S_MWANDATK+1]),
	S_BRIGHT2 (MWND, 'B',	6, A_MWandAttack		    , &States[S_MWANDATK+2], 0, 48),
	S_NORMAL2 (MWND, 'A',	3, NULL					    , &States[S_MWANDATK+3], 0, 40),
	S_NORMAL2 (MWND, 'A',	3, A_ReFire				    , &States[S_MWANDREADY], 0, 36),
};

IMPLEMENT_ACTOR (AMWeapWand, Hexen, -1, 0)
	PROP_Weapon_SelectionOrder (3600)
	PROP_Weapon_UpState (S_MWANDUP)
	PROP_Weapon_DownState (S_MWANDDOWN)
	PROP_Weapon_ReadyState (S_MWANDREADY)
	PROP_Weapon_AtkState (S_MWANDATK)
	PROP_Weapon_Kickback (0)
	PROP_Weapon_YAdjust (9)
	PROP_Weapon_MoveCombatDist (25000000)
	PROP_Weapon_ProjectileType ("MageWandMissile")
END_DEFAULTS

// Wand Smoke ---------------------------------------------------------------

class AMageWandSmoke : public AActor
{
	DECLARE_ACTOR (AMageWandSmoke, AActor)
};

FState AMageWandSmoke::States[] =
{
	S_NORMAL (MWND, 'C',	4, NULL					    , &States[1]),
	S_NORMAL (MWND, 'D',	4, NULL					    , &States[2]),
	S_NORMAL (MWND, 'C',	4, NULL					    , &States[3]),
	S_NORMAL (MWND, 'D',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMageWandSmoke, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_SHADOW)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_CANNOTPUSH|MF2_NODMGTHRUST)
	PROP_SpawnState (0)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
END_DEFAULTS

// Wand Missile -------------------------------------------------------------

class AMageWandMissile : public AActor
{
	DECLARE_ACTOR (AMageWandMissile, AActor)
public:
	void Tick ();
};

FState AMageWandMissile::States[] =
{
#define S_MWAND_MISSILE 0
	S_BRIGHT (MWND, 'C',	4, NULL					    , &States[S_MWAND_MISSILE+1]),
	S_BRIGHT (MWND, 'D',	4, NULL					    , &States[S_MWAND_MISSILE]),

#define S_MWANDPUFF (S_MWAND_MISSILE+2)
	S_BRIGHT (MWND, 'E',	4, NULL					    , &States[S_MWANDPUFF+1]),
	S_BRIGHT (MWND, 'F',	3, NULL					    , &States[S_MWANDPUFF+2]),
	S_BRIGHT (MWND, 'G',	4, NULL					    , &States[S_MWANDPUFF+3]),
	S_BRIGHT (MWND, 'H',	3, NULL					    , &States[S_MWANDPUFF+4]),
	S_BRIGHT (MWND, 'I',	4, NULL					    , NULL),
};

IMPLEMENT_ACTOR (AMageWandMissile, Hexen, -1, 0)
	PROP_SpeedFixed (184)
	PROP_RadiusFixed (12)
	PROP_HeightFixed (8)
	PROP_Damage (2)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE)
	PROP_Flags2 (MF2_NOTELEPORT|MF2_RIP|MF2_CANNOTPUSH|MF2_NODMGTHRUST|MF2_IMPACT|MF2_PCROSS)

	PROP_SpawnState (S_MWAND_MISSILE)
	PROP_DeathState (S_MWANDPUFF)
END_DEFAULTS

void AMageWandMissile::Tick ()
{
	int i;
	fixed_t xfrac;
	fixed_t yfrac;
	fixed_t zfrac;
	fixed_t hitz;
	bool changexy;

	PrevX = x;
	PrevY = y;
	PrevZ = z;

	// [RH] Ripping is a little different than it was in Hexen
	DoRipping = true;

	// Handle movement
	if (momx || momy || (z != floorz) || momz)
	{
		xfrac = momx>>3;
		yfrac = momy>>3;
		zfrac = momz>>3;
		changexy = xfrac || yfrac;
		for (i = 0; i < 8; i++)
		{
			if (changexy)
			{
				LastRipped = NULL;	// [RH] Do rip damage each step, like Hexen
				if (!P_TryMove (this, x+xfrac,y+yfrac, true))
				{ // Blocked move
					P_ExplodeMissile (this, BlockingLine, BlockingMobj);
					DoRipping = false;
					return;
				}
			}
			z += zfrac;
			if (z <= floorz)
			{ // Hit the floor
				z = floorz;
				P_HitFloor (this);
				P_ExplodeMissile (this, NULL, NULL);
				DoRipping = false;
				return;
			}
			if (z+height > ceilingz)
			{ // Hit the ceiling
				z = ceilingz-height;
				P_ExplodeMissile (this, NULL, NULL);
				DoRipping = false;
				return;
			}
			if (changexy)
			{
				//if (pr_smoke() < 128)	// [RH] I think it looks better if it's consistant
				{
					hitz = z-8*FRACUNIT;
					if (hitz < floorz)
					{
						hitz = floorz;
					}
					Spawn<AMageWandSmoke> (x, y, hitz, ALLOW_REPLACE);
				}
			}
		}
	}
	DoRipping = false;
	// Advance the state
	if (tics != -1)
	{
		tics--;
		while (!tics)
		{
			if (!SetState (state->GetNextState()))
			{ // actor was removed
				return;
			}
		}
	}
}

//============================================================================
//
// A_MWandAttack
//
//============================================================================

void A_MWandAttack (AActor *actor)
{
	AActor *mo;

	mo = P_SpawnPlayerMissile (actor, RUNTIME_CLASS(AMageWandMissile));
	S_Sound (actor, CHAN_WEAPON, "MageWandFire", 1, ATTN_NORM);
}
