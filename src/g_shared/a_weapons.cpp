#include <string.h>

#include "a_pickups.h"
#include "gi.h"
#include "d_player.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_state.h"
#include "p_pspr.h"
#include "c_dispatch.h"
#include "configfile.h"

#define BONUSADD 6

FWeaponInfo *wpnlev1info[NUMWEAPONS], *wpnlev2info[NUMWEAPONS];

int WeaponValue[] =
{
	1,		// fist
	3,		// pistol
	4,		// shotgun
	6,		// chaingun
	7,		// rocket launcher
	8,		// plasma rifle
	9,		// BFG
	2,		// chainsaw
	5,		// supershotgun

	1,		// staff
	3,		// goldwand
	4,		// crossbow
	5,		// blaster
	6,		// skullrod
	7,		// phoenixrod
	8,		// mace
	2,		// gauntlets
	0		// beak
};

int maxammo[NUMAMMO] =
{
	200,		// bullets
	50,			// shells
	300,		// cells
	50,			// rockets

	100,		// gold wand
	50,			// crossbow
	200,		// blaster
	200,		// skull rod
	20,			// phoenix rod
	150,		// mace

	MAX_MANA,	// blue mana
	MAX_MANA	// green mana
};

static weapontype_t GetAmmoChange[] =
{
	wp_nochange,
	wp_nochange,
	wp_nochange,
	wp_nochange,

	wp_goldwand,
	wp_crossbow,
	wp_blaster,
	wp_skullrod,
	wp_phoenixrod,
	wp_mace
};

FState AWeapon::States[] =
{
	S_NORMAL (SHTG, 'E',	0, A_Light0 			, NULL)
};

IMPLEMENT_ACTOR (AWeapon, Any, -1, 0)
END_DEFAULTS

void AWeapon::PlayPickupSound (AActor *toucher)
{
	S_Sound (toucher, CHAN_PICKUP, "misc/w_pkup", 1, ATTN_NORM);
}



//--------------------------------------------------------------------------
//
// FUNC P_GiveAmmo
//
// Returns true if the player accepted the ammo, false if it was
// refused (player has maxammo[ammo]). Unlike Doom, count is not
// the number of clip loads.
//
//--------------------------------------------------------------------------

bool P_GiveAmmo (player_t *player, ammotype_t ammo, int count)
{
	int prevAmmo;

	if (ammo == am_noammo || ammo == MANA_BOTH)
	{
		return false;
	}
	if (ammo < 0 || ammo > NUMAMMO)
	{
		I_Error ("P_GiveAmmo: bad type %i", ammo);
	}
	if (player->ammo[ammo] == player->maxammo[ammo])
	{
		return false;
	}
	if (gameskill == sk_baby || gameskill == sk_nightmare)
	{ // extra ammo in baby mode and nightmare mode
		if (gameinfo.gametype == GAME_Doom)
			count <<= 1;
		else
			count += count>>1;
	}
	prevAmmo = player->ammo[ammo];

	player->ammo[ammo] += count;
	if (player->ammo[ammo] > player->maxammo[ammo])
	{
		player->ammo[ammo] = player->maxammo[ammo];
	}
	if (prevAmmo)
	{
		// Don't attempt to change weapons if the player already had
		// ammo of the type just given
		return true;
	}
	if (!player->userinfo.neverswitch)
	{
		switch (gameinfo.gametype)
		{
		case GAME_Doom:
			switch (ammo)
			{
			case am_clip:
				if (player->readyweapon == wp_fist)
				{
					if (player->weaponowned[wp_chaingun])
						player->pendingweapon = wp_chaingun;
					else
						player->pendingweapon = wp_pistol;
				}
				break;
				
			case am_shell:
				if (player->readyweapon == wp_fist
					|| player->readyweapon == wp_pistol)
				{
					if (player->weaponowned[wp_shotgun])
						player->pendingweapon = wp_shotgun;
				}
				break;
				
			case am_cell:
				if (player->readyweapon == wp_fist
					|| player->readyweapon == wp_pistol)
				{
					if (player->weaponowned[wp_plasma])
						player->pendingweapon = wp_plasma;
				}
				break;
				
			case am_misl:
				if (player->readyweapon == wp_fist)
				{
					if (player->weaponowned[wp_missile])
						player->pendingweapon = wp_missile;
				}
			default:
				break;
			}
			break;

		case GAME_Heretic:
			if ((player->readyweapon == wp_staff
				|| player->readyweapon == wp_gauntlets) && ammo < MANA_1)
			{
				if (player->weaponowned[GetAmmoChange[ammo]])
				{
					player->pendingweapon = GetAmmoChange[ammo];
				}
			}
			break;

		case GAME_Hexen:
			// Hexen never switches weapons when picking up mana
			break;

		default:
			break;	// Silence GCC
		}
	}
	return true;
}

