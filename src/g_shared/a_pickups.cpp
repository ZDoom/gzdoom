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
#include "thingdef/thingdef.h"
#include "g_level.h"
#include "g_game.h"
#include "doomstat.h"
#include "farchive.h"
#include "d_player.h"
#include "p_spec.h"

static FRandom pr_restore ("RestorePos");

IMPLEMENT_CLASS(PClassInventory)

PClassInventory::PClassInventory()
{
	GiveQuest = 0;
	AltHUDIcon.SetNull();
}

void PClassInventory::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassInventory)));
	Super::DeriveData(newclass);
	PClassInventory *newc = static_cast<PClassInventory *>(newclass);

	newc->PickupMessage = PickupMessage;
	newc->GiveQuest = GiveQuest;
	newc->AltHUDIcon = AltHUDIcon;
	newc->ForbiddenToPlayerClass = ForbiddenToPlayerClass;
	newc->RestrictedToPlayerClass = RestrictedToPlayerClass;
}

void PClassInventory::ReplaceClassRef(PClass *oldclass, PClass *newclass)
{
	Super::ReplaceClassRef(oldclass, newclass);
	AInventory *def = (AInventory*)Defaults;
	if (def != NULL)
	{
		if (def->PickupFlash == oldclass) def->PickupFlash = static_cast<PClassActor *>(newclass);
		for (unsigned i = 0; i < ForbiddenToPlayerClass.Size(); i++)
		{
			if (ForbiddenToPlayerClass[i] == oldclass)
				ForbiddenToPlayerClass[i] = static_cast<PClassPlayerPawn*>(newclass);
		}
		for (unsigned i = 0; i < RestrictedToPlayerClass.Size(); i++)
		{
			if (RestrictedToPlayerClass[i] == oldclass)
				RestrictedToPlayerClass[i] = static_cast<PClassPlayerPawn*>(newclass);
		}
	}
}

IMPLEMENT_CLASS(PClassAmmo)

PClassAmmo::PClassAmmo()
{
	DropAmount = 0;
}

void PClassAmmo::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassAmmo)));
	Super::DeriveData(newclass);
	PClassAmmo *newc = static_cast<PClassAmmo *>(newclass);

	newc->DropAmount = DropAmount;
}

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

PClassActor *AAmmo::GetParentAmmo () const
{
	PClass *type = GetClass();

	while (type->ParentClass != RUNTIME_CLASS(AAmmo) && type->ParentClass != NULL)
	{
		type = type->ParentClass;
	}
	return static_cast<PClassActor *>(type);
}

//===========================================================================
//
// AAmmo :: HandlePickup
//
//===========================================================================
EXTERN_CVAR(Bool, sv_unlimited_pickup)

