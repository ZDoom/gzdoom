#ifndef __A_PICKUPS_H__
#define __A_PICKUPS_H__

#include "actor.h"
#include "info.h"
#include "s_sound.h"

#define NUM_WEAPON_SLOTS		10

class player_t;
class FConfigFile;
class AWeapon;
struct visstyle_t;

class FWeaponSlot
{
public:
	FWeaponSlot() { Clear(); }
	FWeaponSlot(const FWeaponSlot &other) { Weapons = other.Weapons; }
	FWeaponSlot &operator= (const FWeaponSlot &other) { Weapons = other.Weapons; return *this; }
	void Clear() { Weapons.Clear(); }
	bool AddWeapon (const char *type);
	bool AddWeapon (const PClass *type);
	void AddWeaponList (const char *list, bool clear);
	AWeapon *PickWeapon (player_t *player, bool checkammo = false);
	int Size () const { return (int)Weapons.Size(); }
	int LocateWeapon (const PClass *type);

	inline const PClass *GetWeapon (int index) const
	{
		if ((unsigned)index < Weapons.Size())
		{
			return Weapons[index].Type;
		}
		else
		{
			return NULL;
		}
	}

	friend struct FWeaponSlots;

private:
	struct WeaponInfo
	{
		const PClass *Type;
		fixed_t Position;
	};
	void SetInitialPositions();
	void Sort();
	TArray<WeaponInfo> Weapons;
};
// FWeaponSlots::AddDefaultWeapon return codes
enum ESlotDef
{
	SLOTDEF_Exists,		// Weapon was already assigned a slot
	SLOTDEF_Added,		// Weapon was successfully added
	SLOTDEF_Full		// The specifed slot was full
};

struct FWeaponSlots
{
	FWeaponSlots() { Clear(); }
	FWeaponSlots(const FWeaponSlots &other);

	FWeaponSlot Slots[NUM_WEAPON_SLOTS];

	AWeapon *PickNextWeapon (player_t *player);
	AWeapon *PickPrevWeapon (player_t *player);

	void Clear ();
	bool LocateWeapon (const PClass *type, int *const slot, int *const index);
	ESlotDef AddDefaultWeapon (int slot, const PClass *type);
	void AddExtraWeapons();
	void SetFromGameInfo();
	void SetFromPlayer(const PClass *type);
	void StandardSetup(const PClass *type);
	void LocalSetup(const PClass *type);
	void SendDifferences(int playernum, const FWeaponSlots &other);
	int RestoreSlots (FConfigFile *config, const char *section);
	void PrintSettings();

	void AddSlot(int slot, const PClass *type, bool feedback);
	void AddSlotDefault(int slot, const PClass *type, bool feedback);

};

void P_PlaybackKeyConfWeapons(FWeaponSlots *slots);
void Net_WriteWeapon(const PClass *type);
const PClass *Net_ReadWeapon(BYTE **stream);

void P_SetupWeapons_ntohton();
void P_WriteDemoWeaponsChunk(BYTE **demo);
void P_ReadDemoWeaponsChunk(BYTE **demo);

/************************************************************************/
/* Class definitions													*/
/************************************************************************/

// A pickup is anything the player can pickup (i.e. weapons, ammo, powerups, etc)

enum
{
	AIMETA_BASE = 0x71000,
	AIMETA_PickupMessage,		// string
	AIMETA_GiveQuest,			// optionally give one of the quest items.
	AIMETA_DropAmount,			// specifies the amount for a dropped ammo item
	AIMETA_LowHealth,
	AIMETA_LowHealthMessage,
	AIMETA_PuzzFailMessage,
};

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


class AInventory : public AActor
{
	DECLARE_CLASS (AInventory, AActor)
	HAS_OBJECT_POINTERS
public:
	virtual void Touch (AActor *toucher);
	virtual void Serialize (FArchive &arc);

	virtual void MarkPrecacheSounds() const;
	virtual void BeginPlay ();
	virtual void Destroy ();
	virtual void DepleteOrDestroy ();
	virtual void Tick ();
	virtual bool ShouldRespawn ();
	virtual bool ShouldStay ();
	virtual void Hide ();
	bool CallTryPickup (AActor *toucher, AActor **toucher_return = NULL);
	virtual void DoPickupSpecial (AActor *toucher);
	virtual bool SpecialDropAction (AActor *dropper);
	virtual bool DrawPowerup (int x, int y);
	virtual void DoEffect ();
	virtual bool Grind(bool items);

