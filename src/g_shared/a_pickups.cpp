#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"
#include "p_lnspec.h"
#include "a_hereticglobal.h"
#include "sbar.h"
#include "statnums.h"
#include "c_dispatch.h"
#include "gstrings.h"
#include "templates.h"

static FRandom pr_restore ("RestorePos");

//===========================================================================
//
// AArmor :: PlayPickupSound
//
//===========================================================================

void AArmor::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/armor_pkup", 1, ATTN_NORM);
}

IMPLEMENT_ABSTRACT_ACTOR (AAmmo)

//===========================================================================
//
// AAmmo :: Serialize
//
//===========================================================================

void AAmmo::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (SaveVersion >= 223)
	{
		arc << BackpackAmount << BackpackMaxAmount;
	}
}

//===========================================================================
//
// AAmmo :: PlayPickupSound
//
//===========================================================================

void AAmmo::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/ammo_pkup", 1, ATTN_NORM);
}

//===========================================================================
//
// AAmmo :: GetParentAmmo
//
// Returns the least-derived ammo type that this ammo is a descendant of.
// That is, if this ammo is an immediate subclass of Ammo, then this ammo's
// type is returned. If this ammo's superclass is not Ammo, then this
// function travels up the inheritance chain until it finds a type that is
// an immediate subclass of Ammo and returns that.
//
// The intent of this is that all unique ammo types will be immediate
// subclasses of Ammo. To make different pickups with different ammo amounts,
// you subclass the type of ammo you want a different amount for and edit
// that.
//
//===========================================================================

const TypeInfo *AAmmo::GetParentAmmo () const
{
	const TypeInfo *type = GetClass ();

	while (type->ParentType != RUNTIME_CLASS(AAmmo))
	{
		type = type->ParentType;
	}
	return type;
}

//===========================================================================
//
// AAmmo :: TryPickup
//
//===========================================================================

bool AAmmo::TryPickup (AActor *toucher)
{
	int count = Amount;

	if (gameskill == sk_baby || gameskill == sk_nightmare)
	{ // extra ammo in baby mode and nightmare mode
		if (gameinfo.gametype & (GAME_Doom|GAME_Strife))
			Amount <<= 1;
		else
			Amount += Amount >> 1;
	}
	if (!Super::TryPickup (toucher))
	{
		Amount = count;
		return false;
	}
	return true;
}

//===========================================================================
//
// AAmmo :: HandlePickup
//
//===========================================================================