//--------------------------------------------------------------------------
//
// FUNC P_GiveWeapon
//
// Returns true if the weapon or its ammo was accepted.
//
//--------------------------------------------------------------------------

bool AWeapon::TryPickup (AActor *toucher)
{
	bool gaveammo;
	bool gaveweapon;
	weapontype_t weapon = OldStyleID ();
	player_t *player = toucher->player;

	// Only players can pick up weapons
	// [RH] Don't get the weapon if no graphics for it
	if (player == NULL || !wpnlev1info[weapon] || weapon >= NUMWEAPONS)
	{
		return false;
	}

	FState *state = wpnlev1info[weapon]->readystate;
	if (state->GetFrame() >= sprites[state->sprite.index].numframes)
		return false;

	if (multiplayer &&
		((!deathmatch && !alwaysapplydmflags) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags & MF_DROPPED))
	{
		// leave placed weapons forever on (cooperative) net games
		if (player->weaponowned[weapon])
			return false;

		player->bonuscount = BONUSADD;
		player->weaponowned[weapon] = true;

		P_GiveAmmo (player, wpnlev1info[weapon]->ammo,
			(deathmatch && gameinfo.gametype == GAME_Doom) ?
				wpnlev1info[weapon]->ammogive*5/2 :
				wpnlev1info[weapon]->ammogive);

		if (!player->userinfo.neverswitch)
			player->pendingweapon = weapon;

		return true;
	}
		
	if (wpnlev1info[weapon]->ammo != am_noammo)
	{
		if (gameinfo.gametype == GAME_Doom && (flags & MF_DROPPED))
		{ // give one clip with a dropped weapon,
		  // two clips with a found weapon
			gaveammo = P_GiveAmmo (player, wpnlev1info[weapon]->ammo,
				wpnlev1info[weapon]->ammogive / 2);
		}
		else
		{
			gaveammo = P_GiveAmmo (player, wpnlev1info[weapon]->ammo,
				wpnlev1info[weapon]->ammogive);
		}
	}
	else
	{
		gaveammo = false;
	}
		
	if (player->weaponowned[weapon])
	{
		gaveweapon = false;
	}
	else
	{
		gaveweapon = true;
		player->weaponowned[weapon] = true;
		if (!player->userinfo.neverswitch)
			player->pendingweapon = weapon;
	}
		
	return (gaveweapon || gaveammo);
}

bool AWeapon::ShouldStay ()
{
	if (multiplayer &&
		((!deathmatch && !alwaysapplydmflags) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags & MF_DROPPED))
	{
		return true;
	}
	return false;
}

weapontype_t AWeapon::OldStyleID () const
{
	return NUMWEAPONS;
}

/* Weapon slots ***********************************************************/

FWeaponSlot WeaponSlots[NUM_WEAPON_SLOTS];

FWeaponSlot::FWeaponSlot ()
{
	Clear ();
}

void FWeaponSlot::Clear ()
{
	for (int i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		Weapons[i] = NUMWEAPONS;
	}
}

bool FWeaponSlot::AddWeapon (const char *type)
{
	return AddWeapon (TypeInfo::IFindType (type));
}

bool FWeaponSlot::AddWeapon (const TypeInfo *type)
{
	for (int i = 0; i < NUMWEAPONS; ++i)
	{
		if (wpnlev1info[i] != NULL && wpnlev1info[i]->type == type)
		{
			return AddWeapon ((weapontype_t)i);
		}
	}
	return false;
}

