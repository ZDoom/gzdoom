#include "info.h"
#include "a_pickups.h"
#include "a_artifacts.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_sharedglobal.h"

#define MORPHTICS (40*TICRATE)

static FRandom pr_morphmonst ("MorphMonster");

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphPlayer (player_t *p, const TypeInfo *spawntype)
{
	AActor *morphed;
	AActor *actor;

	actor = p->mo;
	if (actor == NULL)
	{
		return false;
	}
	if (actor->flags3 & MF3_DONTMORPH)
	{
		return false;
	}
	if (p->powers[pw_invulnerability])
	{ // Immune when invulnerable
		return false;
	}
	if (p->morphTics)
	{ // Player is already a beast
		return false;
	}

	if (spawntype == NULL)
	{
		return false;
	}

	morphed = Spawn (spawntype, actor->x, actor->y, actor->z);
	DObject::PointerSubstitution (actor, morphed);
	morphed->angle = actor->angle;
	morphed->target = actor->target;
	morphed->tracer = actor;
	morphed->special1 = p->readyweapon;
	morphed->special2 = actor->flags & ~MF_JUSTHIT;
	morphed->player = p;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->special2 |= MF_JUSTHIT;
	}
	if (actor->flags2 & MF2_FLY)
	{
		morphed->flags |= MF2_FLY;
	}
	if (actor->flags3 & MF3_GHOST)
	{
		morphed->flags3 |= MF3_GHOST;
	}
	if (actor->flags & MF_SHADOW)
	{
		morphed->flags |= MF_SHADOW;
	}
	Spawn<ATeleportFog> (actor->x, actor->y, actor->z + TELEFOGHEIGHT);
	actor->player = NULL;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	p->morphTics = MORPHTICS;
	p->health = morphed->health;
	p->mo = static_cast<APlayerPawn *>(morphed);
	memset (&p->armorpoints[0], 0, NUMARMOR*sizeof(int));
	P_ActivateMorphWeapon (p);
	if (p->camera == actor)
	{
		p->camera = morphed;
	}
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoPlayerMorph
//
//----------------------------------------------------------------------------

