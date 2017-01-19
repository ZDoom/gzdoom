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
#include "c_functions.h"
#include "g_levellocals.h"

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

	newc->PickupMsg = PickupMsg;
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
DEFINE_FIELD(PClassInventory, PickupMsg)

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

DEFINE_ACTION_FUNCTION(AInventory, GoAway)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_BOOL(self->GoAway());
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
	// stop all sounds this item is playing.
	for(int i = 1;i<=7;i++) S_StopSound(this, i);
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
			VMValue params[1] = { (DObject*)self };
			double retval;
			VMReturn ret(&retval);
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
			VMValue params[1] = { (DObject*)self };
			int retval;
			VMReturn ret(&retval);
			GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
			if (retval) return true;
		}
		self = self->Inventory;
	}
	return false;
}

//===========================================================================
//
// AInventory :: Use
//
//===========================================================================

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
static int StaticLastMessageTic;
static FString StaticLastMessage;

DEFINE_ACTION_FUNCTION(AInventory, PrintPickupMessage)
{
	PARAM_PROLOGUE;
	PARAM_BOOL(localview);
	PARAM_STRING(str);
	if (str.IsNotEmpty() && localview && (StaticLastMessageTic != gametic || StaticLastMessage.Compare(str)))
	{
		StaticLastMessageTic = gametic;
		StaticLastMessage = str;
		const char *pstr = str.GetChars();

		if (pstr[0] == '$')	pstr = GStrings(pstr + 1);
		if (pstr[0] != 0) Printf(PRINT_LOW, "%s\n", pstr);
		StatusBar->FlashCrosshair();
	}
	return 0;
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

DEFINE_ACTION_FUNCTION(AInventory, DoPickupSpecial)
{
	PARAM_SELF_PROLOGUE(AInventory);
	PARAM_OBJECT(toucher, AActor);
	self->DoPickupSpecial(toucher);
	return 0;
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
	return false;
}


//===========================================================================
//
// AInventory :: Destroy
//
//===========================================================================

void AInventory::OnDestroy ()
{
	if (Owner != NULL)
	{
		Owner->RemoveInventory (this);
	}
	Inventory = NULL;
	Super::OnDestroy();

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
	IFVIRTUAL(AInventory, DepleteOrDestroy)
	{
		VMValue params[1] = { (DObject*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
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

DEFINE_ACTION_FUNCTION(AInventory, DoRespawn)
{
	PARAM_SELF_PROLOGUE(AInventory);
	ACTION_RETURN_BOOL(self->DoRespawn());
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
	C_PrintInv(players[pnum].mo);
}

CCMD (targetinv)
{
	FTranslatedLineTarget t;

	if (CheckCheatmode () || players[consoleplayer].mo == NULL)
		return;

	C_AimLine(&t, true);

	if (t.linetarget)
	{
		C_PrintInv(t.linetarget);
	}
	else Printf("No target found. Targetinv cannot find actors that have "
				"the NOBLOCKMAP flag or have height/radius of 0.\n");
}

//===========================================================================
//
// AInventory :: AttachToOwner
//
//===========================================================================

void AInventory::CallAttachToOwner(AActor *other)
{
	IFVIRTUAL(AInventory, AttachToOwner)
	{
		VMValue params[2] = { (DObject*)this, (DObject*)other };
		GlobalVMStack.Call(func, params, 2, nullptr, 0, nullptr);
	}
}


//===========================================================================
//===========================================================================



IMPLEMENT_CLASS(AStateProvider, false, false)

