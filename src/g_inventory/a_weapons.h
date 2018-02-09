#pragma once

#include "a_pickups.h"
class PClassActor;
class AWeapon;

class FWeaponSlot
{
public:
	FWeaponSlot() { Clear(); }
	FWeaponSlot(const FWeaponSlot &other) { Weapons = other.Weapons; }
	FWeaponSlot &operator= (const FWeaponSlot &other) { Weapons = other.Weapons; return *this; }
	void Clear() { Weapons.Clear(); }
	bool AddWeapon (const char *type);
	bool AddWeapon (PClassActor *type);
	void AddWeaponList (const char *list, bool clear);
	AWeapon *PickWeapon (player_t *player, bool checkammo = false);
	int Size () const { return (int)Weapons.Size(); }
	int LocateWeapon (PClassActor *type);

	inline PClassActor *GetWeapon (int index) const
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
		PClassActor *Type;
		int Position;
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
	bool LocateWeapon (PClassActor *type, int *const slot, int *const index);
	ESlotDef AddDefaultWeapon (int slot, PClassActor *type);
	void AddExtraWeapons();
	void SetFromGameInfo();
	void SetFromPlayer(PClassActor *type);
	void StandardSetup(PClassActor *type);
	void LocalSetup(PClassActor *type);
	void SendDifferences(int playernum, const FWeaponSlots &other);
	int RestoreSlots (FConfigFile *config, const char *section);
	void PrintSettings();

	void AddSlot(int slot, PClassActor *type, bool feedback);
	void AddSlotDefault(int slot, PClassActor *type, bool feedback);

};

void P_PlaybackKeyConfWeapons(FWeaponSlots *slots);
void Net_WriteWeapon(PClassActor *type);
PClassActor *Net_ReadWeapon(uint8_t **stream);

void P_SetupWeapons_ntohton();
void P_WriteDemoWeaponsChunk(uint8_t **demo);
void P_ReadDemoWeaponsChunk(uint8_t **demo);


class AWeapon : public AStateProvider
{
	DECLARE_CLASS(AWeapon, AStateProvider)
	HAS_OBJECT_POINTERS
public:
	uint32_t WeaponFlags;
	PClassActor *AmmoType1, *AmmoType2;		// Types of ammo used by this weapon
	int AmmoGive1, AmmoGive2;				// Amount of each ammo to get when picking up weapon
	int MinAmmo1, MinAmmo2;					// Minimum ammo needed to switch to this weapon
	int AmmoUse1, AmmoUse2;					// How much ammo to use with each shot
	int Kickback;
	float YAdjust;							// For viewing the weapon fullscreen (visual only so no need to be a double)
	FSoundIDNoInit UpSound, ReadySound;		// Sounds when coming up and idle
	PClassActor *SisterWeaponType;			// Another weapon to pick up with this one
	PClassActor *ProjectileType;			// Projectile used by primary attack
	PClassActor *AltProjectileType;			// Projectile used by alternate attack
	int SelectionOrder;						// Lower-numbered weapons get picked first
	int MinSelAmmo1, MinSelAmmo2;			// Ignore in BestWeapon() if inadequate ammo
	double MoveCombatDist;					// Used by bots, but do they *really* need it?
	int ReloadCounter;						// For A_CheckForReload
	int BobStyle;							// [XA] Bobbing style. Defines type of bobbing (e.g. Normal, Alpha)  (visual only so no need to be a double)
	float BobSpeed;							// [XA] Bobbing speed. Defines how quickly a weapon bobs.
	float BobRangeX, BobRangeY;				// [XA] Bobbing range. Defines how far a weapon bobs in either direction.
	int SlotNumber;
	int SlotPriority;

	// In-inventory instance variables
	TObjPtr<AInventory*> Ammo1, Ammo2;
	TObjPtr<AWeapon*> SisterWeapon;
	float FOVScale;
	int Crosshair;							// 0 to use player's crosshair
	bool GivenAsMorphWeapon;

	bool bAltFire;	// Set when this weapon's alternate fire is used.

	virtual void MarkPrecacheSounds() const;
	
	void Finalize(FStateDefinitions &statedef) override;
	void Serialize(FSerializer &arc) override;

	void PostMorphWeapon();

	// scripted virtuals.
	FState *GetUpState ();
	FState *GetDownState ();
	FState *GetReadyState ();
	
	FState *GetStateForButtonName (FName button);

	enum
	{
		PrimaryFire,
		AltFire,
		EitherFire
	};
	bool CheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo=false, int ammocount = -1);
	bool DoCheckAmmo(int fireMode, bool autoSwitch, bool requireAmmo, int ammocount);
	bool DepleteAmmo (bool altFire, bool checkEnough=true, int ammouse = -1);
	bool DoDepleteAmmo(bool altFire, bool checkEnough, int ammouse);

	enum
	{
		BobNormal,
		BobInverse,
		BobAlpha,
		BobInverseAlpha,
		BobSmooth,
		BobInverseSmooth
	};

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
	WIF_NODEATHDESELECT =	0x00020000, // Don't jump to the Deselect state when the player dies
	WIF_NODEATHINPUT =		0x00040000, // The weapon cannot be fired/reloaded/whatever when the player is dead
	WIF_CHEATNOTWEAPON	=	0x08000000,	// Give cheat considers this not a weapon (used by Sigil)

	// Flags used only by bot AI:

	WIF_BOT_REACTION_SKILL_THING = 1<<31, // I don't understand this
	WIF_BOT_EXPLOSIVE =		1<<30,		// weapon fires an explosive
	WIF_BOT_BFG =			1<<28,		// this is a BFG
};