bool AAmmo::HandlePickup (AInventory *item)
{
	if (GetClass() == item->GetClass() ||
		(item->IsKindOf (RUNTIME_CLASS(AAmmo)) && static_cast<AAmmo*>(item)->GetParentAmmo() == GetClass()))
	{
		if (Amount < MaxAmount)
		{
			int oldamount = Amount;
			Amount += item->Amount;
			if (Amount > MaxAmount)
			{
				Amount = MaxAmount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;

			// If the player previously had this ammo but ran out, possibly switch
			// to a weapon that uses it, but only if the player doesn't already
			// have a weapon pending.

			if (oldamount == 0 && Owner->player != NULL &&
				!Owner->player->userinfo.neverswitch &&
				(Owner->player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON) &&
				Owner->player->PendingWeapon == WP_NOCHANGE)
			{
				AWeapon *best = static_cast<APlayerPawn *>(Owner)->BestWeapon (GetClass());
				if (best != NULL &&
					best->SelectionOrder < Owner->player->ReadyWeapon->SelectionOrder)
				{
					Owner->player->PendingWeapon = best;
				}
			}
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AAmmo :: CreateCopy
//
//===========================================================================

AInventory *AAmmo::CreateCopy (AActor *other)
{
	AInventory *copy;

	if (GetClass()->ParentType != RUNTIME_CLASS(AAmmo))
	{
		const TypeInfo *type = GetParentAmmo();
		if (!GoAway ())
		{
			Destroy ();
		}
		copy = static_cast<AInventory *>(Spawn (type, 0, 0, 0));
		copy->Amount = Amount;
		copy->BecomeItem ();
		return copy;
	}
	return Super::CreateCopy (other);
}

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

	max = MAXHEALTH + player->stamina;
	if (player->morphTics)
	{
		max = MAXMORPHHEALTH;
	}
	// [RH] For Strife: A negative body gives sets you up with a percentage
	// of your full health.
	if (num < 0)
	{
		num = max * -num / 100;
		if (player->health >= num)
		{
			return false;
		}
		player->health = num;
		player->mo->health = num;
	}
	else
	{
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
	}
	return true;
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
	if (!(thing->GetDefault()->flags & MF_NOGRAVITY))
	{
		thing->flags &= ~MF_NOGRAVITY;
	}
	thing->SetState (thing->SpawnState);
}

/***************************************************************************/
/* AItemFog, shown for respawning Doom and Strife items					   */
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
	if (!(self->GetDefault()->flags & MF_NOGRAVITY))
	{
		self->flags &= ~MF_NOGRAVITY;
	}
	if (static_cast<AInventory *>(self)->DoRespawn ())
	{
		self->SetState (self->SpawnState);
		S_Sound (self, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
		Spawn<AItemFog> (self->x, self->y, self->z);
	}
}

//---------------------------------------------------------------------------
//
// PROP A_RestoreSpecialPosition
//
//---------------------------------------------------------------------------

void A_RestoreSpecialPosition (AActor *self)
{
	// Move item back to its original location
	fixed_t _x, _y;
	sector_t *sec;

	_x = self->SpawnPoint[0] << FRACBITS;
	_y = self->SpawnPoint[1] << FRACBITS;
	sec = R_PointInSubsector (_x, _y)->sector;

	self->SetOrigin (_x, _y, sec->floorplane.ZatPoint (_x, _y));
	P_CheckPosition (self, _x, _y);

	if (self->flags & MF_SPAWNCEILING)
	{
		self->z = self->ceilingz - self->height - (self->SpawnPoint[2] << FRACBITS);
	}
	else if (self->flags2 & MF2_SPAWNFLOAT)
	{
		fixed_t space = self->ceilingz - self->height - self->floorz;
		if (space > 48*FRACUNIT)
		{
			space -= 40*FRACUNIT;
			self->z = ((space * pr_restore())>>8) + self->floorz + 40*FRACUNIT;
		}
		else
		{
			self->z = self->floorz;
		}
	}
	else
	{
		self->z = (self->SpawnPoint[2] << FRACBITS) + self->floorz;
		if (self->flags2 & MF2_FLOATBOB)
		{
			self->z += FloatBobOffsets[(self->FloatBobPhase + level.time) & 63];
		}
	}
}

// Pickup flash -------------------------------------------------------------

class APickupFlash : public AActor
{
	DECLARE_ACTOR (APickupFlash, AActor)
};

FState APickupFlash::States[] =
{
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[1]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[2]),
	S_NORMAL (ACLO, 'D',    3, NULL                         , &States[3]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[4]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[5]),
	S_NORMAL (ACLO, 'C',    3, NULL                         , &States[6]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[7]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , &States[8]),
	S_NORMAL (ACLO, 'B',    3, NULL                         , &States[9]),
	S_NORMAL (ACLO, 'A',    3, NULL                         , NULL)
};

IMPLEMENT_ACTOR (APickupFlash, Raven, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOGRAVITY)
END_DEFAULTS

/***************************************************************************/
/* AInventory implementation											   */
/***************************************************************************/

FState AInventory::States[] =
{
#define S_HIDEDOOMISH 0
	S_NORMAL (TNT1, 'A', 1050, NULL							, &States[S_HIDEDOOMISH+1]),
	S_NORMAL (TNT1, 'A',	0, A_RestoreSpecialPosition		, &States[S_HIDEDOOMISH+2]),
	S_NORMAL (TNT1, 'A',    1, A_RestoreSpecialDoomThing	, NULL),

#define S_HIDESPECIAL (S_HIDEDOOMISH+3)
	S_NORMAL (ACLO, 'E', 1400, NULL                         , &States[S_HIDESPECIAL+1]),
	S_NORMAL (ACLO, 'A',	0, A_RestoreSpecialPosition		, &States[S_HIDESPECIAL+2]),
	S_NORMAL (ACLO, 'A',    4, A_RestoreSpecialThing1       , &States[S_HIDESPECIAL+3]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+4]),
	S_NORMAL (ACLO, 'A',    4, NULL                         , &States[S_HIDESPECIAL+5]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+6]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+7]),
	S_NORMAL (ACLO, 'B',    4, NULL                         , &States[S_HIDESPECIAL+8]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+9]),
	S_NORMAL (ACLO, 'D',    4, NULL                         , &States[S_HIDESPECIAL+10]),
	S_NORMAL (ACLO, 'C',    4, NULL                         , &States[S_HIDESPECIAL+11]),
	S_NORMAL (ACLO, 'D',    4, A_RestoreSpecialThing2       , NULL),

#define S_HELD (S_HIDESPECIAL+12)
	S_NORMAL (TNT1, 'A',   -1, NULL							, NULL),

#define S_HOLDANDDESTROY (S_HELD+1)
	S_NORMAL (TNT1, 'A',	1, NULL							, NULL),
};

int AInventory::StaticLastMessageTic;
const char *AInventory::StaticLastMessage;

IMPLEMENT_POINTY_CLASS (AInventory)
 DECLARE_POINTER (Owner)
END_POINTERS

BEGIN_DEFAULTS (AInventory, Any, -1, 0)
	PROP_Inventory_Amount (1)
	PROP_Inventory_MaxAmount (1)
	PROP_UseSound ("misc/invuse")
END_DEFAULTS

//===========================================================================
//
// AInventory :: Tick
//
//===========================================================================

void AInventory::Tick ()
{
	Super::Tick ();
	if (DropTime)
	{
		if (--DropTime == 0)
		{
			flags |= GetDefault()->flags & (MF_SPECIAL|MF_SOLID);
		}
	}
}

//===========================================================================
//
// AInventory :: Serialize
//
//===========================================================================

void AInventory::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Owner << Amount << MaxAmount << RespawnTics << ItemFlags;
	if (arc.IsStoring ())
	{
		TexMan.WriteTexture (arc, Icon);
	}
	else
	{
		Icon = TexMan.ReadTexture (arc);
	}
}