bool P_UndoPlayerMorph (player_t *player, bool force)
{
	APlayerPawn *mo;
	AActor *pmo;
	angle_t angle;

	pmo = player->mo;
	if (pmo->tracer == NULL)
	{
		return false;
	}
	mo = static_cast<APlayerPawn *>(pmo->tracer);
	mo->SetOrigin (pmo->x, pmo->y, pmo->z);
	mo->flags |= MF_SOLID;
	pmo->flags &= ~MF_SOLID;
	if (!force && P_TestMobjLocation (mo) == false)
	{ // Didn't fit
		mo->flags &= ~MF_SOLID;
		pmo->flags |= MF_SOLID;
		player->morphTics = 2*TICRATE;
		return false;
	}
	pmo->player = NULL;

	DObject::PointerSubstitution (pmo, mo);
	mo->angle = pmo->angle;
	mo->player = player;
	mo->reactiontime = 18;
	mo->flags = pmo->special2 & ~MF_JUSTHIT;
	mo->momx = 0;
	mo->momy = 0;
	player->momx = 0;
	player->momy = 0;
	mo->momz = pmo->momz;
	if (!(pmo->special2 & MF_JUSTHIT))
	{
		mo->renderflags &= ~RF_INVISIBLE;
	}

	player->morphTics = 0;
	player->powers[pw_weaponlevel2] = 0;
	player->health = mo->health = mo->GetDefault()->health;
	player->mo = mo;
	if (player->camera == pmo)
	{
		player->camera = mo;
	}
	angle = mo->angle >> ANGLETOFINESHIFT;
	Spawn<ATeleportFog> (pmo->x + 20*finecosine[angle],
		pmo->y + 20*finesine[angle], pmo->z + TELEFOGHEIGHT);
	P_PostMorphWeapon (player, (weapontype_t)pmo->special1);
	pmo->tracer = NULL;
	pmo->Destroy ();
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphMonster
//
// Returns true if the monster gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphMonster (AActor *actor, const TypeInfo *spawntype)
{
	AActor *morphed;

	if (actor->player ||
		actor->flags3 & MF3_DONTMORPH ||
		!(actor->flags & MF_COUNTKILL))
	{
		return false;
	}

	morphed = Spawn (spawntype, actor->x, actor->y, actor->z);
	DObject::PointerSubstitution (actor, morphed);
	morphed->tid = actor->tid;
	morphed->angle = actor->angle;
	morphed->target = actor->target;
	morphed->tracer = actor;
	morphed->special1 = MORPHTICS + pr_morphmonst();
	morphed->special2 = actor->flags & ~MF_JUSTHIT;
	morphed->special = actor->special;
	memcpy (morphed->args, actor->args, sizeof(actor->args));
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->special2 |= MF_JUSTHIT;
	}
	if (actor->flags3 & MF3_GHOST)
	{
		morphed->flags3 |= MF3_GHOST;
		if (actor->flags & MF_SHADOW)
			morphed->flags |= MF_SHADOW;
	}
	morphed->AddToHash ();
	actor->RemoveFromHash ();
	actor->tid = 0;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	Spawn<ATeleportFog> (actor->x, actor->y, actor->z + TELEFOGHEIGHT);
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UpdateMorphedMonster
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UpdateMorphedMonster (AActor *beast, int tics)
{
	AActor *actor;

	beast->special1 -= tics;
	if (beast->special1 > 0 ||
		beast->tracer == NULL ||
		beast->flags3 & MF3_STAYMORPHED)
	{
		return false;
	}
	actor = beast->tracer;
	actor->SetOrigin (beast->x, beast->y, beast->z);
	actor->flags |= MF_SOLID;
	beast->flags &= ~MF_SOLID;
	if (P_TestMobjLocation (actor) == false)
	{ // Didn't fit
		actor->flags &= ~MF_SOLID;
		beast->flags |= MF_SOLID;
		beast->special1 = 5*TICRATE; // Next try in 5 seconds
		return false;
	}
	actor->angle = beast->angle;
	actor->target = beast->target;
	actor->flags = beast->special2 & ~MF_JUSTHIT;
	if (!(beast->special2 & MF_JUSTHIT))
		actor->renderflags &= ~RF_INVISIBLE;
	actor->health = actor->GetDefault()->health;
	actor->momx = beast->momx;
	actor->momy = beast->momy;
	actor->momz = beast->momz;
	actor->tid = beast->tid;
	actor->special = beast->special;
	memcpy (actor->args, beast->args, sizeof(actor->args));
	actor->AddToHash ();
	beast->tracer = NULL;
	DObject::PointerSubstitution (beast, actor);
	beast->Destroy ();
	Spawn<ATeleportFog> (beast->x, beast->y, beast->z + TELEFOGHEIGHT);
	return true;
}

// Egg ----------------------------------------------------------------------

class AEggFX : public AActor
{
	DECLARE_ACTOR (AEggFX, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState AEggFX::States[] =
{
#define S_EGGFX 0
	S_NORMAL (EGGM, 'A',	4, NULL, &States[S_EGGFX+1]),
	S_NORMAL (EGGM, 'B',	4, NULL, &States[S_EGGFX+2]),
	S_NORMAL (EGGM, 'C',	4, NULL, &States[S_EGGFX+3]),
	S_NORMAL (EGGM, 'D',	4, NULL, &States[S_EGGFX+4]),
	S_NORMAL (EGGM, 'E',	4, NULL, &States[S_EGGFX+0]),

#define S_EGGFXI1 (S_EGGFX+5)
	S_BRIGHT (FX01, 'E',	3, NULL, &States[S_EGGFXI1+1]),
	S_BRIGHT (FX01, 'F',	3, NULL, &States[S_EGGFXI1+2]),
	S_BRIGHT (FX01, 'G',	3, NULL, &States[S_EGGFXI1+3]),
	S_BRIGHT (FX01, 'H',	3, NULL, NULL),
};

IMPLEMENT_ACTOR (AEggFX, Heretic, -1, 40)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (18)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_EGGFX)
	PROP_DeathState (S_EGGFXI1)
END_DEFAULTS

int AEggFX::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player)
	{
		P_MorphPlayer (target->player, TypeInfo::FindType ("ChickenPlayer"));
	}
	else
	{
		P_MorphMonster (target, TypeInfo::FindType ("Chicken"));
	}
	return -1;
}

// Morph Ovum ---------------------------------------------------------------

BASIC_ARTI (Egg, arti_egg, GStrings(TXT_ARTIEGG))
	AT_GAME_SET_FRIEND (Egg)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		AActor *mo = player->mo;
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(AEggFX));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(AEggFX), mo->angle-(ANG45/6));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(AEggFX), mo->angle+(ANG45/6));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(AEggFX), mo->angle-(ANG45/3));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(AEggFX), mo->angle+(ANG45/3));
		return true;
	}
};