	virtual const char *PickupMessage ();
	virtual void PlayPickupSound (AActor *toucher);

	bool DoRespawn ();
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
	const PClass *SpawnPointClass;	// For respawning like Heretic's mace

	DWORD ItemFlags;
	const PClass *PickupFlash;	// actor to spawn as pickup flash

	FSoundIDNoInit PickupSound;

	virtual void BecomeItem ();
	virtual void BecomePickup ();
	virtual void AttachToOwner (AActor *other);
	virtual void DetachFromOwner ();
	virtual AInventory *CreateCopy (AActor *other);
	virtual AInventory *CreateTossable ();
	virtual bool GoAway ();
	virtual void GoAwayAndDie ();
	virtual bool HandlePickup (AInventory *item);
	virtual bool Use (bool pickup);
	virtual void Travelled ();
	virtual void OwnerDied ();

	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage);
	virtual void ModifyDamage (int damage, FName damageType, int &newdamage, bool passive);
	virtual fixed_t GetSpeedFactor();
	virtual bool GetNoTeleportFreeze();
	virtual int AlterWeaponSprite (visstyle_t *vis);

	virtual PalEntry GetBlend ();

protected:
	virtual bool TryPickup (AActor *&toucher);
	virtual bool TryPickupRestricted (AActor *&toucher);
	bool CanPickup(AActor * toucher);
	void GiveQuest(AActor * toucher);

private:
	static int StaticLastMessageTic;
	static const char *StaticLastMessage;
};

// CustomInventory: Supports the Use, Pickup, and Drop states from 96x
class ACustomInventory : public AInventory
{
	DECLARE_CLASS (ACustomInventory, AInventory)
public:

	// This is used when an inventory item's use state sequence is executed.
	bool CallStateChain (AActor *actor, FState *state);

	bool TryPickup (AActor *&toucher);
	bool Use (bool pickup);
	bool SpecialDropAction (AActor *dropper);
};

// Ammo: Something a weapon needs to operate
class AAmmo : public AInventory
{
	DECLARE_CLASS (AAmmo, AInventory)
public:
	void Serialize (FArchive &arc);
	AInventory *CreateCopy (AActor *other);
	bool HandlePickup (AInventory *item);
	const PClass *GetParentAmmo () const;
	AInventory *CreateTossable ();

	int BackpackAmount, BackpackMaxAmount;
};

// A weapon is just that.
enum
{
	AWMETA_BASE = 0x72000,
	AWMETA_SlotNumber,
	AWMETA_SlotPriority,
};

class AWeapon : public AInventory
{
	DECLARE_CLASS (AWeapon, AInventory)
	HAS_OBJECT_POINTERS
public:
	DWORD WeaponFlags;
	const PClass *AmmoType1, *AmmoType2;	// Types of ammo used by this weapon
	int AmmoGive1, AmmoGive2;				// Amount of each ammo to get when picking up weapon
	int MinAmmo1, MinAmmo2;					// Minimum ammo needed to switch to this weapon
	int AmmoUse1, AmmoUse2;					// How much ammo to use with each shot
	int Kickback;
	fixed_t YAdjust;						// For viewing the weapon fullscreen
	FSoundIDNoInit UpSound, ReadySound;		// Sounds when coming up and idle
	const PClass *SisterWeaponType;			// Another weapon to pick up with this one
	const PClass *ProjectileType;			// Projectile used by primary attack
	const PClass *AltProjectileType;		// Projectile used by alternate attack
	int SelectionOrder;						// Lower-numbered weapons get picked first
	int MinSelAmmo1, MinSelAmmo2;			// Ignore in BestWeapon() if inadequate ammo
	fixed_t MoveCombatDist;					// Used by bots, but do they *really* need it?
	int ReloadCounter;						// For A_CheckForReload
	int BobStyle;							// [XA] Bobbing style. Defines type of bobbing (e.g. Normal, Alpha)
	fixed_t BobSpeed;						// [XA] Bobbing speed. Defines how quickly a weapon bobs.
	fixed_t BobRangeX, BobRangeY;			// [XA] Bobbing range. Defines how far a weapon bobs in either direction.

	// In-inventory instance variables
	TObjPtr<AAmmo> Ammo1, Ammo2;
	TObjPtr<AWeapon> SisterWeapon;
	float FOVScale;
	int Crosshair;							// 0 to use player's crosshair
	bool GivenAsMorphWeapon;