//===========================================================================
//
// AInventory :: SpecialDropAction
//
// Called by P_DropItem. Return true to prevent the standard drop tossing.
// A few Strife items that are meant to trigger actions rather than be
// picked up use this. Normal items shouldn't need it.
//
//===========================================================================

bool AInventory::SpecialDropAction (AActor *dropper)
{
	return false;
}

//===========================================================================
//
// AInventory :: ShouldRespawn
//
// Returns true if the item should hide itself and reappear later when picked
// up.
//
//===========================================================================

bool AInventory::ShouldRespawn ()
{
	return (multiplayer || alwaysapplydmflags) && 
		   (dmflags & DF_ITEMS_RESPAWN);
}

//===========================================================================
//
// AInventory :: BeginPlay
//
//===========================================================================

void AInventory::BeginPlay ()
{
	Super::BeginPlay ();
	ChangeStatNum (STAT_INVENTORY);
	flags |= MF_DROPPED;	// [RH] Items are dropped by default
}

//===========================================================================
//
// AInventory :: HandlePickup
//
// Returns true if the pickup was handled (or should not happen at all),
// false if not.
//
//===========================================================================

bool AInventory::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		if (Amount < MaxAmount)
		{
			Amount += item->Amount;
			if (Amount > MaxAmount)
			{
				Amount = MaxAmount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AInventory :: GoAway
//
// Returns true if you must create a copy of this item to give to the player
// or false if you can use this one instead.
//
//===========================================================================

bool AInventory::GoAway ()
{
	// Dropped items never stick around
	if (flags & MF_DROPPED)
	{
		if (ItemFlags & IF_PICKUPFLASH)
		{
			Spawn<APickupFlash> (x, y, z);
		}
		return false;
	}

	if (!ShouldStay ())
	{
		if (ItemFlags & IF_PICKUPFLASH)
		{
			Spawn<APickupFlash> (x, y, z);
		}
		Hide ();
		if (ShouldRespawn ())
		{
			return true;
		}
		return false;
	}
	return true;
}

//===========================================================================
//
// AInventory :: GoAwayAndDie
//
// Like GoAway but used by items that don't insert themselves into the
// inventory. If they won't be respawning, then they can destroy themselves.
//
//===========================================================================

void AInventory::GoAwayAndDie ()
{
	if (!GoAway ())
	{
		flags &= ~MF_SPECIAL;
		SetState (&States[S_HOLDANDDESTROY]);
	}
}

//===========================================================================
//
// AInventory :: CreateCopy
//
// Returns an actor suitable for placing in an inventory, either itself or
// a copy based on whether it needs to respawn or not. Returning NULL
// indicates the item should not be picked up.
//
//===========================================================================

AInventory *AInventory::CreateCopy (AActor *other)
{
	AInventory *copy;

	if (GoAway ())
	{
		copy = static_cast<AInventory *>(Spawn (GetClass(), 0, 0, 0));
		copy->Amount = Amount;
		copy->MaxAmount = MaxAmount;
	}
	else
	{
		copy = this;
	}
	return copy;
}

//===========================================================================
//
// AInventory::CreateTossable
//
// Creates a copy of the item suitable for dropping. If this actor embodies
// only one item, then it is tossed out itself. Otherwise, the count drops
// by one and a new item with an amount of 1 is spawned.
//
//===========================================================================

AInventory *AInventory::CreateTossable ()
{
	AInventory *copy;

	// If this actor lacks a SpawnState, don't drop it. (e.g. A base weapon
	// like the fist can't be dropped because you'll never see it.)
	if (SpawnState == &AActor::States[AActor::S_NULL] ||
		SpawnState == NULL)
	{
		return NULL;
	}
	if ((ItemFlags & IF_UNDROPPABLE) || Owner == NULL || Amount <= 0)
	{
		return NULL;
	}
	if (Amount == 1 && !IsKindOf (RUNTIME_CLASS(AAmmo)))
	{
		BecomePickup ();
		DropTime = 30;
		flags &= ~(MF_SPECIAL|MF_SOLID);
		return this;
	}
	copy = static_cast<AInventory *>(Spawn (GetClass(), Owner->x,
		Owner->y, Owner->z));
	if (copy != NULL)
	{
		copy->MaxAmount = MaxAmount;
		copy->Amount = 1;
		Amount--;
	}
	copy->DropTime = 30;
	copy->flags &= ~(MF_SPECIAL|MF_SOLID);
	return copy;
}

//===========================================================================
//
// AInventory :: BecomeItem
//
// Lets this actor know that it's about to be placed in an inventory.
//
//===========================================================================

void AInventory::BecomeItem ()
{
	if (!(flags & (MF_NOBLOCKMAP|MF_NOSECTOR)))
	{
		UnlinkFromWorld ();
		flags |= MF_NOBLOCKMAP|MF_NOSECTOR;
		LinkToWorld ();
	}
	flags &= ~MF_SPECIAL;
	SetState (&States[S_HELD]);
}

//===========================================================================
//
// AInventory :: BecomePickup
//
// Lets this actor know it should wait to be picked up.
//
//===========================================================================

void AInventory::BecomePickup ()
{
	if (Owner != NULL)
	{
		Owner->RemoveInventory (this);
	}
	if (flags & (MF_NOBLOCKMAP|MF_NOSECTOR))
	{
		UnlinkFromWorld ();
		flags &= ~(MF_NOBLOCKMAP|MF_NOSECTOR);
		LinkToWorld ();
		P_FindFloorCeiling (this);
	}
	flags = GetDefault()->flags | MF_DROPPED;
	renderflags &= ~RF_INVISIBLE;
	SetState (SpawnState);
}

//===========================================================================
//
// AInventory :: AbsorbDamage
//
// Allows inventory items (primarily armor) to reduce the amount of damage
// taken. Damage is the amount of damage that would be done without armor,
// and newdamage is the amount that should be done after the armor absorbs
// it.
//
//===========================================================================

void AInventory::AbsorbDamage (int damage, int &newdamage)
{
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, newdamage);
	}
}

