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
// $Log:$
//
// DESCRIPTION:
//		Player related stuff.
//		Bobbing POV/weapon, movement.
//		Pending weapon.
//
//-----------------------------------------------------------------------------

#include "templates.h"
#include "doomdef.h"
#include "d_event.h"
#include "p_local.h"
#include "doomstat.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_draw.h"
#include "gi.h"
#include "m_random.h"
#include "p_pspr.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "s_sound.h"
#include "a_sharedglobal.h"
#include "statnums.h"
#include "v_palette.h"
#include "v_video.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sbar.h"
#include "f_finale.h"

static FRandom pr_healradius ("HealRadius");

// [RH] # of ticks to complete a turn180
#define TURN180_TICKS	((TICRATE / 4) + 1)

// Variables for prediction
CVAR (Bool, cl_noprediction, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static player_t PredictionPlayerBackup;
static BYTE PredictionActorBackup[sizeof(AActor)];
static TArray<sector_t *> PredictionTouchingSectorsBackup;

//
// Movement.
//

// 16 pixels of bob
#define MAXBOB			0x100000

BOOL onground;


// This function supplements the pointer cleanup in dobject.cpp, because
// player_s is not derived from DObject. (I tried it, and DestroyScan was
// unable to properly determine the player object's type--possibly
// because it gets staticly allocated in an array.)
//
// This function checks all the DObject pointers in a player_s and NULLs any
// that match the pointer passed in. If you add any pointers that point to
// DObject (or a subclass), add them here too.

void player_s::FixPointers (const DObject *old, DObject *rep)
{
	APlayerPawn *replacement = static_cast<APlayerPawn *>(rep);
	if (mo == old)				mo = replacement;
	if (poisoner == old)		poisoner = replacement;
	if (attacker == old)		attacker = replacement;
	if (camera == old)			camera = replacement;
	if (dest == old)			dest = replacement;
	if (prev == old)			prev = replacement;
	if (enemy == old)			enemy = replacement;
	if (missile == old)			missile = replacement;
	if (mate == old)			mate = replacement;
	if (last_mate == old)		last_mate = replacement;
	if (ReadyWeapon == old)		ReadyWeapon = static_cast<AWeapon *>(rep);
	if (PendingWeapon == old)	PendingWeapon = static_cast<AWeapon *>(rep);
}

void player_s::SetLogNumber (int num)
{
	char lumpname[16];
	int lumpnum;

	sprintf (lumpname, "LOG%d", num);
	lumpnum = Wads.CheckNumForName (lumpname);
	if (lumpnum == -1)
	{
		// Leave the log message alone if this one doesn't exist.
		//SetLogText (lumpname);
	}
	else
	{
		FMemLump data = Wads.ReadLump (lumpnum);
		SetLogText ((char *)data.GetMem());
	}
}

void player_s::SetLogText (const char *text)
{
	ReplaceString (&LogText, text);
}

IMPLEMENT_ABSTRACT_ACTOR (APlayerPawn)
IMPLEMENT_ABSTRACT_ACTOR (APlayerChunk)

void APlayerPawn::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_PLAYER);
}

void APlayerPawn::AddInventory (AInventory *item)
{
	// Adding inventory to a voodoo doll should add it to the real player instead.
	if (player != NULL && player->mo != this)
	{
		player->mo->AddInventory (item);
		return;
	}
	Super::AddInventory (item);

	// If nothing is selected, select this item.
	if (player != NULL && player->InvSel == NULL && (item->ItemFlags & IF_INVBAR))
	{
		player->InvSel = item;
	}
}

void APlayerPawn::RemoveInventory (AInventory *item)
{
	bool pickWeap = false;

	// Since voodoo dolls aren't supposed to have an inventory, there should be
	// no need to redirect them to the real player here as there is with AddInventory.

	// If the item removed is the selected one, select something else, either the next
	// item, if there is one, or the previous item.
	if (player != NULL)
	{
		if (player->InvSel == item)
		{
			player->InvSel = item->NextInv ();
			if (player->InvSel == NULL)
			{
				player->InvSel = item->PrevInv ();
			}
		}
		if (player->InvFirst == item)
		{
			player->InvFirst = item->NextInv ();
			if (player->InvFirst == NULL)
			{
				player->InvFirst = item->PrevInv ();
			}
		}
		if (item == player->PendingWeapon)
		{
			player->PendingWeapon = WP_NOCHANGE;
		}
		if (item == player->ReadyWeapon)
		{
			// If the current weapon is removed, pick a new one.
			pickWeap = true;
			player->ReadyWeapon = NULL;
		}
	}
	Super::RemoveInventory (item);
	if (pickWeap && player->mo == this && player->PendingWeapon == WP_NOCHANGE)
	{
		PickNewWeapon (NULL);
	}
}

