#include "actor.h"
#include "g_level.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "vm.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_keys.h"
#include "a_sharedglobal.h"
#include "templates.h"
#include "d_event.h"
#include "v_font.h"
#include "serializer.h"
#include "p_spec.h"
#include "portal.h"
#include "vm.h"

// Include all the other Strife stuff here to reduce compile time
#include "a_strifeitems.cpp"
#include "a_strifeweapons.cpp"

// Notes so I don't forget them:
// Strife does some extra stuff in A_Explode if a player caused the explosion. (probably NoiseAlert)
// See the instructions @ 21249.
//
// Strife's FLOATSPEED is 5 and not 4.
//
// In P_CheckMissileRange, mobjtypes 53,54,55,56,57,58 shift the distance right 4 bits (some, but not all the acolytes)
//						   mobjtypes 61,63,91 shift it right 1 bit
//
// When shooting missiles at something, if MF_SHADOW is set, the angle is adjusted with the formula:
//		angle += pr_spawnmissile.Random2() << 21
// When MF_STRIFEx4000000 is set, the angle is adjusted similarly:
//		angle += pr_spawnmissile.Random2() << 22
// Note that these numbers are different from those used by all the other Doom engine games.

static FRandom pr_gibtosser ("GibTosser");

// Force Field Guard --------------------------------------------------------

void A_RemoveForceField (AActor *);

class AForceFieldGuard : public AActor
{
	DECLARE_CLASS (AForceFieldGuard, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS(AForceFieldGuard, false, false)

int AForceFieldGuard::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor == NULL || !inflictor->IsKindOf (RUNTIME_CLASS(ADegninOre)))
	{
		return -1;
	}
	return health;
}

// Power Coupling -----------------------------------------------------------

class APowerCoupling : public AActor
{
	DECLARE_CLASS (APowerCoupling, AActor)
public:
	void Die (AActor *source, AActor *inflictor, int dmgflags);
};

IMPLEMENT_CLASS(APowerCoupling, false, false)

void APowerCoupling::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	Super::Die (source, inflictor, dmgflags);

	int i;

	for (i = 0; i < MAXPLAYERS; ++i)
		if (playeringame[i] && players[i].health > 0)
			break;

	if (i == MAXPLAYERS)
		return;

	// [RH] In case the player broke it with the dagger, alert the guards now.
	if (LastHeard != source)
	{
		P_NoiseAlert (source, this);
	}
	EV_DoDoor (DDoor::doorClose, NULL, players[i].mo, 225, 2., 0, 0, 0);
	EV_DoFloor (DFloor::floorLowerToHighest, NULL, 44, 1., 0., -1, 0, false);
	players[i].mo->GiveInventoryType (QuestItemClasses[5]);
	S_Sound (CHAN_VOICE, "svox/voc13", 1, ATTN_NORM);
	players[i].SetLogNumber (13);
	P_DropItem (this, PClass::FindActor("BrokenPowerCoupling"), -1, 256);
	Destroy ();
}

// Gibs for things that bleed -----------------------------------------------

class AMeat : public AActor
{
	DECLARE_CLASS (AMeat, AActor)
public:
	void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + pr_gibtosser() % 20);
	}
};

IMPLEMENT_CLASS(AMeat, false, false)

