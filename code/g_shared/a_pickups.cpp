#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"
#include "p_lnspec.h"
#include "a_hereticglobal.h"
#include "sbar.h"

IMPLEMENT_ABSTRACT_ACTOR (AHealth)
IMPLEMENT_ABSTRACT_ACTOR (AAmmo)

const char *AmmoPics[NUMAMMO];
const char *ArmorPics[NUMARMOR];

/* Keys *******************************************************************/

//---------------------------------------------------------------------------
//
// FUNC P_GiveBody
//
// Returns false if the body isn't needed at all.
//
//---------------------------------------------------------------------------

bool P_GiveBody (player_t *player, int num)
{
	int max;

	max = MAXHEALTH;
	if (player->morphTics)
	{
		max = MAXMORPHHEALTH;
	}
	if (player->health >= max)
	{
		return false;
	}
	player->health += num;
	if (player->health > max)
	{
		player->health = max;
	}
	player->mo->health = player->health;
	return true;
}

//---------------------------------------------------------------------------
//
// FUNC P_GiveArmor
//
// Returns false if the armor is worse than the current armor.
//
//---------------------------------------------------------------------------

bool P_GiveArmor (player_t *player, armortype_t armortype, int amount)
{
	if (player->armorpoints[0] >= amount)
	{
		return false;
	}
	player->armortype = armortype;
	player->armorpoints[0] = amount;
	return true;
}

//---------------------------------------------------------------------------
//
// PROC P_GiveKey
//
//---------------------------------------------------------------------------

void P_GiveKey (player_t *player, keytype_t key)
{
	extern int playerkeys;
	extern vertex_t KeyPoints[];
	
	if (player->keys[key])
	{
		return;
	}
	player->bonuscount = BONUSADD;
	player->keys[key] = true;
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing1
//
// Make a special thing visible again.
//
//---------------------------------------------------------------------------

void A_RestoreSpecialThing1 (AActor *thing)
{
	thing->renderflags &= ~RF_INVISIBLE;
	if (static_cast<AInventory *>(thing)->DoRespawn ())
	{
		S_Sound (thing, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
	}
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing2
//
//---------------------------------------------------------------------------

void A_RestoreSpecialThing2 (AActor *thing)
{
	thing->flags |= MF_SPECIAL;
	thing->SetState (thing->SpawnState);
}

/***************************************************************************/
/* AItemFog, shown for respawning Doom items							   */
/***************************************************************************/

class AItemFog : public AActor
{
	DECLARE_ACTOR (AItemFog, AActor)
};

FState AItemFog::States[] =
{
	S_BRIGHT (IFOG, 'A',	6, NULL 						, &States[1]),
	S_BRIGHT (IFOG, 'B',	6, NULL 						, &States[2]),
	S_BRIGHT (IFOG, 'A',	6, NULL 						, &States[3]),
	S_BRIGHT (IFOG, 'B',	6, NULL 						, &States[4]),
	S_BRIGHT (IFOG, 'C',	6, NULL 						, &States[5]),
	S_BRIGHT (IFOG, 'D',	6, NULL 						, &States[6]),
	S_BRIGHT (IFOG, 'E',	6, NULL 						, NULL)
};

IMPLEMENT_ACTOR (AItemFog, Doom, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_SpawnState (0)
END_DEFAULTS

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialDoomThing
//
//---------------------------------------------------------------------------

void A_RestoreSpecialDoomThing (AActor *self)
{
	self->renderflags &= ~RF_INVISIBLE;
	self->flags |= MF_SPECIAL;
	if (static_cast<AInventory *>(self)->DoRespawn ())
	{
		self->SetState (self->SpawnState);
		S_Sound (self, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
		Spawn<AItemFog> (self->x, self->y, self->z);
	}
}

/***************************************************************************/
/* AInventory implementation												   */
/***************************************************************************/

FState AInventory::States[] =
{
#define S_HIDEDOOMISH 0
	S_NORMAL (TNT1, 'A', 1050, NULL							, &States[S_HIDEDOOMISH+1]),
	S_NORMAL (TNT1, 'A',    1, A_RestoreSpecialDoomThing	, NULL),

#define S_HIDESPECIAL (S_HIDEDOOMISH+2)
	S_NORMAL (ACLO, 'E', 1400, NULL                         , &States[S_HIDESPECIAL+1]),
	S_NORMAL (ACLO, 'A',    4, A_RestoreSpecialThing1       , &States[S_HIDESPECIAL+2]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+3]),
	S_NORMAL (ACLO, 'A',    4, NULL                         , &States[S_HIDESPECIAL+4]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+5]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+6]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+7]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+8]),
	S_NORMAL (ACLO, 'D',    4, NULL                         , &States[S_HIDESPECIAL+9]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+10]),
	S_NORMAL (ACLO, 'D',    4, A_RestoreSpecialThing2       , NULL)
};

