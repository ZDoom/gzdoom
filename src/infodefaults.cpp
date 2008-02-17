/*
** infodefaults.cpp
** Parses default lists to create default copies of actors
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <assert.h>
#include "actor.h"
#include "info.h"
#include "s_sound.h"
#include "p_conversation.h"
#include "a_pickups.h"
#include "gi.h"
#include "a_artifacts.h"
#include "a_keys.h"
#include "i_system.h"
#include "r_data.h"
#include "w_wad.h"
#include "a_strifeglobal.h"
#include "thingdef/thingdef.h"

void FActorInfo::BuildDefaults ()
{
	if (Class->Defaults == NULL)
	{
		Class->Defaults = new BYTE[Class->Size];
		if (Class == RUNTIME_CLASS(AActor))
		{
			memset (Class->Defaults, 0, Class->Size);
		}
		else
		{
			PClass *parent;

			parent = Class->ParentClass;
			parent->ActorInfo->BuildDefaults ();
			Class->Meta = parent->Meta;
			Class->Symbols.SetParentTable (&parent->Symbols);
			assert (Class->Size >= parent->Size);
			memcpy (Class->Defaults, parent->Defaults, parent->Size);
			if (Class->Size > parent->Size)
			{
				memset (Class->Defaults + parent->Size, 0, Class->Size - parent->Size);
			}
		}
		ApplyDefaults (Class->Defaults);
	}
}

static FState *DefaultStates (PClass *type)
{
	FState *states = type->ActorInfo->OwnedStates;

	if (states == NULL)
	{
		do
		{
			type = type->ParentClass;
			states = type->ActorInfo->OwnedStates;
		} while (states == NULL && type != RUNTIME_CLASS(AActor));
	}
	return states;
}

static PClass *sgClass;
static BYTE *sgDefaults;


static void ApplyActorDefault (int defnum, const char *datastr, int dataint)
{
	int datasound = 0;
	FState *datastate = NULL;
	const PClass *datatype;

	if (defnum <= ADEF_LastString)
	{
		if (defnum <= ADEF_Inventory_PickupSound)
		{
			datasound = S_FindSound (datastr);
		}
	}
	else if (defnum > ADEF_LastString && dataint >= 0 && dataint < PROP_CLEAR_STATE)
	{
		datastate = DefaultStates (sgClass) + dataint;
	}

	// Different casts for the same thing
	AActor *const actor = (AActor *)sgDefaults;
	APowerupGiver *const giver = (APowerupGiver *)sgDefaults;
	AInventory *const item = (AInventory *)sgDefaults;
	ABasicArmorBonus *const armorb = (ABasicArmorBonus *)sgDefaults;
	ABasicArmorPickup *const armorp = (ABasicArmorPickup *)sgDefaults;
	APuzzleItem *const puzzl = (APuzzleItem *)sgDefaults;
	APowerup *const power = (APowerup *)sgDefaults;
	AWeapon *const weapon = (AWeapon *)sgDefaults;
	ASigil *const sigil = (ASigil *)sgDefaults;
	AAmmo *const ammo = (AAmmo *)sgDefaults;
	APlayerPawn *const player = (APlayerPawn *)sgDefaults;

	switch (defnum)
	{
	case ADEF_Weapon_ProjectileType:
	case ADEF_PowerupGiver_Powerup:
		datatype = PClass::FindClass (datastr);
		if (datatype == NULL)
		{
			I_FatalError ("Unknown class %s in %s's default list", datastr, sgClass->TypeName.GetChars());
		}
		break;

	default:
		datatype = NULL;
	}

	switch (defnum)
	{
	case ADEF_SeeSound:		actor->SeeSound = datasound;	break;
	case ADEF_AttackSound:	actor->AttackSound = datasound;	break;
	case ADEF_PainSound:	actor->PainSound = datasound;	break;
	case ADEF_DeathSound:	actor->DeathSound = datasound;	break;
	case ADEF_ActiveSound:	actor->ActiveSound = datasound;	break;
	case ADEF_UseSound:		actor->UseSound = datasound;	break;
	case ADEF_Tag:
		{
			char name[256];
			int i;

			// Replace underscores with spaces
			i = 0;
			do
			{
				name[i] = datastr[i] == '_' ? ' ' : datastr[i];
			} while (datastr[i++] != 0);
			sgClass->Meta.SetMetaString (AMETA_StrifeName, name);
			break;
		}

	case ADEF_Obituary:
		sgClass->Meta.SetMetaString (AMETA_Obituary, datastr);
		break;

	case ADEF_HitObituary:
		sgClass->Meta.SetMetaString (AMETA_HitObituary, datastr);
		break;

	case ADEF_Inventory_PickupMsg:
		sgClass->Meta.SetMetaString (AIMETA_PickupMessage, datastr);
		break;

	case ADEF_PowerupGiver_Powerup:
		giver->PowerupType = datatype;
		break;

	case ADEF_Inventory_Icon:
		item->Icon = TexMan.AddPatch (datastr);
		if (item->Icon <= 0)
		{
			item->Icon = TexMan.AddPatch (datastr, ns_sprites);
		}
		break;

	case ADEF_XScale:			actor->scaleX = dataint;			break;
	case ADEF_YScale:			actor->scaleY = dataint;			break;
	case ADEF_SpawnHealth:		actor->health = dataint;			break;
	case ADEF_ReactionTime:		actor->reactiontime = dataint;		break;
	case ADEF_PainChance:		actor->PainChance = dataint;		break;
	case ADEF_Speed:			actor->Speed = dataint;				break;
	case ADEF_FloatSpeed:		actor->FloatSpeed = dataint;		break;
	case ADEF_Radius:			actor->radius = dataint;			break;
	case ADEF_Height:			actor->height = dataint;			break;
	case ADEF_Mass:				actor->Mass = dataint;				break;
	case ADEF_Damage:			actor->Damage = dataint;			break;
	case ADEF_DamageType:		actor->DamageType = (ENamedName)dataint;		break;
	case ADEF_Flags:			actor->flags = dataint;				break;
	case ADEF_Flags2:			actor->flags2 = dataint;			break;
	case ADEF_Flags3:			actor->flags3 = dataint;			break;
	case ADEF_Flags4:			actor->flags4 = dataint;			break;
	case ADEF_Flags5:			actor->flags5 = dataint;			break;
	case ADEF_FlagsSet:			actor->flags |= dataint;			break;
	case ADEF_Flags2Set:		actor->flags2 |= dataint;			break;
	case ADEF_Flags3Set:		actor->flags3 |= dataint;			break;
	case ADEF_Flags4Set:		actor->flags4 |= dataint;			break;
	case ADEF_Flags5Set:		actor->flags5 |= dataint;			break;
	case ADEF_FlagsClear:		actor->flags &= ~dataint;			break;
	case ADEF_Flags2Clear:		actor->flags2 &= ~dataint;			break;
	case ADEF_Flags3Clear:		actor->flags3 &= ~dataint;			break;
	case ADEF_Flags4Clear:		actor->flags4 &= ~dataint;			break;
	case ADEF_Flags5Clear:		actor->flags5 &= ~dataint;			break;
	case ADEF_Alpha:			actor->alpha = dataint;				break;
	case ADEF_RenderStyle:		actor->RenderStyle = ERenderStyle(dataint);		break;
	case ADEF_RenderFlags:		actor->renderflags = dataint;		break;
	case ADEF_Translation:		actor->Translation = dataint;		break;
	case ADEF_MinMissileChance:	actor->MinMissileChance = dataint;	break;
	case ADEF_MeleeRange:		actor->meleerange = dataint;		break;
	case ADEF_MaxDropOffHeight:	actor->MaxDropOffHeight = dataint;	break;
	case ADEF_MaxStepHeight:	actor->MaxStepHeight = dataint;		break;
	case ADEF_BounceFactor:		actor->bouncefactor = dataint;		break;
	case ADEF_BounceCount:		actor->bouncecount = dataint;		break;
	case ADEF_RDFactor:			sgClass->Meta.SetMetaFixed (AMETA_RDFactor, dataint); break;
	case ADEF_FXFlags:			actor->effects = dataint;			break;
	case ADEF_Gravity:			actor->gravity = dataint;			break;

	case ADEF_SpawnState:		AddState("Spawn", datastate);		break;
	case ADEF_SeeState:			AddState("See", datastate);			break;
	case ADEF_PainState:		AddState("Pain", datastate);		break;
	case ADEF_MeleeState:		AddState("Melee", datastate);		break;
	case ADEF_MissileState:		AddState("Missile", datastate);		break;
	case ADEF_CrashState:		AddState("Crash", datastate);		break;
	case ADEF_DeathState:		AddState("Death", datastate);		break;
	case ADEF_XDeathState:		AddState("XDeath", datastate);		break;
	case ADEF_BDeathState:		AddState("Burn", datastate);		break;
	case ADEF_IDeathState:		AddState("Ice", datastate);			break;
	case ADEF_EDeathState:		AddState("Disintegrate", datastate);break;
	case ADEF_RaiseState:		AddState("Raise", datastate);		break;
	case ADEF_WoundState:		AddState("Wound", datastate);		break;

	case ADEF_StrifeType:	if (!(gameinfo.flags & GI_SHAREWARE)) StrifeTypes[dataint] = sgClass;	break;
	case ADEF_StrifeTeaserType:
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE))
			StrifeTypes[dataint] = sgClass;
		break;
	case ADEF_StrifeTeaserType2:
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE|GI_TEASER2))
			StrifeTypes[dataint] = sgClass;
		break;


	case ADEF_Inventory_FlagsSet:	item->ItemFlags |= dataint; break;
	case ADEF_Inventory_FlagsClear:	item->ItemFlags &= ~dataint; break;
	case ADEF_Inventory_PickupFlash:if(dataint) { item->PickupFlash = fuglyname("PickupFlash"); } else { item->PickupFlash = NULL; } break;
	case ADEF_Inventory_Amount:		item->Amount = dataint;	break;
	case ADEF_Inventory_RespawnTics:item->RespawnTics = dataint; break;
	case ADEF_Inventory_MaxAmount:	item->MaxAmount = dataint; break;
	case ADEF_Inventory_DefMaxAmount:
		// In Heretic, the maximum number of artifacts of one kind you can carry is 16.
		// In Hexen, it was upped to 25.
		item->MaxAmount = gameinfo.gametype == GAME_Heretic ? 16 : 25;
		break;
	case ADEF_Inventory_PickupSound:item->PickupSound = datasound; break;

	case ADEF_BasicArmorPickup_SavePercent:		armorp->SavePercent = dataint; break;
	case ADEF_BasicArmorPickup_SaveAmount:		armorp->SaveAmount = dataint; break;
	case ADEF_BasicArmorBonus_SavePercent:		armorb->SavePercent = dataint; break;
	case ADEF_BasicArmorBonus_SaveAmount:		armorb->SaveAmount = dataint; break;
	case ADEF_BasicArmorBonus_MaxSaveAmount:	armorb->MaxSaveAmount = dataint; break;
	case ADEF_HexenArmor_ArmorAmount:			item->Amount = dataint; break;

	case ADEF_PuzzleItem_Number:	puzzl->PuzzleItemNumber = dataint; break;

	case ADEF_PowerupGiver_EffectTics:giver->EffectTics = dataint; break;

	case ADEF_Powerup_EffectTics:	power->EffectTics = dataint; break;
	case ADEF_Powerup_Color:		power->BlendColor = dataint; break;

	case ADEF_Ammo_BackpackAmount:	ammo->BackpackAmount = dataint; break;
	case ADEF_Ammo_BackpackMaxAmount:ammo->BackpackMaxAmount = dataint; break;
	case ADEF_Ammo_DropAmount:		sgClass->Meta.SetMetaInt (AIMETA_DropAmount, dataint); break;

	case ADEF_Weapon_Flags:			weapon->WeaponFlags = dataint; break;
	case ADEF_Weapon_FlagsSet:		weapon->WeaponFlags |= dataint; break;
	case ADEF_Weapon_UpSound:		weapon->UpSound = datasound; break;
	case ADEF_Weapon_ReadySound:	weapon->ReadySound = datasound; break;
	case ADEF_Weapon_SisterType:	weapon->SisterWeaponType = fuglyname(datastr); break;
	case ADEF_Weapon_ProjectileType:weapon->ProjectileType = datatype; break;
	case ADEF_Weapon_AmmoType1:		weapon->AmmoType1 = fuglyname(datastr); break;
	case ADEF_Weapon_AmmoType2:		weapon->AmmoType2 = fuglyname(datastr); break;
	case ADEF_Weapon_AmmoGive1:		weapon->AmmoGive1 = dataint; break;
	case ADEF_Weapon_AmmoGive2:		weapon->AmmoGive2 = dataint; break;
	case ADEF_Weapon_AmmoUse1:		weapon->AmmoUse1 = dataint; break;
	case ADEF_Weapon_AmmoUse2:		weapon->AmmoUse2 = dataint; break;
	case ADEF_Weapon_Kickback:		weapon->Kickback = dataint; break;
	case ADEF_Weapon_YAdjust:		weapon->YAdjust = (dataint<<8)>>8; break;
	case ADEF_Weapon_SelectionOrder:weapon->SelectionOrder = dataint; break;
	case ADEF_Weapon_MoveCombatDist:weapon->MoveCombatDist = dataint; break;

	case ADEF_Weapon_UpState:		AddState("Select", datastate);		break;
	case ADEF_Weapon_DownState:		AddState("Deselect", datastate);	break;
	case ADEF_Weapon_ReadyState:	AddState("Ready", datastate);		break;
	case ADEF_Weapon_AtkState:		AddState("Fire", datastate);		break;
	case ADEF_Weapon_HoldAtkState:	AddState("Hold", datastate);		break;
	case ADEF_Weapon_FlashState:	AddState("Flash", datastate);		break;
	case ADEF_Sigil_NumPieces:		sigil->NumPieces = dataint; break;

	// [GRB] Player class properties
	case ADEF_PlayerPawn_JumpZ:		player->JumpZ = dataint; break;
	case ADEF_PlayerPawn_AttackZOffset:	player->AttackZOffset = dataint; break;
	case ADEF_PlayerPawn_ViewHeight: player->ViewHeight = dataint; break;
	case ADEF_PlayerPawn_ForwardMove1: player->ForwardMove1 = dataint; break;
	case ADEF_PlayerPawn_ForwardMove2: player->ForwardMove2 = dataint; break;
	case ADEF_PlayerPawn_SideMove1:	player->SideMove1 = dataint; break;
	case ADEF_PlayerPawn_SideMove2:	player->SideMove2 = dataint; break;
	case ADEF_PlayerPawn_ColorRange: sgClass->Meta.SetMetaInt (APMETA_ColorRange, dataint); break;
	case ADEF_PlayerPawn_CrouchSprite: player->crouchsprite = GetSpriteIndex(datastr);	break;
	case ADEF_PlayerPawn_SpawnMask:	player->SpawnMask = dataint; break;
	case ADEF_PlayerPawn_MorphWeapon: player->MorphWeapon = FName(datastr); break;

	case ADEF_PlayerPawn_DisplayName:
		sgClass->Meta.SetMetaString (APMETA_DisplayName, datastr);
		break;
	case ADEF_PlayerPawn_SoundClass:
		sgClass->Meta.SetMetaString (APMETA_SoundClass, datastr);
		break;
	case ADEF_PlayerPawn_ScoreIcon:
		player->ScoreIcon = TexMan.AddPatch (datastr);
		if (player->ScoreIcon <= 0)
		{
			player->ScoreIcon = TexMan.AddPatch (datastr, ns_sprites);
		}
		break;
	}
}

#ifndef _MSC_VER
void ApplyActorDefault (int defnum, const char *datastr)
{
	ApplyActorDefault (defnum, datastr, 0);
}
void ApplyActorDefault (int defnum, int dataint)
{
	ApplyActorDefault (defnum, 0, dataint);
}
#endif

void FActorInfo::ApplyDefaults (BYTE *defaults)
{
	sgClass = Class;
	sgDefaults = defaults;

	if (Class != RUNTIME_CLASS(AActor))
	{
		MakeStateDefines(Class->ParentClass->ActorInfo->StateList);
	}
	else
	{
		ClearStateLabels();
	}
#if _MSC_VER
	const BYTE *parser = DefaultList;

	const char *datastr = NULL;
	int dataint = 0;
	int defnum;
	int deftype;

	while ((defnum = *(WORD *)parser) != ADEF_EOL)
	{
		deftype = defnum >> (16-2);
		defnum &= 0x3FFF;
		parser += 2;
		if (defnum <= ADEF_LastString)
		{
			datastr = (const char *)parser;
			parser = (const BYTE *)datastr + strlen (datastr) + 1;
		}
		else
		{
			switch (deftype)
			{
			case 0:		// byte
				dataint = *parser;
				parser++;
				break;

			case 1:		// byte * FRACUNIT;
				dataint = *parser << FRACBITS;
				parser++;
				break;

			case 2:		// word
				dataint = *(WORD *)parser;
				parser += 2;
				break;

			case 3:		// long
				dataint = *(DWORD *)parser;
				parser += 4;
				break;
			}
			defnum &= ~ADEFTYPE_MASK;
		}

		ApplyActorDefault (defnum, datastr, dataint);
	}
#else
	DefaultsConstructor ();
#endif
	InstallStates(this, ((AActor *)defaults));
	// Anything that is CountKill is also a monster, even if it doesn't specify it.
	if (((AActor *)defaults)->flags & MF_COUNTKILL)
	{
		((AActor *)defaults)->flags3 |= MF3_ISMONSTER;
		((AActor *)defaults)->flags4 |= MF4_CANUSEWALLS;	// I won't bother changing all internal monster definitions so let's set this here.
	}
	// Any default projectile in Raven's games produces blood splatter
	if (gameinfo.gametype & GAME_Raven && ((AActor *)defaults)->flags & MF_MISSILE)
	{
		((AActor *)defaults)->flags5 |= MF5_BLOODSPLATTER;
	}
}
