#include "a_pickups.h"
#include "gi.h"
#include "d_player.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_state.h"
#include "p_pspr.h"

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
	200,	// bullets
	50,		// shells
	300,	// cells
	50,		// rockets

	100,	// gold wand
	50,		// crossbow
	200,	// blaster
	200,	// skull rod
	20,		// phoenix rod
	150		// mace
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
	S_Sound (toucher, CHAN_ITEM, "misc/w_pkup", 1, ATTN_NORM);
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
	if (*gameskill == sk_baby || *gameskill == sk_nightmare)
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
		if (player->readyweapon == wp_staff
			|| player->readyweapon == wp_gauntlets)
		{
			if (player->weaponowned[GetAmmoChange[ammo]])
			{
				player->pendingweapon = GetAmmoChange[ammo];
			}
		}
		break;

	case GAME_Hexen:
		break;
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

bool P_GiveWeapon (player_t *player, weapontype_t weapon, BOOL dropped)
{
	bool gaveammo;
	bool gaveweapon;

	// [RH] Don't get the weapon if no graphics for it
	if (!wpnlev1info[weapon])
		return false;
	FState *state = wpnlev1info[weapon]->readystate;
	if (state->frame >= sprites[state->sprite.index].numframes)
		return false;

	if (multiplayer &&
		((!*deathmatch && !*alwaysapplydmflags) || *dmflags & DF_WEAPONS_STAY) &&
		!dropped)
	{
		// leave placed weapons forever on (cooperative) net games
		if (player->weaponowned[weapon])
			return false;

		player->bonuscount = BONUSADD;
		player->weaponowned[weapon] = true;

		P_GiveAmmo (player, wpnlev1info[weapon]->ammo,
			(*deathmatch && gameinfo.gametype == GAME_Doom) ?
				wpnlev1info[weapon]->ammogive*5/2 :
				wpnlev1info[weapon]->ammogive);

		if (!player->userinfo.neverswitch)
			player->pendingweapon = weapon;

		S_Sound (player->mo, CHAN_ITEM, "misc/w_pkup", 1, ATTN_NORM);
		return false;
	}
		
	if (wpnlev1info[weapon]->ammo != am_noammo)
	{
		if (gameinfo.gametype == GAME_Doom && dropped)
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

/* Weapon slots ***********************************************************/

FWeaponSlot WeaponSlots[NUM_WEAPON_SLOTS];

FWeaponSlot::FWeaponSlot ()
{
	int i;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		Weapons[i].Weapon = NUMWEAPONS;
		Weapons[i].Priority = 0;
	}
}

bool FWeaponSlot::AddWeapon (weapontype_t weap, int priority)
{
	int i;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		if (Weapons[i].Weapon == weap)
		{
			if (i > 0)
			{
				memmove (&Weapons[1], &Weapons[0], sizeof(Weapons[0]) * i);
			}
			Weapons[0].Weapon = NUMWEAPONS;
			Weapons[i].Priority = 0;
			break;
		}
	}
	for (i = MAX_WEAPONS_PER_SLOT - 1; i >= 0; i--)
	{
		if (Weapons[i].Priority <= priority)
		{
			if (i > 0)
			{
				memmove (&Weapons[0], &Weapons[1], sizeof(Weapons[0]) * i);
			}
			Weapons[i].Priority = priority;
			Weapons[i].Weapon = weap;
			return true;
		}
	}
	return false;
}

weapontype_t FWeaponSlot::PickWeapon (player_t *player)
{
	int i, j;
	FWeaponInfo **infos;

	infos = (player->powers[pw_weaponlevel2] && *deathmatch) ?
		wpnlev2info : wpnlev1info;

	for (i = 0; i < MAX_WEAPONS_PER_SLOT; i++)
	{
		if (Weapons[i].Weapon == player->readyweapon)
		{
			for (j = (unsigned)(i - 1) % MAX_WEAPONS_PER_SLOT;
				 j != i;
				 j = (unsigned)(j - 1) % MAX_WEAPONS_PER_SLOT)
			{
				int weap = Weapons[j].Weapon;

				if (weap >= NUMWEAPONS
					|| !player->weaponowned[weap]
					|| (infos[weap]->ammo < NUMAMMO &&
						player->ammo[infos[weap]->ammo] < infos[weap]->ammouse))
				{
					continue;
				}
				break;
			}
			return (weapontype_t)Weapons[j].Weapon;
		}
	}
	for (i = MAX_WEAPONS_PER_SLOT - 1; i >= 0; i--)
	{
		weapontype_t weap = (weapontype_t)Weapons[i].Weapon;

		if (weap >= NUMWEAPONS
			|| !player->weaponowned[weap]
			|| (infos[weap]->ammo < NUMAMMO &&
				player->ammo[infos[weap]->ammo] < infos[weap]->ammouse))
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
			if (WeaponSlots[i].Weapons[j].Weapon == weap)
			{
				*slot = i;
				*index = j;
				return true;
			}
		}
	}
	return false;
}

static bool FindMostRecentWeapon (player_s *player, int *slot, int *index)
{
	if (player->pendingweapon != wp_nochange)
	{
		if (player->psprites[ps_weapon].state->action.acp2 == A_Raise)
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
		
		infos = (player->powers[pw_weaponlevel2] && *deathmatch) ?
			wpnlev2info : wpnlev1info;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = (unsigned)((start + i) / MAX_WEAPONS_PER_SLOT) % NUM_WEAPON_SLOTS;
			int index = (unsigned)(start + i) % MAX_WEAPONS_PER_SLOT;
			int weap = WeaponSlots[slot].Weapons[index].Weapon;

			if (weap >= NUMWEAPONS
				|| !player->weaponowned[weap]
				|| (infos[weap]->ammo < NUMAMMO &&
					player->ammo[infos[weap]->ammo] < infos[weap]->ammouse))
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
		
		infos = (player->powers[pw_weaponlevel2] && *deathmatch) ?
			wpnlev2info : wpnlev1info;

		for (i = 1; i < NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT + 1; i++)
		{
			int slot = start - i;
			if (slot < 0)
				slot += NUM_WEAPON_SLOTS * MAX_WEAPONS_PER_SLOT;
			int index = slot % MAX_WEAPONS_PER_SLOT;
			slot /= MAX_WEAPONS_PER_SLOT;
			int weap = WeaponSlots[slot].Weapons[index].Weapon;

			if (weap >= NUMWEAPONS
				|| !player->weaponowned[weap]
				|| (infos[weap]->ammo < NUMAMMO &&
					player->ammo[infos[weap]->ammo] < infos[weap]->ammouse))
			{
				continue;
			}
			return (weapontype_t)weap;
		}
	}
	return player->readyweapon;
}
