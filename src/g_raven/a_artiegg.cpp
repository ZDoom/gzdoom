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
#include "ravenshared.h"

#define MORPHTICS (40*TICRATE)

static FRandom pr_morphmonst ("MorphMonster");

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphPlayer (player_t *p, const PClass *spawntype)
{
	AInventory *item;
	APlayerPawn *morphed;
	APlayerPawn *actor;

	actor = p->mo;
	if (actor == NULL)
	{
		return false;
	}
	if (actor->flags3 & MF3_DONTMORPH)
	{
		return false;
	}
	if (p->mo->flags2 & MF2_INVULNERABLE)
	{ // Immune when invulnerable
		return false;
	}
	if (p->morphTics)
	{ // Player is already a beast
		return false;
	}
	if (p->health <= 0)
	{ // Dead players cannot morph
		return false;
	}

	if (spawntype == NULL)
	{
		return false;
	}

	morphed = static_cast<APlayerPawn *>(Spawn (spawntype, actor->x, actor->y, actor->z, NO_REPLACE));
	DObject::PointerSubstitution (actor, morphed);
	morphed->angle = actor->angle;
	morphed->target = actor->target;
	morphed->tracer = actor;
	p->PremorphWeapon = p->ReadyWeapon;
	morphed->special2 = actor->flags & ~MF_JUSTHIT;
	morphed->player = p;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->special2 |= MF_JUSTHIT;
	}
	morphed->flags  |= actor->flags & (MF_SHADOW|MF_NOGRAVITY);
	morphed->flags2 |= actor->flags2 & MF2_FLY;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	Spawn<ATeleportFog> (actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	actor->player = NULL;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	p->morphTics = MORPHTICS;
	p->health = morphed->health;
	p->mo = morphed;
	p->momx = p->momy = 0;
	morphed->ObtainInventory (actor);
	// Remove all armor
	for (item = morphed->Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		if (item->IsKindOf (RUNTIME_CLASS(AArmor)))
		{
			item->Destroy ();
		}
		item = next;
	}
	morphed->ActivateMorphWeapon ();
	if (p->camera == actor)
	{
		p->camera = morphed;
	}
	morphed->ScoreIcon = actor->ScoreIcon;	// [GRB]
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UndoPlayerMorph
//
//----------------------------------------------------------------------------

bool P_UndoPlayerMorph (player_t *player, bool force)
{
	AWeapon *beastweap;
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

	mo->ObtainInventory (pmo);
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
	mo->flags  = (mo->flags & ~(MF_SHADOW|MF_NOGRAVITY)) | (pmo->flags & (MF_SHADOW|MF_NOGRAVITY));
	mo->flags2 = (mo->flags2 & ~MF2_FLY) | (pmo->flags2 & MF2_FLY);
	mo->flags3 = (mo->flags3 & ~MF3_GHOST) | (pmo->flags3 & MF3_GHOST);

	player->morphTics = 0;
	player->viewheight = mo->ViewHeight;
	AInventory *level2 = mo->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2));
	if (level2 != NULL)
	{
		level2->Destroy ();
	}
	player->health = mo->health = mo->GetDefault()->health;
	player->mo = mo;
	if (player->camera == pmo)
	{
		player->camera = mo;
	}
	angle = mo->angle >> ANGLETOFINESHIFT;
	Spawn<ATeleportFog> (pmo->x + 20*finecosine[angle],
		pmo->y + 20*finesine[angle], pmo->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	beastweap = player->ReadyWeapon;
	if (player->PremorphWeapon != NULL)
	{
		player->PremorphWeapon->PostMorphWeapon ();
	}
	else
	{
		player->ReadyWeapon = player->PendingWeapon = NULL;
	}
	if (beastweap != NULL)
	{ // You don't get to keep your morphed weapon.
		if (beastweap->SisterWeapon != NULL)
		{
			beastweap->SisterWeapon->Destroy ();
		}
		beastweap->Destroy ();
	}
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

bool P_MorphMonster (AActor *actor, const PClass *spawntype)
{
	AMorphedMonster *morphed;

	if (actor->player || spawntype == NULL ||
		actor->flags3 & MF3_DONTMORPH ||
		!(actor->flags3 & MF3_ISMONSTER) ||
		!spawntype->IsDescendantOf (RUNTIME_CLASS(AMorphedMonster)))
	{
		return false;
	}

	morphed = static_cast<AMorphedMonster *>(Spawn (spawntype, actor->x, actor->y, actor->z, NO_REPLACE));
	DObject::PointerSubstitution (actor, morphed);
	morphed->tid = actor->tid;
	morphed->angle = actor->angle;
	morphed->UnmorphedMe = actor;
	morphed->alpha = actor->alpha;
	morphed->RenderStyle = actor->RenderStyle;

	morphed->UnmorphTime = level.time + MORPHTICS + pr_morphmonst();
	morphed->FlagsSave = actor->flags & ~MF_JUSTHIT;
	//morphed->special = actor->special;
	//memcpy (morphed->args, actor->args, sizeof(actor->args));
	morphed->CopyFriendliness (actor, true);
	morphed->flags |= actor->flags & MF_SHADOW;
	morphed->flags3 |= actor->flags3 & MF3_GHOST;
	if (actor->renderflags & RF_INVISIBLE)
	{
		morphed->FlagsSave |= MF_JUSTHIT;
	}
	morphed->AddToHash ();
	actor->RemoveFromHash ();
	actor->tid = 0;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	Spawn<ATeleportFog> (actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	return true;
}

//----------------------------------------------------------------------------
//
// FUNC P_UpdateMorphedMonster
//
// Returns true if the monster unmorphs.
//
//----------------------------------------------------------------------------

bool P_UpdateMorphedMonster (AMorphedMonster *beast)
{
	AActor *actor;

	if (beast->UnmorphTime == 0 ||
		beast->UnmorphTime > level.time ||
		beast->UnmorphedMe == NULL ||
		beast->flags3 & MF3_STAYMORPHED)
	{
		return false;
	}
	actor = beast->UnmorphedMe;
	actor->SetOrigin (beast->x, beast->y, beast->z);
	actor->flags |= MF_SOLID;
	beast->flags &= ~MF_SOLID;
	if (P_TestMobjLocation (actor) == false)
	{ // Didn't fit
		actor->flags &= ~MF_SOLID;
		beast->flags |= MF_SOLID;
		beast->UnmorphTime = level.time + 5*TICRATE; // Next try in 5 seconds
		return false;
	}
	actor->angle = beast->angle;
	actor->target = beast->target;
	actor->FriendPlayer = beast->FriendPlayer;
	actor->flags = beast->FlagsSave & ~MF_JUSTHIT;
	actor->flags  = (actor->flags & ~(MF_FRIENDLY|MF_SHADOW)) | (beast->flags & (MF_FRIENDLY|MF_SHADOW));
	actor->flags3 = (actor->flags3 & ~(MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST))
					| (beast->flags3 & (MF3_NOSIGHTCHECK|MF3_HUNTPLAYERS|MF3_GHOST));
	actor->flags4 = (actor->flags4 & ~MF4_NOHATEPLAYERS) | (beast->flags4 & MF4_NOHATEPLAYERS);
	if (!(beast->FlagsSave & MF_JUSTHIT))
		actor->renderflags &= ~RF_INVISIBLE;
	actor->health = actor->GetDefault()->health;
	actor->momx = beast->momx;
	actor->momy = beast->momy;
	actor->momz = beast->momz;
	actor->tid = beast->tid;
	actor->special = beast->special;
	memcpy (actor->args, beast->args, sizeof(actor->args));
	actor->AddToHash ();
	beast->UnmorphedMe = NULL;
	DObject::PointerSubstitution (beast, actor);
	beast->Destroy ();
	Spawn<ATeleportFog> (beast->x, beast->y, beast->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	return true;
}

// Egg ----------------------------------------------------------------------

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

	PROP_EggFX_PlayerClass ("ChickenPlayer")
	PROP_EggFX_MonsterClass ("Chicken")
END_DEFAULTS

int AEggFX::DoSpecialDamage (AActor *target, int damage)
{
	if (target->player)
	{
		P_MorphPlayer (target->player, PClass::FindClass (ENamedName(PlayerClass)));
	}
	else
	{
		P_MorphMonster (target, PClass::FindClass (ENamedName(MonsterClass)));
	}
	return -1;
}

void AEggFX::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PlayerClass << MonsterClass;
}

// Morph Ovum ----------------------------------------------------------------

class AArtiEgg : public AInventory
{
	DECLARE_ACTOR (AArtiEgg, AInventory)
public:
	bool Use (bool pickup);
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
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_PICKUPFLASH|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIEGGC")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIEGG")
END_DEFAULTS

bool AArtiEgg::Use (bool pickup)
{
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(AEggFX));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(AEggFX), Owner->angle-(ANG45/6));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(AEggFX), Owner->angle+(ANG45/6));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(AEggFX), Owner->angle-(ANG45/3));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(AEggFX), Owner->angle+(ANG45/3));
	return true;
}