bool APlayerPawn::UseInventory (AInventory *item)
{
	const TypeInfo *itemtype = item->GetClass();

	if (player->cheats & CF_TOTALLYFROZEN)
	{ // You can't use items if you're totally frozen
		return false;
	}
	if (!Super::UseInventory (item))
	{
		// Heretic and Hexen advance the inventory cursor if the use failed.
		// Should this behavior be retained?
		return false;
	}
	if (player == &players[consoleplayer])
	{
		S_SoundID (this, CHAN_ITEM, item->UseSound, 1, ATTN_NORM);
		StatusBar->FlashItem (itemtype);
	}
	return true;
}

//===========================================================================
//
// APlayerPawn :: BestWeapon
//
// Returns the best weapon a player has, possibly restricted to a single
// type of ammo.
//
//===========================================================================

AWeapon *APlayerPawn::BestWeapon (const TypeInfo *ammotype)
{
	AWeapon *bestMatch = NULL;
	int bestOrder = INT_MAX;
	AInventory *item;
	AWeapon *weap;

	// Find the best weapon the player has.
	for (item = Inventory; item != NULL; item = item->Inventory)
	{
		if (!item->IsKindOf (RUNTIME_CLASS(AWeapon)))
			continue;

		weap = static_cast<AWeapon *> (item);

		// Don't select it if it's worse than what was already found.
		if (weap->SelectionOrder > bestOrder)
			continue;

		// Don't select it if its primary fire doesn't use the desired ammo.
		if (ammotype != NULL &&
			(weap->Ammo1 == NULL ||
			 weap->Ammo1->GetClass() != ammotype))
			continue;

		// Don't select it if there isn't enough ammo to use its primary fire.
		if (!(weap->WeaponFlags & WIF_AMMO_OPTIONAL) &&
			!weap->CheckAmmo (AWeapon::PrimaryFire, false))
			continue;

		// This weapon is usable!
		bestOrder = weap->SelectionOrder;
		bestMatch = weap;
	}
	return bestMatch;
}

//===========================================================================
//
// APlayerPawn :: PickNewWeapon
//
// Picks a new weapon for this player. Used mostly for running out of ammo,
// but it also works when an ACS script explicitly takes the ready weapon
// away or the player picks up some ammo they had previously run out of.
//
//===========================================================================

AWeapon *APlayerPawn::PickNewWeapon (const TypeInfo *ammotype)
{
	AWeapon *best = BestWeapon (ammotype);

	if (best != NULL)
	{
		player->PendingWeapon = best;
		if (player->ReadyWeapon != NULL)
		{
			P_SetPsprite (player, ps_weapon, player->ReadyWeapon->DownState);
		}
		else if (player->PendingWeapon != WP_NOCHANGE)
		{
			P_BringUpWeapon (player);
		}
	}
	return best;
}


const char *APlayerPawn::GetSoundClass ()
{
	if (player != NULL &&
		player->userinfo.skin != 0 &&
		(unsigned)player->userinfo.skin < numskins)
	{
		return skins[player->userinfo.skin].name;
	}
	return "player";
}

void APlayerPawn::PlayIdle ()
{
	if (state >= SeeState && state < MissileState)
		SetState (SpawnState);
}

void APlayerPawn::PlayRunning ()
{
	if (state == SpawnState)
		SetState (SeeState);
}

void APlayerPawn::PlayAttacking ()
{
	SetState (MissileState);
}

void APlayerPawn::PlayAttacking2 ()
{
	SetState (MissileState+1);
}

void APlayerPawn::ThrowPoisonBag ()
{
}

void APlayerPawn::GiveDefaultInventory ()
{
}

void APlayerPawn::MorphPlayerThink ()
{
}

void APlayerPawn::ActivateMorphWeapon ()
{
}

fixed_t APlayerPawn::GetJumpZ ()
{
	return 8*FRACUNIT;
}

void APlayerPawn::Die (AActor *source, AActor *inflictor)
{
	Super::Die (source, inflictor);

	if (player != NULL && player->mo != this)
	{ // Make the real player die, too
		player->mo->Die (source, inflictor);
	}
	else
	{
		if (dmflags2 & DF2_YES_WEAPONDROP)
		{ // Voodoo dolls don't drop weapons
			AWeapon *weap = player->ReadyWeapon;
			if (weap != NULL)
			{
				if (weap->SpawnState != NULL &&
					weap->SpawnState != &AActor::States[AActor::S_NULL])
				{
					weap->BecomePickup ();
					weap->SetOrigin (x, y, z);
					P_TossItem (weap);
				}
				else
				{
					AInventory *item;

					item = P_DropItem (this, weap->AmmoType1, -1, 256);
					if (item != NULL)
					{
						item->Amount = weap->Ammo1->Amount;
					}
					item = P_DropItem (this, weap->AmmoType2, -1, 256);
					if (item != NULL)
					{
						item->Amount = weap->Ammo2->Amount;
					}
				}
			}
		}
		// Kill the player's inventory
		while (Inventory != NULL)
		{
			Inventory->Destroy ();
		}
		if (!multiplayer && (level.flags & LEVEL_DEATHSLIDESHOW))
		{
			F_StartSlideshow ();
		}
	}
}