	bool bAltFire;	// Set when this weapon's alternate fire is used.

	virtual void MarkPrecacheSounds() const;
	virtual void Serialize (FArchive &arc);
	virtual bool ShouldStay ();
	virtual void AttachToOwner (AActor *other);
	virtual bool HandlePickup (AInventory *item);
	virtual AInventory *CreateCopy (AActor *other);
	virtual AInventory *CreateTossable ();
	virtual bool TryPickup (AActor *&toucher);
	virtual bool TryPickupRestricted (AActor *&toucher);
	virtual bool PickupForAmmo (AWeapon *ownedWeapon);
	virtual bool Use (bool pickup);
	virtual void Destroy();

	virtual FState *GetUpState ();
	virtual FState *GetDownState ();
	virtual FState *GetReadyState ();
	virtual FState *GetAtkState (bool hold);
	virtual FState *GetAltAtkState (bool hold);
	virtual FState *GetStateForButtonName (FName button);

	virtual void PostMorphWeapon ();
	virtual void EndPowerup ();

	enum
	{
		PrimaryFire,
		AltFire,
		EitherFire
	};
	bool CheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo=false, int ammocount = -1);
	bool DepleteAmmo (bool altFire, bool checkEnough=true, int ammouse = -1);

	enum
	{
		BobNormal,
		BobInverse,
		BobAlpha,
		BobInverseAlpha,
		BobSmooth,
		BobInverseSmooth
	};

protected:
	AAmmo *AddAmmo (AActor *other, const PClass *ammotype, int amount);
	bool AddExistingAmmo (AAmmo *ammo, int amount);
	AWeapon *AddWeapon (const PClass *weapon);
};

enum
{
	WIF_NOAUTOFIRE =		0x00000001, // weapon does not autofire
	WIF_READYSNDHALF =		0x00000002, // ready sound is played ~1/2 the time
	WIF_DONTBOB =			0x00000004, // don't bob the weapon
	WIF_AXEBLOOD =			0x00000008, // weapon makes axe blood on impact (Hexen only)
	WIF_NOALERT =			0x00000010,	// weapon does not alert monsters
	WIF_AMMO_OPTIONAL =		0x00000020, // weapon can use ammo but does not require it
	WIF_ALT_AMMO_OPTIONAL = 0x00000040, // alternate fire can use ammo but does not require it
	WIF_PRIMARY_USES_BOTH =	0x00000080, // primary fire uses both ammo
	WIF_ALT_USES_BOTH =		0x00000100, // alternate fire uses both ammo
	WIF_WIMPY_WEAPON =		0x00000200, // change away when ammo for another weapon is replenished
	WIF_POWERED_UP =		0x00000400, // this is a tome-of-power'ed version of its sister
	WIF_AMMO_CHECKBOTH =	0x00000800, // check for both primary and secondary fire before switching it off
	WIF_NO_AUTO_SWITCH =	0x00001000,	// never switch to this weapon when it's picked up
	WIF_STAFF2_KICKBACK =	0x00002000, // the powered-up Heretic staff has special kickback
	WIF_NOAUTOAIM =			0x00004000, // this weapon never uses autoaim (useful for ballistic projectiles)
	WIF_MELEEWEAPON =		0x00008000,	// melee weapon. Used by bots and monster AI.
	WIF_DEHAMMO	=			0x00010000,	// Uses Doom's original amount of ammo for the respective attack functions so that old DEHACKED patches work as intended.
										// AmmoUse1 will be set to the first attack's ammo use so that checking for empty weapons still works
	WIF_CHEATNOTWEAPON	=	0x08000000,	// Give cheat considers this not a weapon (used by Sigil)

	// Flags used only by bot AI:

	WIF_BOT_REACTION_SKILL_THING = 1<<31, // I don't understand this
	WIF_BOT_EXPLOSIVE =		1<<30,		// weapon fires an explosive
	WIF_BOT_BFG =			1<<28,		// this is a BFG
};

class AWeaponGiver : public AWeapon
{
	DECLARE_CLASS(AWeaponGiver, AWeapon)

public:
	bool TryPickup(AActor *&toucher);
	void Serialize(FArchive &arc);

	fixed_t DropAmmoFactor;
};


// Health is some item that gives the player health when picked up.
class AHealth : public AInventory
{
	DECLARE_CLASS (AHealth, AInventory)

