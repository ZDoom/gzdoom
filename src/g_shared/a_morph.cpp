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
#include "sbar.h"
#include "a_morph.h"

static FRandom pr_morphmonst ("MorphMonster");

//---------------------------------------------------------------------------
//
// FUNC P_MorphPlayer
//
// Returns true if the player gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphPlayer (player_t *activator, player_t *p, const PClass *spawntype, int duration, int style, const PClass *enter_flash, const PClass *exit_flash)
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
	if ((p->mo->flags2 & MF2_INVULNERABLE) && ((p != activator) || (!(style & MORPH_WHENINVULNERABLE))))
	{ // Immune when invulnerable unless this is a power we activated
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
	if (!spawntype->IsDescendantOf (RUNTIME_CLASS(APlayerPawn)))
	{
		return false;
	}
	if (spawntype == p->mo->GetClass())
	{
		return false;
	}

	morphed = static_cast<APlayerPawn *>(Spawn (spawntype, actor->x, actor->y, actor->z, NO_REPLACE));
	DObject::StaticPointerSubstitution (actor, morphed);
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
	Spawn(((enter_flash) ? enter_flash : RUNTIME_CLASS(ATeleportFog)), actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	actor->player = NULL;
	actor->flags &= ~(MF_SOLID|MF_SHOOTABLE);
	actor->flags |= MF_UNMORPHED;
	actor->renderflags |= RF_INVISIBLE;
	p->morphTics = (duration) ? duration : MORPHTICS;

	// [MH] Used by SBARINFO to speed up face drawing
	p->MorphedPlayerClass = 0;
	for (unsigned int i = 1; i < PlayerClasses.Size (); i++)
	{
		if (PlayerClasses[i].Type == spawntype)
		{
			p->MorphedPlayerClass = i;
			break;
		}
	}

	p->MorphStyle = style;
	p->MorphExitFlash = (exit_flash) ? exit_flash : RUNTIME_CLASS(ATeleportFog);
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
			if (item->IsKindOf (RUNTIME_CLASS(AHexenArmor)))
			{
				// Set the HexenArmor slots to 0, except the class slot.
				AHexenArmor *hxarmor = static_cast<AHexenArmor *>(item);
				hxarmor->Slots[0] = 0;
				hxarmor->Slots[1] = 0;
				hxarmor->Slots[2] = 0;
				hxarmor->Slots[3] = 0;
				hxarmor->Slots[4] = spawntype->Meta.GetMetaFixed (APMETA_Hexenarmor0);
			}
			else if (item->ItemFlags & IF_KEEPDEPLETED)
			{
				// Set depletable armor to 0 (this includes BasicArmor).
				item->Amount = 0;
			}
			else
			{
				item->Destroy ();
			}
		}
		item = next;
	}
	morphed->ActivateMorphWeapon ();
	if (p->camera == actor)
	{
		p->camera = morphed;
	}
	morphed->ScoreIcon = actor->ScoreIcon;	// [GRB]

	// [MH]
	// If the player that was morphed is the one
	// taking events, set up the face, if any;
	// this is only needed for old-skool skins
	// and for the original DOOM status bar.
	if ((p == &players[consoleplayer]) && 
		(strcmp(spawntype->Meta.GetMetaString (APMETA_Face), "None") != 0))
	{
		StatusBar->SetFace(&skins[p->MorphedPlayerClass]);
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
	AWeapon *beastweap;
	APlayerPawn *mo;
	AActor *pmo;
	angle_t angle;

	pmo = player->mo;
	if (pmo->tracer == NULL)
	{
		return false;
	}
	mo = barrier_cast<APlayerPawn *>(pmo->tracer);
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
	DObject::StaticPointerSubstitution (pmo, mo);
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

	const PClass *exit_flash = player->MorphExitFlash;

	player->morphTics = 0;
	player->MorphedPlayerClass = 0;
	player->MorphStyle = 0;
	player->MorphExitFlash = NULL;
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

	// [MH]
	// If the player that was morphed is the one
	// taking events, reset up the face, if any;
	// this is only needed for old-skool skins
	// and for the original DOOM status bar.
	if ((player == &players[consoleplayer]) && 
		(strcmp(pmo->GetClass()->Meta.GetMetaString (APMETA_Face), "None") != 0))
	{
		// Assume root-level base skin to begin with
		size_t skinindex = 0;
		// If a custom skin was in use, then reload it
		// or else the base skin for the player class.
		if ((unsigned int)player->userinfo.skin >= PlayerClasses.Size () &&
			(size_t)player->userinfo.skin < numskins)
		{
			skinindex = player->userinfo.skin;
		}
		else if (PlayerClasses.Size () > 1)
		{
			const PClass *whatami = player->mo->GetClass();
			for (unsigned int i = 0; i < PlayerClasses.Size (); ++i)
			{
				if (PlayerClasses[i].Type == whatami)
				{
					skinindex = i;
					break;
				}
			}
		}
		StatusBar->SetFace(&skins[skinindex]);
	}

	angle = mo->angle >> ANGLETOFINESHIFT;
	Spawn(exit_flash, pmo->x + 20*finecosine[angle], pmo->y + 20*finesine[angle], pmo->z + TELEFOGHEIGHT, ALLOW_REPLACE);
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
	// Restore playerclass armor to its normal amount.
	AHexenArmor *hxarmor = mo->FindInventory<AHexenArmor>();
	if (hxarmor != NULL)
	{
		hxarmor->Slots[4] = mo->GetClass()->Meta.GetMetaFixed (APMETA_Hexenarmor0);
	}
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_MorphMonster
//
// Returns true if the monster gets turned into a chicken/pig.
//
//---------------------------------------------------------------------------

bool P_MorphMonster (AActor *actor, const PClass *spawntype, int duration, int style, const PClass *enter_flash, const PClass *exit_flash)
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
	DObject::StaticPointerSubstitution (actor, morphed);
	morphed->tid = actor->tid;
	morphed->angle = actor->angle;
	morphed->UnmorphedMe = actor;
	morphed->alpha = actor->alpha;
	morphed->RenderStyle = actor->RenderStyle;

	morphed->UnmorphTime = level.time + ((duration) ? duration : MORPHTICS) + pr_morphmonst();
	morphed->MorphStyle = style;
	morphed->MorphExitFlash = (exit_flash) ? exit_flash : RUNTIME_CLASS(ATeleportFog);
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
	Spawn(((enter_flash) ? enter_flash : RUNTIME_CLASS(ATeleportFog)), actor->x, actor->y, actor->z + TELEFOGHEIGHT, ALLOW_REPLACE);
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
	DObject::StaticPointerSubstitution (beast, actor);
	const PClass *exit_flash = beast->MorphExitFlash;
	beast->Destroy ();
	Spawn(exit_flash, beast->x, beast->y, beast->z + TELEFOGHEIGHT, ALLOW_REPLACE);
	return true;
}