bool FWeaponSlot::AddWeapon (weapontype_t weap)
{
	int i;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		if (Weapons[i] == NUMWEAPONS)
			break;
	}
	if (i == MAX_WEAPONS_PER_SLOT)
	{ // This slot is full
		return false;
	}
	Weapons[i] = weap;
	return true;
}

weapontype_t FWeaponSlot::PickWeapon (player_t *player)
{
	int i, j;
	FWeaponInfo **infos;

	infos = (player->powers[pw_weaponlevel2] && deathmatch) ?
		wpnlev2info : wpnlev1info;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		if (Weapons[i] == player->readyweapon)
		{
			for (j = (unsigned)(i - 1) % MAX_WEAPONS_PER_SLOT;
				 j != i;
				 j = (unsigned)(j - 1) % MAX_WEAPONS_PER_SLOT)
			{
				int weap = Weapons[j];

				if (weap >= NUMWEAPONS
					|| !player->weaponowned[weap]
					|| (infos[weap]->ammo < NUMAMMO &&
						player->ammo[infos[weap]->ammo] < infos[weap]->GetMinAmmo()))
				{
					continue;
				}
				break;
			}
			return (weapontype_t)Weapons[j];
		}
	}
	for (i = MAX_WEAPONS_PER_SLOT - 1; i >= 0; i--)
	{
		weapontype_t weap = (weapontype_t)Weapons[i];

		if (weap >= NUMWEAPONS
			|| !player->weaponowned[weap]
			|| (infos[weap]->ammo < NUMAMMO &&
				player->ammo[infos[weap]->ammo] < infos[weap]->GetMinAmmo()))
		{
			continue;
		}
		return weap;
	}
	return player->readyweapon;
}

bool FWeaponSlot::LocateWeapon (weapontype_t weap, int *const slot, int *const index)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		for (j = 0; j < MAX_WEAPONS_PER_SLOT; j++)
		{
			if (WeaponSlots[i].Weapons[j] == weap)
			{
				*slot = i;
				*index = j;
				return true;
			}
			else if (WeaponSlots[i].Weapons[j] == NUMWEAPONS)
			{ // No more weapons in this slot, so try the next
				break;
			}
		}
	}
	return false;
}

static bool FindMostRecentWeapon (player_s *player, int *slot, int *index)
{
	if (player->pendingweapon != wp_nochange)
	{
		if (player->psprites[ps_weapon].state->GetAction().acp2 == A_Raise)
		{
			if (FWeaponSlot::LocateWeapon (player->pendingweapon, slot, index))
			{
				P_SetPsprite (player, ps_weapon,
					player->powers[pw_weaponlevel2] ?
					  wpnlev2info[player->readyweapon]->downstate :
					  wpnlev1info[player->readyweapon]->downstate);
				return true;
			}
			return false;
		}
		else
		{
			return FWeaponSlot::LocateWeapon (player->pendingweapon, slot, index);
		}
	}
	else
	{
		return FWeaponSlot::LocateWeapon (player->readyweapon, slot, index);
	}
}

weapontype_t PickNextWeapon (player_s *player)
{
	int startslot, startindex;

	if (FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int start = startslot * MAX_WEAPONS_PER_SLOT + startindex;
		int i;
		FWeaponInfo **infos;
		
		infos = (player->powers[pw_weaponlevel2] && deathmatch) ?
			wpnlev2info : wpnlev1info;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = (unsigned)((start + i) / MAX_WEAPONS_PER_SLOT) % NUM_WEAPON_SLOTS;
			int index = (unsigned)(start + i) % MAX_WEAPONS_PER_SLOT;
			int weap = WeaponSlots[slot].Weapons[index];

			if (weap >= NUMWEAPONS
				|| !player->weaponowned[weap]
				|| (infos[weap]->ammo < NUMAMMO &&
					player->ammo[infos[weap]->ammo] < infos[weap]->GetMinAmmo()))
			{
				continue;
			}
			return (weapontype_t)weap;
		}
	}
	return player->readyweapon;
}