bool AAmmo::HandlePickup (AInventory *item)
{
	if (GetClass() == item->GetClass() ||
		(item->IsKindOf (RUNTIME_CLASS(AAmmo)) && static_cast<AAmmo*>(item)->GetParentAmmo() == GetClass()))
	{
		if (Amount < MaxAmount || sv_unlimited_pickup)
		{
			int receiving = item->Amount;

			if (!(item->ItemFlags & IF_IGNORESKILL))
			{ // extra ammo in baby mode and nightmare mode
				receiving = int(receiving * G_SkillProperty(SKILLP_AmmoFactor));
			}
			int oldamount = Amount;

			if (Amount > 0 && Amount + receiving < 0)
			{
				Amount = 0x7fffffff;
			}
			else
			{
				Amount += receiving;
			}
			if (Amount > MaxAmount && !sv_unlimited_pickup)
			{
				Amount = MaxAmount;
			}
			item->ItemFlags |= IF_PICKUPGOOD;

			// If the player previously had this ammo but ran out, possibly switch
			// to a weapon that uses it, but only if the player doesn't already
			// have a weapon pending.

			assert (Owner != NULL);

			if (oldamount == 0 && Owner != NULL && Owner->player != NULL)
			{
				barrier_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(GetClass());
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
		amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
	}

	if (GetClass()->ParentClass != RUNTIME_CLASS(AAmmo) && GetClass() != RUNTIME_CLASS(AAmmo))
	{
		PClassActor *type = GetParentAmmo();
		if (!GoAway ())
		{
			Destroy ();
		}

		copy = static_cast<AInventory *>(Spawn (type));
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

bool P_GiveBody (AActor *actor, int num, int max)
{
	if (actor->health <= 0 || (actor->player != NULL && actor->player->playerstate == PST_DEAD))
	{ // Do not heal dead things.
		return false;
	}

	player_t *player = actor->player;

	num = clamp(num, -65536, 65536);	// prevent overflows for bad values
	if (player != NULL)
	{
		// Max is 0 by default, preserving default behavior for P_GiveBody()
		// calls while supporting AHealth.
		if (max <= 0)
		{
			max = static_cast<APlayerPawn*>(actor)->GetMaxHealth() + player->mo->stamina;
			// [MH] First step in predictable generic morph effects
 			if (player->morphTics)
 			{
				if (player->MorphStyle & MORPH_FULLHEALTH)
				{
					if (!(player->MorphStyle & MORPH_ADDSTAMINA))
					{
						max -= player->mo->stamina;
					}
				}
				else // old health behaviour
				{
					max = MAXMORPHHEALTH;
					if (player->MorphStyle & MORPH_ADDSTAMINA)
					{
						max += player->mo->stamina;
					}
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
		else if (num > 0)
		{
			if (player->health < max)
			{
				num = int(num * G_SkillProperty(SKILLP_HealthFactor));
				if (num < 1) num = 1;
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
		// Parameter value for max is ignored on monsters, preserving original
		// behaviour on AHealth as well as on existing calls to P_GiveBody().
		max = actor->SpawnHealth();
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

DEFINE_ACTION_FUNCTION(AActor, A_RestoreSpecialThing1)
{
	PARAM_ACTION_PROLOGUE;

	self->renderflags &= ~RF_INVISIBLE;
	if (static_cast<AInventory *>(self)->DoRespawn ())
	{
		S_Sound (self, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing2
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RestoreSpecialThing2)
{
	PARAM_ACTION_PROLOGUE;

	self->flags |= MF_SPECIAL;
	if (!(self->GetDefault()->flags & MF_NOGRAVITY))
	{
		self->flags &= ~MF_NOGRAVITY;
	}
	self->SetState (self->SpawnState);
	return 0;
}


//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialDoomThing
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RestoreSpecialDoomThing)
{
	PARAM_ACTION_PROLOGUE;

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
		Spawn ("ItemFog", self->Pos(), ALLOW_REPLACE);
	}
	return 0;
}

//---------------------------------------------------------------------------
//
// PROP A_RestoreSpecialPosition
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_RestoreSpecialPosition)
{
	PARAM_ACTION_PROLOGUE;

	// Move item back to its original location
	DVector2 sp = self->SpawnPoint;

	self->UnlinkFromWorld();
	self->SetXY(sp);
	self->LinkToWorld(true);
	self->SetZ(self->Sector->floorplane.ZatPoint(sp));
	P_FindFloorCeiling(self, FFCF_ONLYSPAWNPOS | FFCF_NOPORTALS);	// no portal checks here so that things get spawned in this sector.

	if (self->flags & MF_SPAWNCEILING)
	{
		self->SetZ(self->ceilingz - self->Height - self->SpawnPoint.Z);
	}
	else if (self->flags2 & MF2_SPAWNFLOAT)
	{
		double space = self->ceilingz - self->Height - self->floorz;
		if (space > 48)
		{
			space -= 40;
			self->SetZ((space * pr_restore()) / 256. + self->floorz + 40);
		}
		else
		{
			self->SetZ(self->floorz);
		}
	}
	else
	{
		self->SetZ(self->SpawnPoint.Z + self->floorz);
	}
	// Redo floor/ceiling check, in case of 3D floors and portals
	P_FindFloorCeiling(self, FFCF_SAMESECTOR | FFCF_ONLY3DFLOORS | FFCF_3DRESTRICT);
	if (self->Z() < self->floorz)
	{ // Do not reappear under the floor, even if that's where we were for the
	  // initial spawn.
		self->SetZ(self->floorz);
	}
	if ((self->flags & MF_SOLID) && (self->Top() > self->ceilingz))
	{ // Do the same for the ceiling.
		self->SetZ(self->ceilingz - self->Height);
	}
	// Do not interpolate from the position the actor was at when it was
	// picked up, in case that is different from where it is now.
	self->ClearInterpolation();
	return 0;
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
// AInventory :: MarkPrecacheSounds
//
//===========================================================================

void AInventory::MarkPrecacheSounds() const
{
	Super::MarkPrecacheSounds();
	PickupSound.MarkUsed();
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
	if ((ItemFlags & IF_BIGPOWERUP) && !(dmflags2 & DF2_RESPAWN_SUPER)) return false;
	if (ItemFlags & IF_NEVERRESPAWN) return false;
	return !!((dmflags & DF_ITEMS_RESPAWN) || (ItemFlags & IF_ALWAYSRESPAWN));
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
// AInventory :: Grind
//
//===========================================================================

bool AInventory::Grind(bool items)
{
	// Does this grind request even care about items?
	if (!items)
	{
		return false;
	}
	// Dropped items are normally destroyed by crushers. Set the DONTGIB flag,
	// and they'll act like corpses with it set and be immune to crushers.
	if (flags & MF_DROPPED)
	{
		if (!(flags3 & MF3_DONTGIB))
		{
			Destroy();
		}
		return false;
	}
	// Non-dropped items call the super method for compatibility.
	return Super::Grind(items);
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
		if (Amount < MaxAmount || (sv_unlimited_pickup && !item->ShouldStay()))
		{
			if (Amount > 0 && Amount + item->Amount < 0)
			{
				Amount = 0x7fffffff;
			}
			else
			{
				Amount += item->Amount;
			}
		
			if (Amount > MaxAmount && !sv_unlimited_pickup)
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
		return false;
	}

	if (!ShouldStay ())
	{
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

	Amount = MIN(Amount, MaxAmount);
	if (GoAway ())
	{
		copy = static_cast<AInventory *>(Spawn (GetClass()));
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
	if ((ItemFlags & (IF_UNDROPPABLE|IF_UNTOSSABLE)) || Owner == NULL || Amount <= 0)
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
	copy = static_cast<AInventory *>(Spawn (GetClass(), Owner->Pos(), NO_REPLACE));
	if (copy != NULL)
	{
		copy->MaxAmount = MaxAmount;
		copy->Amount = 1;
		copy->DropTime = 30;
		copy->flags &= ~(MF_SPECIAL|MF_SOLID);
		Amount--;
	}
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
	flags = (GetDefault()->flags | MF_DROPPED) & ~MF_COUNTITEM;
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

double AInventory::GetSpeedFactor ()
{
	if (Inventory != NULL)
	{
		return Inventory->GetSpeedFactor();
	}
	else
	{
		return 1.;
	}
}

//===========================================================================
//
// AInventory :: GetNoTeleportFreeze
//
//===========================================================================

bool AInventory::GetNoTeleportFreeze ()
{
	// do not check the flag here because it's only active when used on PowerUps, not on PowerupGivers.
	if (Inventory != NULL)
	{
		return Inventory->GetNoTeleportFreeze();
	}
	else
	{
		return false;
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

int AInventory::AlterWeaponSprite (visstyle_t *vis)
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

	assert(HideDoomishState != NULL || HideSpecialState != NULL);

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
	player_t *player = toucher->player;

	// If a voodoo doll touches something, pretend the real player touched it instead.
	if (player != NULL)
	{
		toucher = player->mo;
	}

	bool localview = toucher->CheckLocalView(consoleplayer);

	if (!CallTryPickup (toucher, &toucher)) return;

	// This is the only situation when a pickup flash should ever play.
	if (PickupFlash != NULL && !ShouldStay())
	{
		Spawn(PickupFlash, Pos(), ALLOW_REPLACE);
	}

	if (!(ItemFlags & IF_QUIET))
	{
		const char * message = PickupMessage ();

		if (message != NULL && *message != 0 && localview
			&& (StaticLastMessageTic != gametic || StaticLastMessage != message))
		{
			StaticLastMessageTic = gametic;
			StaticLastMessage = message;
			PrintPickupMessage (message);
			StatusBar->FlashCrosshair ();
		}

		// Special check so voodoo dolls picking up items cause the
		// real player to make noise.
		if (player != NULL)
		{
			PlayPickupSound (player->mo);
			if (!(ItemFlags & IF_NOSCREENFLASH))
			{
				player->bonuscount = BONUSADD;
			}
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
		if (player != NULL)
		{
			player->itemcount++;
		}
		level.found_items++;
	}

	if (flags5 & MF5_COUNTSECRET)
	{
		P_GiveSecret(player != NULL? (AActor*)player->mo : toucher, true, true, -1);
	}

	//Added by MC: Check if item taken was the roam destination of any bot
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (players[i].Bot != NULL && this == players[i].Bot->dest)
			players[i].Bot->dest = NULL;
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
		P_ExecuteSpecial(special, NULL, toucher, false,
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
	return GetClass()->PickupMessage;
}

//===========================================================================
//
// AInventory :: PlayPickupSound
//
//===========================================================================

void AInventory::PlayPickupSound (AActor *toucher)
{
	float atten;
	int chan;

	if (ItemFlags & IF_NOATTENPICKUPSOUND)
	{
		atten = ATTN_NONE;
	}
#if 0
	else if ((ItemFlags & IF_FANCYPICKUPSOUND) &&
		(toucher == NULL || toucher->CheckLocalView(consoeplayer)))
	{
		atten = ATTN_NONE;
	}
#endif
	else
	{
		atten = ATTN_NORM;
	}

	if (toucher != NULL && toucher->CheckLocalView(consoleplayer))
	{
		chan = CHAN_PICKUP|CHAN_NOPAUSE;
	}
	else
	{
		chan = CHAN_PICKUP;
	}
	S_Sound (toucher, chan, PickupSound, 1, atten);
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

	// Although contrived it can theoretically happen that these variables still got a pointer to this item
	if (SendItemUse == this) SendItemUse = NULL;
	if (SendItemDrop == this) SendItemDrop = NULL;
}

//===========================================================================
//
// AInventory :: DepleteOrDestroy
//
// If the item is depleted, just change its amount to 0, otherwise it's destroyed.
//
//===========================================================================

void AInventory::DepleteOrDestroy ()
{
	// If it's not ammo or an internal armor, destroy it.
	// Ammo needs to stick around, even when it's zero for the benefit
	// of the weapons that use it and to maintain the maximum ammo
	// amounts a backpack might have given.
	// Armor shouldn't be removed because they only work properly when
	// they are the last items in the inventory.
	if (ItemFlags & IF_KEEPDEPLETED)
	{
		Amount = 0;
	}
	else
	{
		Destroy();
	}
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
			SetOrigin (spot->Pos(), false);
			SetZ(floorz);
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
	int quest = GetClass()->GiveQuest;
	if (quest > 0 && quest <= (int)countof(QuestItemClasses))
	{
		toucher->GiveInventoryType (QuestItemClasses[quest-1]);
	}
}

//===========================================================================
//
// AInventory :: TryPickup
//
//===========================================================================

bool AInventory::TryPickup (AActor *&toucher)
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
	else if (MaxAmount == 0 && !IsKindOf(RUNTIME_CLASS(AAmmo)))
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

		if (usegood)
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
	return true;
}

//===========================================================================
//
// AInventory :: TryPickupRestricted
//
//===========================================================================

bool AInventory::TryPickupRestricted (AActor *&toucher)
{
	return false;
}

//===========================================================================
//
// AInventory :: CallTryPickup
//
//===========================================================================

bool AInventory::CallTryPickup (AActor *toucher, AActor **toucher_return)
{
	TObjPtr<AInventory> Invstack = Inventory; // A pointer of the inventories item stack.

	// unmorphed versions of a currently morphed actor cannot pick up anything. 
	if (toucher->flags & MF_UNMORPHED) return false;

	bool res;
	if (CanPickup(toucher))
		res = TryPickup(toucher);
	else if (!(ItemFlags & IF_RESTRICTABSOLUTELY))
		res = TryPickupRestricted(toucher);	// let an item decide for itself how it will handle this
	else
		return false;

	// Morph items can change the toucher so we need an option to return this info.
	if (toucher_return != NULL) *toucher_return = toucher;

	if (!res && (ItemFlags & IF_ALWAYSPICKUP) && !ShouldStay())
	{
		res = true;
		GoAwayAndDie();
	}

	if (res)
	{
		GiveQuest(toucher);

		// Transfer all inventory accross that the old object had, if requested.
		if ((ItemFlags & IF_TRANSFER))
		{
			while (Invstack)
			{
				AInventory* titem = Invstack;
				Invstack = titem->Inventory;
				if (titem->Owner == this)
				{
					if (!titem->CallTryPickup(toucher)) // The object no longer can exist
					{
						titem->Destroy();
					}
				}
			}
		}
	}
	return res;
}


//===========================================================================
//
// AInventory :: CanPickup
//
//===========================================================================

bool AInventory::CanPickup (AActor *toucher)
{
	if (!toucher)
		return false;

	PClassInventory *ai = GetClass();
	// Is the item restricted to certain player classes?
	if (ai->RestrictedToPlayerClass.Size() != 0)
	{
		for (unsigned i = 0; i < ai->RestrictedToPlayerClass.Size(); ++i)
		{
			if (toucher->IsKindOf(ai->RestrictedToPlayerClass[i]))
				return true;
		}
		return false;
	}
	// Or is it forbidden to certain other classes?
	else
	{
		for (unsigned i = 0; i < ai->ForbiddenToPlayerClass.Size(); ++i)
		{
			if (toucher->IsKindOf(ai->ForbiddenToPlayerClass[i]))
				return false;
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

bool ACustomInventory::TryPickup (AActor *&toucher)
{
	FState *pickupstate = FindState(NAME_Pickup);
	bool useok = CallStateChain (toucher, pickupstate);
	if ((useok || pickupstate == NULL) && FindState(NAME_Use) != NULL)
	{
		useok = Super::TryPickup (toucher);
	}
	else if (useok)
	{
		GoAwayAndDie();
	}
	return useok;
}

IMPLEMENT_CLASS(PClassHealth)

//===========================================================================
//
// PClassHealth Constructor
//
//===========================================================================

PClassHealth::PClassHealth()
{
	LowHealth = 0;
}

//===========================================================================
//
// PClassHealth :: Derive
//
//===========================================================================

void PClassHealth::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassHealth)));
	Super::DeriveData(newclass);
	PClassHealth *newc = static_cast<PClassHealth *>(newclass);
	
	newc->LowHealth = LowHealth;
	newc->LowHealthMessage = LowHealthMessage;
}

IMPLEMENT_CLASS (AHealth)

//===========================================================================
//
// AHealth :: PickupMessage
//
//===========================================================================
const char *AHealth::PickupMessage ()
{
	int threshold = GetClass()->LowHealth;

	if (PrevHealth < threshold)
	{
		FString message = GetClass()->LowHealthMessage;

		if (message.IsNotEmpty())
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

bool AHealth::TryPickup (AActor *&other)
{
	PrevHealth = other->player != NULL ? other->player->health : other->health;

	// P_GiveBody adds one new feature, applied only if it is possible to pick up negative health:
	// Negative values are treated as positive percentages, ie Amount -100 means 100% health, ignoring max amount.
	if (P_GiveBody(other, Amount, MaxAmount))
	{
		GoAwayAndDie();
		return true;
	}
	return false;
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

//===========================================================================
//
// AHealthPickup :: Serialize
//
//===========================================================================

void AHealthPickup::Serialize (FArchive &arc)
{
	Super::Serialize(arc);
	arc << autousemode;
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
	// he doesn't have it already, and double its maximum capacity.
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClass *type = PClassActor::AllActorClasses[i];

		if (type->ParentClass == RUNTIME_CLASS(AAmmo))
		{
			PClassActor *atype = static_cast<PClassActor *>(type);
			AAmmo *ammo = static_cast<AAmmo *>(other->FindInventory(atype));
			int amount = static_cast<AAmmo *>(GetDefaultByType(type))->BackpackAmount;
			// extra ammo in baby mode and nightmare mode
			if (!(ItemFlags&IF_IGNORESKILL))
			{
				amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
			}
			if (amount < 0) amount = 0;
			if (ammo == NULL)
			{ // The player did not have the ammo. Add it.
				ammo = static_cast<AAmmo *>(Spawn(atype));
				ammo->Amount = bDepleted ? 0 : amount;
				if (ammo->BackpackMaxAmount > ammo->MaxAmount)
				{
					ammo->MaxAmount = ammo->BackpackMaxAmount;
				}
				if (ammo->Amount > ammo->MaxAmount)
				{
					ammo->Amount = ammo->MaxAmount;
				}
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
				if (probe->Amount < probe->MaxAmount || sv_unlimited_pickup)
				{
					int amount = static_cast<AAmmo*>(probe->GetDefault())->BackpackAmount;
					// extra ammo in baby mode and nightmare mode
					if (!(item->ItemFlags&IF_IGNORESKILL))
					{
						amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
					}
					probe->Amount += amount;
					if (probe->Amount > probe->MaxAmount && !sv_unlimited_pickup)
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
	if (pack != NULL)
	{
		pack->bDepleted = true;
	}
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

bool AMapRevealer::TryPickup (AActor *&toucher)
{
	level.flags2 |= LEVEL2_ALLMAP;
	GoAwayAndDie ();
	return true;
}


//===========================================================================
//
// AScoreItem
//
//===========================================================================

IMPLEMENT_CLASS(AScoreItem)

//===========================================================================
//
// AScoreItem :: TryPickup
//
// Adds the value (Amount) of the item to the toucher's Score property.
//
//===========================================================================

bool AScoreItem::TryPickup (AActor *&toucher)
{
	toucher->Score += Amount;
	GoAwayAndDie();
	return true;
}

