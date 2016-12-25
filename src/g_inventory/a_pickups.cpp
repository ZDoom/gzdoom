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
#include "a_morph.h"
#include "a_specialspot.h"
#include "g_level.h"
#include "g_game.h"
#include "doomstat.h"
#include "d_player.h"
#include "p_spec.h"
#include "serializer.h"
#include "virtual.h"
#include "a_ammo.h"

EXTERN_CVAR(Bool, sv_unlimited_pickup)

IMPLEMENT_CLASS(PClassInventory, false, false)

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

size_t PClassInventory::PointerSubstitution(DObject *oldclass, DObject *newclass)
{
	size_t changed = Super::PointerSubstitution(oldclass, newclass);
	AInventory *def = (AInventory*)Defaults;
	if (def != NULL)
	{
		if (def->PickupFlash == oldclass) def->PickupFlash = static_cast<PClassActor *>(newclass);
		for (unsigned i = 0; i < ForbiddenToPlayerClass.Size(); i++)
		{
			if (ForbiddenToPlayerClass[i] == oldclass)
			{
				ForbiddenToPlayerClass[i] = static_cast<PClassPlayerPawn*>(newclass);
				changed++;
			}
		}
		for (unsigned i = 0; i < RestrictedToPlayerClass.Size(); i++)
		{
			if (RestrictedToPlayerClass[i] == oldclass)
			{
				RestrictedToPlayerClass[i] = static_cast<PClassPlayerPawn*>(newclass);
				changed++;
			}
		}
	}
	return changed;
}

void PClassInventory::Finalize(FStateDefinitions &statedef)
{
	Super::Finalize(statedef);
	((AActor*)Defaults)->flags |= MF_SPECIAL;
}

//---------------------------------------------------------------------------
//
// PROC A_RestoreSpecialThing1
//
// Make a special thing visible again.
//
//---------------------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AInventory, A_RestoreSpecialThing1)
{
	PARAM_SELF_PROLOGUE(AInventory);

	self->renderflags &= ~RF_INVISIBLE;
	if (self->DoRespawn ())
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

DEFINE_ACTION_FUNCTION(AInventory, A_RestoreSpecialThing2)
{
	PARAM_SELF_PROLOGUE(AInventory);

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

DEFINE_ACTION_FUNCTION(AInventory, A_RestoreSpecialDoomThing)
{
	PARAM_SELF_PROLOGUE(AInventory);

	self->renderflags &= ~RF_INVISIBLE;
	self->flags |= MF_SPECIAL;
	if (!(self->GetDefault()->flags & MF_NOGRAVITY))
	{
		self->flags &= ~MF_NOGRAVITY;
	}
	if (self->DoRespawn ())
	{
		self->SetState (self->SpawnState);
		S_Sound (self, CHAN_VOICE, "misc/spawn", 1, ATTN_IDLE);
		Spawn ("ItemFog", self->Pos(), ALLOW_REPLACE);
	}
	return 0;
}

int AInventory::StaticLastMessageTic;
FString AInventory::StaticLastMessage;

IMPLEMENT_CLASS(AInventory, false, true)

IMPLEMENT_POINTERS_START(AInventory)
	IMPLEMENT_POINTER(Owner)
IMPLEMENT_POINTERS_END

DEFINE_FIELD_BIT(AInventory, ItemFlags, bPickupGood, IF_PICKUPGOOD)
DEFINE_FIELD_BIT(AInventory, ItemFlags, bCreateCopyMoved, IF_CREATECOPYMOVED)
DEFINE_FIELD_BIT(AInventory, ItemFlags, bInitEffectFailed, IF_INITEFFECTFAILED)
DEFINE_FIELD(AInventory, Owner)		
DEFINE_FIELD(AInventory, Amount)
DEFINE_FIELD(AInventory, MaxAmount)
DEFINE_FIELD(AInventory, InterHubAmount)
DEFINE_FIELD(AInventory, RespawnTics)
DEFINE_FIELD(AInventory, Icon)
DEFINE_FIELD(AInventory, DropTime)
DEFINE_FIELD(AInventory, SpawnPointClass)
DEFINE_FIELD(AInventory, PickupFlash)
DEFINE_FIELD(AInventory, PickupSound)

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

void AInventory::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);

	auto def = (AInventory*)GetDefault();
	arc("owner", Owner)
		("amount", Amount, def->Amount)
		("maxamount", MaxAmount, def->MaxAmount)
		("interhubamount", InterHubAmount, def->InterHubAmount)
		("respawntics", RespawnTics, def->RespawnTics)
		("itemflags", ItemFlags, def->ItemFlags)
		("icon", Icon, def->Icon)
		("pickupsound", PickupSound, def->PickupSound)
		("spawnpointclass", SpawnPointClass, def->SpawnPointClass)
		("droptime", DropTime, def->DropTime);
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

DEFINE_ACTION_FUNCTION(AInventory, SpecialDropAction)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT_NOT_NULL(dropper, AActor);
	ACTION_RETURN_BOOL(self->SpecialDropAction(dropper));
}

