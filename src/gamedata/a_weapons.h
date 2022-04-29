#pragma once

#include "a_pickups.h"
class PClassActor;

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

private:
	FWeaponSlot Slots[NUM_WEAPON_SLOTS];
public:

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
	static void SetupWeaponSlots(AActor *pp);

	void AddSlot(int slot, PClassActor *type, bool feedback);
	void AddSlotDefault(int slot, PClassActor *type, bool feedback);
	// Abstract access interface to the slots

	void AddWeapon(int slot, PClassActor *type)
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			Slots[slot].AddWeapon(type);
		}
	}

	void AddWeapon(int slot, const char *type)
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			Slots[slot].AddWeapon(type);
		}
	}

	void ClearSlot(int slot)
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			Slots[slot].Weapons.Clear();
		}
	}

	int SlotSize(int slot) const
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			return Slots[slot].Weapons.Size();
		}
		return 0;
	}

	PClassActor *GetWeapon(int slot, int index) const
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS && (unsigned)index < Slots[slot].Weapons.Size())
		{
			return Slots[slot].GetWeapon(index);
		}
		return nullptr;
	}
};

void P_PlaybackKeyConfWeapons(FWeaponSlots *slots);
void Net_WriteWeapon(PClassActor *type);
PClassActor *Net_ReadWeapon(uint8_t **stream);

void P_SetupWeapons_ntohton();
void P_WriteDemoWeaponsChunk(uint8_t **demo);
void P_ReadDemoWeaponsChunk(uint8_t **demo);


enum class EBobStyle
{
	BobNormal,
	BobInverse,
	BobAlpha,
	BobInverseAlpha,
	BobSmooth,
	BobInverseSmooth
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
	WIF_NODEATHDESELECT =	0x00010000, // Don't jump to the Deselect state when the player dies
	WIF_NODEATHINPUT =		0x00020000, // The weapon cannot be fired/reloaded/whatever when the player is dead
	WIF_CHEATNOTWEAPON	=	0x00040000,	// Give cheat considers this not a weapon (used by Sigil)
	WIF_NOAUTOSWITCHTO =	0x00080000, // cannot be switched to when autoswitching weapons.
};