int AInventory::StaticLastMessageTic;
const char *AInventory::StaticLastMessage;

IMPLEMENT_ACTOR (AInventory, Any, -1, 0)
END_DEFAULTS

bool AInventory::ShouldRespawn ()
{
	return (*deathmatch || *alwaysapplydmflags) && 
		   (*dmflags & DF_ITEMS_RESPAWN);
}

//----------------------------------------------------------------------------
//
// PROC P_HideSpecialThing
//
//----------------------------------------------------------------------------

void AInventory::Hide ()
{
	flags &= ~MF_SPECIAL;
	renderflags |= RF_INVISIBLE;
	if (gameinfo.gametype == GAME_Doom)
		SetState (&States[S_HIDEDOOMISH]);
	else
		SetState (&States[S_HIDESPECIAL]);
}

void AInventory::Touch (AActor *toucher)
{
	if (!TryPickup (toucher))
		return;

	const char *message = PickupMessage ();

	if (toucher == players[consoleplayer].camera
		&& (StaticLastMessageTic != gametic || StaticLastMessage != message))
	{
		StaticLastMessageTic = gametic;
		StaticLastMessage = message;
		Printf (PRINT_LOW, "%s\n", message);
		StatusBar->FlashCrosshair ();
	}

	//Added by MC: Change to favorite weapon.
	//VerifFavoritWeapon (player);

	// [RH] Execute an attached special (if any)
	if (special)
	{
		LineSpecials[special] (NULL, toucher,
			args[0], args[1], args[2], args[3], args[4]);
		special = 0;
	}

	if (flags & MF_COUNTITEM)
	{
		toucher->player->itemcount++;
		level.found_items++;
	}

	if (!ShouldStay ())
	{
		if (!(flags & MF_DROPPED))
		{
			if (IsKindOf (RUNTIME_CLASS(AArtifact)))
			{
				static_cast<AArtifact *>(this)->SetDormant ();
			}
			else if (ShouldRespawn ())
			{
				Hide ();
			}
			else
			{
				Destroy ();
			}
		}
		else
		{
			Destroy ();
		}

		//Added by MC: Check if item taken was the roam destination of any bot
		int i;
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && this == players[i].dest)
	    		players[i].dest = NULL;
		}
	}

	toucher->player->bonuscount = BONUSADD;

	// Special check so voodoo dolls picking up items cause the
	// real player to make noise.
	if (toucher->player)
		PlayPickupSound (toucher->player->mo);
	else
		PlayPickupSound (toucher);
}

const char *AInventory::PickupMessage ()
{
	return "You got a pickup";
}

void AInventory::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_ITEM, "misc/i_pkup", 1, ATTN_NORM);
}

bool AInventory::ShouldStay ()
{
	return false;
}

/***************************************************************************/
/* AArtifact implementation												   */
/***************************************************************************/

//---------------------------------------------------------------------------
//
// PROC A_RestoreArtifact
//
//---------------------------------------------------------------------------

void A_RestoreArtifact (AActor *arti)
{
	arti->flags |= MF_SPECIAL;
	arti->SetState (arti->SpawnState);
	S_Sound (arti, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
}

void A_HideThing (AActor *);
void A_UnHideThing (AActor *);
void A_RestoreArtifact (AActor *);

FState AArtifact::States[] =
{
#define S_DORMANTARTI1 0
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI1+1]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+2]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI1+3]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+4]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+5]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+6]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+7]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI1+8]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+9]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI1+10]),
	S_NORMAL (ACLO, 'A', 1400, A_HideThing                  , &States[S_DORMANTARTI1+11]),
	S_NORMAL (ACLO, 'A',    3, A_UnHideThing                , &States[S_DORMANTARTI1+12]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+13]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI1+14]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+15]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+16]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI1+17]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+18]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI1+19]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI1+20]),
	S_NORMAL (ACLO, 'D',    3, A_RestoreArtifact            , NULL),