// Pork missile --------------------------------------------------------------

class APorkFX : public AEggFX
{
	DECLARE_ACTOR (APorkFX, AEggFX)
};

FState APorkFX::States[] =
{
//#define S_EGGFX 0
	S_NORMAL (PRKM, 'A',	4, NULL, &States[S_EGGFX+1]),
	S_NORMAL (PRKM, 'B',	4, NULL, &States[S_EGGFX+2]),
	S_NORMAL (PRKM, 'C',	4, NULL, &States[S_EGGFX+3]),
	S_NORMAL (PRKM, 'D',	4, NULL, &States[S_EGGFX+4]),
	S_NORMAL (PRKM, 'E',	4, NULL, &States[S_EGGFX+0]),

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

	PROP_EggFX_PlayerClass ("PigPlayer")
	PROP_EggFX_MonsterClass ("Pig")
END_DEFAULTS

// Porkalator ---------------------------------------------------------------

class AArtiPork : public AInventory
{
	DECLARE_ACTOR (AArtiPork, AInventory)
public:
	bool Use (bool pickup);
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
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR|IF_PICKUPFLASH|IF_FANCYPICKUPSOUND)
	PROP_Inventory_Icon ("ARTIPORK")
	PROP_Inventory_PickupSound ("misc/p_pkup")
	PROP_Inventory_PickupMessage("$TXT_ARTIEGG2")
END_DEFAULTS

bool AArtiPork::Use (bool pickup)
{
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(APorkFX));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(APorkFX), Owner->angle-(ANG45/6));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(APorkFX), Owner->angle+(ANG45/6));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(APorkFX), Owner->angle-(ANG45/3));
	P_SpawnPlayerMissile (Owner, RUNTIME_CLASS(APorkFX), Owner->angle+(ANG45/3));
	return true;
}

// Morphed Monster (you must subclass this to do something useful) ---------

IMPLEMENT_POINTY_CLASS (AMorphedMonster)
 DECLARE_POINTER (UnmorphedMe)
END_POINTERS

BEGIN_STATELESS_DEFAULTS (AMorphedMonster, Any, -1, 0)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_MCROSS|MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH|MF3_ISMONSTER)
END_DEFAULTS

void AMorphedMonster::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << UnmorphedMe << UnmorphTime << FlagsSave;
}

void AMorphedMonster::Destroy ()
{
	if (UnmorphedMe != NULL)
	{
		UnmorphedMe->Destroy ();
	}
	Super::Destroy ();
}

void AMorphedMonster::Die (AActor *source, AActor *inflictor)
{
	// Dead things don't unmorph
	source->flags3 |= MF3_STAYMORPHED;
	Super::Die (source, inflictor);
	if (UnmorphedMe != NULL && (UnmorphedMe->flags & MF_UNMORPHED))
	{
		UnmorphedMe->health = health;
		UnmorphedMe->Die (source, inflictor);
	}
}

void AMorphedMonster::Tick ()
{
	if (!P_UpdateMorphedMonster (this))
	{
		Super::Tick ();
	}
}