bool AInventory::CallSpecialDropAction(AActor *dropper)
{
	IFVIRTUAL(AInventory, SpecialDropAction)
	{
		VMValue params[2] = { (DObject*)this, (DObject*)dropper };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return !!retval;
	}
	return SpecialDropAction(dropper);
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

DEFINE_ACTION_FUNCTION(AInventory, DoEffect)
{
	PARAM_SELF_PROLOGUE(AInventory);
	self->DoEffect();
	return 0;
}

void AInventory::CallDoEffect()
{
	IFVIRTUAL(AInventory, DoEffect)
	{
		VMValue params[1] = { (DObject*)this };
		VMFrameStack stack;
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else DoEffect();
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
		if (Amount < MaxAmount || (sv_unlimited_pickup && !item->CallShouldStay()))
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
	return false;
}

DEFINE_ACTION_FUNCTION(AInventory, HandlePickup)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT_NOT_NULL(item, AInventory);
	ACTION_RETURN_BOOL(self->HandlePickup(item));
}

bool AInventory::CallHandlePickup(AInventory *item)
{
	auto self = this;
	while (self != nullptr)
	{
		IFVIRTUALPTR(self, AInventory, HandlePickup)
		{
			// Without the type cast this picks the 'void *' assignment...
			VMValue params[2] = { (DObject*)self, (DObject*)item };
			VMReturn ret;
			int retval;
			ret.IntAt(&retval);
			GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
			if (retval) return true;
		}
		else if (self->HandlePickup(item)) return true;
		self = self->Inventory;
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

	if (!CallShouldStay ())
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

DEFINE_ACTION_FUNCTION(AInventory, GoAwayAndDie)
{
	PARAM_SELF_PROLOGUE(AInventory);
	self->GoAwayAndDie();
	return 0;
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

DEFINE_ACTION_FUNCTION(AInventory, CreateCopy)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT(other, AActor);
	ACTION_RETURN_OBJECT(self->CreateCopy(other));
}

AInventory *AInventory::CallCreateCopy(AActor *other)
{
	IFVIRTUAL(AInventory, CreateCopy)
	{
		VMValue params[2] = { (DObject*)this, (DObject*)other };
		VMReturn ret;
		AInventory *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return retval;
	}
	else return CreateCopy(other);
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

DEFINE_ACTION_FUNCTION(AInventory, CreateTossable)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_OBJECT(self->CreateTossable());
}

AInventory *AInventory::CallCreateTossable()
{
	IFVIRTUAL(AInventory, CreateTossable)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		AInventory *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	else return CreateTossable();
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
		UnlinkFromWorld (nullptr);
		flags |= MF_NOBLOCKMAP|MF_NOSECTOR;
		LinkToWorld (nullptr);
	}
	RemoveFromHash ();
	flags &= ~MF_SPECIAL;
	ChangeStatNum(STAT_INVENTORY);
	SetState (FindState("Held"));
}

DEFINE_ACTION_FUNCTION(AInventory, BecomeItem)
{
	PARAM_SELF_PROLOGUE(AInventory);
	self->BecomeItem();
	return 0;
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
		UnlinkFromWorld (nullptr);
		flags &= ~(MF_NOBLOCKMAP|MF_NOSECTOR);
		LinkToWorld (nullptr);
		P_FindFloorCeiling (this);
	}
	flags = (GetDefault()->flags | MF_DROPPED) & ~MF_COUNTITEM;
	renderflags &= ~RF_INVISIBLE;
	ChangeStatNum(STAT_DEFAULT);
	SetState (SpawnState);
}

DEFINE_ACTION_FUNCTION(AInventory, BecomePickup)
{
	PARAM_SELF_PROLOGUE(AInventory);
	self->BecomePickup();
	return 0;
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

double AInventory::GetSpeedFactor()
{
	double factor = 1.;
	auto self = this;
	while (self != nullptr)
	{
		IFVIRTUALPTR(self, AInventory, GetSpeedFactor)
		{
			VMValue params[2] = { (DObject*)self };
			VMReturn ret;
			double retval;
			ret.FloatAt(&retval);
			GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
			factor *= retval;
		}
		self = self->Inventory;
	}
	return factor;
}

//===========================================================================
//
// AInventory :: GetNoTeleportFreeze
//
//===========================================================================

bool AInventory::GetNoTeleportFreeze ()
{
	auto self = this;
	while (self != nullptr)
	{
		IFVIRTUALPTR(self, AInventory, GetNoTeleportFreeze)
		{
			VMValue params[2] = { (DObject*)self };
			VMReturn ret;
			int retval;
			ret.IntAt(&retval);
			GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
			if (retval) return true;
		}
		self = self->Inventory;
	}
	return false;
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

DEFINE_ACTION_FUNCTION(AInventory, Use)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_BOOL(pickup);
	ACTION_RETURN_BOOL(self->Use(pickup));
}

bool AInventory::CallUse(bool pickup)
{
	IFVIRTUAL(AInventory, Use)
	{
		VMValue params[2] = { (DObject*)this, pickup };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return !!retval;

	}
	else return Use(pickup);
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
	if (PickupFlash != NULL && !CallShouldStay())
	{
		Spawn(PickupFlash, Pos(), ALLOW_REPLACE);
	}

	if (!(ItemFlags & IF_QUIET))
	{
		FString message = GetPickupMessage ();

		if (message.IsNotEmpty() && localview
			&& (StaticLastMessageTic != gametic || StaticLastMessage.Compare(message)))
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

FString AInventory::PickupMessage ()
{
	return GetClass()->PickupMessage;
}

DEFINE_ACTION_FUNCTION(AInventory, PickupMessage)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_STRING(self->PickupMessage());
}

FString AInventory::GetPickupMessage()
{
	IFVIRTUAL(AInventory, PickupMessage)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FString retval;
		ret.StringAt(&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	else return PickupMessage();
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

DEFINE_ACTION_FUNCTION(AInventory, PlayPickupSound)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT(other, AActor);
	self->PlayPickupSound(other);
	return 0;
}

void AInventory::CallPlayPickupSound(AActor *other)
{
	IFVIRTUAL(AInventory, PlayPickupSound)
	{
		VMValue params[2] = { (DObject*)this, (DObject*)other };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
	else PlayPickupSound(other);
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

DEFINE_ACTION_FUNCTION(AInventory, ShouldStay)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_BOOL(self->ShouldStay());
}

bool AInventory::CallShouldStay()
{
	IFVIRTUAL(AInventory, ShouldStay)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return !!retval;
	}
	else return ShouldStay();
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

DEFINE_ACTION_FUNCTION(AInventory, GetBlend)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_INT(self->GetBlend());
}

PalEntry AInventory::CallGetBlend()
{
	IFVIRTUAL(AInventory, GetBlend)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	else return GetBlend();
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
	if (toucher->Inventory != NULL && toucher->Inventory->CallHandlePickup (this))
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
		AInventory *copy = CallCreateCopy (toucher);
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
			if (copy->CallUse (true))
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

DEFINE_ACTION_FUNCTION(AInventory, TryPickup)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_POINTER_NOT_NULL(toucher, AActor*);
	ACTION_RETURN_BOOL(self->TryPickup(*toucher));
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

DEFINE_ACTION_FUNCTION(AInventory, TryPickupRestricted)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_POINTER_NOT_NULL(toucher, AActor*);
	ACTION_RETURN_BOOL(self->TryPickupRestricted(*toucher));
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
	{
		IFVIRTUAL(AInventory, TryPickup)
		{
			VMValue params[2] = { (DObject*)this, (void*)&toucher };
			VMReturn ret;
			int retval;
			ret.IntAt(&retval);
			GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
			res = !!retval;
		}
		else res = TryPickup(toucher);
	}
	else if (!(ItemFlags & IF_RESTRICTABSOLUTELY))
	{
		// let an item decide for itself how it will handle this
		IFVIRTUAL(AInventory, TryPickupRestricted)
		{
			VMValue params[2] = { (DObject*)this, (void*)&toucher };
			VMReturn ret;
			int retval;
			ret.IntAt(&retval);
			GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
			res = !!retval;
		}
		else res = TryPickupRestricted(toucher);
	}
	else
		return false;

	// Morph items can change the toucher so we need an option to return this info.
	if (toucher_return != NULL) *toucher_return = toucher;

	if (!res && (ItemFlags & IF_ALWAYSPICKUP) && !CallShouldStay())
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

DEFINE_ACTION_FUNCTION(AInventory, CallTryPickup)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT(toucher, AActor);
	AActor *t_ret;
	bool res = self->CallTryPickup(toucher, &t_ret);
	if (numret > 0) ret[0].SetInt(res);
	if (numret > 1) ret[1].SetPointer(t_ret, ATAG_OBJECT), numret = 2;
	return numret;
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

CCMD (targetinv)
{
	AInventory *item;
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL)
		return;

	P_AimLineAttack(players[consoleplayer].mo,players[consoleplayer].mo->Angles.Yaw, MISSILERANGE,
		&t, 0.,	ALF_CHECKNONSHOOTABLE|ALF_FORCENOSMART);

	if (t.linetarget)
	{
		for (item = t.linetarget->Inventory; item != NULL; item = item->Inventory)
		{
			Printf ("%s #%u (%d/%d)\n", item->GetClass()->TypeName.GetChars(),
				item->InventoryID,
				item->Amount, item->MaxAmount);
		}
	}
	else Printf("No target found. Targetinv cannot find actors that have "
				"the NOBLOCKMAP flag or have height/radius of 0.\n");
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

DEFINE_ACTION_FUNCTION(AInventory, AttachToOwner)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT_NOT_NULL(other, AActor);
	self->AttachToOwner(other);
	return 0;
}

void AInventory::CallAttachToOwner(AActor *other)
{
	IFVIRTUAL(AInventory, AttachToOwner)
	{
		VMValue params[2] = { (DObject*)this, (DObject*)other };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
	else AttachToOwner(other);
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

DEFINE_ACTION_FUNCTION(AInventory, DetachFromOwner)
{
	PARAM_SELF_PROLOGUE(AInventory);
	self->DetachFromOwner();
	return 0;
}

void AInventory::CallDetachFromOwner()
{
	IFVIRTUAL(AInventory, DetachFromOwner)
	{
		VMValue params[1] = { (DObject*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else DetachFromOwner();
}

//===========================================================================
//===========================================================================



IMPLEMENT_CLASS(AStateProvider, false, false)
IMPLEMENT_CLASS(ACustomInventory, false, false)

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