//===========================================================================
//
// AInventory :: AlterWeaponSprite
//
// Allows inventory items to alter a player's weapon sprite just before it
// is drawn.
//
//===========================================================================

void AInventory::AlterWeaponSprite (vissprite_t *vis)
{
	if (Inventory != NULL)
	{
		Inventory->AlterWeaponSprite (vis);
	}
}

//===========================================================================
//
// AInventory :: Activate
//
//===========================================================================

bool AInventory::Use ()
{
	return false;
}

//===========================================================================
//
// AInventory :: Hide
//
// Hides this actor until it's time to respawn again. 
//
//===========================================================================

void AInventory::Hide ()
{
 	flags = (flags & ~MF_SPECIAL) | MF_NOGRAVITY;
	renderflags |= RF_INVISIBLE;
	if (gameinfo.gametype & GAME_Raven)
	{
		SetState (&States[S_HIDESPECIAL]);
		tics = 1400;
	}
	else
	{
		SetState (&States[S_HIDEDOOMISH]);
		tics = 1050;
	}
	if (RespawnTics != 0)
	{
		tics = RespawnTics;
	}
}

//===========================================================================
//
// AInventory :: Touch
//
// Handles collisions from another actor, possible adding itself to the
// collider's inventory.
//
//===========================================================================

void AInventory::Touch (AActor *toucher)
{
	if (!TryPickup (toucher))
		return;

	if (!(ItemFlags & IF_QUIET))
	{
		const char *message = GetClass()->Meta.GetMetaString (AIMETA_PickupMessage);
		if (message == NULL)
		{
			message = PickupMessage ();
		}

		if (toucher == players[consoleplayer].camera
			&& (StaticLastMessageTic != gametic || StaticLastMessage != message))
		{
			StaticLastMessageTic = gametic;
			StaticLastMessage = message;
			Printf (PRINT_LOW, "%s\n", message);
			StatusBar->FlashCrosshair ();
		}

		// Special check so voodoo dolls picking up items cause the
		// real player to make noise.
		if (toucher->player)
			DoPlayPickupSound (toucher->player->mo);
		else
			DoPlayPickupSound (toucher);

		toucher->player->bonuscount = BONUSADD;
	}

	// [RH] Execute an attached special (if any)
	DoPickupSpecial (toucher);

	if (flags & MF_COUNTITEM)
	{
		toucher->player->itemcount++;
		level.found_items++;
	}

	//Added by MC: Check if item taken was the roam destination of any bot
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && this == players[i].dest)
    		players[i].dest = NULL;
	}
}

//===========================================================================
//
// AInventory :: DoPickupSpecial
//
// Executes this actor's special when it is picked up.
//
//===========================================================================

void AInventory::DoPickupSpecial (AActor *toucher)
{
	if (special)
	{
		LineSpecials[special] (NULL, toucher, false,
			args[0], args[1], args[2], args[3], args[4]);
		special = 0;
	}
}

//===========================================================================
//
// AInventory :: PickupMessage
//
// Returns the message to print when this actor is picked up.
//
//===========================================================================

const char *AInventory::PickupMessage ()
{
	return "You got a pickup";
}

//===========================================================================
//
// AInventory :: DoPlayPickupSound
//
// Plays a sound when this actor is picked up.
//
//===========================================================================

void AInventory::DoPlayPickupSound (AActor *toucher)
{
	int pickupsound = GetClass()->Meta.GetMetaInt (AIMETA_PickupSound);
	if (pickupsound != 0)
	{
		S_SoundID (toucher, CHAN_PICKUP, pickupsound, 1, ATTN_NORM);
	}
	else
	{
		PlayPickupSound (toucher);
	}
}

//===========================================================================
//
// AInventory :: PlayPickupSound
//
//===========================================================================

void AInventory::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/i_pkup", 1, ATTN_NORM);
}

//===========================================================================
//
// AInventory :: ShouldStay
//
// Returns true if the item should not disappear, even temporarily.
//
//===========================================================================

bool AInventory::ShouldStay ()
{
	return false;
}

//===========================================================================
//
// AInventory :: Destroy
//
//===========================================================================

void AInventory::Destroy ()
{
	if (Owner != NULL)
	{
		Owner->RemoveInventory (this);
	}
	Inventory = NULL;
	Super::Destroy ();
}

//===========================================================================
//
// AInventory :: GetBlend
//
// Returns a color to blend to the player's view as long as they possess this
// item.
//
//===========================================================================

