#ifndef __A_PICKUPS_H__
#define __A_PICKUPS_H__

#include "actor.h"
#include "info.h"
#include "s_sound.h"

#define NUM_WEAPON_SLOTS		10

class player_t;
class FConfigFile;
class PClassPlayerPawn;
struct visstyle_t;

/************************************************************************/
/* Class definitions													*/
/************************************************************************/

// A pickup is anything the player can pickup (i.e. weapons, ammo, powerups, etc)

enum
{
	IF_ACTIVATABLE		= 1<<0,		// can be activated
	IF_ACTIVATED		= 1<<1,		// is currently activated
	IF_PICKUPGOOD		= 1<<2,		// HandlePickup wants normal pickup FX to happen
	IF_QUIET			= 1<<3,		// Don't give feedback when picking up
	IF_AUTOACTIVATE		= 1<<4,		// Automatically activate item on pickup
	IF_UNDROPPABLE		= 1<<5,		// Item cannot be removed unless done explicitly with RemoveInventory
	IF_INVBAR			= 1<<6,		// Item appears in the inventory bar
	IF_HUBPOWER			= 1<<7,		// Powerup is kept when moving in a hub
	IF_UNTOSSABLE		= 1<<8,		// The player cannot manually drop the item
	IF_ADDITIVETIME		= 1<<9,		// when picked up while another item is active, time is added instead of replaced.
	IF_ALWAYSPICKUP		= 1<<10,	// For IF_AUTOACTIVATE, MaxAmount=0 items: Always "pick up", even if it doesn't do anything
	IF_FANCYPICKUPSOUND	= 1<<11,	// Play pickup sound in "surround" mode
	IF_BIGPOWERUP		= 1<<12,	// Affected by RESPAWN_SUPER dmflag
	IF_KEEPDEPLETED		= 1<<13,	// Items with this flag are retained even when they run out.
	IF_IGNORESKILL		= 1<<14,	// Ignores any skill related multiplicators when giving this item.
	IF_CREATECOPYMOVED	= 1<<15,	// CreateCopy changed the owner (copy's Owner field holds new owner).
	IF_INITEFFECTFAILED	= 1<<16,	// CreateCopy tried to activate a powerup and activation failed (can happen with PowerMorph)
	IF_NOATTENPICKUPSOUND = 1<<17,	// Play pickup sound with ATTN_NONE
	IF_PERSISTENTPOWER	= 1<<18,	// Powerup is kept when travelling between levels
	IF_RESTRICTABSOLUTELY = 1<<19,	// RestrictedTo and ForbiddenTo do not allow pickup in any form by other classes
	IF_NEVERRESPAWN		= 1<<20,	// Never, ever respawns
	IF_NOSCREENFLASH	= 1<<21,	// No pickup flash on the player's screen
	IF_TOSSED			= 1<<22,	// Was spawned by P_DropItem (i.e. as a monster drop)
	IF_ALWAYSRESPAWN	= 1<<23,	// Always respawn, regardless of dmflag
	IF_TRANSFER			= 1<<24,	// All inventory items that the inventory item contains is also transfered to the pickuper
	IF_NOTELEPORTFREEZE	= 1<<25,	// does not 'freeze' the player right after teleporting.
};


class PClassInventory : public PClassActor
{
	DECLARE_CLASS(PClassInventory, PClassActor)
public:
	PClassInventory();
	virtual void DeriveData(PClass *newclass);
	virtual size_t PointerSubstitution(DObject *oldclass, DObject *newclass);
	void Finalize(FStateDefinitions &statedef);

	FString PickupMessage;
	int GiveQuest;			// Optionally give one of the quest items.
	FTextureID AltHUDIcon;
	TArray<PClassPlayerPawn *> RestrictedToPlayerClass;
	TArray<PClassPlayerPawn *> ForbiddenToPlayerClass;
};

class AInventory : public AActor
{
	DECLARE_CLASS_WITH_META(AInventory, AActor, PClassInventory)
	HAS_OBJECT_POINTERS
public:
	
	virtual void Touch (AActor *toucher) override;
	virtual void Serialize(FSerializer &arc) override;
	virtual void MarkPrecacheSounds() const override;
	virtual void BeginPlay () override;
	virtual void Destroy () override;
	virtual void Tick() override;
	virtual bool Grind(bool items) override;

