#ifndef __A_PICKUPS_H__
#define __A_PICKUPS_H__

#include "dobject.h"
#include "actor.h"
#include "info.h"

#define STREAM_ENUM(e) \
	inline FArchive &operator<< (FArchive &arc, e &i) \
	{ \
		BYTE val = (BYTE)i; \
		arc << val; \
		i = (e)val; \
		return arc; \
	} \

// Ammunition types defined.
typedef enum
{
// Doom ammo
	am_clip,	// Pistol / chaingun ammo.
	am_shell,	// Shotgun / double barreled shotgun.
	am_cell,	// Plasma rifle, BFG.
	am_misl,	// Missile launcher.

// Heretic ammo
 	am_goldwand,
	am_crossbow,
	am_blaster,
	am_skullrod,
	am_phoenixrod,
	am_mace,

// Hexen ammo
	MANA_1,
	MANA_2,

// Strife ammo
	// am_clip
	am_electricbolt,
	am_poisonbolt,
	// am_cell
	// am_misl
	am_hegrenade,
	am_phgrenade,

	NUMAMMO,

	MANA_BOTH,
	am_noammo,	// Unlimited for chainsaw / fist.
	MANA_NONE = am_noammo,

	SAVEVER217_NUMAMMO = MANA_2+1

} ammotype_t;

STREAM_ENUM (ammotype_t)

#define MAX_MANA	200

extern const char *AmmoPics[MANA_BOTH+1];

typedef enum
{
	ARMOR_ARMOR = 0,
	ARMOR_SHIELD,
	ARMOR_HELMET,
	ARMOR_AMULET,
	NUMARMOR
} armortype_t;

extern const char *ArmorPics[NUMARMOR];

STREAM_ENUM (armortype_t)

// LUT of ammunition limits for each kind.
// This doubles with BackPack powerup item.
extern int maxammo[NUMAMMO];

// The defined weapons,
//	including a marker indicating
//	user has not changed weapon.
typedef enum
{
// Doom weapons
	wp_fist,
	wp_pistol,
	wp_shotgun,
	wp_chaingun,
	wp_missile,
	wp_plasma,
	wp_bfg,
	wp_chainsaw,
	wp_supershotgun,

// Heretic weapons
	wp_staff,
	wp_goldwand,
	wp_crossbow,
	wp_blaster,
	wp_skullrod,
	wp_phoenixrod,
	wp_mace,
	wp_gauntlets,
	wp_beak,

// Hexen weapons
	wp_snout,
	wp_ffist,
	wp_cmace,
	wp_mwand,
	wp_faxe,
	wp_cstaff,
	wp_mfrost,
	wp_fhammer,
	wp_cfire,
	wp_mlightning,
	wp_fsword,
	wp_choly,
	wp_mstaff,

// Strife weapons
	wp_dagger,
	wp_electricxbow,
	wp_assaultgun,
	wp_minimissile,
	wp_hegrenadelauncher,
	wp_flamethrower,
	wp_maulerscatter,
	wp_sigil,
	wp_poisonxbow,
	wp_phgrenadelauncher,
	wp_maulertorpedo,

	NUMWEAPONS,
	
	wp_nochange,		// No pending weapon change.

	SAVEVER217_NUMWEAPONS = wp_mstaff+1

} weapontype_t;

STREAM_ENUM (weapontype_t)

// Weapon info: sprite frames, ammunition use.
struct FWeaponInfo
{
	DWORD			flags;
	ammotype_t		ammo;
	ammotype_t		givingammo;
	int				ammouse;
	int				ammogive;
	FState			*upstate;
	FState			*downstate;
	FState			*readystate;
	FState			*atkstate;
	FState			*holdatkstate;
	FState			*flashstate;
	const TypeInfo	*droptype;
	int				kickback;
	fixed_t			yadjust;
	const char		*upsound;
	const char		*readysound;
	TypeInfo		*type;		// type of actor that represents this weapon
	int				minammo;	// minimum ammo needed to switch to this weapon

	int GetMinAmmo () const
	{
		return (minammo < 0) ? ammouse : minammo;
	}
};

enum
{
	WIF_NOAUTOFIRE =		0x00000001, // weapon does not autofire
	WIF_READYSNDHALF =		0x00000002, // ready sound is played ~1/2 the time
	WIF_DONTBOB =			0x00000004, // don't bob the weapon
	WIF_AXEBLOOD =			0x00000008, // weapon makes axe blood on impact (Hexen only)
	WIF_FIREDAMAGE =		0x00000010, // weapon does fire damage on impact
	WIF_NOALERT =			0x00000020,	// weapon does not alert monsters
	WIF_AMMO_OPTIONAL =		0x00000040, // weapon can use ammo but does not require it
};