void APlayerPawn::TweakSpeeds (int &forward, int &side)
{
	if ((player->Powers & PW_SPEED) && !player->morphTics)
	{ // Adjust for a player with a speed artifact
		forward = (3*forward)>>1;
		side = (3*side)>>1;
	}
}

// The standard healing radius behavior is the cleric's
bool APlayerPawn::DoHealingRadius (APlayerPawn *other)
{
	if (P_GiveBody (other->player, 50 + (pr_healradius()%50)))
	{
		S_Sound (other, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM);
		return true;
	}
	return false;
}

void APlayerPawn::SpecialInvulnerabilityHandling (EInvulState setting)
{
}

/*
==================
=
= P_Thrust
=
= moves the given origin along a given angle
=
==================
*/

void P_SideThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle = (angle - ANGLE_90) >> ANGLETOFINESHIFT;

	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

void P_ForwardThrust (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	if ((player->mo->waterlevel || (player->mo->flags2 & MF2_FLY))
		&& player->mo->pitch != 0)
	{
		angle_t pitch = (angle_t)player->mo->pitch >> ANGLETOFINESHIFT;
		fixed_t zpush = FixedMul (move, finesine[pitch]);
		if (player->mo->waterlevel && player->mo->waterlevel < 2 && zpush < 0)
			zpush = 0;
		player->mo->momz -= zpush;
		move = FixedMul (move, finecosine[pitch]);
	}
	player->mo->momx += FixedMul (move, finecosine[angle]);
	player->mo->momy += FixedMul (move, finesine[angle]);
}

//
// P_Bob
// Same as P_Thrust, but only affects bobbing.
//
// killough 10/98: We apply thrust separately between the real physical player
// and the part which affects bobbing. This way, bobbing only comes from player
// motion, nothing external, avoiding many problems, e.g. bobbing should not
// occur on conveyors, unless the player walks on one, and bobbing should be
// reduced at a regular rate, even on ice (where the player coasts).
//

void P_Bob (player_t *player, angle_t angle, fixed_t move)
{
	angle >>= ANGLETOFINESHIFT;

	player->momx += FixedMul(move,finecosine[angle]);
	player->momy += FixedMul(move,finesine[angle]);
}

/*
==================
=
= P_CalcHeight
=
=
Calculate the walking / running height adjustment
=
==================
*/

void P_CalcHeight (player_t *player) 
{
	int 		angle;
	fixed_t 	bob;
	bool		still = false;

	// Regular movement bobbing
	// (needs to be calculated for gun swing even if not on ground)
	// OPTIMIZE: tablify angle

	// killough 10/98: Make bobbing depend only on player-applied motion.
	//
	// Note: don't reduce bobbing here if on ice: if you reduce bobbing here,
	// it causes bobbing jerkiness when the player moves from ice to non-ice,
	// and vice-versa.

	if ((player->mo->flags2 & MF2_FLY) && !onground)
	{
		player->bob = FRACUNIT / 2;
	}
	else
	{
		player->bob = DMulScale16 (player->momx, player->momx, player->momy, player->momy);
		if (player->bob == 0)
		{
			still = true;
		}
		else
		{
			player->bob = FixedMul (player->bob, player->userinfo.MoveBob);

			if (player->bob > MAXBOB)
				player->bob = MAXBOB;
		}
	}

	if (player->cheats & CF_NOMOMENTUM)
	{
		player->viewz = player->mo->z + VIEWHEIGHT;

		if (player->viewz > player->mo->ceilingz-4*FRACUNIT)
			player->viewz = player->mo->ceilingz-4*FRACUNIT;

		return;
	}

	if (still)
	{
		if (player->health > 0)
		{
			angle = DivScale13 (level.time, 120*TICRATE/35) & FINEMASK;
			bob = FixedMul (player->userinfo.StillBob, finesine[angle]);
		}
		else
		{
			bob = 0;
		}
	}
	else
	{
		// DivScale 13 because FINEANGLES == (1<<13)
		angle = DivScale13 (level.time, 20*TICRATE/35) & FINEMASK;
		bob = FixedMul (player->bob>>(player->mo->waterlevel > 1 ? 2 : 1), finesine[angle]);
	}

	// move viewheight
	if (player->playerstate == PST_LIVE)
	{
		player->viewheight += player->deltaviewheight;

		if (player->viewheight > VIEWHEIGHT)
		{
			player->viewheight = VIEWHEIGHT;
			player->deltaviewheight = 0;
		}
		else if (player->viewheight < VIEWHEIGHT/2)
		{
			player->viewheight = VIEWHEIGHT/2;
			if (player->deltaviewheight <= 0)
				player->deltaviewheight = 1;
		}
		
		if (player->deltaviewheight)	
		{
			player->deltaviewheight += FRACUNIT/4;
			if (!player->deltaviewheight)
				player->deltaviewheight = 1;
		}
	}

	if (player->morphTics)
	{
		player->viewz = player->mo->z + player->viewheight - (20 * FRACUNIT);
	}
	else
	{
		player->viewz = player->mo->z + player->viewheight + bob;
	}
	if (player->mo->floorclip && player->playerstate != PST_DEAD
		&& player->mo->z <= player->mo->floorz)
	{
		player->viewz -= player->mo->floorclip;
	}
	if (player->viewz > player->mo->ceilingz - 4*FRACUNIT)
	{
		player->viewz = player->mo->ceilingz - 4*FRACUNIT;
	}
	if (player->viewz < player->mo->floorz + 4*FRACUNIT)
	{
		player->viewz = player->mo->floorz + 4*FRACUNIT;
	}
}