#define S_DORMANTARTI2 (S_DORMANTARTI1+21)
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI2+1]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+2]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI2+3]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+4]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+5]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+6]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+7]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI2+8]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+9]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI2+10]),
	S_NORMAL (ACLO, 'A', 4200, A_HideThing                  , &States[S_DORMANTARTI2+11]),
	S_NORMAL (ACLO, 'A',    3, A_UnHideThing                , &States[S_DORMANTARTI2+12]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+13]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI2+14]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+15]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+16]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI2+17]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+18]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI2+19]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI2+20]),
	S_NORMAL (ACLO, 'D',    3, A_RestoreArtifact            , NULL),

#define S_DORMANTARTI3 (S_DORMANTARTI2+21)
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI3+1]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+2]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI3+3]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+4]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+5]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+6]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+7]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI3+8]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+9]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI3+10]),
	S_NORMAL (ACLO, 'A',21000, A_HideThing                  , &States[S_DORMANTARTI3+11]),
	S_NORMAL (ACLO, 'A',    3, A_UnHideThing                , &States[S_DORMANTARTI3+12]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+13]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DORMANTARTI3+14]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+15]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+16]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DORMANTARTI3+17]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+18]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DORMANTARTI3+19]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DORMANTARTI3+20]),
	S_NORMAL (ACLO, 'D',    3, A_RestoreArtifact            , NULL),

#define S_DEADARTI (S_DORMANTARTI3+21)
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DEADARTI+1]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DEADARTI+2]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[S_DEADARTI+3]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DEADARTI+4]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DEADARTI+5]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[S_DEADARTI+6]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DEADARTI+7]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[S_DEADARTI+8]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[S_DEADARTI+9]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , NULL)
};

IMPLEMENT_ACTOR (AArtifact, Any, -1, 0)
END_DEFAULTS

void AArtifact::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_ITEM, "misc/p_pkup", 1,
		toucher == NULL || toucher == players[consoleplayer].camera
		? ATTN_SURROUND : ATTN_NORM);
}

//---------------------------------------------------------------------------
//
// PROC AArtifact::SetDormant
//
// Removes the MF_SPECIAL flag, and initiates the artifact pickup
// animation.
//
//---------------------------------------------------------------------------

void AArtifact::SetDormant ()
{
	flags &= ~MF_SPECIAL;
	if (!(flags & MF_DROPPED) && ShouldRespawn ())
	{
		SetHiddenState ();
	}
	else
	{ // Don't respawn
		SetState (&States[S_DEADARTI]);
	}
}

void AArtifact::SetHiddenState ()
{
	if (gameinfo.gametype == GAME_Doom)
		SetState (&AInventory::States[S_HIDEDOOMISH]);
	else
		SetState (&States[S_DORMANTARTI1]);
}


#if 0
void ASummonMaulator::SetHiddenState ()
{
	SetState (&States[S_DORMANTARTI2]);
}

void AFlight::SetHiddenState ()
{
	SetState (&States[S_DORMANTARTI2]);
}

void AInvulnerability::SetHiddenState ()
{
	if (gameinfo.gametype == GAME_Doom)
		Super::SetHiddenState ();
	else
		SetState (&States[S_DORMANTARTI3]);
}

bool AInvulnerability::ShouldRespawn ()
{
	return Super::ShouldRespawn () && (dmflags & DF_RESPAWN_SUPER);
}

void AInvisibility::SetHiddenState ()
{
	if (gameinfo.gametype == GAME_Doom)
		Super::SetHiddenState ();
	else
		SetState (&States[S_DORMANTARTI3]);
}

bool AInvisibility::ShouldRespawn ()
{
	return Super::ShouldRespawn () && (dmflags & DF_RESPAWN_SUPER);
}
#endif

IMPLEMENT_ABSTRACT_ACTOR (AKey)

bool AKey::TryPickup (AActor *toucher)
{
	keytype_t keytype = GetKeyType ();
	player_t *player = toucher->player;
	if (player->keys[keytype])
		return false;

	P_GiveKey (player, keytype);
	return true;
}

bool AKey::ShouldStay ()
{
	return !!multiplayer;
}

void AKey::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_ITEM, "misc/k_pkup", 1, ATTN_NORM);
}

bool AInventory::DoRespawn ()
{
	return true;
}

bool AInventory::TryPickup (AActor *toucher)
{
	return false;
}

keytype_t AKey::GetKeyType ()
{
	return it_bluecard;
}