PalEntry AInventory::GetBlend ()
{
	return 0;
}

//===========================================================================
//
// AInventory :: PrevItem
//
// Returns the previous item.
//
//===========================================================================

AInventory *AInventory::PrevItem () const
{
	AInventory *item = Owner->Inventory;

	while (item != NULL && item->Inventory != this)
	{
		item = item->Inventory;
	}
	return item;
}

//===========================================================================
//
// AInventory :: PrevInv
//
// Returns the previous item with IF_INVBAR set.
//
//===========================================================================

AInventory *AInventory::PrevInv () const
{
	AInventory *lastgood = NULL;
	AInventory *item = Owner->Inventory;

	while (item != NULL && item != this)
	{
		if (item->ItemFlags & IF_INVBAR)
		{
			lastgood = item;
		}
		item = item->Inventory;
	}
	return lastgood;
}
//===========================================================================
//
// AInventory :: NextInv
//
// Returns the next item with IF_INVBAR set.
//
//===========================================================================

AInventory *AInventory::NextInv () const
{
	AInventory *item = Inventory;

	while (item != NULL && !(item->ItemFlags & IF_INVBAR))
	{
		item = item->Inventory;
	}
	return item;
}

//===========================================================================
//
// AInventory :: DrawPowerup
//
// Gives this item a chance to draw a special status indicator on the screen.
// Returns false if it didn't draw anything.
//
//===========================================================================

bool AInventory::DrawPowerup (int x, int y)
{
	return false;
}

/***************************************************************************/
/* AArtifact implementation												   */
/***************************************************************************/

IMPLEMENT_STATELESS_ACTOR (APowerupGiver, Any, -1, 0)
	PROP_Inventory_RespawnTics (30+1400)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR)
END_DEFAULTS

//===========================================================================
//
// APowerupGiver :: PlayPickupSound
//
//===========================================================================

void APowerupGiver::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/p_pkup", 1,
		toucher == NULL || toucher == players[consoleplayer].camera
		? ATTN_SURROUND : ATTN_NORM);
}

//===========================================================================
//
// AInventory :: DoRespawn
//
//===========================================================================

bool AInventory::DoRespawn ()
{
	return true;
}

//===========================================================================
//
// AInventory :: TryPickup
//
//===========================================================================

bool AInventory::TryPickup (AActor *toucher)
{
	// If HandlePickup() returns true, it will set the IF_PICKUPGOOD flag
	// to indicate that this item has been picked up. If the item cannot be
	// picked up, then it leaves the flag cleared.

	ItemFlags &= ~IF_PICKUPGOOD;
	if (toucher->Inventory != NULL && toucher->Inventory->HandlePickup (this))
	{
		// Let something else the player is holding intercept the pickup.
		if (!(ItemFlags & IF_PICKUPGOOD))
		{
			return false;
		}
		ItemFlags &= ~IF_PICKUPGOOD;
		GoAwayAndDie ();
	}
	else if (MaxAmount == 0)
	{
		// Special case: If an item's MaxAmount is 0, you can still pick it
		// up if it is autoactivate-able.
		if (!(ItemFlags & IF_AUTOACTIVATE))
		{
			return false;
		}
		// The item is placed in the inventory just long enough to be used.
		toucher->AddInventory (this);
		bool usegood = Use ();
		toucher->RemoveInventory (this);

		if (usegood || (ItemFlags & IF_ALWAYSPICKUP))
		{
			GoAwayAndDie ();
		}
		else
		{
			return false;
		}
	}
	else
	{
		// Add the item to the inventory. It is not already there, or HandlePickup
		// would have already taken care of it.
		AInventory *copy = CreateCopy (toucher);
		if (copy == NULL)
		{
			return false;
		}
		copy->AttachToOwner (toucher);
		if (ItemFlags & IF_AUTOACTIVATE)
		{
			if (copy->Use ())
			{
				if (--copy->Amount <= 0)
				{
					flags &= ~MF_SPECIAL;
					SetState (&States[S_HOLDANDDESTROY]);
				}
			}
		}
	}
	return true;
}

//===========================================================================
//
// CCMD printinv
//
// Prints the console player's current inventory.
//
//===========================================================================

CCMD (printinv)
{
	AInventory *item;

	for (item = players[consoleplayer].mo->Inventory; item != NULL; item = item->Inventory)
	{
		Printf ("%s #%lu (%d/%d)\n", item->GetClass()->Name+1, item->InventoryID, item->Amount, item->MaxAmount);
	}
}

//===========================================================================
//
// AInventory :: AttachToOwner
//
//===========================================================================

void AInventory::AttachToOwner (AActor *other)
{
	BecomeItem ();
	other->AddInventory (this);
}

//===========================================================================
//
// AInventory :: DetachFromOwner
//
// Performs any special work needed when the item leaves an inventory,
// either through destruction or becoming a pickup.
//
//===========================================================================

void AInventory::DetachFromOwner ()
{
}

IMPLEMENT_ABSTRACT_ACTOR (AArmor)