weapontype_t PickPrevWeapon (player_s *player)
{
	int startslot, startindex;

	if (FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int start = startslot * MAX_WEAPONS_PER_SLOT + startindex;
		int i;
		FWeaponInfo **infos;
		
		infos = (player->powers[pw_weaponlevel2] && deathmatch) ?
			wpnlev2info : wpnlev1info;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = start - i;
			if (slot < 0)
				slot += NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT;
			int index = slot % MAX_WEAPONS_PER_SLOT;
			slot /= MAX_WEAPONS_PER_SLOT;
			int weap = WeaponSlots[slot].Weapons[index];

			if (weap >= NUMWEAPONS
				|| !player->weaponowned[weap]
				|| (infos[weap]->ammo < NUMAMMO &&
					player->ammo[infos[weap]->ammo] < infos[weap]->GetMinAmmo()))
			{
				continue;
			}
			return (weapontype_t)weap;
		}
	}
	return player->readyweapon;
}

CCMD (setslot)
{
	int slot, i;

	if (argv.argc() < 2 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: setslot [slot] [weapons]\nCurrent slot assignments:\n");
		for (slot = 0; slot < NUM_WEAPON_SLOTS; ++slot)
		{
			Printf (" Slot %d:", slot);
			for (i = 0;
				i < MAX_WEAPONS_PER_SLOT && WeaponSlots[slot].GetWeapon(i) < NUMWEAPONS;
				++i)
			{
				Printf (" %s", wpnlev1info[WeaponSlots[slot].GetWeapon(i)]->type->Name+1);
			}
			Printf ("\n");
		}
		return;
	}

	WeaponSlots[slot].Clear();
	if (argv.argc() == 2)
	{
		Printf ("Slot %d cleared\n", slot);
	}
	else
	{
		for (i = 2; i < argv.argc(); ++i)
		{
			if (!WeaponSlots[slot].AddWeapon (argv[i]))
			{
				Printf ("Could not add %s to slot %d\n", argv[i], slot);
			}
		}
	}
}

CCMD (addslot)
{
	size_t slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslot <slot> <weapon>\n");
		return;
	}

	if (!WeaponSlots[slot].AddWeapon (argv[2]))
	{
		Printf ("Could not add %s to slot %d\n", argv[2], slot);
	}
}

void FWeaponSlot::StaticRestoreSlots (FConfigFile &config)
{
	char buff[MAX_WEAPONS_PER_SLOT*64];
	const char *key, *value;

	buff[sizeof(buff)-1] = 0;
	while (config.NextInSection (key, value))
	{
		if (strnicmp (key, "Slot[", 5) != 0 ||
			key[5] < '0' ||
			key[5] > '0'+NUM_WEAPON_SLOTS ||
			key[6] != ']' ||
			key[7] != 0)
		{
			continue;
		}
		int slot = key[5] - '0';
		strncpy (buff, value, sizeof(buff)-1);
		char *tok;

		WeaponSlots[slot].Clear ();
		tok = strtok (buff, " ");
		while (tok != NULL)
		{
			WeaponSlots[slot].AddWeapon (tok);
			tok = strtok (NULL, " ");
		}
	}
}

void FWeaponSlot::StaticSaveSlots (FConfigFile &config)
{
	char buff[MAX_WEAPONS_PER_SLOT*64];
	char keyname[16];

	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		int index = 0;

		for (int j = 0; j < MAX_WEAPONS_PER_SLOT; ++j)
		{
			if (WeaponSlots[i].Weapons[j] >= NUMWEAPONS)
			{
				break;
			}
			if (index > 0)
			{
				buff[index++] = ' ';
			}
			const char *name = wpnlev1info[WeaponSlots[i].Weapons[j]]->type->Name+1;
			strcpy (buff+index, name);
			index += (int)strlen (name);
		}
		if (index > 0)
		{
			sprintf (keyname, "Slot[%d]", i);
			config.SetValueForKey (keyname, buff);
		}
	}
}