FState AArtiEgg::States[] =
{
	S_NORMAL (EGGC, 'A',	6, NULL, &States[1]),
	S_NORMAL (EGGC, 'B',	6, NULL, &States[2]),
	S_NORMAL (EGGC, 'C',	6, NULL, &States[3]),
	S_NORMAL (EGGC, 'B',	6, NULL, &States[0]),
};

IMPLEMENT_ACTOR (AArtiEgg, Heretic, 30, 14)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Egg)
{
	ArtiDispatch[arti_egg] = AArtiEgg::ActivateArti;
	ArtiPics[arti_egg] = "ARTIEGGC";
}

// Pork thing ---------------------------------------------------------------

class APorkFX : public AActor
{
	DECLARE_ACTOR (APorkFX, AActor)
public:
	int DoSpecialDamage (AActor *target, int damage);
};

FState APorkFX::States[] =
{
//#define S_EGGFX 0
	S_NORMAL (EGGM, 'A',	4, NULL, &States[S_EGGFX+1]),
	S_NORMAL (EGGM, 'B',	4, NULL, &States[S_EGGFX+2]),
	S_NORMAL (EGGM, 'C',	4, NULL, &States[S_EGGFX+3]),
	S_NORMAL (EGGM, 'D',	4, NULL, &States[S_EGGFX+4]),
	S_NORMAL (EGGM, 'E',	4, NULL, &States[S_EGGFX+0]),

#define S_EGGFXI2 (S_EGGFX+5)
	S_BRIGHT (FHFX, 'I',	3, NULL, &States[S_EGGFXI2+1]),
	S_BRIGHT (FHFX, 'J',	3, NULL, &States[S_EGGFXI2+2]),
	S_BRIGHT (FHFX, 'K',	3, NULL, &States[S_EGGFXI2+3]),
	S_BRIGHT (FHFX, 'L',	3, NULL, NULL)
};

IMPLEMENT_ACTOR (APorkFX, Hexen, -1, 40)
	PROP_RadiusFixed (8)
	PROP_HeightFixed (8)
	PROP_SpeedFixed (18)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)

	PROP_SpawnState (S_EGGFX)
	PROP_DeathState (S_EGGFXI2)
END_DEFAULTS

int APorkFX::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player)
	{
		P_MorphPlayer (target->player, TypeInfo::FindType ("PigPlayer"));
	}
	else
	{
		P_MorphMonster (target, TypeInfo::FindType ("Pig"));
	}
	return -1;
}

// Porkalator ---------------------------------------------------------------

BASIC_ARTI (Pork, arti_pork, GStrings(TXT_ARTIEGG2))
	AT_GAME_SET_FRIEND (Pork)
private:
	static bool ActivateArti (player_t *player, artitype_t arti)
	{
		AActor *mo = player->mo;
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(APorkFX));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(APorkFX), mo->angle-(ANG45/6));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(APorkFX), mo->angle+(ANG45/6));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(APorkFX), mo->angle-(ANG45/3));
		P_SpawnPlayerMissile (mo, RUNTIME_CLASS(APorkFX), mo->angle+(ANG45/3));
		return true;
	}
};

FState AArtiPork::States[] =
{
	S_NORMAL (PORK, 'A',	5, NULL, &States[1]),
	S_NORMAL (PORK, 'B',	5, NULL, &States[2]),
	S_NORMAL (PORK, 'C',	5, NULL, &States[3]),
	S_NORMAL (PORK, 'D',	5, NULL, &States[4]),
	S_NORMAL (PORK, 'E',	5, NULL, &States[5]),
	S_NORMAL (PORK, 'F',	5, NULL, &States[6]),
	S_NORMAL (PORK, 'G',	5, NULL, &States[7]),
	S_NORMAL (PORK, 'H',	5, NULL, &States[0])
};

IMPLEMENT_ACTOR (AArtiPork, Hexen, 30, 14)
	PROP_Flags (MF_SPECIAL|MF_COUNTITEM)
	PROP_Flags2 (MF2_FLOATBOB)
	PROP_SpawnState (0)
END_DEFAULTS

AT_GAME_SET (Pork)
{
	ArtiDispatch[arti_pork] = AArtiPork::ActivateArti;
	ArtiPics[arti_pork] = "ARTIPORK";
}