	// virtual methods that only get overridden by special internal classes, like DehackedPickup.
	// There is no need to expose these to scripts.
	virtual void DepleteOrDestroy ();
	virtual bool ShouldRespawn ();
	virtual void DoPickupSpecial (AActor *toucher);

	// methods that can be overridden by scripts, plus their callers.
	virtual bool SpecialDropAction (AActor *dropper);
	bool CallSpecialDropAction(AActor *dropper);

	virtual bool TryPickup(AActor *&toucher);
	virtual bool TryPickupRestricted(AActor *&toucher);
	bool CallTryPickup(AActor *toucher, AActor **toucher_return = NULL);	// This wraps both virtual methods plus a few more checks. 

	virtual AInventory *CreateCopy(AActor *other);
	AInventory *CallCreateCopy(AActor *other);

	virtual AInventory *CreateTossable();
	AInventory *CallCreateTossable();

	virtual FString PickupMessage();
	FString GetPickupMessage();

	virtual bool HandlePickup(AInventory *item);
	bool CallHandlePickup(AInventory *item);

	virtual bool Use(bool pickup);
	bool CallUse(bool pickup);

	virtual PalEntry GetBlend();
	PalEntry CallGetBlend();

	virtual bool ShouldStay();
	bool CallShouldStay();

	virtual void DoEffect();
	void CallDoEffect();

	virtual void PlayPickupSound(AActor *toucher);
	void CallPlayPickupSound(AActor *toucher);

	virtual void AttachToOwner(AActor *other);
	void CallAttachToOwner(AActor *other);

	virtual void DetachFromOwner();
	void CallDetachFromOwner();

	// still need to be done.
	virtual void AbsorbDamage(int damage, FName damageType, int &newdamage);
	virtual void ModifyDamage(int damage, FName damageType, int &newdamage, bool passive);

	// visual stuff is for later. Right now the VM has not yet access to the needed functionality.
	virtual bool DrawPowerup(int x, int y);
	virtual int AlterWeaponSprite(visstyle_t *vis);


	// virtual on the script side only.
	double GetSpeedFactor();
	bool GetNoTeleportFreeze();
	// Stuff for later when more features are exported.
	virtual void Travelled();
	virtual void OwnerDied();


	bool GoAway();
	void GoAwayAndDie();

	void Hide();
	void BecomeItem ();
	void BecomePickup ();

	bool DoRespawn();
	AInventory *PrevItem();		// Returns the item preceding this one in the list.
	AInventory *PrevInv();		// Returns the previous item with IF_INVBAR set.
	AInventory *NextInv();		// Returns the next item with IF_INVBAR set.

	TObjPtr<AActor> Owner;		// Who owns this item? NULL if it's still a pickup.
	int Amount;					// Amount of item this instance has
	int MaxAmount;				// Max amount of item this instance can have
	int InterHubAmount;			// Amount of item that can be kept between hubs or levels
	int RespawnTics;			// Tics from pickup time to respawn time
	FTextureID Icon;			// Icon to show on status bar or HUD
	int DropTime;				// Countdown after dropping
	PClassActor *SpawnPointClass;	// For respawning like Heretic's mace

	DWORD ItemFlags;
	PClassActor *PickupFlash;	// actor to spawn as pickup flash

	FSoundIDNoInit PickupSound;


protected:
	bool CanPickup(AActor * toucher);
	void GiveQuest(AActor * toucher);

private:
	static int StaticLastMessageTic;
	static FString StaticLastMessage;
};

class AStateProvider : public AInventory
{
	DECLARE_CLASS(AStateProvider, AInventory)
};

// CustomInventory: Supports the Use, Pickup, and Drop states from 96x
class ACustomInventory : public AStateProvider
{
	DECLARE_CLASS (ACustomInventory, AStateProvider)
public:

	// This is used when an inventory item's use state sequence is executed.
	bool CallStateChain (AActor *actor, FState *state);

	virtual bool TryPickup (AActor *&toucher) override;
	virtual bool Use (bool pickup) override;
	virtual bool SpecialDropAction (AActor *dropper) override;
};

extern PClassActor *QuestItemClasses[31];


#endif //__A_PICKUPS_H__