IMPLEMENT_STATELESS_ACTOR (ABasicArmor, Any, -1, 0)
	PROP_SpawnState (S_HELD)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABasicArmorPickup, Any, -1, 0)
	PROP_Inventory_MaxAmount (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (ABasicArmorBonus, Any, -1, 0)
	PROP_Inventory_MaxAmount (0)
	PROP_Inventory_FlagsSet (IF_AUTOACTIVATE|IF_ALWAYSPICKUP)
	PROP_BasicArmorBonus_SavePercent (FRACUNIT/3)
END_DEFAULTS

IMPLEMENT_STATELESS_ACTOR (AHexenArmor, Any, -1, 0)
	PROP_HexenArmor_ArmorAmount (0)
END_DEFAULTS


//===========================================================================
//
// ABasicArmorPickup :: Serialize
//
//===========================================================================

void ABasicArmorPickup::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent << SaveAmount;
	if (SaveVersion >= 222)
	{
		arc << DropTime;
	}
}

//===========================================================================
//
// ABasicArmorPickup :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorPickup::CreateCopy (AActor *other)
{
	ABasicArmorPickup *copy = static_cast<ABasicArmorPickup *> (Super::CreateCopy (other));
	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	return copy;
}

//===========================================================================
//
// ABasicArmorPickup :: Use
//
// Either gives you new armor or replaces the armor you already have (if
// the SaveAmount is greater than the amount of armor you own).
//
//===========================================================================

bool ABasicArmorPickup::Use ()
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();

	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> (0,0,0);
		armor->SavePercent = SavePercent;
		armor->Amount = armor->MaxAmount = SaveAmount;
		armor->Icon = Icon;
		Owner->AddInventory (armor);
		return true;
	}
	// If you already have more armor than this item gives you, you can't
	// use it.
	if (armor->Amount >= SaveAmount)
	{
		return false;
	}
	armor->SavePercent = SavePercent;
	armor->Amount = armor->MaxAmount = SaveAmount;
	armor->Icon = Icon;
	return true;
}

//===========================================================================
//
// ABasicArmorPickup :: Serialize
//
//===========================================================================

void ABasicArmorBonus::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent << SaveAmount << MaxSaveAmount;
}

//===========================================================================
//
// ABasicArmorBonus :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmorBonus::CreateCopy (AActor *other)
{
	ABasicArmorBonus *copy = static_cast<ABasicArmorBonus *> (Super::CreateCopy (other));
	copy->SavePercent = SavePercent;
	copy->SaveAmount = SaveAmount;
	copy->MaxSaveAmount = MaxSaveAmount;
	return copy;
}

//===========================================================================
//
// ABasicArmorBonus :: Use
//
// Tries to add to the amount of BasicArmor a player has.
//
//===========================================================================

bool ABasicArmorBonus::Use ()
{
	ABasicArmor *armor = Owner->FindInventory<ABasicArmor> ();
	int saveAmount = MIN (SaveAmount, MaxSaveAmount);

	if (saveAmount <= 0)
	{ // If it can't give you anything, it's as good as used.
		return true;
	}
	if (armor == NULL)
	{
		armor = Spawn<ABasicArmor> (0,0,0);
		armor->SavePercent = SavePercent;
		armor->Amount = saveAmount;
		armor->MaxAmount = MaxSaveAmount;
		armor->Icon = Icon;
		Owner->AddInventory (armor);
		return true;
	}
	// If you already have more armor than this item can give you, you can't
	// use it.
	if (armor->Amount >= MaxSaveAmount)
	{
		return false;
	}
	armor->Amount += saveAmount;
	armor->MaxAmount = MAX (armor->MaxAmount, MaxSaveAmount);
	return true;
}

//===========================================================================
//
// ABasicArmor :: Serialize
//
//===========================================================================

void ABasicArmor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << SavePercent;
}

//===========================================================================
//
// ABasicArmor :: PostBeginPlay
//
// If BasicArmor is given to the player by means other than a
// BasicArmorPickup, then it may not have an icon set. Fix that here.
//
//===========================================================================

void ABasicArmor::PostBeginPlay ()
{
	if (Icon == 0)
	{
		switch (gameinfo.gametype)
		{
		case GAME_Doom:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/3 ? "ARM1A0" : "ARM2A0", FTexture::TEX_Any);
			break;

		case GAME_Heretic:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/2 ? "SHLDA0" : "SHD2A0", FTexture::TEX_Any);
			break;

		case GAME_Strife:
			Icon = TexMan.CheckForTexture (SavePercent == FRACUNIT/3 ? "I_ARM2" : "I_ARM1", FTexture::TEX_Any);
			break;
		
		default:
			break;
		}
	}
}

//===========================================================================
//
// ABasicArmor :: CreateCopy
//
//===========================================================================