#define MAX_WEAPONS_PER_SLOT	8
#define NUM_WEAPON_SLOTS		10

class player_s;
class FConfigFile;

class FWeaponSlot
{
public:
	FWeaponSlot ();
	void Clear ();
	bool AddWeapon (const char *type);
	bool AddWeapon (const TypeInfo *type);
	bool AddWeapon (weapontype_t weap);
	weapontype_t PickWeapon (player_s *player);
	int CountWeapons ();
	void StreamOut ();
	void StreamIn (byte **stream);

	inline weapontype_t GetWeapon (int index) const
	{
		return (weapontype_t)Weapons[index];
	}

	friend weapontype_t PickNextWeapon (player_s *player);
	friend weapontype_t PickPrevWeapon (player_s *player);

	friend struct FWeaponSlots;

private:
	byte Weapons[MAX_WEAPONS_PER_SLOT];
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
	FWeaponSlot Slots[NUM_WEAPON_SLOTS];

	void Clear ();
	bool LocateWeapon (weapontype_t weap, int *const slot, int *const index);
	ESlotDef AddDefaultWeapon (int slot, const char *weapName, weapontype_t weap);
	void RestoreSlots (FConfigFile &config);
	void SaveSlots (FConfigFile &config);
	void StreamOutSlots ();
	void StreamInSlots (byte **stream);
};

extern FWeaponSlots LocalWeapons;
extern FWeaponInfo *wpnlev1info[NUMWEAPONS], *wpnlev2info[NUMWEAPONS];

//
// Keys
//
typedef enum
{
// Heretic keys
	key_blue = 0,
	key_yellow,
	key_green,
	
// Doom keys
	it_bluecard = 0,
	it_yellowcard,
	it_redcard,
	it_blueskull,
	it_yellowskull,
	it_redskull,

// Hexen keys
	KEY_STEEL = 0,
	KEY_CAVE,
	KEY_AXE,
	KEY_FIRE,
	KEY_EMERALD,
	KEY_DUNGEON,
	KEY_SILVER,
	KEY_RUSTED,
	KEY_HORN,
	KEY_SWAMP,
	KEY_CASTLE,

	NUMKEYS
	
} keytype_t;

STREAM_ENUM (keytype_t)

/************************************************************************/
/* Class definitions													*/
/************************************************************************/

// A pickup is anything the player can pickup (i.e. weapons, ammo,
// powerups, etc)
class AInventory : public AActor
{
	DECLARE_ACTOR (AInventory, AActor)
public:
	virtual void Touch (AActor *toucher);

	virtual void BeginPlay ();
	virtual bool ShouldRespawn ();
	virtual bool ShouldStay ();
	virtual void Hide ();
	virtual bool DoRespawn ();
	virtual bool TryPickup (AActor *toucher);
	virtual void DoPickupSpecial (AActor *toucher);

	virtual const char *PickupMessage ();
	virtual void PlayPickupSound (AActor *toucher);
private:
	static int StaticLastMessageTic;
	static const char *StaticLastMessage;
};

// A weapon is just that.
class AWeapon : public AInventory
{
	DECLARE_ACTOR (AWeapon, AInventory)
public:
	virtual bool TryPickup (AActor *toucher);
	virtual weapontype_t OldStyleID() const;
protected:
	virtual void PlayPickupSound (AActor *toucher);
	virtual bool ShouldStay ();
};
#define S_LIGHTDONE 0

// Health is some item that gives the player health when picked up.
class AHealth : public AInventory
{
	DECLARE_CLASS (AHealth, AInventory)
protected:
	AHealth () {}
	virtual void PlayPickupSound (AActor *toucher);
};

// Armor gives the player armor when picked up.
class AArmor : public AInventory
{
	DECLARE_CLASS (AArmor, AInventory)
protected:
	AArmor () {}
	virtual void PlayPickupSound (AActor *toucher);
};

class AAmmo : public AInventory
{
	DECLARE_CLASS (AAmmo, AInventory)
public:
	virtual ammotype_t GetAmmoType () const;
protected:
	AAmmo () {}
	virtual void PlayPickupSound (AActor *toucher);
};

// A key is something the player can use to unlock something
class AKey : public AInventory
{
	DECLARE_CLASS (AKey, AInventory)
public:
	virtual bool TryPickup (AActor *toucher);
protected:
	virtual bool ShouldStay ();
	virtual void PlayPickupSound (AActor *toucher);
	virtual keytype_t GetKeyType ();
	AKey () {}
};

#undef STREAM_ENUM

#endif //__A_PICKUPS_H__