/*
=================
=
= P_MovePlayer
=
=================
*/
CUSTOM_CVAR (Float, sv_aircontrol, 0.00390625f, CVAR_SERVERINFO|CVAR_NOSAVE)
{
	level.aircontrol = (fixed_t)(self * 65536.f);
	G_AirControlChanged ();
}

void P_MovePlayer (player_t *player)
{
	ticcmd_t *cmd = &player->cmd;
	APlayerPawn *mo = player->mo;

	// [RH] 180-degree turn overrides all other yaws
	if (player->turnticks)
	{
		player->turnticks--;
		mo->angle += (ANGLE_180 / TURN180_TICKS);
	}
	else
	{
		mo->angle += cmd->ucmd.yaw << 16;
	}

	onground = (mo->z <= mo->floorz) || (mo->flags2 & MF2_ONMOBJ);

	// killough 10/98:
	//
	// We must apply thrust to the player and bobbing separately, to avoid
	// anomalies. The thrust applied to bobbing is always the same strength on
	// ice, because the player still "works just as hard" to move, while the
	// thrust applied to the movement varies with 'movefactor'.

	if (cmd->ucmd.forwardmove | cmd->ucmd.sidemove)
	{
		fixed_t forwardmove, sidemove;
		int bobfactor;
		int friction, movefactor;
		int fm, sm;

		movefactor = P_GetMoveFactor (mo, &friction);
		bobfactor = friction < ORIG_FRICTION ? movefactor : ORIG_FRICTION_FACTOR;
		if (!onground && !(player->mo->flags2 & MF2_FLY) && !player->mo->waterlevel)
		{
			// [RH] allow very limited movement if not on ground.
			movefactor = FixedMul (movefactor, level.aircontrol);
			bobfactor = FixedMul (bobfactor, level.aircontrol);
		}

		fm = cmd->ucmd.forwardmove;
		sm = cmd->ucmd.sidemove;
		mo->TweakSpeeds (fm, sm);
		fm = FixedMul (fm, player->mo->Speed);
		sm = FixedMul (sm, player->mo->Speed);

		forwardmove = Scale (fm, movefactor * 35, TICRATE << 8);
		sidemove = Scale (sm, movefactor * 35, TICRATE << 8);

		if (forwardmove)
		{
			P_Bob (player, mo->angle, (cmd->ucmd.forwardmove * bobfactor) >> 8);
			P_ForwardThrust (player, mo->angle, forwardmove);
		}
		if (sidemove)
		{
			P_Bob (player, mo->angle-ANG90, (cmd->ucmd.sidemove * bobfactor) >> 8);
			P_SideThrust (player, mo->angle, sidemove);
		}

		if (debugfile)
		{
			fprintf (debugfile, "move player for pl %d%c: (%ld,%ld,%ld) (%ld,%ld) %d %d w%d [", player-players,
				player->cheats&CF_PREDICTING?'p':' ',
				player->mo->x, player->mo->y, player->mo->z,forwardmove, sidemove, movefactor, friction, player->mo->waterlevel);
			msecnode_t *n = player->mo->touching_sectorlist;
			while (n != NULL)
			{
				fprintf (debugfile, "%d ", n->m_sector-sectors);
				n = n->m_tnext;
			}
			fprintf (debugfile, "]\n");
		}

		if (!(player->cheats & CF_PREDICTING))
		{
			player->mo->PlayRunning ();
		}

		if (player->cheats & CF_REVERTPLEASE)
		{
			player->cheats &= ~CF_REVERTPLEASE;
			player->camera = player->mo;
		}
	}
}		

//==========================================================================
//
// P_FallingDamage
//
//==========================================================================

