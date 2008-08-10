#include <assert.h>

#include "info.h"
#include "m_random.h"
#include "p_local.h"
#include "s_sound.h"
#include "gi.h"
#include "p_lnspec.h"
#include "sbar.h"
#include "statnums.h"
#include "c_dispatch.h"
#include "gstrings.h"
#include "templates.h"
#include "a_strifeglobal.h"
#include "a_morph.h"
#include "a_specialspot.h"

static FRandom pr_restore ("RestorePos");

IMPLEMENT_CLASS (AAmmo)

//===========================================================================
//
// AAmmo :: Serialize
//
//===========================================================================

void AAmmo::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << BackpackAmount << BackpackMaxAmount;
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

const PClass *AAmmo::GetParentAmmo () const
{
	const PClass *type = GetClass ();

	while (type->ParentClass != RUNTIME_CLASS(AAmmo))
	{
		type = type->ParentClass;
	}
	return type;
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
			int receiving = item->Amount;

			if (!(item->ItemFlags & IF_IGNORESKILL))
			{ // extra ammo in baby mode and nightmare mode
				receiving = FixedMul(receiving, G_SkillProperty(SKILLP_AmmoFactor));
			}
			int oldamount = Amount;

			Amount += receiving;
			if (Amount > MaxAmount)
			{
				Amount = MaxAmount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;

			// If the player previously had this ammo but ran out, possibly switch
			// to a weapon that uses it, but only if the player doesn't already
			// have a weapon pending.

			assert (Owner != NULL);

			if (oldamount == 0 && Owner != NULL && Owner->player != NULL &&
				!Owner->player->userinfo.neverswitch &&
				Owner->player->PendingWeapon == WP_NOCHANGE && 
				(Owner->player->ReadyWeapon == NULL ||
				 (Owner->player->ReadyWeapon->WeaponFlags & WIF_WIMPY_WEAPON)))
			{
				AWeapon *best = barrier_cast<APlayerPawn *>(Owner)->BestWeapon (GetClass());
				if (best != NULL && (Owner->player->ReadyWeapon == NULL ||
					best->SelectionOrder < Owner->player->ReadyWeapon->SelectionOrder))
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
	int amount = Amount;

	// extra ammo in baby mode and nightmare mode
	if (!(ItemFlags&IF_IGNORESKILL))
	{
		amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
	}

	if (GetClass()->ParentClass != RUNTIME_CLASS(AAmmo))
	{
		const PClass *type = GetParentAmmo();
		assert (type->ActorInfo != NULL);
		if (!GoAway ())
		{
			Destroy ();
		}

		copy = static_cast<AInventory *>(Spawn (type, 0, 0, 0, NO_REPLACE));
		copy->Amount = amount;
		copy->BecomeItem ();
	}
	else
	{
		copy = Super::CreateCopy (other);
		copy->Amount = amount;
	}
	if (copy->Amount > copy->MaxAmount)
	{ // Don't pick up more ammo than you're supposed to be able to carry.
		copy->Amount = copy->MaxAmount;
	}
	return copy;
}

//===========================================================================
//
// AAmmo :: CreateTossable
//
//===========================================================================

AInventory *AAmmo::CreateTossable()
{
	AInventory *copy = Super::CreateTossable();
	if (copy != NULL)
	{ // Do not increase ammo by dropping it and picking it back up at
	  // certain skill levels.
		copy->ItemFlags |= IF_IGNORESKILL;
	}
	return copy;
}

//---------------------------------------------------------------------------
//
// FUNC P_GiveBody
//
// Returns false if the body isn't needed at all.
//
//---------------------------------------------------------------------------

bool P_GiveBody (AActor *actor, int num)
{
	int max;
	player_t *player = actor->player;

	if (player != NULL)
	{
		max = static_cast<APlayerPawn*>(actor)->GetMaxHealth() + player->stamina;
		// [MH] First step in predictable generic morph effects
 		if (player->morphTics)
 		{
			if (player->MorphStyle & MORPH_FULLHEALTH)
			{
				if (!(player->MorphStyle & MORPH_ADDSTAMINA))
				{
					max -= player->stamina;
				}
			}
			else // old health behaviour
			{
				max = MAXMORPHHEALTH;
				if (player->MorphStyle & MORPH_ADDSTAMINA)
				{
					max += player->stamina;
				}
			}
 		}
		// [RH] For Strife: A negative body sets you up with a percentage
		// of your full health.
		if (num < 0)
		{
			num = max * -num / 100;
			if (player->health < num)
			{
				player->health = num;
				actor->health = num;
				return true;
			}
		}
		else
		{
			if (player->health < max)
			{
				player->health += num;
				if (player->health > max)
				{
					player->health = max;
				}
				actor->health = player->health;
				return true;
			}
		}
	}
	else
	{
		max = actor->GetDefault()->health;
		if (num < 0)
		{
			num = max * -num / 100;
			if (actor->health < num)
			{
				actor->health = num;
				return true;
			}
		}
		else if (actor->health < max)
		{
			actor->health += num;
			if (actor->health > max)
			{
				actor->health = max;
			}
			return true;
		}
	}
	return false;
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
		Spawn ("ItemFog", self->x, self->y, self->z, ALLOW_REPLACE);
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

	_x = self->SpawnPoint[0];
	_y = self->SpawnPoint[1];
	sec = P_PointInSector (_x, _y);

	self->SetOrigin (_x, _y, sec->floorplane.ZatPoint (_x, _y));
	P_CheckPosition (self, _x, _y);

	if (self->flags & MF_SPAWNCEILING)
	{
		self->z = self->ceilingz - self->height - self->SpawnPoint[2];
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
		self->z = self->SpawnPoint[2] + self->floorz;
		if (self->flags2 & MF2_FLOATBOB)
		{
			self->z += FloatBobOffsets[(self->FloatBobPhase + level.maptime) & 63];
		}
	}
}

int AInventory::StaticLastMessageTic;
const char *AInventory::StaticLastMessage;

IMPLEMENT_POINTY_CLASS (AInventory)
 DECLARE_POINTER (Owner)
END_POINTERS

//===========================================================================
//
// AInventory :: Tick
//
//===========================================================================

void AInventory::Tick ()
{
	if (Owner == NULL)
	{
		// AActor::Tick is only handling interaction with the world
		// and we don't want that for owned inventory items.
		Super::Tick ();
	}
	else if (tics != -1)	// ... but at least we have to advance the states
	{
		tics--;
				
		// you can cycle through multiple states in a tic
		// [RH] Use <= 0 instead of == 0 so that spawnstates
		// of 0 tics work as expected.
		if (tics <= 0)
		{
			assert (state != NULL);
			if (state == NULL)
			{
				Destroy();
				return;
			}
			if (!SetState (state->GetNextState()))
				return; 		// freed itself
		}
	}
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
	arc << Owner << Amount << MaxAmount << RespawnTics << ItemFlags << Icon << PickupSound << SpawnPointClass;
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
	if ((ItemFlags & IF_BIGPOWERUP) && !(dmflags & DF_RESPAWN_SUPER)) return false;
	return !!(dmflags & DF_ITEMS_RESPAWN);
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
// AInventory :: DoEffect
//
// Handles any effect an item might apply to its owner
// Normally only used by subclasses of APowerup
//
//===========================================================================

void AInventory::DoEffect ()
{
}

//===========================================================================
//
// AInventory :: Travelled
//
// Called when an item in somebody's inventory is carried over to another
// map, in case it needs to do special reinitialization.
//
//===========================================================================

void AInventory::Travelled ()
{
}

//===========================================================================
//
// AInventory :: OwnerDied
//
// Items receive this message when their owners die.
//
//===========================================================================

void AInventory::OwnerDied ()
{
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
		if (PickupFlash != NULL)
		{
			Spawn(PickupFlash, x, y, z, ALLOW_REPLACE);
		}
		return false;
	}

	if (!ShouldStay ())
	{
		if (PickupFlash != NULL)
		{
			Spawn(PickupFlash, x, y, z, ALLOW_REPLACE);
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
		SetState (FindState("HoldAndDestroy"));
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
		copy = static_cast<AInventory *>(Spawn (GetClass(), 0, 0, 0, NO_REPLACE));
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
	if (SpawnState == ::GetDefault<AActor>()->SpawnState ||
		SpawnState == NULL)
	{
		return NULL;
	}
	if ((ItemFlags & IF_UNDROPPABLE) || Owner == NULL || Amount <= 0)
	{
		return NULL;
	}
	if (Amount == 1 && !(ItemFlags & IF_KEEPDEPLETED))
	{
		BecomePickup ();
		DropTime = 30;
		flags &= ~(MF_SPECIAL|MF_SOLID);
		return this;
	}
	copy = static_cast<AInventory *>(Spawn (GetClass(), Owner->x,
		Owner->y, Owner->z, NO_REPLACE));
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
		if (sector_list)
		{
			P_DelSeclist (sector_list);
			sector_list = NULL;
		}
		flags |= MF_NOBLOCKMAP|MF_NOSECTOR;
		LinkToWorld ();
	}
	RemoveFromHash ();
	flags &= ~MF_SPECIAL;
	SetState (FindState("Held"));
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

void AInventory::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// AInventory :: ModifyDamage
//
// Allows inventory items to manipulate the amount of damage
// inflicted. Damage is the amount of damage that would be done without manipulation,
// and newdamage is the amount that should be done after the item has changed
// it.
// 'active' means it is called by the inflictor, 'passive' by the target.
// It may seem that this is redundant and AbsorbDamage is the same. However,
// AbsorbDamage is called only for players and also depends on other settings
// which are undesirable for a protection artifact.
//
//===========================================================================

void AInventory::ModifyDamage (int damage, FName damageType, int &newdamage, bool passive)
{
	if (Inventory != NULL)
	{
		Inventory->ModifyDamage (damage, damageType, newdamage, passive);
	}
}

//===========================================================================
//
// AInventory :: GetSpeedFactor
//
//===========================================================================

fixed_t AInventory::GetSpeedFactor ()
{
	if (Inventory != NULL)
	{
		return Inventory->GetSpeedFactor();
	}
	else
	{
		return FRACUNIT;
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

int AInventory::AlterWeaponSprite (vissprite_t *vis)
{
	if (Inventory != NULL)
	{
		return Inventory->AlterWeaponSprite (vis);
	}
	return 0;
}

//===========================================================================
//
// AInventory :: Use
//
//===========================================================================

bool AInventory::Use (bool pickup)
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
	FState *HideSpecialState = NULL, *HideDoomishState = NULL;

 	flags = (flags & ~MF_SPECIAL) | MF_NOGRAVITY;
	renderflags |= RF_INVISIBLE;

	if (gameinfo.gametype & GAME_Raven)
	{
		HideSpecialState = FindState("HideSpecial");
		if (HideSpecialState == NULL)
		{
			HideDoomishState = FindState("HideDoomish");
		}
	}
	else
	{
		HideDoomishState = FindState("HideDoomish");
		if (HideDoomishState == NULL)
		{
			HideSpecialState = FindState("HideSpecial");
		}
	}

	if (HideSpecialState != NULL)
	{
		SetState (HideSpecialState);
		tics = 1400;
		if (PickupFlash != NULL) tics += 30;
	}
	else if (HideDoomishState != NULL)
	{
		SetState (HideDoomishState);
		tics = 1050;
	}
	else
	{
		GoAwayAndDie();
		return;
	}
	if (RespawnTics != 0)
	{
		tics = RespawnTics;
	}
}


//===========================================================================
//
//
//===========================================================================

static void PrintPickupMessage (const char *str)
{
	if (str != NULL)
	{
		if (str[0]=='$') 
		{
			str=GStrings(str+1);
		}
		if (str[0] != 0) Printf (PRINT_LOW, "%s\n", str);
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
	// If a voodoo doll touches something, pretend the real player touched it instead.
	if (toucher->player != NULL)
	{
		toucher = toucher->player->mo;
	}

	if (!TryPickup (toucher))
		return;

	if (!(ItemFlags & IF_QUIET))
	{
		const char * message = PickupMessage ();

		if (toucher->CheckLocalView (consoleplayer)
			&& (StaticLastMessageTic != gametic || StaticLastMessage != message))
		{
			StaticLastMessageTic = gametic;
			StaticLastMessage = message;
			PrintPickupMessage (message);
			StatusBar->FlashCrosshair ();
		}

		// Special check so voodoo dolls picking up items cause the
		// real player to make noise.
		if (toucher->player != NULL)
		{
			PlayPickupSound (toucher->player->mo);
			toucher->player->bonuscount = BONUSADD;
		}
		else
		{
			PlayPickupSound (toucher);
		}
	}							

	// [RH] Execute an attached special (if any)
	DoPickupSpecial (toucher);

	if (flags & MF_COUNTITEM)
	{
		if (toucher->player != NULL)
		{
			toucher->player->itemcount++;
		}
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
	const char *message = GetClass()->Meta.GetMetaString (AIMETA_PickupMessage);

	return message != NULL? message : "You got a pickup";
}

//===========================================================================
//
// AInventory :: PlayPickupSound
//
//===========================================================================

void AInventory::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, PickupSound, 1,
		(ItemFlags & IF_FANCYPICKUPSOUND) &&
		(toucher == NULL || toucher->CheckLocalView (consoleplayer))
		? ATTN_NONE : ATTN_NORM);
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

AInventory *AInventory::PrevItem ()
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

AInventory *AInventory::PrevInv ()
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

AInventory *AInventory::NextInv ()
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

IMPLEMENT_CLASS (APowerupGiver)

//===========================================================================
//
// AInventory :: DoRespawn
//
//===========================================================================

bool AInventory::DoRespawn ()
{
	if (SpawnPointClass != NULL)
	{
		AActor *spot = NULL;
		DSpotState *state = DSpotState::GetSpotState();

		if (state != NULL) spot = state->GetRandomSpot(SpawnPointClass);
		if (spot != NULL) 
		{
			SetOrigin (spot->x, spot->y, spot->z);
			z = floorz;
		}
	}
	return true;
}


//===========================================================================
//
// AInventory :: GiveQuest
//
//===========================================================================

void AInventory::GiveQuest (AActor *toucher)
{
	int quest = GetClass()->Meta.GetMetaInt(AIMETA_GiveQuest);
	if (quest>0 && quest<31)
	{
		toucher->GiveInventoryType (QuestItemClasses[quest-1]);
	}
}
//===========================================================================
//
// AInventory :: TryPickup
//
//===========================================================================

bool AInventory::TryPickup (AActor *toucher)
{
	AActor *newtoucher = toucher; // in case changed by the powerup

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
		bool usegood = Use (true);
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
		// Some powerups cannot activate absolutely, for
		// example, PowerMorph; fail the pickup if so.
		if (copy->ItemFlags & IF_INITEFFECTFAILED)
		{
			if (copy != this) copy->Destroy();
			else ItemFlags &= ~IF_INITEFFECTFAILED;
			return false;
		}
		// Handle owner-changing powerups
		if (copy->ItemFlags & IF_CREATECOPYMOVED)
		{
			newtoucher = copy->Owner;
			copy->Owner = NULL;
			copy->ItemFlags &= ~IF_CREATECOPYMOVED;
		}
		// Continue onwards with the rest
		copy->AttachToOwner (newtoucher);
		if (ItemFlags & IF_AUTOACTIVATE)
		{
			if (copy->Use (true))
			{
				if (--copy->Amount <= 0)
				{
					copy->flags &= ~MF_SPECIAL;
					copy->SetState (copy->FindState("HoldAndDestroy"));
				}
			}
		}
	}

	GiveQuest(newtoucher);
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
	int pnum = consoleplayer;

#ifdef _DEBUG
	// Only allow peeking on other players' inventory in debug builds.
	if (argv.argc() > 1)
	{
		pnum = atoi (argv[1]);
		if (pnum < 0 || pnum >= MAXPLAYERS)
		{
			return;
		}
	}
#endif
	if (players[pnum].mo == NULL)
	{
		return;
	}
	for (item = players[pnum].mo->Inventory; item != NULL; item = item->Inventory)
	{
		Printf ("%s #%u (%d/%d)\n", item->GetClass()->TypeName.GetChars(),
			item->InventoryID,
			item->Amount, item->MaxAmount);
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

IMPLEMENT_CLASS (ACustomInventory)

//===========================================================================
//
// ACustomInventory :: SpecialDropAction
//
//===========================================================================

bool ACustomInventory::SpecialDropAction (AActor *dropper)
{
	return CallStateChain (dropper, FindState(NAME_Drop));
}

//===========================================================================
//
// ACustomInventory :: Use
//
//===========================================================================

bool ACustomInventory::Use (bool pickup)
{
	return CallStateChain (Owner, FindState(NAME_Use));
}

//===========================================================================
//
// ACustomInventory :: TryPickup
//
//===========================================================================

bool ACustomInventory::TryPickup (AActor *toucher)
{
	FState *pickupstate = FindState(NAME_Pickup);
	bool useok = CallStateChain (toucher, pickupstate);
	if ((useok || pickupstate == NULL) && FindState(NAME_Use) != NULL)
	{
		useok = Super::TryPickup (toucher);
	}
	else if (useok || ItemFlags & IF_ALWAYSPICKUP)
	{
		GiveQuest (toucher);
		GoAwayAndDie();
	}
	return useok;
}

IMPLEMENT_CLASS (AHealth)

//===========================================================================
//
// AHealth :: TryPickup
//
//===========================================================================
const char *AHealth::PickupMessage ()
{
	int threshold = GetClass()->Meta.GetMetaInt(AIMETA_LowHealth, 0);

	if (PrevHealth < threshold)
	{
		const char *message = GetClass()->Meta.GetMetaString (AIMETA_LowHealthMessage);

		if (message != NULL)
		{
			return message;
		}
	}
	return Super::PickupMessage();
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
	
	if (player != NULL)
	{
		PrevHealth = other->player->health;
		if (max == 0)
		{
			max = static_cast<APlayerPawn*>(other)->GetMaxHealth() + player->stamina;
			// [MH] First step in predictable generic morph effects
 			if (player->morphTics)
 			{
				if (player->MorphStyle & MORPH_FULLHEALTH)
				{
					if (!(player->MorphStyle & MORPH_ADDSTAMINA))
					{
						max -= player->stamina;
					}
				}
				else // old health behaviour
				{
					max = MAXMORPHHEALTH;
					if (player->MorphStyle & MORPH_ADDSTAMINA)
					{
						max += player->stamina;
					}
				}
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
	}
	else
	{
		PrevHealth = INT_MAX;
		if (P_GiveBody(other, Amount) || ItemFlags & IF_ALWAYSPICKUP)
		{
			GoAwayAndDie ();
			return true;
		}
		return false;
	}
	GoAwayAndDie ();
	return true;
}

IMPLEMENT_CLASS (AHealthPickup)

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

bool AHealthPickup::Use (bool pickup)
{
	return P_GiveBody (Owner, health);
}

// Backpack -----------------------------------------------------------------

//===========================================================================
//
// ABackpackItem :: Serialize
//
//===========================================================================

void ABackpackItem::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << bDepleted;
}

//===========================================================================
//
// ABackpackItem :: CreateCopy
//
// A backpack is being added to a player who doesn't yet have one. Give them
// every kind of ammo, and increase their max amounts.
//
//===========================================================================

AInventory *ABackpackItem::CreateCopy (AActor *other)
{
	// Find every unique type of ammo. Give it to the player if
	// he doesn't have it already, and double it's maximum capacity.
	for (unsigned int i = 0; i < PClass::m_Types.Size(); ++i)
	{
		const PClass *type = PClass::m_Types[i];

		if (type->ParentClass == RUNTIME_CLASS(AAmmo))
		{
			AAmmo *ammo = static_cast<AAmmo *>(other->FindInventory (type));
			int amount = static_cast<AAmmo *>(GetDefaultByType(type))->BackpackAmount;
			// extra ammo in baby mode and nightmare mode
			if (!(ItemFlags&IF_IGNORESKILL))
			{
				amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
			}
			if (amount < 0) amount = 0;
			if (ammo == NULL)
			{ // The player did not have the ammo. Add it.
				ammo = static_cast<AAmmo *>(Spawn (type, 0, 0, 0, NO_REPLACE));
				ammo->Amount = bDepleted ? 0 : amount;
				if (ammo->BackpackMaxAmount > ammo->MaxAmount) ammo->MaxAmount = ammo->BackpackMaxAmount;
				ammo->AttachToOwner (other);
			}
			else
			{ // The player had the ammo. Give some more.
				if (ammo->MaxAmount < ammo->BackpackMaxAmount)
				{
					ammo->MaxAmount = ammo->BackpackMaxAmount;
				}
				if (!bDepleted && ammo->Amount < ammo->MaxAmount)
				{
					ammo->Amount += amount;
					if (ammo->Amount > ammo->MaxAmount)
					{
						ammo->Amount = ammo->MaxAmount;
					}
				}
			}
		}
	}
	return Super::CreateCopy (other);
}

//===========================================================================
//
// ABackpackItem :: HandlePickup
//
// When the player picks up another backpack, just give them more ammo.
//
//===========================================================================

bool ABackpackItem::HandlePickup (AInventory *item)
{
	// Since you already have a backpack, that means you already have every
	// kind of ammo in your inventory, so we don't need to look at the
	// entire PClass list to discover what kinds of ammo exist, and we don't
	// have to alter the MaxAmount either.
	if (item->IsKindOf (RUNTIME_CLASS(ABackpackItem)))
	{
		for (AInventory *probe = Owner->Inventory; probe != NULL; probe = probe->Inventory)
		{
			if (probe->GetClass()->ParentClass == RUNTIME_CLASS(AAmmo))
			{
				if (probe->Amount < probe->MaxAmount)
				{
					int amount = static_cast<AAmmo*>(probe->GetDefault())->BackpackAmount;
					// extra ammo in baby mode and nightmare mode
					if (!(item->ItemFlags&IF_IGNORESKILL))
					{
						amount = FixedMul(amount, G_SkillProperty(SKILLP_AmmoFactor));
					}
					probe->Amount += amount;
					if (probe->Amount > probe->MaxAmount)
					{
						probe->Amount = probe->MaxAmount;
					}
				}
			}
		}
		// The pickup always succeeds, even if you didn't get anything
		item->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	else if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	else
	{
		return false;
	}
}

//===========================================================================
//
// ABackpackItem :: CreateTossable
//
// The tossed backpack must not give out any more ammo, otherwise a player
// could cheat by dropping their backpack and picking it up for more ammo.
//
//===========================================================================

AInventory *ABackpackItem::CreateTossable ()
{
	ABackpackItem *pack = static_cast<ABackpackItem *>(Super::CreateTossable());
	pack->bDepleted = true;
	return pack;
}

//===========================================================================
//
// ABackpackItem :: DetachFromOwner
//
//===========================================================================

void ABackpackItem::DetachFromOwner ()
{
	// When removing a backpack, drop the player's ammo maximums to normal
	AInventory *item;

	for (item = Owner->Inventory; item != NULL; item = item->Inventory)
	{
		if (item->GetClass()->ParentClass == RUNTIME_CLASS(AAmmo) &&
			item->MaxAmount == static_cast<AAmmo*>(item)->BackpackMaxAmount)
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
// ABackpack
//
//===========================================================================

IMPLEMENT_CLASS(ABackpackItem)

IMPLEMENT_CLASS (AMapRevealer)

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
	GiveQuest (toucher);
	GoAwayAndDie ();
	return true;
}