// Base class for morphing projectiles --------------------------------------

IMPLEMENT_STATELESS_ACTOR(AMorphProjectile, Any, -1, 0)
	PROP_Damage (1)
	PROP_Flags (MF_NOBLOCKMAP|MF_MISSILE|MF_DROPOFF|MF_NOGRAVITY)
	PROP_Flags2 (MF2_NOTELEPORT)
END_DEFAULTS

int AMorphProjectile::DoSpecialDamage (AActor *target, int damage)
{
	const PClass *morph_flash = PClass::FindClass (MorphFlash);
	const PClass *unmorph_flash = PClass::FindClass (UnMorphFlash);
	if (target->player)
	{
		const PClass *player_class = PClass::FindClass (PlayerClass);
		P_MorphPlayer (NULL, target->player, player_class, Duration, MorphStyle, morph_flash, unmorph_flash);
	}
	else
	{
		const PClass *monster_class = PClass::FindClass (MonsterClass);
		P_MorphMonster (target, monster_class, Duration, MorphStyle, morph_flash, unmorph_flash);
	}
	return -1;
}

void AMorphProjectile::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PlayerClass << MonsterClass << Duration << MorphStyle << MorphFlash << UnMorphFlash;
}


// Morphed Monster (you must subclass this to do something useful) ---------

IMPLEMENT_POINTY_CLASS (AMorphedMonster)
 DECLARE_POINTER (UnmorphedMe)
END_POINTERS

BEGIN_STATELESS_DEFAULTS (AMorphedMonster, Any, -1, 0)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_MCROSS|MF2_FLOORCLIP|MF2_PASSMOBJ|MF2_PUSHWALL)
	PROP_Flags3 (MF3_DONTMORPH|MF3_ISMONSTER)
END_DEFAULTS

void AMorphedMonster::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << UnmorphedMe << UnmorphTime << MorphStyle << MorphExitFlash << FlagsSave;
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
	flags3 |= MF3_STAYMORPHED;
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