AInventory *ABasicArmor::CreateCopy (AActor *other)
{
	// BasicArmor that is in use is stored in the inventory as BasicArmor.
	// BasicArmor that is in reserve is not.
	ABasicArmor *copy = Spawn<ABasicArmor> (0, 0, 0);
	copy->SavePercent = SavePercent != 0 ? SavePercent : FRACUNIT/3;
	copy->Amount = Amount;
	copy->MaxAmount = MaxAmount;
	copy->Icon = Icon;
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// ABasicArmor :: HandlePickup
//
//===========================================================================

bool ABasicArmor::HandlePickup (AInventory *item)
{
	if (item->GetClass() == RUNTIME_CLASS(ABasicArmor))
	{
		// You shouldn't be picking up BasicArmor anyway.
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// ABasicArmor :: AbsorbDamage
//
//===========================================================================

void ABasicArmor::AbsorbDamage (int damage, int &newdamage)
{
	int saved = FixedMul (damage, SavePercent);
	if (Amount < saved)
	{
		saved = Amount;
	}
	newdamage -= saved;
	Amount -= saved;
	if (Amount == 0)
	{
		// The armor has become useless
		SavePercent = 0;
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, newdamage);
	}
}

//===========================================================================
//
// AHexenArmor :: Serialize
//
//===========================================================================

void AHexenArmor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Slots[0] << Slots[1] << Slots[2] << Slots[3];
}

//===========================================================================
//
// AHexenArmor :: CreateCopy
//
//===========================================================================

AInventory *AHexenArmor::CreateCopy (AActor *other)
{
	// Like BasicArmor, HexenArmor is used in the inventory but not the map.
	// health is the slot this armor occupies.
	// Amount is the quantity to give (0 = normal max).
	AHexenArmor *copy = Spawn<AHexenArmor> (0, 0, 0);
	copy->AddArmorToSlot (other, health, Amount);
	GoAwayAndDie ();
	return copy;
}

//===========================================================================
//
// AHexenArmor :: CreateTossable
//
// Since this isn't really a single item, you can't drop it. Ever.
//
//===========================================================================

AInventory *AHexenArmor::CreateTossable ()
{
	return NULL;
}

//===========================================================================
//
// AHexenArmor :: HandlePickup
//
//===========================================================================

bool AHexenArmor::HandlePickup (AInventory *item)
{
	if (item->IsKindOf (RUNTIME_CLASS(AHexenArmor)))
	{
		if (AddArmorToSlot (Owner, item->health, item->Amount))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		return true;
	}
	else if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AddArmorToSlot
//
//===========================================================================

bool AHexenArmor::AddArmorToSlot (AActor *actor, int slot, int amount)
{
	APlayerPawn *ppawn;
	int hits;

	if (actor->player != NULL)
	{
		ppawn = static_cast<APlayerPawn *>(actor);
	}
	else
	{
		ppawn = NULL;
	}

	if (slot < 0 || slot > 3)
	{
		return false;
	}
	if (amount <= 0)
	{
		hits = ppawn != NULL ? ppawn->GetArmorIncrement (slot) : 5*FRACUNIT;
		if (Slots[slot] < hits)
		{
			Slots[slot] = hits;
			return true;
		}
	}
	else if (ppawn != NULL)
	{
		hits = amount * 5 * FRACUNIT;
		fixed_t total = Slots[0]+Slots[1]+Slots[2]+Slots[3]+ppawn->GetAutoArmorSave();
		if (total < ppawn->GetArmorMax()*5*FRACUNIT)
		{
			Slots[slot] += hits;
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// AHexenArmor :: AbsorbDamage
//
//===========================================================================

void AHexenArmor::AbsorbDamage (int damage, int &newdamage)
{
	fixed_t savedPercent = Slots[0] + Slots[1] + Slots[2] + Slots[3];
	APlayerPawn *ppawn = Owner->player != NULL ? Owner->player->mo : NULL;

	if (ppawn != NULL)
	{
		savedPercent += ppawn->GetAutoArmorSave ();
	}
	if (savedPercent)
	{ // armor absorbed some damage
		if (savedPercent > 100*FRACUNIT)
		{
			savedPercent = 100*FRACUNIT;
		}
		for (int i = 0; i < 4; i++)
		{
			if (Slots[i])
			{
				// 300 damage always wipes out the armor unless some was added
				// with the dragon skin bracers.
				if (damage < 10000)
				{
					Slots[i] -= 
						Scale (damage, ppawn != NULL ? ppawn->GetArmorIncrement (i) : 5*FRACUNIT, 300);
					if (Slots[i] < 2*FRACUNIT)
					{
						Slots[i] = 0;
					}
				}
				else
				{
					Slots[i] = 0;
				}
			}
		}
		int saved = Scale (damage, savedPercent, 100*FRACUNIT);
		if (saved > savedPercent >> (FRACBITS-1))
		{	
			saved = savedPercent >> (FRACBITS-1);
		}
		newdamage -= saved;
	}
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, newdamage);
	}
}

IMPLEMENT_STATELESS_ACTOR (AHealth, Any, -1, 0)
	PROP_Inventory_MaxAmount (0)
END_DEFAULTS

//===========================================================================
//
// AHealth :: PlayPickupSound
//
//===========================================================================

void AHealth::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/health_pkup", 1, ATTN_NORM);
}

//===========================================================================
//
// AHealth :: TryPickup
//
//===========================================================================

bool AHealth::TryPickup (AActor *other)
{
	player_t *player = other->player;
	int max = MaxAmount;
	
	if (max == 0)
	{
		max = MAXHEALTH + (player != NULL ? player->stamina : 0);
		if (player->morphTics)
		{
			max = MAXMORPHHEALTH;
		}
	}
	if (player->health >= max)
	{
		// You should be able to pick up the Doom health bonus even if
		// you are already full on health.
		if (ItemFlags & IF_ALWAYSPICKUP)
		{
			GoAwayAndDie ();
			return true;
		}
		return false;
	}
	player->health += Amount;
	if (player->health > max)
	{
		player->health = max;
	}
	player->mo->health = player->health;
	GoAwayAndDie ();
	return true;
}

IMPLEMENT_STATELESS_ACTOR (AHealthPickup, Any, -1, 0)
	PROP_Inventory_DefMaxAmount
	PROP_Inventory_FlagsSet (IF_INVBAR)
END_DEFAULTS

//===========================================================================
//
// AHealthPickup :: CreateCopy
//
//===========================================================================

AInventory *AHealthPickup::CreateCopy (AActor *other)
{
	AInventory *copy = Super::CreateCopy (other);
	copy->health = health;
	return copy;
}

//===========================================================================
//
// AHealthPickup :: CreateTossable
//
//===========================================================================

AInventory *AHealthPickup::CreateTossable ()
{
	AInventory *copy = Super::CreateTossable ();
	if (copy != NULL)
	{
		copy->health = health;
	}
	return copy;
}

//===========================================================================
//
// AHealthPickup :: HandlePickup
//
//===========================================================================

bool AHealthPickup::HandlePickup (AInventory *item)
{
	// HealthPickups that are the same type but have different health amounts
	// do not count as the same item.
	if (item->health == health)
	{
		return Super::HandlePickup (item);
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// AHealthPickup :: Use
//
//===========================================================================

bool AHealthPickup::Use ()
{
	return P_GiveBody (Owner->player, health);
}

// Backpack -----------------------------------------------------------------

//===========================================================================
//
// ABackpack :: TryPickup
//
//===========================================================================

bool ABackpack::TryPickup (AActor *other)
{
	// Find every unique type of ammo. Give it to the player if
	// he doesn't have it already, and double it's maximum capacity.
	for (int i = 0; i < TypeInfo::m_NumTypes; ++i)
	{
		const TypeInfo *type = TypeInfo::m_Types[i];

		if (type->ParentType == RUNTIME_CLASS(AAmmo) &&
			((AAmmo *)GetDefaultByType (type))->BackpackAmount > 0)
		{
			AAmmo *ammo = static_cast<AAmmo *>(other->FindInventory (type));
			if (ammo == NULL)
			{
				ammo = static_cast<AAmmo *>(Spawn (type, 0, 0, 0));
				ammo->Amount = ammo->BackpackAmount;
				ammo->MaxAmount = ammo->BackpackMaxAmount;
				ammo->AttachToOwner (other);
			}
			else
			{
				if (ammo->MaxAmount < ammo->BackpackMaxAmount)
				{
					ammo->MaxAmount = ammo->BackpackMaxAmount;
				}
				if (ammo->Amount < ammo->MaxAmount)
				{
					ammo->Amount += static_cast<AAmmo*>(ammo->GetDefault())->BackpackAmount;
					if (ammo->Amount > ammo->MaxAmount)
					{
						ammo->Amount = ammo->MaxAmount;
					}
				}
			}
		}
	}
	GoAwayAndDie ();
	return true;
}

//===========================================================================
//
// ABackpack :: DetachFromOwner
//
//===========================================================================

void ABackpack::DetachFromOwner ()
{
	// When removing a backpack, drop the player's ammo maximums to normal
	AInventory *item;

	for (item = Owner->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->GetClass()->ParentType == RUNTIME_CLASS(AAmmo))
		{
			item->MaxAmount = static_cast<AInventory*>(item->GetDefault())->MaxAmount;
			if (item->Amount > item->MaxAmount)
			{
				item->Amount = item->MaxAmount;
			}
		}
	}
}

//===========================================================================
//
// ABackpack :: PickupMessage
//
//===========================================================================

const char *ABackpack::PickupMessage ()
{
	return GStrings(GOTBACKPACK);
}

FState ABackpack::States[] =
{
	S_NORMAL (BPAK, 'A',   -1, NULL 						, NULL)
};

IMPLEMENT_ACTOR (ABackpack, Doom, 8, 144)
	PROP_HeightFixed (26)
	PROP_Flags (MF_SPECIAL)
	PROP_SpawnState (0)
END_DEFAULTS

IMPLEMENT_ABSTRACT_ACTOR (AMapRevealer)

//===========================================================================
//
// AMapRevealer :: TryPickup
//
// The MapRevealer doesn't actually go in your inventory. Instead, it sets
// a flag on the level.
//
//===========================================================================

bool AMapRevealer::TryPickup (AActor *toucher)
{
	level.flags |= LEVEL_ALLMAP;
	GoAwayAndDie ();
	return true;
}

FState ACommunicator::States[] =
{
	S_NORMAL (COMM, 'A', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (ACommunicator, Strife, 206, 0)
	PROP_Flags (MF_SPECIAL|MF_NOTDMATCH)
	PROP_SpawnState (0)
	PROP_StrifeType (176)
	PROP_StrifeTeaserType (168)
	PROP_Inventory_Icon ("I_COMM")
	PROP_Tag ("Communicator")
END_DEFAULTS

//===========================================================================
//
// ACommunicator :: PickupMessage
//
//===========================================================================

const char *ACommunicator::PickupMessage ()
{
	return "You picked up the Communicator";
}
