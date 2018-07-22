#ifndef __A_PICKUPS_H__
#define __A_PICKUPS_H__

#include "actor.h"
#include "info.h"
#include "s_sound.h"

#define NUM_WEAPON_SLOTS		10

class player_t;
class FConfigFile;

// This encapsulates the fields of vissprite_t that can be altered by AlterWeaponSprite
struct visstyle_t
{
	bool			Invert;
	float			Alpha;
	ERenderStyle	RenderStyle;
};



/************************************************************************/
/* Class definitions													*/
/************************************************************************/

// A pickup is anything the player can pickup (i.e. weapons, ammo, powerups, etc)

enum ItemFlag
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
	IF_NOSCREENBLINK	= 1<<26,	// Does not blink the screen overlay when expiring.
	IF_ISHEALTH			= 1<<27,	// for the DM flag so that it can recognize items that are not obviously health givers.
	IF_ISARMOR			= 1<<28,	// for the DM flag so that it can recognize items that are not obviously armor givers.
};

typedef TFlags<ItemFlag> InvFlags;
//typedef TFlags<ItemFlag2> ItemFlags2;
DEFINE_TFLAGS_OPERATORS(InvFlags)
//DEFINE_TFLAGS_OPERATORS(ItemFlags2)

class AInventory : public AActor
{
	DECLARE_CLASS(AInventory, AActor)
	HAS_OBJECT_POINTERS
public:
	
	virtual void Finalize(FStateDefinitions &statedef) override;
	virtual void Serialize(FSerializer &arc) override;
	virtual void MarkPrecacheSounds() const override;
	virtual void OnDestroy() override;
	virtual void Tick() override;
	virtual bool Massacre() override;
	virtual bool Grind(bool items) override;

	bool CallTryPickup(AActor *toucher, AActor **toucher_return = NULL);	// Wrapper for script function.

	void DepleteOrDestroy ();			// virtual on the script side. 
	bool CallUse(bool pickup);			// virtual on the script side.
	PalEntry CallGetBlend();			// virtual on the script side.
	double GetSpeedFactor();			// virtual on the script side.
	bool GetNoTeleportFreeze();			// virtual on the script side.

	void BecomeItem ();
	void BecomePickup ();

	bool DoRespawn();

	AInventory *PrevItem();		// Returns the item preceding this one in the list.
	AInventory *PrevInv();		// Returns the previous item with IF_INVBAR set.
	AInventory *NextInv();		// Returns the next item with IF_INVBAR set.

	TObjPtr<AActor*> Owner;		// Who owns this item? NULL if it's still a pickup.
	int Amount;					// Amount of item this instance has
	int MaxAmount;				// Max amount of item this instance can have
	int InterHubAmount;			// Amount of item that can be kept between hubs or levels
	int RespawnTics;			// Tics from pickup time to respawn time
	FTextureID Icon;			// Icon to show on status bar or HUD
	int DropTime;				// Countdown after dropping
	PClassActor *SpawnPointClass;	// For respawning like Heretic's mace
	FTextureID AltHUDIcon;

	InvFlags ItemFlags;
	PClassActor *PickupFlash;	// actor to spawn as pickup flash

	FSoundIDNoInit PickupSound;
};

class AStateProvider : public AInventory
{
	DECLARE_CLASS (AStateProvider, AInventory)
public:
	bool CallStateChain(AActor *actor, FState *state);
};

#endif //__A_PICKUPS_H__