	int PrevHealth;
public:
	virtual bool TryPickup (AActor *&other);
	virtual const char *PickupMessage ();
};

// HealthPickup is some item that gives the player health when used.
class AHealthPickup : public AInventory
{
	DECLARE_CLASS (AHealthPickup, AInventory)
public:
	int autousemode;

	virtual void Serialize (FArchive &arc);
	virtual AInventory *CreateCopy (AActor *other);
	virtual AInventory *CreateTossable ();
	virtual bool HandlePickup (AInventory *item);
	virtual bool Use (bool pickup);
};

// Armor absorbs some damage for the player.
class AArmor : public AInventory
{
	DECLARE_CLASS (AArmor, AInventory)
};

// Basic armor absorbs a specific percent of the damage. You should
// never pickup a BasicArmor. Instead, you pickup a BasicArmorPickup
// or BasicArmorBonus and those gives you BasicArmor when it activates.
class ABasicArmor : public AArmor
{
	DECLARE_CLASS (ABasicArmor, AArmor)
public:
	virtual void Serialize (FArchive &arc);
	virtual void Tick ();
	virtual AInventory *CreateCopy (AActor *other);
	virtual bool HandlePickup (AInventory *item);
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage);

	int AbsorbCount;
	fixed_t SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int BonusCount;
	FNameNoInit ArmorType;
	int ActualSaveAmount;
};

// BasicArmorPickup replaces the armor you have.
class ABasicArmorPickup : public AArmor
{
	DECLARE_CLASS (ABasicArmorPickup, AArmor)
public:
	virtual void Serialize (FArchive &arc);
	virtual AInventory *CreateCopy (AActor *other);
	virtual bool Use (bool pickup);

	fixed_t SavePercent;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;
};

// BasicArmorBonus adds to the armor you have.
class ABasicArmorBonus : public AArmor
{
	DECLARE_CLASS (ABasicArmorBonus, AArmor)
public:
	virtual void Serialize (FArchive &arc);
	virtual AInventory *CreateCopy (AActor *other);
	virtual bool Use (bool pickup);

	fixed_t SavePercent;	// The default, for when you don't already have armor
	int MaxSaveAmount;
	int MaxAbsorb;
	int MaxFullAbsorb;
	int SaveAmount;
	int BonusCount;
	int BonusMax;
};

// Hexen armor consists of four separate armor types plus a conceptual armor
// type (the player himself) that work together as a single armor.
class AHexenArmor : public AArmor
{
	DECLARE_CLASS (AHexenArmor, AArmor)
public:
	virtual void Serialize (FArchive &arc);
	virtual AInventory *CreateCopy (AActor *other);
	virtual AInventory *CreateTossable ();
	virtual bool HandlePickup (AInventory *item);
	virtual void AbsorbDamage (int damage, FName damageType, int &newdamage);

	fixed_t Slots[5];
	fixed_t SlotsIncrement[4];

protected:
	bool AddArmorToSlot (AActor *actor, int slot, int amount);
};

// PuzzleItems work in conjunction with the UsePuzzleItem special
class APuzzleItem : public AInventory
{
	DECLARE_CLASS (APuzzleItem, AInventory)
public:
	void Serialize (FArchive &arc);
	bool ShouldStay ();
	bool Use (bool pickup);
	bool HandlePickup (AInventory *item);

	int PuzzleItemNumber;
};

// A MapRevealer reveals the whole map for the player who picks it up.
class AMapRevealer : public AInventory
{
	DECLARE_CLASS (AMapRevealer, AInventory)
public:
	bool TryPickup (AActor *&toucher);
};

// A backpack gives you one clip of each ammo and doubles your
// normal maximum ammo amounts.
class ABackpackItem : public AInventory
{
	DECLARE_CLASS (ABackpackItem, AInventory)
public:
	void Serialize (FArchive &arc);
	bool HandlePickup (AInventory *item);
	AInventory *CreateCopy (AActor *other);
	AInventory *CreateTossable ();
	void DetachFromOwner ();

	bool bDepleted;
};


// A score item is picked up without being added to the inventory.
// It differs from FakeInventory by doing nothing more than increasing the player's score.
class AScoreItem : public AInventory
{
	DECLARE_CLASS (AScoreItem, AInventory)

public:
	bool TryPickup(AActor *&toucher);
};


#endif //__A_PICKUPS_H__