void P_FallingDamage (AActor *actor)
{
	int damagestyle;
	int damage;
	fixed_t mom;

	damagestyle = ((level.flags >> 15) | (dmflags)) &
		(DF_FORCE_FALLINGZD | DF_FORCE_FALLINGHX);

	if (damagestyle == 0)
		return;

	mom = abs (actor->momz);

	// Since Hexen falling damage is stronger than ZDoom's, it takes
	// precedence. ZDoom falling damage may not be as strong, but it
	// gets felt sooner.

	switch (damagestyle)
	{
	case DF_FORCE_FALLINGHX:		// Hexen falling damage
		if (mom <= 23*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 63*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			mom = FixedMul (mom, 16*FRACUNIT/23);
			damage = ((FixedMul (mom, mom) / 10) >> FRACBITS) - 24;
			if (actor->momz > -39*FRACUNIT && damage > actor->health
				&& actor->health != 1)
			{ // No-death threshold
				damage = actor->health-1;
			}
		}
		break;
	
	case DF_FORCE_FALLINGZD:		// ZDoom falling damage
		if (mom <= 19*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		if (mom >= 84*FRACUNIT)
		{ // automatic death
			damage = 1000000;
		}
		else
		{
			damage = ((MulScale23 (mom, mom*11) >> FRACBITS) - 30) / 2;
			if (damage < 1)
			{
				damage = 1;
			}
		}
		break;

	case DF_FORCE_FALLINGST:		// Strife falling damage
		if (mom <= 20*FRACUNIT)
		{ // Not fast enough to hurt
			return;
		}
		// The minimum amount of damage you take from falling in Strife
		// is 52. Ouch!
		damage = mom / 25000;
		break;
	}

	if (actor->player)
	{
		S_Sound (actor, CHAN_AUTO, "*land", 1, ATTN_NORM);
		P_NoiseAlert (actor, actor, true);
		if (damage == 1000000 && (actor->player->cheats & CF_GODMODE))
		{
			damage = 999;
		}
	}
	P_DamageMobj (actor, NULL, NULL, damage, MOD_FALLING);
}

//==========================================================================
//
// P_DeathThink
//
//==========================================================================

void P_DeathThink (player_t *player)
{
	int dir;
	angle_t delta;
	int lookDelta;

	P_MovePsprites (player);

	onground = (player->mo->z <= player->mo->floorz);
	if (player->mo->IsKindOf (RUNTIME_CLASS(APlayerChunk)))
	{ // Flying bloody skull or flying ice chunk
		player->viewheight = 6 * FRACUNIT;
		player->deltaviewheight = 0;
		if (onground)
		{
			if (player->mo->pitch > -(int)ANGLE_1*19)
			{
				lookDelta = (-(int)ANGLE_1*19 - player->mo->pitch) / 8;
				player->mo->pitch += lookDelta;
			}
		}
	}
	else if (player->mo->DamageType != MOD_ICE)
	{ // Fall to ground (if not frozen)
		player->deltaviewheight = 0;
		if (player->viewheight > 6*FRACUNIT)
		{
			player->viewheight -= FRACUNIT;
		}
		if (player->viewheight < 6*FRACUNIT)
		{
			player->viewheight = 6*FRACUNIT;
		}
		if (player->mo->pitch < 0)
		{
			player->mo->pitch += ANGLE_1*3;
		}
		else if (player->mo->pitch > 0)
		{
			player->mo->pitch -= ANGLE_1*3;
		}
		if (abs(player->mo->pitch) < ANGLE_1*3)
		{
			player->mo->pitch = 0;
		}
	}
	P_CalcHeight (player);
		
	if (player->attacker && player->attacker != player->mo)
	{ // Watch killer
		dir = P_FaceMobj (player->mo, player->attacker, &delta);
		if (delta < ANGLE_1*10)
		{ // Looking at killer, so fade damage and poison counters
			if (player->damagecount)
			{
				player->damagecount--;
			}
			if (player->poisoncount)
			{
				player->poisoncount--;
			}
		}
		delta /= 8;
		if (delta > ANGLE_1*5)
		{
			delta = ANGLE_1*5;
		}
		if (dir)
		{ // Turn clockwise
			player->mo->angle += delta;
		}
		else
		{ // Turn counter clockwise
			player->mo->angle -= delta;
		}
	}
	else
	{
		if (player->damagecount)
		{
			player->damagecount--;
		}
		if (player->poisoncount)
		{
			player->poisoncount--;
		}
	}		

	if (player->cmd.ucmd.buttons & BT_USE ||
		((deathmatch || alwaysapplydmflags) && (dmflags & DF_FORCE_RESPAWN)))
	{
		if (level.time >= player->respawn_time || ((player->cmd.ucmd.buttons & BT_USE) && !player->isbot))
		{
			player->cls = NULL;		// Force a new class if the player is using a random class
			player->playerstate = multiplayer ? PST_REBORN : PST_ENTER;
			if (player->mo->special1 > 2)
			{
				player->mo->special1 = 0;
			}
		}
	}
}

//----------------------------------------------------------------------------
//
// PROC P_PlayerThink
//
//----------------------------------------------------------------------------

void P_PlayerThink (player_t *player)
{
	ticcmd_t *cmd;

	if (player->mo == NULL)
	{
		I_Error ("No player %d start\n", player - players + 1);
	}

	if (debugfile && !(player->cheats & CF_PREDICTING))
	{
		fprintf (debugfile, "tic %d for pl %d: (%ld, %ld, %ld, %lu) b:%02x p:%d y:%d f:%d s:%d u:%d\n",
			gametic, player-players, player->mo->x, player->mo->y, player->mo->z,
			player->mo->angle>>ANGLETOFINESHIFT, player->cmd.ucmd.buttons,
			player->cmd.ucmd.pitch, player->cmd.ucmd.yaw, player->cmd.ucmd.forwardmove,
			player->cmd.ucmd.sidemove, player->cmd.ucmd.upmove);
	}

	player->xviewshift = 0;		// [RH] Make sure view is in right place

	// [RH] Zoom the player's FOV
	if (player->FOV != player->DesiredFOV)
	{
		if (fabsf (player->FOV - player->DesiredFOV) < 7.f)
		{
			player->FOV = player->DesiredFOV;
		}
		else
		{
			float zoom = MAX(7.f, fabsf (player->FOV - player->DesiredFOV) * 0.025f);
			if (player->FOV > player->DesiredFOV)
			{
				player->FOV = player->FOV - zoom;
			}
			else
			{
				player->FOV = player->FOV + zoom;
			}
		}
	}
	if (player->inventorytics)
	{
		player->inventorytics--;
	}
	// No-clip cheat
	if (player->cheats & CF_NOCLIP)
	{
		player->mo->flags |= MF_NOCLIP;
	}
	else
	{
		player->mo->flags &= ~MF_NOCLIP;
	}
	cmd = &player->cmd;
	if (player->mo->flags & MF_JUSTATTACKED)
	{ // Chainsaw/Gauntlets attack auto forward motion
		cmd->ucmd.yaw = 0;
		cmd->ucmd.forwardmove = 0xc800/2;
		cmd->ucmd.sidemove = 0;
		player->mo->flags &= ~MF_JUSTATTACKED;
	}

	// [RH] Being totally frozen zeros out most input parameters.
	if (player->cheats & CF_TOTALLYFROZEN)
	{
		cmd->ucmd.buttons &= BT_USE;
		cmd->ucmd.pitch = 0;
		cmd->ucmd.yaw = 0;
		cmd->ucmd.roll = 0;
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
		player->turnticks = 0;
	}
	else if (player->cheats & CF_FROZEN)
	{
		cmd->ucmd.forwardmove = 0;
		cmd->ucmd.sidemove = 0;
		cmd->ucmd.upmove = 0;
	}

	if (player->playerstate == PST_DEAD)
	{
		P_DeathThink (player);
		return;
	}
	if (player->jumpTics)
	{
		player->jumpTics--;
	}
	if (player->morphTics && !(player->cheats & CF_PREDICTING))
	{
		player->mo->MorphPlayerThink ();
	}

	// [RH] Look up/down stuff
	if (dmflags & DF_NO_FREELOOK)
	{
		player->mo->pitch = 0;
	}
	else
	{
		int look = cmd->ucmd.pitch << 16;

		// The player's view pitch is clamped between -32 and +56 degrees,
		// which translates to about half a screen height up and (more than)
		// one full screen height down from straight ahead when view panning
		// is used.
		if (look)
		{
			if (look == -32768 << 16)
			{ // center view
				player->mo->pitch = 0;
			}
			else
			{
				player->mo->pitch -= look;
				if (look > 0)
				{ // look up
					if (player->mo->pitch < -ANGLE_1*32)
						player->mo->pitch = -ANGLE_1*32;
				}
				else
				{ // look down
					if (player->mo->pitch > ANGLE_1*56)
						player->mo->pitch = ANGLE_1*56;
				}
			}
		}
	}

	// [RH] Check for fast turn around
	if (cmd->ucmd.buttons & BT_TURN180 && !(player->oldbuttons & BT_TURN180))
	{
		player->turnticks = TURN180_TICKS;
	}

	// Handle movement
	if (player->mo->reactiontime)
	{ // Player is frozen
		player->mo->reactiontime--;
	}
	else
	{
		P_MovePlayer (player);

		// [RH] check for jump
		if (cmd->ucmd.buttons & BT_JUMP)
		{
			if (player->mo->waterlevel >= 2)
			{
				player->mo->momz = 4*FRACUNIT;
			}
			else if (player->mo->flags2 & MF2_FLY)
			{
				player->mo->momz = 3*FRACUNIT;
			}
			else if (!(dmflags & DF_NO_JUMP) && onground && !player->jumpTics)
			{
				player->mo->momz += player->mo->GetJumpZ ()*35/TICRATE;
				S_Sound (player->mo, CHAN_BODY, "*jump", 1, ATTN_NORM);
				player->mo->flags2 &= ~MF2_ONMOBJ;
				player->jumpTics = 18*TICRATE/35;
			}
		}

		if (cmd->ucmd.upmove == -32768)
		{ // Only land if in the air
			if ((player->mo->flags2 & MF2_FLY) && player->mo->waterlevel < 2)
			{
				player->mo->flags2 &= ~MF2_FLY;
				player->mo->flags &= ~MF_NOGRAVITY;
			}
		}
		else if (cmd->ucmd.upmove != 0)
		{
			if (player->mo->waterlevel >= 2 || (player->mo->flags2 & MF2_FLY))
			{
				player->mo->momz = cmd->ucmd.upmove << 9;
				if (player->mo->waterlevel < 2 && !(player->mo->flags2 & MF2_FLY))
				{
					player->mo->flags2 |= MF2_FLY;
					player->mo->flags |= MF_NOGRAVITY;
					if (player->mo->momz <= -39*FRACUNIT)
					{ // Stop falling scream
						S_StopSound (player->mo, CHAN_VOICE);
					}
				}
			}
			else if (cmd->ucmd.upmove > 0 && !(player->cheats & CF_PREDICTING))
			{
				AInventory *fly = player->mo->FindInventory (TypeInfo::FindType ("ArtiFly"));
				if (fly != NULL)
				{
					player->mo->UseInventory (fly);
				}
			}
		}
	}

	P_CalcHeight (player);

	if (!(player->cheats & CF_PREDICTING))
	{
		if (player->mo->Sector->special || player->mo->Sector->damage)
		{
			P_PlayerInSpecialSector (player);
		}
		P_PlayerOnSpecialFlat (player, P_GetThingFloorType (player->mo));
		if (player->mo->momz <= -35*FRACUNIT
			&& player->mo->momz >= -40*FRACUNIT && !player->morphTics)
		{
			int id = S_FindSkinnedSound (player->mo, "*falling");
			if (id != 0 && !S_GetSoundPlayingInfo (player->mo, id))
			{
				S_SoundID (player->mo, CHAN_VOICE, id, 1, ATTN_NORM);
			}
		}
		// check for use
		if ((cmd->ucmd.buttons & BT_USE) && !(player->oldbuttons & BT_USE))
		{
			P_UseLines (player);
		}
		// Morph counter
		if (player->morphTics)
		{
			if (player->chickenPeck)
			{ // Chicken attack counter
				player->chickenPeck -= 3;
			}
			if (!--player->morphTics)
			{ // Attempt to undo the chicken/pig
				P_UndoPlayerMorph (player);
			}
		}
		// Cycle psprites
		P_MovePsprites (player);

		// Other Counters
		if (player->damagecount)
			player->damagecount--;

		if (player->bonuscount)
			player->bonuscount--;

		if (player->hazardcount)
		{
			player->hazardcount--;
			if (!(level.time & 31) && player->hazardcount > 16*TICRATE)
				P_DamageMobj (player->mo, NULL, NULL, 5, MOD_SLIME);
		}

		if (player->poisoncount && !(level.time & 15))
		{
			player->poisoncount -= 5;
			if (player->poisoncount < 0)
			{
				player->poisoncount = 0;
			}
			P_PoisonDamage (player, player->poisoner, 1, true);
		}

		// Handle air supply
		if (player->mo->waterlevel < 3 ||
			(player->mo->flags2 & MF2_INVULNERABLE) ||
			(player->cheats & CF_GODMODE))
		{
			player->air_finished = level.time + 10*TICRATE;
		}
		else if (player->air_finished <= level.time && !(level.time & 31))
		{
			P_DamageMobj (player->mo, NULL, NULL, 2 + 2*((level.time-player->air_finished)/TICRATE), MOD_WATER);
		}
	}

	// Save buttons
	player->oldbuttons = cmd->ucmd.buttons;
}

void P_PredictPlayer (player_t *player)
{
	int maxtic;

	if (cl_noprediction ||
		singletics ||
		demoplayback ||
		player->mo == NULL ||
		player != &players[consoleplayer] ||
		player->playerstate != PST_LIVE ||
		!netgame ||
		/*player->morphTics ||*/
		(player->cheats & CF_PREDICTING))
	{
		return;
	}

	maxtic = maketic;

	if (gametic == maxtic)
	{
		return;
	}

	// Save original values for restoration later
	PredictionPlayerBackup = *player;

	AActor *act = player->mo;
	memcpy (PredictionActorBackup, &act->x, sizeof(AActor)-((BYTE *)&act->x-(BYTE *)act));

	act->flags &= ~MF_PICKUP;
	act->flags2 &= ~MF2_PUSHWALL;
	player->cheats |= CF_PREDICTING;

	// The ordering of the touching_sectorlist needs to remain unchanged
	msecnode_t *mnode = act->touching_sectorlist;
	PredictionTouchingSectorsBackup.Clear ();

	while (mnode != NULL)
	{
		PredictionTouchingSectorsBackup.Push (mnode->m_sector);
		mnode = mnode->m_tnext;
	}

	// Blockmap ordering also needs to stay the same, so unlink the block nodes
	// without releasing them. (They will be used again in P_UnpredictPlayer).
	FBlockNode *block = act->BlockNode;

	while (block != NULL)
	{
		if (block->NextActor != NULL)
		{
			block->NextActor->PrevActor = block->PrevActor;
		}
		*(block->PrevActor) = block->NextActor;
		block = block->NextBlock;
	}
	act->BlockNode = NULL;


	for (int i = gametic; i < maxtic; ++i)
	{
		player->cmd = localcmds[i % LOCALCMDTICS];
		P_PlayerThink (player);
		player->mo->Tick ();
	}
}

extern msecnode_t *P_AddSecnode (sector_t *s, AActor *thing, msecnode_t *nextnode);

void P_UnPredictPlayer ()
{
	player_t *player = &players[consoleplayer];

	if (player->cheats & CF_PREDICTING)
	{
		AActor *act = player->mo;

		*player = PredictionPlayerBackup;

		act->UnlinkFromWorld ();
		memcpy (&act->x, PredictionActorBackup, sizeof(AActor)-((BYTE *)&act->x-(BYTE *)act));

		// Make the sector_list match the player's touching_sectorlist before it got predicted.
		P_DelSeclist (sector_list);
		sector_list = NULL;
		for (size_t i = PredictionTouchingSectorsBackup.Size (); i-- > 0; )
		{
			sector_list = P_AddSecnode (PredictionTouchingSectorsBackup[i], act, sector_list);
		}

		// The blockmap ordering needs to remain unchanged, too. Right now, act has the right
		// pointers, so temporarily set its MF_NOBLOCKMAP flag so that LinkToWorld() does not
		// mess with them.
		act->flags |= MF_NOBLOCKMAP;
		act->LinkToWorld ();
		act->flags &= ~MF_NOBLOCKMAP;

		// Now fix the pointers in the blocknode chain
		FBlockNode *block = act->BlockNode;

		while (block != NULL)
		{
			*(block->PrevActor) = block;
			if (block->NextActor != NULL)
			{
				block->NextActor->PrevActor = &block->NextActor;
			}
			block = block->NextBlock;
		}
	}
}

void player_s::Serialize (FArchive &arc)
{
	int i;
	userinfo_t *ui;
	userinfo_t dummy;

	if (arc.IsStoring ())
	{
		arc.UserWriteClass (cls);
		ui = &userinfo;
	}
	else
	{
		arc.UserReadClass (cls);
		ui = &dummy;
	}

	arc << mo
		<< camera
		<< playerstate
		<< cmd
		<< *ui
		<< DesiredFOV << FOV
		<< viewz
		<< viewheight
		<< deltaviewheight
		<< bob
		<< momx
		<< momy
		<< centering
		<< health
		<< inventorytics
		<< InvFirst << InvSel
		<< pieces
		<< backpack
		<< fragcount
		<< spreecount
		<< multicount
		<< lastkilltime
		<< ReadyWeapon << PendingWeapon
		<< cheats
		<< refire
		<< inconsistant
		<< killcount
		<< itemcount
		<< secretcount
		<< damagecount
		<< bonuscount
		<< hazardcount
		<< poisoncount
		<< poisoner
		<< attacker
		<< extralight
		<< fixedcolormap
		<< xviewshift
		<< morphTics
		<< PremorphWeapon
		<< chickenPeck
		<< jumpTics
		<< respawn_time
		<< air_finished
		<< turnticks
		<< oldbuttons
		<< isbot
		<< BlendR
		<< BlendG
		<< BlendB
		<< BlendA
		<< accuracy << stamina
		<< LogText;
	for (i = 0; i < MAXPLAYERS; i++)
		arc << frags[i];
	for (i = 0; i < NUMPSPRITES; i++)
		arc << psprites[i];

	arc << CurrentPlayerClass;

	if (isbot)
	{
		arc	<< angle
			<< dest
			<< prev
			<< enemy
			<< missile
			<< mate
			<< last_mate
			<< skill
			<< t_active
			<< t_respawn
			<< t_strafe
			<< t_react
			<< t_fight
			<< t_roam
			<< t_rocket
			<< first_shot
			<< sleft
			<< allround
			<< oldx
			<< oldy;
		if (arc.IsLoading ())
		{
			if (consoleplayer != this - players)
			{
				userinfo = *ui;
			}
			botinfo_t *thebot = bglobal.botinfo;
			while (thebot && stricmp (userinfo.netname, thebot->name))
				thebot = thebot->next;
			if (thebot)
			{
				thebot->inuse = true;
			}
			bglobal.botnum++;
			bglobal.botingame[this - players] = true;
		}
	}
	else
	{
		dest = prev = enemy = missile = mate = last_mate = NULL;
	}
	if (arc.IsLoading ())
	{
		// If the player reloaded because they pressed +use after dying, we
		// don't want +use to still be down after the game is loaded.
		oldbuttons = ~0;
	}
}
