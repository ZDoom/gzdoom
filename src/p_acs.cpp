/*
** p_acs.cpp
** General BEHAVIOR management and ACS execution environment
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** This code makes lots of little-endian assumptions. If ZDoom ever gets
** ported to a big-endian system, you'll need to go through this file with
** a fine tooth comb.
*/

#include <assert.h>

#include "templates.h"
#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"
#include "g_level.h"
#include "s_sound.h"
#include "p_acs.h"
#include "p_saveg.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "doomstat.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "s_sndseq.h"
#include "i_system.h"
#include "i_movie.h"
#include "sbar.h"
#include "vectors.h"
#include "m_swap.h"
#include "a_sharedglobal.h"
#include "a_doomglobal.h"
#include "v_video.h"
#include "w_wad.h"

FRandom pr_acs ("ACS");

// I imagine this much stack space is probably overkill, but it could
// potentially get used with recursive functions.
#define STACK_SIZE 4096

#define CLAMPCOLOR(c)	(EColorRange)((unsigned)(c)>CR_UNTRANSLATED?CR_UNTRANSLATED:(c))
#define HUDMSG_LOG		(0x80000000)
#define LANGREGIONMASK	MAKE_ID(0,0,0xff,0xff)	

struct CallReturn
{
	int ReturnAddress;
	ScriptFunction *ReturnFunction;
	FBehavior *ReturnModule;
	BYTE bDiscardResult;
	BYTE Pad[3];
};

static SDWORD Stack[STACK_SIZE];

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code, FBehavior *module,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay);

struct FBehavior::ArrayInfo
{
	DWORD ArraySize;
	SDWORD *Elements;
};

TArray<FBehavior *> FBehavior::StaticModules;

//---- Temporary inventory functions --------------------------------------//
// to be simplified dramatically once I have a real inventory system
//
#include "gi.h"

static void DoClearInv (player_t *player)
{
	int i;

	memset (player->inventory, 0, sizeof(player->inventory));
	memset (player->weaponowned, 0, sizeof(player->weaponowned));
	memset (player->keys, 0, sizeof(player->keys));
	memset (player->ammo, 0, sizeof(player->ammo));
	if (player->backpack)
	{
		player->backpack = false;
		for (i = 0; i < NUMAMMO; i++)
		{
			player->maxammo[i] /= 2;
		}
	}
	player->pendingweapon = NUMWEAPONS;
}

static void ClearInventory (AActor *activator)
{
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoClearInv (&players[i]);
		}
	}
	else if (activator->player != NULL)
	{
		DoClearInv (activator->player);
	}
}

static const char *AmmoNames[NUMAMMO] =
{
	"Clip", "Shell", "Cell", "RocketAmmo",
	"GoldWandAmmo", "CrossbowAmmo", "BlasterAmmo",
	"SkullRodAmmo", "PhoenixRodAmmo", "MaceAmmo",
	"Mana1", "Mana2"
};

static const char *WeaponNames[NUMWEAPONS] =
{
	"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher",
	"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun",
	"Staff", "GoldWand", "Crossbow", "Blaster", "SkullRod",
	"PhoenixRod", "Mace", "Gauntlets", "Beak",
	"Snout",
	"FWeapFist", "CWeapMace", "MWeapWand",
	"FWeapAxe", "CWeapStaff", "MWeapFrost",
	"FWeapHammer", "CWeapFlame", "MWeapLightning",
	"FWeapQuietus", "CWeapWraithverge", "MWeapBloodscourge"
};

static const char *HereticKeyNames[3] =
{
	"KeyBlue", "KeyYellow", "KeyGreen",
};
static const char *DoomKeyNames[6] =
{
	"BlueCard", "YellowCard", "RedCard",
	"BlueSkull", "YellowSkull", "RedSkull"
};

static void DoGiveInv (player_t *player, const char *type, int amount)
{
	int i;
	weapontype_t savedpendingweap = player->pendingweapon;

	if (strcmp (type, "Armor") == 0)
	{
		player->armorpoints[0] = MIN (player->armorpoints[0] + amount, deh.MaxArmor);
	}

	for (i = 0; i < NUMAMMO; ++i)
	{
		if (strcmp (AmmoNames[i], type) == 0)
		{
			player->ammo[i] = MIN(player->ammo[i]+amount, player->maxammo[i]);
			return;
		}
	}
	for (i = 0; i < NUMARTIFACTS; ++i)
	{
		if (strcmp (ArtifactNames[i], type) == 0)
		{
			int maxCount;

			if (i >= arti_firstpuzzitem)
			{
				maxCount = 1;
			}
			else if (gameinfo.gametype == GAME_Heretic)
			{
				maxCount = 16;
			}
			else
			{
				maxCount = 25;
			}
			player->inventory[i] = MIN(player->inventory[i]+amount, maxCount);
			return;
		}
	}
	const TypeInfo *info = TypeInfo::FindType (type);
	if (info == NULL)
	{
		Printf ("I don't know what %s is.\n", type);
	}
	else if (!info->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		Printf ("%s is not an inventory item.\n", type);
	}
	else
	{
		do
		{
			AInventory *item = static_cast<AInventory *>(Spawn
				(info, player->mo->x, player->mo->y, player->mo->z));
			item->TryPickup (player->mo);
			if (!(item->ObjectFlags & OF_MassDestruction))
			{
				item->Destroy ();
			}
		} while (--amount > 0);
	}

	// If the item was a weapon, don't bring it up automatically
	player->pendingweapon = savedpendingweap;
}

static void GiveInventory (AActor *activator, const char *type, int amount)
{
	if (amount <= 0)
	{
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoGiveInv (&players[i], type, amount);
		}
	}
	else if (activator->player != NULL)
	{
		DoGiveInv (activator->player, type, amount);
	}
}

static void TakeWeapon (player_t *player, int weapon)
{
	player->weaponowned[weapon] = false;
	if (player->readyweapon == weapon || player->pendingweapon == weapon)
	{
		P_PickNewWeapon (player);
	}
}

static void TakeAmmo (player_t *player, int ammo, int amount)
{
	if (amount == 0)
	{
		player->ammo[ammo] = 0;
	}
	else
	{
		player->ammo[ammo] = MAX(player->ammo[ammo]-amount, 0);
	}
	if (player->pendingweapon != wp_nochange)
	{ // Make sure we have the ammo for the weapon being switched to
		weapontype_t readynow = player->readyweapon;
		player->readyweapon = player->pendingweapon;
		player->pendingweapon = wp_nochange;
		if (P_CheckAmmo (player))
		{ // There was enough ammo for the pending weapon, so keep switching
			player->pendingweapon = player->readyweapon;
			player->readyweapon = readynow;
		}
		else
		{
			player->pendingweapon = player->readyweapon = readynow;
			P_CheckAmmo (player);
		}
	}
	else
	{ // Make sure we still have enough ammo for the current weapon
		P_CheckAmmo (player);
	}
}

static void TakeBackpack (player_t *player)
{
	if (!player->backpack)
		return;

	player->backpack = false;
	for (int i = 0; i < NUMAMMO; ++i)
	{
		player->maxammo[i] /= 2;
		if (player->ammo[i] > player->maxammo[i])
		{
			player->ammo[i] = player->maxammo[i];
		}
	}
}

static void DoTakeInv (player_t *player, const char *type, int amount)
{
	int i;

	if (strcmp (type, "Armor") == 0)
	{
		player->armorpoints[0] = MAX (0, player->armorpoints[0] - amount);
	}

	for (i = 0; i < NUMAMMO; ++i)
	{
		if (strcmp (AmmoNames[i], type) == 0)
		{
			TakeAmmo (player, i, amount);
			return;
		}
	}
	for (i = 0; i < NUMARTIFACTS; ++i)
	{
		if (strcmp (ArtifactNames[i], type) == 0)
		{
			if (amount == 0)
			{
				player->inventory[i] = 0;
			}
			else
			{
				player->inventory[i] = MAX(player->inventory[i]-amount, 0);
			}
			return;
		}
	}
	for (i = 0; i < NUMWEAPONS; ++i)
	{
		if (strcmp (WeaponNames[i], type) == 0)
		{
			TakeWeapon (player, i);
			return;
		}
	}
	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (DoomKeyNames[i], type) == 0)
			{
				player->keys[i] = 0;
			}
		}
		if (strcmp ("Backpack", type) == 0)
		{
			TakeBackpack (player);
		}
	}
	else
	{
		for (i = 0; i < 3; ++i)
		{
			if (strcmp (HereticKeyNames[i], type) == 0)
			{
				player->keys[i] = 0;
			}
		}
		if (strcmp ("BagOfHolding", type) == 0)
		{
			TakeBackpack (player);
		}
	}
}

static void TakeInventory (AActor *activator, const char *type, int amount)
{
	if (amount < 0)
	{
	}
	if (activator == NULL)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
				DoTakeInv (&players[i], type, amount);
		}
	}
	else if (activator->player != NULL)
	{
		DoTakeInv (activator->player, type, amount);
	}
}

static int CheckInventory (AActor *activator, const char *type)
{
	if (activator == NULL || activator->player == NULL)
		return 0;

	player_t *player = activator->player;
	int i;

	if (strcmp (type, "Armor") == 0)
	{
		return player->armorpoints[0];
	}
	else if (strcmp (type, "Health") == 0)
	{
		return player->health;
	}

	for (i = 0; i < NUMAMMO; ++i)
	{
		if (strcmp (AmmoNames[i], type) == 0)
		{
			return player->ammo[i];
		}
	}

	for (i = 0; i < NUMARTIFACTS; ++i)
	{
		if (strcmp (ArtifactNames[i], type) == 0)
		{
			return player->inventory[i];
		}
	}

	for (i = 0; i < NUMWEAPONS; ++i)
	{
		if (strcmp (WeaponNames[i], type) == 0)
		{
			return player->weaponowned[i] ? 1 : 0;
		}
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 6; ++i)
		{
			if (strcmp (DoomKeyNames[i], type) == 0)
			{
				return player->keys[i] ? 1 : 0;
			}
		}
		if (strcmp ("Backpack", type) == 0)
		{
			return player->backpack ? 1 : 0;
		}
	}
	else
	{
		for (i = 0; i < 3; ++i)
		{
			if (strcmp (HereticKeyNames[i], type) == 3)
			{
				return player->keys[i] ? 1 : 0;
			}
		}
		if (strcmp ("BagOfHolding", type) == 0)
		{
			return player->backpack ? 1 : 0;
		}
	}
	return 0;
}

//---- Plane watchers ----//

class DPlaneWatcher : public DThinker
{
	DECLARE_CLASS (DPlaneWatcher, DThinker)
	HAS_OBJECT_POINTERS
public:
	DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
		int tag, int height, int special,
		int arg0, int arg1, int arg2, int arg3, int arg4);
	void Tick ();
	void Serialize (FArchive &arc);
private:
	sector_t *Sector;
	fixed_t WatchD, LastD;
	int Special, Arg0, Arg1, Arg2, Arg3, Arg4;
	AActor *Activator;
	line_t *Line;
	int LineSide;
	bool bCeiling;

	DPlaneWatcher() {}
};

IMPLEMENT_POINTY_CLASS (DPlaneWatcher)
 DECLARE_POINTER (Activator)
END_POINTERS

DPlaneWatcher::DPlaneWatcher (AActor *it, line_t *line, int lineSide, bool ceiling,
	int tag, int height, int special,
	int arg0, int arg1, int arg2, int arg3, int arg4)
	: Special (special), Arg0 (arg0), Arg1 (arg1), Arg2 (arg2), Arg3 (arg3), Arg4 (arg4),
	  Activator (it), Line (line), LineSide (lineSide), bCeiling (ceiling)
{
	int secnum;

	secnum = P_FindSectorFromTag (tag, -1);
	if (secnum >= 0)
	{
		secplane_t plane;

		Sector = &sectors[secnum];
		if (bCeiling)
		{
			plane = Sector->ceilingplane;
		}
		else
		{
			plane = Sector->floorplane;
		}
		LastD = plane.d;
		plane.ChangeHeight (height << FRACBITS);
		WatchD = plane.d;
	}
	else
	{
		Sector = NULL;
		WatchD = LastD = 0;
	}
}

void DPlaneWatcher::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	arc << Special << Arg0 << Arg1 << Arg2 << Arg3 << Arg4
		<< Sector << bCeiling << WatchD << LastD << Activator
		<< Line << LineSide << bCeiling;
}

void DPlaneWatcher::Tick ()
{
	if (Sector == NULL)
	{
		Destroy ();
		return;
	}

	fixed_t newd;
	
	if (bCeiling)
	{
		newd = Sector->ceilingplane.d;
	}
	else
	{
		newd = Sector->floorplane.d;
	}

	if ((LastD < WatchD && newd >= WatchD) ||
		(LastD > WatchD && newd <= WatchD))
	{
		TeleportSide = LineSide;
		LineSpecials[Special] (Line, Activator, Arg0, Arg1, Arg2, Arg3, Arg4);
		Destroy ();
	}

}

//---- ACS lump manager ----//

FBehavior *FBehavior::StaticLoadModule (int lumpnum)
{
	for (size_t i = 0; i < StaticModules.Size(); ++i)
	{
		if (StaticModules[i]->LumpNum == lumpnum)
		{
			return StaticModules[i];
		}
	}

	return new FBehavior (lumpnum);
}

bool FBehavior::StaticCheckAllGood ()
{
	for (size_t i = 0; i < StaticModules.Size(); ++i)
	{
		if (!StaticModules[i]->IsGood())
		{
			return false;
		}
	}
	return true;
}

void FBehavior::StaticUnloadModules ()
{
	for (size_t i = StaticModules.Size(); i-- > 0; )
	{
		delete StaticModules[i];
	}
	StaticModules.Clear ();
}

FBehavior *FBehavior::StaticGetModule (int lib)
{
	if ((size_t)lib >= StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib];
}

void FBehavior::StaticSerializeModuleStates (FArchive &arc)
{
	DWORD modnum;

	modnum = StaticModules.Size();
	arc << modnum;

	if (modnum != StaticModules.Size())
	{
		I_Error ("Level was saved with a different number of ACS modules.");
	}

	for (modnum = 0; modnum < StaticModules.Size(); ++modnum)
	{
		FBehavior *module = StaticModules[modnum];

		if (arc.IsStoring())
		{
			arc.WriteString (module->ModuleName);
		}
		else
		{
			char *modname = NULL;
			arc << modname;
			if (stricmp (modname, module->ModuleName) != 0)
			{
				delete[] modname;
				I_Error ("Level was saved with a different set of ACS modules.");
			}
			delete[] modname;
		}
		module->SerializeVars (arc);
	}
}

void FBehavior::SerializeVars (FArchive &arc)
{
	SerializeVarSet (arc, MapVarStore, NUM_MAPVARS);
	for (int i = 0; i < NumArrays; ++i)
	{
		SerializeVarSet (arc, ArrayStore[i].Elements, ArrayStore[i].ArraySize);
	}
}

void FBehavior::SerializeVarSet (FArchive &arc, SDWORD *vars, int max)
{
	SDWORD arcval;
	SDWORD first, last;

	if (arc.IsStoring ())
	{
		// Find first non-zero variable
		for (first = 0; first < max; ++first)
		{
			if (vars[first] != 0)
			{
				break;
			}
		}

		// Find last non-zero variable
		for (last = max - 1; last >= first; --last)
		{
			if (vars[last] != 0)
			{
				break;
			}
		}

		if (last < first)
		{ // no non-zero variables
			arcval = 0;
			arc << arcval;
			return;
		}

		arcval = last - first + 1;
		arc << arcval;
		arcval = first;
		arc << arcval;

		while (first <= last)
		{
			arc << vars[first];
			++first;
		}
	}
	else
	{
		SDWORD truelast;

		arc << last;
		if (last == 0)
		{
			return;
		}
		arc << first;
		last += first;
		truelast = last;

		if (last > max)
		{
			last = max;
		}

		while (first < last)
		{
			arc << vars[first];
			++first;
		}
		while (first < truelast)
		{
			arc << arcval;
			++first;
		}
	}
}

FBehavior::FBehavior (int lumpnum)
{
	BYTE *object;
	int len;
	int i;

	NumScripts = 0;
	NumFunctions = 0;
	NumArrays = 0;
	NumTotalArrays = 0;
	Scripts = NULL;
	Functions = NULL;
	Arrays = NULL;
	ArrayStore = NULL;
	Chunks = NULL;
	Format = ACS_Unknown;
	LumpNum = lumpnum;
	memset (MapVarStore, 0, sizeof(MapVarStore));
	ModuleName[0] = 0;

	len = W_LumpLength (lumpnum);

	// Any behaviors smaller than 32 bytes cannot possibly contain anything useful.
	// (16 bytes for a completely empty behavior + 12 bytes for one script header
	//  + 4 bytes for PCD_TERMINATE for an old-style object. A new-style object
	// has 24 bytes if it is completely empty. An empty SPTR chunk adds 8 bytes.)
	if (len < 32)
	{
		return;
	}

	object = new byte[len];
	W_ReadLump (lumpnum, object);

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		return;
	}

	switch (object[3])
	{
	case 0:
		Format = ACS_Old;
		break;
	case 'E':
		Format = ACS_Enhanced;
		break;
	case 'e':
		Format = ACS_LittleEnhanced;
		break;
	default:
		return;
	}

	W_GetLumpName (ModuleName, lumpnum);
	ModuleName[8] = 0;
	Data = object;
	DataSize = len;
	
	if (Format == ACS_Old)
	{
		Chunks = object + len;
		Scripts = object + ((DWORD *)object)[1];
		NumScripts = ((DWORD *)Scripts)[0];
		// Check for redesigned ACSE/ACSe
		if (((DWORD *)object)[1] >= 6*4 &&
			(((DWORD *)Scripts)[-1] == MAKE_ID('A','C','S','e') ||
			((DWORD *)Scripts)[-1] == MAKE_ID('A','C','S','E')))
		{
			Format = (((BYTE *)Scripts)[-1] == 'e') ? ACS_LittleEnhanced : ACS_Enhanced;
			Chunks = object + ((DWORD *)Scripts)[-2];
			// Forget about the compatibility cruft at the end of the lump
			DataSize = ((DWORD *)object)[1] - 8;
		}
		else
		{
			Scripts += 4;
			for (i = 0; i < NumScripts; ++i)
			{
				ScriptPtr2 ptr1 = *(ScriptPtr2 *)(Scripts + 12*i);
				ScriptPtr *ptr2 =  (ScriptPtr  *)(Scripts +  8*i);
				ptr2->Number = ptr1.Number % 1000;
				ptr2->Type = ptr1.Number / 1000;
				ptr2->ArgCount = ptr1.ArgCount;
				ptr2->Address = ptr1.Address;
			}
		}
	}
	else
	{
		Chunks = object + ((DWORD *)object)[1];
	}
	if (Format != ACS_Old)
	{
		Scripts = FindChunk (MAKE_ID('S','P','T','R'));
		if (Scripts == NULL)
		{
			NumScripts = 0;
		}
		else
		{
			if (object[3] != 0)
			{
				NumScripts = ((DWORD *)Scripts)[1] / 12;
				Scripts += 8;
				for (i = 0; i < NumScripts; ++i)
				{
					ScriptPtr1 ptr1 = *(ScriptPtr1 *)(Scripts + 12*i);
					ScriptPtr *ptr2 =  (ScriptPtr  *)(Scripts +  8*i);
					ptr2->Number = ptr1.Number;
					ptr2->Type = ptr1.Type;
					ptr2->ArgCount = ptr1.ArgCount;
					ptr2->Address = ptr1.Address;
				}
			}
			else
			{
				NumScripts = ((DWORD *)Scripts)[1] / 8;
				Scripts += 8;
			}
		}

		UnencryptStrings ();
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 0)
	{
		qsort (Scripts, NumScripts, 8, SortScripts);
	}

	if (Format == ACS_Old)
	{
		LanguageNeutral = ((DWORD *)Data)[1];
		LanguageNeutral += ((DWORD *)(Data + LanguageNeutral))[0] * 12 + 4;
	}
	else
	{
		LanguageNeutral = FindLanguage (0, false);
		PrepLocale (LanguageIDs[0], LanguageIDs[1], LanguageIDs[2], LanguageIDs[3]);
	}

	if (Format == ACS_Old)
	{
		// Do initialization for old-style behavior lumps
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}
		LibraryID = StaticModules.Push (this) << 16;
	}
	else
	{
		DWORD *chunk;

		Functions = FindChunk (MAKE_ID('F','U','N','C'));
		if (Functions != NULL)
		{
			NumFunctions = LONG(((DWORD *)Functions)[1]) / 8;
			Functions += 8;
		}

		// Initialize this object's map variables
		memset (MapVarStore, 0, sizeof(MapVarStore));
		chunk = (DWORD *)FindChunk (MAKE_ID('M','I','N','I'));
		while (chunk != NULL)
		{
			int numvars = LONG(chunk[1])/4;
			int firstvar = LONG(chunk[2]);
			for (i = 0; i < numvars; ++i)
			{
				MapVarStore[i+firstvar] = LONG(chunk[3+i]);
			}
			chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		}

		// Initialize this object's map variable pointers to defaults. They can be changed
		// later once the imported modules are loaded.
		for (i = 0; i < NUM_MAPVARS; ++i)
		{
			MapVars[i] = &MapVarStore[i];
		}

		// Create arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','R','A','Y'));
		if (chunk != NULL)
		{
			NumArrays = LONG(chunk[1])/8;
			ArrayStore = new ArrayInfo[NumArrays];
			memset (ArrayStore, 0, sizeof(*Arrays)*NumArrays);
			for (i = 0; i < NumArrays; ++i)
			{
				MapVarStore[LONG(chunk[2+i*2])] = i;
				ArrayStore[i].ArraySize = LONG(chunk[3+i*2]);
				ArrayStore[i].Elements = new SDWORD[ArrayStore[i].ArraySize];
				memset(ArrayStore[i].Elements, 0, ArrayStore[i].ArraySize*sizeof(DWORD));
			}
		}

		// Initialize arrays for this module
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','N','I'));
		while (chunk != NULL)
		{
			int arraynum = MapVarStore[LONG(chunk[2])];
			if ((unsigned)arraynum < (unsigned)NumArrays)
			{
				int initsize = MIN<int> (ArrayStore[arraynum].ArraySize, (LONG(chunk[1])-4)/4);
				SDWORD *elems = ArrayStore[arraynum].Elements;
				for (i = 0; i < initsize; ++i)
				{
					elems[i] = LONG(chunk[3+i]);
				}
			}
			chunk = (DWORD *)NextChunk((BYTE *)chunk);
		}

		// Start setting up array pointers
		NumTotalArrays = NumArrays;
		chunk = (DWORD *)FindChunk (MAKE_ID('A','I','M','P'));
		if (chunk != NULL)
		{
			NumTotalArrays += LONG(chunk[2]);
		}
		if (NumTotalArrays != 0)
		{
			Arrays = new ArrayInfo *[NumTotalArrays];
			for (i = 0; i < NumArrays; ++i)
			{
				Arrays[i] = &ArrayStore[i];
			}
		}

		// Now that everything is set up, record this module as being among the loaded modules.
		// We need to do this before resolving any imports, because an import might (indirectly)
		// need to resolve exports in this module. The only things that can be exported are
		// functions and map variables, which must already be present if they're exported, so
		// this is okay.
        LibraryID = StaticModules.Push (this) << 16;

		// Tag the library ID to any map variables that are initialized with strings
		if (LibraryID != 0)
		{
			chunk = (DWORD *)FindChunk (MAKE_ID('M','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < chunk[1]/4; ++i)
				{
					MapVarStore[chunk[i+2]] |= LibraryID;
				}
			}

			chunk = (DWORD *)FindChunk (MAKE_ID('A','S','T','R'));
			if (chunk != NULL)
			{
				for (DWORD i = 0; i < chunk[1]/4; ++i)
				{
					int arraynum = MapVarStore[LONG(chunk[i+2])];
					if ((unsigned)arraynum < (unsigned)NumArrays)
					{
						SDWORD *elems = ArrayStore[arraynum].Elements;
						for (int j = ArrayStore[arraynum].ArraySize; j > 0; --j, ++elems)
						{
							*elems |= LibraryID;
						}
					}
				}
			}
		}

		if (NULL != (chunk = (DWORD *)FindChunk (MAKE_ID('L','O','A','D'))))
		{
			const char *const parse = (char *)&chunk[2];
			DWORD i;

			for (i = 0; i < chunk[1]; )
			{
				if (parse[i])
				{
					FBehavior *module = NULL;
					int lump = W_CheckNumForName (&parse[i], ns_acslibrary);
					if (lump < 0)
					{
						Printf ("Could not find ACS library %s.\n", &parse[i]);
					}
					else
					{
						module = StaticLoadModule (lump);
					}
					Imports.Push (module);
					do ; while (parse[++i]);
				}
				++i;
			}

			// Go through each imported module in order and resolve all imported functions
			// and map variables.
			for (i = 0; i < Imports.Size(); ++i)
			{
				FBehavior *lib = Imports[i];
				int j;

				if (lib == NULL)
					continue;

				// Resolve functions
				chunk = (DWORD *)FindChunk(MAKE_ID('F','N','A','M'));
				for (j = 0; j < NumFunctions; ++j)
				{
					ScriptFunction *func = &((ScriptFunction *)Functions)[j];
					if (func->Address == 0 && func->ImportNum == 0)
					{
						int libfunc = lib->FindFunctionName ((char *)(chunk + 2) + chunk[3+j]);
						if (libfunc >= 0)
						{
							ScriptFunction *realfunc = &((ScriptFunction *)lib->Functions)[libfunc];
							// Make sure that the library really defines this function. It might simply
							// be importing it itself.
							if (realfunc->Address != 0 && realfunc->ImportNum == 0)
							{
								func->Address = libfunc;
								func->ImportNum = i+1;
								if (realfunc->ArgCount != func->ArgCount)
								{
									Printf ("Function %s in %s has %d arguments. %s expects it to have %d.\n",
										(char *)(chunk + 2) + chunk[3+j], lib->ModuleName, realfunc->ArgCount,
										ModuleName, func->ArgCount);
									Format = ACS_Unknown;
								}
								// The next two properties do not effect code compatibility, so it is
								// okay for them to be different in the imported module than they are
								// in this one, as long as we make sure to use the real values.
								func->LocalCount = realfunc->LocalCount;
								func->HasReturnValue = realfunc->HasReturnValue;
							}
						}
					}
				}

				// Resolve map variables
				chunk = (DWORD *)FindChunk(MAKE_ID('M','I','M','P'));
				if (chunk != NULL)
				{
					char *parse = (char *)&chunk[2];
					for (DWORD j = 0; j < chunk[1]; )
					{
						DWORD varNum = LONG(*(DWORD *)&parse[j]);
						j += 4;
						int impNum = lib->FindMapVarName (&parse[j]);
						if (impNum >= 0)
						{
							MapVars[varNum] = &lib->MapVarStore[impNum];
						}
						do ; while (parse[++j]);
						++j;
					}
				}

				// Resolve arrays
				if (NumTotalArrays > NumArrays)
				{
					chunk = (DWORD *)FindChunk(MAKE_ID('A','I','M','P'));
					char *parse = (char *)&chunk[3];
					for (DWORD j = 0; j < LONG(chunk[2]); ++j)
					{
						DWORD varNum = LONG(*(DWORD *)parse);
						parse += 4;
						DWORD expectedSize = LONG(*(DWORD *)parse);
						parse += 4;
						int impNum = lib->FindMapArray (parse);
						if (impNum >= 0)
						{
							Arrays[NumArrays + j] = &lib->ArrayStore[impNum];
							MapVarStore[varNum] = NumArrays + j;
							if (lib->ArrayStore[impNum].ArraySize != expectedSize)
							{
								Format = ACS_Unknown;
								Printf ("The array %s in %s has %ld elements, but %s expects it to only have %ld.\n",
									parse, lib->ModuleName, lib->ArrayStore[impNum].ArraySize,
									ModuleName, expectedSize);
							}
						}
						do ; while (*++parse);
						++parse;
					}
				}
			}
		}
	}

	DPrintf ("Loaded %d scripts, %d functions\n", NumScripts, NumFunctions);
}

FBehavior::~FBehavior ()
{
	if (Arrays != NULL)
	{
		delete[] Arrays;
		Arrays = NULL;
	}
	if (ArrayStore != NULL)
	{
		for (int i = 0; i < NumArrays; ++i)
		{
			if (ArrayStore[i].Elements != NULL)
			{
				delete[] ArrayStore[i].Elements;
				ArrayStore[i].Elements = NULL;
			}
		}
		delete[] ArrayStore;
		ArrayStore = NULL;
	}
	if (Data != NULL)
	{
		delete[] Data;
		Data = NULL;
	}
}

int STACK_ARGS FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

void FBehavior::UnencryptStrings ()
{
	DWORD *prevchunk = NULL;
	DWORD *chunk = (DWORD *)FindChunk(MAKE_ID('S','T','R','E'));
	while (chunk != NULL)
	{
		for (DWORD strnum = 0; strnum < chunk[3]; ++strnum)
		{
			int ofs = chunk[5+strnum];
			BYTE *data = (BYTE *)chunk + ofs + 8, last;
			int p = (BYTE)(ofs*157135);
			int i = 0;
			do
			{
				last = (data[i] ^= (BYTE)(p+(i>>1)));
				++i;
			} while (last != 0);
		}
		prevchunk = chunk;
		chunk = (DWORD *)NextChunk ((BYTE *)chunk);
		*prevchunk = MAKE_ID('S','T','R','L');
	}
	if (prevchunk != NULL)
	{
		*prevchunk = MAKE_ID('S','T','R','L');
	}
}

bool FBehavior::IsGood ()
{
	bool bad;
	int i;

	// Check that the data format was understood
	if (Format == ACS_Unknown)
	{
		return false;
	}

	// Check that all functions are resolved
	bad = false;
	for (i = 0; i < NumFunctions; ++i)
	{
		ScriptFunction *funcdef = (ScriptFunction *)Functions + i;
		if (funcdef->Address == 0 && funcdef->ImportNum == 0)
		{
			DWORD *chunk = (DWORD *)FindChunk (MAKE_ID('F','N','A','M'));
			Printf ("Could not find ACS function %s for use in %s.\n",
				(char *)(chunk + 2) + chunk[3+i], ModuleName);
			bad = true;
		}
	}

	// Check that all imported modules were loaded
	for (i = Imports.Size() - 1; i >= 0; --i)
	{
		if (Imports[i] == NULL)
		{
			Printf ("Not all the libraries used by %s could be found.\n", ModuleName);
			return false;
		}
	}

	return !bad;
}

int *FBehavior::FindScript (int script) const
{
	const ScriptPtr *ptr = BinarySearch<ScriptPtr, WORD>
		((ScriptPtr *)Scripts, NumScripts, &ScriptPtr::Number, (WORD)script);

	return ptr ? (int *)(ptr->Address + Data) : NULL;
}

int *FBehavior::StaticFindScript (int script, FBehavior *&module)
{
	for (DWORD i = 0; i < StaticModules.Size(); ++i)
	{
		int *code = StaticModules[i]->FindScript (script);
		if (code != NULL)
		{
			module = StaticModules[i];
			return code;
		}
	}
	return NULL;
}

ScriptFunction *FBehavior::GetFunction (int funcnum, FBehavior *&module) const
{
	if ((unsigned)funcnum >= (unsigned)NumFunctions)
	{
		return NULL;
	}
	ScriptFunction *funcdef = (ScriptFunction *)Functions + funcnum;
	if (funcdef->ImportNum)
	{
		return Imports[funcdef->ImportNum - 1]->GetFunction (funcdef->Address, module);
	}
	// Should I just un-const this function instead of using a const_cast?
	module = const_cast<FBehavior *>(this);
	return funcdef;
}

int FBehavior::FindFunctionName (const char *funcname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('F','N','A','M')), funcname);
}

int FBehavior::FindMapVarName (const char *varname) const
{
	return FindStringInChunk ((DWORD *)FindChunk (MAKE_ID('M','E','X','P')), varname);
}

int FBehavior::FindMapArray (const char *arrayname) const
{
	int var = FindMapVarName (arrayname);
	if (var >= 0)
	{
		return MapVarStore[var];
	}
	return -1;
}

int FBehavior::FindStringInChunk (DWORD *names, const char *varname) const
{
	if (names != NULL)
	{
		DWORD i;

		for (i = 0; i < names[2]; ++i)
		{
			if (stricmp (varname, (char *)(names + 2) + names[3+i]) == 0)
			{
				return (int)i;
			}
		}
	}
	return -1;
}

int FBehavior::GetArrayVal (int arraynum, int index) const
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return 0;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return 0;
	return array->Elements[index];
}

void FBehavior::SetArrayVal (int arraynum, int index, int value)
{
	if ((unsigned)arraynum >= (unsigned)NumTotalArrays)
		return;
	const ArrayInfo *array = Arrays[arraynum];
	if ((unsigned)index >= (unsigned)array->ArraySize)
		return;
	array->Elements[index] = value;
}

BYTE *FBehavior::FindChunk (DWORD id) const
{
	BYTE *chunk = Chunks;

	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

BYTE *FBehavior::NextChunk (BYTE *chunk) const
{
	DWORD id = *(DWORD *)chunk;
	chunk += ((DWORD *)chunk)[1] + 8;
	while (chunk != NULL && chunk < Data + DataSize)
	{
		if (((DWORD *)chunk)[0] == id)
		{
			return chunk;
		}
		chunk += ((DWORD *)chunk)[1] + 8;
	}
	return NULL;
}

const char *FBehavior::StaticLookupString (DWORD index)
{
	DWORD lib = index >> 16;
	if (lib >= (DWORD)StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib]->LookupString (index & 0xffff);
}

const char *FBehavior::StaticLocalizeString (DWORD index)
{
	DWORD lib = index >> 16;
	if (lib >= (DWORD)StaticModules.Size())
	{
		return NULL;
	}
	return StaticModules[lib]->LocalizeString (index & 0xffff);
}

const char *FBehavior::LookupString (DWORD index, DWORD ofs) const
{
	if (Format == ACS_Old)
	{
		DWORD *list = (DWORD *)(Data + LanguageNeutral);

		if (index >= list[0])
			return NULL;	// Out of range for this list;
		return (const char *)(Data + list[1+index]);
	}
	else
	{
		if (ofs == 0)
		{
			ofs = LanguageNeutral;
			if (ofs == 0)
			{
				return NULL;
			}
		}
		DWORD *list = (DWORD *)(Data + ofs);

		if (index >= list[1])
			return NULL;	// Out of range for this list
		if (list[3+index] == 0)
			return NULL;	// Not defined in this list
		return (const char *)(Data + ofs + list[3+index]);
	}
}

const char *FBehavior::LocalizeString (DWORD index) const
{
	if (Format != ACS_Old)
	{
		DWORD ofs = Localized;
		const char *str = NULL;

		while (ofs != 0 && (str = LookupString (index, ofs)) == NULL)
		{
			ofs = ((DWORD *)(Data + ofs))[2];
		}
		return str;
	}
	else
	{
		return LookupString (index);
	}
}

void FBehavior::StaticPrepLocale (DWORD userpref, DWORD userdef, DWORD syspref, DWORD sysdef)
{
	for (size_t i = 0; i < StaticModules.Size(); ++i)
	{
		StaticModules[i]->PrepLocale (userpref, userdef, syspref, sysdef);
	}
}

void FBehavior::PrepLocale (DWORD userpref, DWORD userdef, DWORD syspref, DWORD sysdef)
{
	BYTE *chunk;
	DWORD *list;

	// Clear away any existing links
	for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
	{
		list = (DWORD *)chunk;
		if (list[0] == MAKE_ID('S','T','R','L'))
		{
			list[4] = 0;
		}
	}
	Localized = 0;

	if (userpref)
		AddLanguage (userpref);
	if (userpref & LANGREGIONMASK)
		AddLanguage (userpref & ~LANGREGIONMASK);
	if (userdef)
		AddLanguage (userdef);
	if (userdef & LANGREGIONMASK)
		AddLanguage (userdef & ~LANGREGIONMASK);
	if (syspref)
		AddLanguage (syspref);
	if (syspref & LANGREGIONMASK)
		AddLanguage (syspref & ~LANGREGIONMASK);
	if (sysdef)
		AddLanguage (sysdef);
	if (sysdef & LANGREGIONMASK)
		AddLanguage (sysdef & ~LANGREGIONMASK);
	AddLanguage (MAKE_ID('e','n',0,0));		// Use English as a fallback
	AddLanguage (0);			// Failing that, use language independent strings
}

void FBehavior::AddLanguage (DWORD langid)
{
	DWORD ofs, *ofsput;
	DWORD *list;
	BYTE *chunk;

	// First, make sure language is not already inserted
	ofsput = CheckIfInList (langid);
	if (ofsput == NULL)
	{ // Already in list
		return;
	}

	// Try to find an exact match first
	ofs = FindLanguage (langid, false);
	if (ofs != 0)
	{
		*ofsput = ofs;
		return;
	}

	// If langid has no sublanguage, add all languages that match the major
	// type, if not in list already
	if ((langid & LANGREGIONMASK) == 0)
	{
		for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
		{
			list = (DWORD *)chunk;
			if (list[0] != MAKE_ID('S','T','R','L'))
				continue;	// not a string list
			if ((list[2] & ~LANGREGIONMASK) != langid)
				continue;	// wrong language
			if (list[4] != 0)
				continue;	// definitely in language list
			ofsput = CheckIfInList (list[2]);
			if (ofsput != NULL)
				*ofsput = chunk - Data + 8;	// add to language list
		}
	}
}

DWORD *FBehavior::CheckIfInList (DWORD langid)
{
	DWORD ofs, *ofsput;
	DWORD *list;

	ofs = Localized;
	ofsput = &Localized;
	while (ofs != 0)
	{
		list = (DWORD *)(Data + ofs);
		if (list[0] == langid)
			return NULL;
		ofsput = &list[2];
		ofs = list[2];
	}
	return ofsput;
}

DWORD FBehavior::FindLanguage (DWORD langid, bool ignoreregion) const
{
	BYTE *chunk;
	DWORD *list;
	DWORD langmask;

	langmask = ignoreregion ? ~LANGREGIONMASK : ~0;

	for (chunk = Chunks; chunk < Data + DataSize; chunk += ((DWORD *)chunk)[1] + 8)
	{
		list = (DWORD *)chunk;
		if (list[0] == MAKE_ID('S','T','R','L') && (list[2] & langmask) == langid)
		{
			return chunk - Data + 8;
		}
	}
	return 0;
}

void FBehavior::StaticStartTypedScripts (WORD type, AActor *activator)
{
	for (size_t i = 0; i < StaticModules.Size(); ++i)
	{
		StaticModules[i]->StartTypedScripts (type, activator);
	}
}

void FBehavior::StartTypedScripts (WORD type, AActor *activator)
{
	ScriptPtr *ptr;
	int i;

	for (i = 0; i < NumScripts; ++i)
	{
		ptr = (ScriptPtr *)(Scripts + 8*i);
		if (ptr->Type == type)
		{
			P_GetScriptGoing (activator, NULL, ptr->Number,
				(int *)(ptr->Address + Data), this, 0, 0, 0, 0, 0, true);
		}
	}
}

//---- The ACS Interpreter ----//

void strbin (char *str);

IMPLEMENT_CLASS (DACSThinker)

DACSThinker *DACSThinker::ActiveThinker = NULL;

DACSThinker::DACSThinker ()
{
	if (ActiveThinker)
	{
		I_Error ("Only one ACSThinker is allowed to exist at a time.\nCheck your code.");
	}
	else
	{
		ActiveThinker = this;
		Scripts = NULL;
		LastScript = NULL;
		for (int i = 0; i < 1000; i++)
			RunningScripts[i] = NULL;
	}
}

DACSThinker::~DACSThinker ()
{
	DLevelScript *script = Scripts;
	while (script)
	{
		DLevelScript *next = script->next;
		script->Destroy ();
		script = next;
	}
	Scripts = NULL;
	ActiveThinker = NULL;
}

void DACSThinker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << Scripts << LastScript;
	if (arc.IsStoring ())
	{
		WORD i;
		for (i = 0; i < 1000; i++)
		{
			if (RunningScripts[i])
				arc << RunningScripts[i] << i;
		}
		DLevelScript *nil = NULL;
		arc << nil;
	}
	else
	{
		WORD scriptnum;
		DLevelScript *script = NULL;
		arc << script;
		while (script)
		{
			arc << scriptnum;
			RunningScripts[scriptnum] = script;
			arc << script;
		}
	}
}

void DACSThinker::Tick ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}
}

IMPLEMENT_POINTY_CLASS (DLevelScript)
 DECLARE_POINTER (activator)
END_POINTERS

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);
	arc << next << prev
		<< script;

	if (SaveVersion < 306)
	{
		int sp;
		arc << sp;
	}

	arc	<< state
		<< statedata
		<< activator
		<< activationline
		<< lineSide;

	for (i = 0; i < LOCAL_SIZE; i++)
	{
		arc << localvars[i];
	}

	if (arc.IsStoring ())
	{
		WORD lib = activeBehavior->GetLibraryID() >> 16;
		arc << lib;
		i = activeBehavior->PC2Ofs (pc);
		arc << i;
	}
	else
	{
		WORD lib;
		arc << lib << i;
		activeBehavior = FBehavior::StaticGetModule (lib);
		pc = activeBehavior->Ofs2PC (i);
	}

	arc << activefont;
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
	activefont = SmallFont;
}

void DLevelScript::Unlink ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		controller->LastScript = prev;
	if (controller->Scripts == this)
		controller->Scripts = next;
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
}

void DLevelScript::Link ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	next = controller->Scripts;
	if (controller->Scripts)
		controller->Scripts->prev = this;
	prev = NULL;
	controller->Scripts = this;
	if (controller->LastScript == NULL)
		controller->LastScript = this;
}

void DLevelScript::PutLast ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->LastScript == this)
		return;

	Unlink ();
	if (controller->Scripts == NULL)
	{
		Link ();
	}
	else
	{
		if (controller->LastScript)
			controller->LastScript->next = this;
		prev = controller->LastScript;
		next = NULL;
		controller->LastScript = this;
	}
}

void DLevelScript::PutFirst ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->Scripts == this)
		return;

	Unlink ();
	Link ();
}

int DLevelScript::Random (int min, int max)
{
	int num1, num2, num3, num4;
	unsigned int num;

	if (max - min > 255)
	{
		num1 = pr_acs();
		num2 = pr_acs();
		num3 = pr_acs();
		num4 = pr_acs();

		num = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
	}
	else
	{
		num = pr_acs();
	}
	num %= (max - min + 1);
	num += min;
	return (int)num;
}

int DLevelScript::ThingCount (int type, int tid)
{
	AActor *actor;
	const TypeInfo *kind;
	int count = 0;

	if (type >= MAX_SPAWNABLES)
	{
		return 0;
	}
	else if (type > 0)
	{
		kind = SpawnableThings[type];
		if (kind == NULL)
			return 0;
	}
	else
	{
		kind = NULL;
	}
	
	if (tid)
	{
		FActorIterator iterator (tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				count++;
			}
		}
	}
	else
	{
		TThinkerIterator<AActor> iterator;
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(kind == NULL || actor->IsA (kind)))
			{
				count++;
			}
		}
	}
	return count;
}

void DLevelScript::ChangeFlat (int tag, int name, bool floorOrCeiling)
{
	int flat, secnum = -1;
	const char *flatname = FBehavior::StaticLookupString (name);

	if (flatname == NULL)
		return;

	flat = R_FlatNumForName (flatname);

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		if (floorOrCeiling == false)
		{
			if (sectors[secnum].floorpic != flat)
			{
				sectors[secnum].floorpic = flat;
				sectors[secnum].AdjustFloorClip ();
			}
		}
		else
		{
			sectors[secnum].ceilingpic = flat;
		}
	}
}

int DLevelScript::CountPlayers ()
{
	int count = 0, i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			count++;
	
	return count;
}

void DLevelScript::SetLineTexture (int lineid, int side, int position, int name)
{
	int texture, linenum = -1;
	const char *texname = FBehavior::StaticLookupString (name);

	if (texname == NULL)
		return;

	side = !!side;

	texture = R_TextureNumForName (texname);

	while ((linenum = P_FindLineFromID (lineid, linenum)) >= 0)
	{
		side_t *sidedef;

		if (lines[linenum].sidenum[side] == NO_INDEX)
			continue;
		sidedef = sides + lines[linenum].sidenum[side];

		switch (position)
		{
		case TEXTURE_TOP:
			sidedef->toptexture = texture;
			break;
		case TEXTURE_MIDDLE:
			sidedef->midtexture = texture;
			break;
		case TEXTURE_BOTTOM:
			sidedef->bottomtexture = texture;
			break;
		default:
			break;
		}

	}
}

int DLevelScript::DoSpawn (int type, fixed_t x, fixed_t y, fixed_t z, int tid, int angle)
{
	const TypeInfo *info = TypeInfo::FindType (FBehavior::StaticLookupString (type));
	AActor *actor = NULL;

	if (info != NULL)
	{
		actor = Spawn (info, x, y, z);
		if (actor != NULL)
		{
			if (P_TestMobjLocation (actor))
			{
				actor->angle = angle << 24;
				actor->tid = tid;
				actor->AddToHash ();
				if (actor->flags & MF_SPECIAL)
					actor->flags |= MF_DROPPED;  // Don't respawn
			}
			else
			{
				// If this is a monster, subtract it from the total monster
				// count, because it already added to it during spawning.
				if (actor->flags & MF_COUNTKILL)
				{
					level.total_monsters--;
				}
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return (int)(actor - (AActor *)0);
}

int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle)
{
	FActorIterator iterator (spot);
	AActor *aspot;
	int spawned = 0;

	while ( (aspot = iterator.Next ()) )
	{
		spawned = DoSpawn (type, aspot->x, aspot->y, aspot->z, tid, angle);
	}
	return spawned;
}

void DLevelScript::DoFadeTo (int r, int g, int b, int a, fixed_t time)
{
	DoFadeRange (0, 0, 0, -1, r, g, b, a, time);
}

void DLevelScript::DoFadeRange (int r1, int g1, int b1, int a1,
								int r2, int g2, int b2, int a2, fixed_t time)
{
	player_t *viewer;
	float ftime = (float)time / 65536.f;
	bool fadingFrom = a1 >= 0;
	float fr1 GCCNOWARN, fg1, fb1, fa1;
	float fr2, fg2, fb2, fa2;
	int i;

	fr2 = (float)r2 / 255.f;
	fg2 = (float)g2 / 255.f;
	fb2 = (float)b2 / 255.f;
	fa2 = (float)a2 / 65536.f;

	if (fadingFrom)
	{
		fr1 = (float)r1 / 255.f;
		fg1 = (float)g1 / 255.f;
		fb1 = (float)b1 / 255.f;
		fa1 = (float)a1 / 65536.f;
	}

	if (activator != NULL)
	{
		viewer = activator->player;
		if (viewer == NULL)
			return;
		i = MAXPLAYERS;
		goto showme;
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				viewer = &players[i];
showme:
				if (ftime <= 0.f)
				{
					viewer->BlendR = fr2;
					viewer->BlendG = fg2;
					viewer->BlendB = fb2;
					viewer->BlendA = fa2;
				}
				else
				{
					if (!fadingFrom)
					{
						if (viewer->BlendA <= 0.f)
						{
							fr1 = fr2;
							fg1 = fg2;
							fb1 = fb2;
							fa1 = 0.f;
						}
						else
						{
							fr1 = viewer->BlendR;
							fg1 = viewer->BlendG;
							fb1 = viewer->BlendB;
							fa1 = viewer->BlendA;
						}
					}
					new DFlashFader (fr1, fg1, fb1, fa1, fr2, fg2, fb2, fa2, ftime, viewer->mo);
				}
			}
		}
	}
}

void DLevelScript::DoSetFont (int fontnum)
{
	const char *fontname = FBehavior::StaticLookupString (fontnum);
	activefont = FFont::FindFont (fontname);
	if (activefont == NULL)
	{
		int lump = W_CheckNumForName (fontname);
		if (lump != -1)
		{
			activefont = new FSingleLumpFont (fontname, lump);
		}
		else
		{
			activefont = SmallFont;
		}
	}
	if (screen != NULL)
	{
		screen->SetFont (activefont);
	}
}

#define APROP_Health		0
#define APROP_Speed			1
#define APROP_Damage		2
#define APROP_Alpha			3
#define APROP_RenderStyle	4
#define APROP_SeeSound		5	// Sounds can only be set, not gotten
#define APROP_AttackSound	6
#define APROP_PainSound		7
#define APROP_DeathSound	8
#define APROP_ActiveSound	9

void DLevelScript::SetActorProperty (int tid, int property, int value)
{
	if (tid == 0)
	{
		DoSetActorProperty (activator, property, value);
	}
	else
	{
		AActor *actor;
		FActorIterator iterator (tid);

		while ((actor = iterator.Next()) != NULL)
		{
			DoSetActorProperty (actor, property, value);
		}
	}
}

void DLevelScript::DoSetActorProperty (AActor *actor, int property, int value)
{
	if (actor == NULL)
	{
		return;
	}
	switch (property)
	{
	case APROP_Health:		actor->health = value;		break;
	case APROP_Speed:		actor->Speed = value;		break;
	case APROP_Damage:		actor->damage = value;		break;
	case APROP_Alpha:		actor->alpha = value;		break;
	case APROP_RenderStyle:	actor->RenderStyle = value;	break;
	case APROP_SeeSound:	actor->SeeSound = S_FindSound (FBehavior::StaticLookupString (value));		break;
	case APROP_AttackSound:	actor->AttackSound = S_FindSound (FBehavior::StaticLookupString (value));	break;
	case APROP_PainSound:	actor->PainSound = S_FindSound (FBehavior::StaticLookupString (value));		break;
	case APROP_DeathSound:	actor->DeathSound = S_FindSound (FBehavior::StaticLookupString (value));	break;
	case APROP_ActiveSound:	actor->ActiveSound = S_FindSound (FBehavior::StaticLookupString (value));	break;
	}
}

int DLevelScript::GetActorProperty (int tid, int property)
{
	AActor *actor;

	if (tid == 0)
	{
		actor = activator;
	}
	else
	{
		FActorIterator iterator (tid);

		actor = iterator.Next();
	}

	if (actor == NULL)
	{
		return 0;
	}
	switch (property)
	{
	case APROP_Health:		return actor->health;
	case APROP_Speed:		return actor->Speed;
	case APROP_Damage:		return actor->damage;
	case APROP_Alpha:		return actor->alpha;
	case APROP_RenderStyle:	return actor->RenderStyle;
	default:				return 0;
	}
}

#define NEXTWORD	(LONG(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define STACK(a)	(Stack[sp - (a)])
#define PushToStack(a)	(Stack[sp++] = (a))

inline int getbyte (int *&pc)
{
	int res = *(BYTE *)pc;
	pc = (int *)((BYTE *)pc+1);
	return res;
}

void DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;
	TeleportSide = lineSide;
	SDWORD *locals = localvars;
	ScriptFunction *activeFunction = NULL;
	BYTE *translation = 0;

	switch (state)
	{
	case SCRIPT_Delayed:
		// Decrement the delay counter and enter state running
		// if it hits 0
		if (--statedata == 0)
			state = SCRIPT_Running;
		break;

	case SCRIPT_TagWait:
		// Wait for tagged sector(s) to go inactive, then enter
		// state running
	{
		int secnum = -1;

		while ((secnum = P_FindSectorFromTag (statedata, secnum)) >= 0)
			if (sectors[secnum].floordata || sectors[secnum].ceilingdata)
				return;
		
		// If we got here, none of the tagged sectors were busy
		state = SCRIPT_Running;
	}
	break;

	case SCRIPT_PolyWait:
		// Wait for polyobj(s) to stop moving, then enter state running
		if (!PO_Busy (statedata))
		{
			state = SCRIPT_Running;
		}
		break;

	case SCRIPT_ScriptWaitPre:
		// Wait for a script to start running, then enter state scriptwait
		if (controller->RunningScripts[statedata])
			state = SCRIPT_ScriptWait;
		break;

	case SCRIPT_ScriptWait:
		// Wait for a script to stop running, then enter state running
		if (controller->RunningScripts[statedata])
			return;

		state = SCRIPT_Running;
		PutFirst ();
		break;

	default:
		break;
	}

	int *pc = this->pc;
	int sp = 0;
	ACSFormat fmt = activeBehavior->GetFormat();
	int runaway = 0;	// used to prevent infinite loops
	int pcd;
	char workreal[4096], *const work = workreal+2, *workwhere = work;
	const char *lookup;
	int optstart = -1;
	int temp;

	if (screen != NULL)
	{
		screen->SetFont (activefont);
	}

	while (state == SCRIPT_Running)
	{
		if (++runaway > 500000)
		{
			Printf ("Runaway script %d terminated\n", script);
			state = SCRIPT_PleaseRemove;
			break;
		}

		if (fmt == ACS_LittleEnhanced)
		{
			pcd = getbyte(pc);
			if (pcd >= 256-16)
			{
				pcd = (256-16) + ((pcd - (256-16)) << 8) + getbyte(pc);
			}
		}
		else
		{
			pcd = NEXTWORD;
		}

		switch (pcd)
		{
		default:
			Printf ("Unknown P-Code %d in script %d\n", pcd, script);
			// fall through
		case PCD_TERMINATE:
			DPrintf ("Script %d finished\n", script);
			state = SCRIPT_PleaseRemove;
			break;

		case PCD_NOP:
			break;

		case PCD_SUSPEND:
			state = SCRIPT_Suspended;
			break;

		case PCD_TAGSTRING:
			Stack[sp-1] |= activeBehavior->GetLibraryID();
			break;

		case PCD_PUSHNUMBER:
			PushToStack (NEXTWORD);
			break;

		case PCD_PUSHBYTE:
			PushToStack (*(BYTE *)pc);
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_PUSH2BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			sp += 2;
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_PUSH3BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			sp += 3;
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_PUSH4BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			sp += 4;
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_PUSH5BYTES:
			Stack[sp] = ((BYTE *)pc)[0];
			Stack[sp+1] = ((BYTE *)pc)[1];
			Stack[sp+2] = ((BYTE *)pc)[2];
			Stack[sp+3] = ((BYTE *)pc)[3];
			Stack[sp+4] = ((BYTE *)pc)[4];
			sp += 5;
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_PUSHBYTES:
			temp = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + temp + 1);
			for (temp = -temp; temp; temp++)
			{
				PushToStack (*((BYTE *)pc + temp));
			}
			break;

		case PCD_DUP:
			Stack[sp] = Stack[sp-1];
			sp++;
			break;

		case PCD_SWAP:
			swap(Stack[sp-2], Stack[sp-1]);
			break;

		case PCD_LSPEC1:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(1), 0, 0, 0, 0);
			sp -= 1;
			break;

		case PCD_LSPEC2:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(2), STACK(1), 0, 0, 0);
			sp -= 2;
			break;

		case PCD_LSPEC3:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(3), STACK(2), STACK(1), 0, 0);
			sp -= 3;
			break;

		case PCD_LSPEC4:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(4), STACK(3), STACK(2),
									STACK(1), 0);
			sp -= 4;
			break;

		case PCD_LSPEC5:
			LineSpecials[NEXTBYTE] (activationline, activator,
									STACK(5), STACK(4), STACK(3),
									STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_LSPEC1DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], 0, 0, 0, 0);
			pc += 1;
			break;

		case PCD_LSPEC2DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], 0, 0, 0);
			pc += 2;
			break;

		case PCD_LSPEC3DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], 0, 0);
			pc += 3;
			break;

		case PCD_LSPEC4DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], pc[3], 0);
			pc += 4;
			break;

		case PCD_LSPEC5DIRECT:
			temp = NEXTBYTE;
			LineSpecials[temp] (activationline, activator,
								pc[0], pc[1], pc[2], pc[3], pc[4]);
			pc += 5;
			break;

		case PCD_LSPEC1DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], 0, 0, 0, 0);
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_LSPEC2DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], 0, 0, 0);
			pc = (int *)((BYTE *)pc + 3);
			break;

		case PCD_LSPEC3DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3], 0, 0);
			pc = (int *)((BYTE *)pc + 4);
			break;

		case PCD_LSPEC4DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], 0);
			pc = (int *)((BYTE *)pc + 5);
			break;

		case PCD_LSPEC5DIRECTB:
			LineSpecials[((BYTE *)pc)[0]] (activationline, activator,
				((BYTE *)pc)[1], ((BYTE *)pc)[2], ((BYTE *)pc)[3],
				((BYTE *)pc)[4], ((BYTE *)pc)[5]);
			pc = (int *)((BYTE *)pc + 6);
			break;

		case PCD_CALL:
		case PCD_CALLDISCARD:
			{
				int funcnum;
				int i;
				ScriptFunction *func;
				FBehavior *module = activeBehavior;

				funcnum = NEXTBYTE;
				func = activeBehavior->GetFunction (funcnum, module);
				if (func == NULL)
				{
					Printf ("Function %d in script %d out of range\n", funcnum, script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				if (sp + func->LocalCount + 64 > STACK_SIZE)
				{ // 64 is the margin for the function's working space
					Printf ("Out of stack space in script %d\n", script);
					state = SCRIPT_PleaseRemove;
					break;
				}
				// The function's first argument is also its first local variable.
				locals = &Stack[sp - func->ArgCount];
				// Make space on the stack for any other variables the function uses.
				for (i = 0; i < func->LocalCount; ++i)
				{
					Stack[sp+i] = 0;
				}
				sp += i;
				((CallReturn *)&Stack[sp])->ReturnAddress = activeBehavior->PC2Ofs (pc);
				((CallReturn *)&Stack[sp])->ReturnFunction = activeFunction;
				((CallReturn *)&Stack[sp])->ReturnModule = activeBehavior;
				((CallReturn *)&Stack[sp])->bDiscardResult = (pcd == PCD_CALLDISCARD);
				sp += sizeof(CallReturn)/sizeof(int);
				pc = module->Ofs2PC (func->Address);
				activeFunction = func;
				activeBehavior = module;
				fmt = module->GetFormat();
			}
			break;

		case PCD_RETURNVOID:
		case PCD_RETURNVAL:
			{
				int value;
				CallReturn *retState;

				if (pcd == PCD_RETURNVAL)
				{
					value = Stack[--sp];
				}
				else
				{
					value = 0;
				}
				sp -= sizeof(CallReturn)/sizeof(int);
				retState = (CallReturn *)&Stack[sp];
				pc = retState->ReturnModule->Ofs2PC (retState->ReturnAddress);
				sp -= activeFunction->ArgCount + activeFunction->LocalCount;
				activeFunction = retState->ReturnFunction;
				activeBehavior = retState->ReturnModule;
				fmt = activeBehavior->GetFormat();
				if (activeFunction == NULL)
				{
					locals = localvars;
				}
				else
				{
					locals = &Stack[sp - activeFunction->ArgCount - activeFunction->LocalCount];
				}
				if (!retState->bDiscardResult)
				{
					Stack[sp++] = value;
				}
			}
			break;

		case PCD_ADD:
			STACK(2) = STACK(2) + STACK(1);
			sp--;
			break;

		case PCD_SUBTRACT:
			STACK(2) = STACK(2) - STACK(1);
			sp--;
			break;

		case PCD_MULTIPLY:
			STACK(2) = STACK(2) * STACK(1);
			sp--;
			break;

		case PCD_DIVIDE:
			STACK(2) = STACK(2) / STACK(1);
			sp--;
			break;

		case PCD_MODULUS:
			STACK(2) = STACK(2) % STACK(1);
			sp--;
			break;

		case PCD_EQ:
			STACK(2) = (STACK(2) == STACK(1));
			sp--;
			break;

		case PCD_NE:
			STACK(2) = (STACK(2) != STACK(1));
			sp--;
			break;

		case PCD_LT:
			STACK(2) = (STACK(2) < STACK(1));
			sp--;
			break;

		case PCD_GT:
			STACK(2) = (STACK(2) > STACK(1));
			sp--;
			break;

		case PCD_LE:
			STACK(2) = (STACK(2) <= STACK(1));
			sp--;
			break;

		case PCD_GE:
			STACK(2) = (STACK(2) >= STACK(1));
			sp--;
			break;

		case PCD_ASSIGNSCRIPTVAR:
			locals[NEXTBYTE] = STACK(1);
			sp--;
			break;


		case PCD_ASSIGNMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNWORLDVAR:
			ACS_WorldVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] = STACK(1);
			sp--;
			break;

		case PCD_ASSIGNMAPARRAY:
			activeBehavior->SetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNWORLDARRAY:
			ACS_WorldArrays[NEXTBYTE].SetVal (STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_ASSIGNGLOBALARRAY:
			ACS_GlobalArrays[NEXTBYTE].SetVal (STACK(2), STACK(1));
			sp -= 2;
			break;

		case PCD_PUSHSCRIPTVAR:
			PushToStack (locals[NEXTBYTE]);
			break;

		case PCD_PUSHMAPVAR:
			PushToStack (*(activeBehavior->MapVars[NEXTBYTE]));
			break;

		case PCD_PUSHWORLDVAR:
			PushToStack (ACS_WorldVars[NEXTBYTE]);
			break;

		case PCD_PUSHGLOBALVAR:
			PushToStack (ACS_GlobalVars[NEXTBYTE]);
			break;

		case PCD_PUSHMAPARRAY:
			STACK(1) = activeBehavior->GetArrayVal (*(activeBehavior->MapVars[NEXTBYTE]), STACK(1));
			break;

		case PCD_PUSHWORLDARRAY:
			STACK(1) = ACS_WorldArrays[NEXTBYTE].GetVal (STACK(1));
			break;

		case PCD_PUSHGLOBALARRAY:
			STACK(1) = ACS_GlobalArrays[NEXTBYTE].GetVal (STACK(1));
			break;

		case PCD_ADDSCRIPTVAR:
			locals[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += STACK(1);
			sp--;
			break;

		case PCD_ADDWORLDVAR:
			ACS_WorldVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] += STACK(1);
			sp--;
			break;

		case PCD_ADDMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_ADDGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) + STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBSCRIPTVAR:
			locals[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= STACK(1);
			sp--;
			break;

		case PCD_SUBWORLDVAR:
			ACS_WorldVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] -= STACK(1);
			sp--;
			break;

		case PCD_SUBMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_SUBGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) - STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULSCRIPTVAR:
			locals[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) *= STACK(1);
			sp--;
			break;

		case PCD_MULWORLDVAR:
			ACS_WorldVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] *= STACK(1);
			sp--;
			break;

		case PCD_MULMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MULGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) * STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVSCRIPTVAR:
			locals[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) /= STACK(1);
			sp--;
			break;

		case PCD_DIVWORLDVAR:
			ACS_WorldVars[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] /= STACK(1);
			sp--;
			break;

		case PCD_DIVMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_DIVGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) / STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODSCRIPTVAR:
			locals[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) %= STACK(1);
			sp--;
			break;

		case PCD_MODWORLDVAR:
			ACS_WorldVars[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODGLOBALVAR:
			ACS_GlobalVars[NEXTBYTE] %= STACK(1);
			sp--;
			break;

		case PCD_MODMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(2);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_MODGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(2);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) % STACK(1));
				sp -= 2;
			}
			break;

		case PCD_INCSCRIPTVAR:
			++locals[NEXTBYTE];
			break;

		case PCD_INCMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) += 1;
			break;

		case PCD_INCWORLDVAR:
			++ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_INCGLOBALVAR:
			++ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_INCMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) + 1);
				sp--;
			}
			break;

		case PCD_INCWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) + 1);
				sp--;
			}
			break;

		case PCD_INCGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) + 1);
				sp--;
			}
			break;

		case PCD_DECSCRIPTVAR:
			--locals[NEXTBYTE];
			break;

		case PCD_DECMAPVAR:
			*(activeBehavior->MapVars[NEXTBYTE]) -= 1;
			break;

		case PCD_DECWORLDVAR:
			--ACS_WorldVars[NEXTBYTE];
			break;

		case PCD_DECGLOBALVAR:
			--ACS_GlobalVars[NEXTBYTE];
			break;

		case PCD_DECMAPARRAY:
			{
				int a = ACS_WorldVars[NEXTBYTE];
				int i = STACK(1);
				activeBehavior->SetArrayVal (a, i, activeBehavior->GetArrayVal (a, i) - 1);
				sp--;
			}
			break;

		case PCD_DECWORLDARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_WorldArrays[a].SetVal (i, ACS_WorldArrays[a].GetVal (i) - 1);
				sp--;
			}
			break;

		case PCD_DECGLOBALARRAY:
			{
				int a = NEXTBYTE;
				int i = STACK(1);
				ACS_GlobalArrays[a].SetVal (i, ACS_GlobalArrays[a].GetVal (i) - 1);
				sp--;
			}
			break;

		case PCD_GOTO:
			pc = activeBehavior->Ofs2PC (*pc);
			break;

		case PCD_IFGOTO:
			if (STACK(1))
				pc = activeBehavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_DROP:
			sp--;
			break;

		case PCD_DELAY:
			state = SCRIPT_Delayed;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_DELAYDIRECT:
			state = SCRIPT_Delayed;
			statedata = NEXTWORD;
			break;

		case PCD_DELAYDIRECTB:
			state = SCRIPT_Delayed;
			statedata = *(BYTE *)pc;
			pc = (int *)((BYTE *)pc + 1);
			break;

		case PCD_RANDOM:
			STACK(2) = Random (STACK(2), STACK(1));
			sp--;
			break;
			
		case PCD_RANDOMDIRECT:
			PushToStack (Random (pc[0], pc[1]));
			pc += 2;
			break;

		case PCD_RANDOMDIRECTB:
			PushToStack (Random (((BYTE *)pc)[0], ((BYTE *)pc)[1]));
			pc = (int *)((BYTE *)pc + 2);
			break;

		case PCD_THINGCOUNT:
			STACK(2) = ThingCount (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_THINGCOUNTDIRECT:
			PushToStack (ThingCount (pc[0], pc[1]));
			pc += 2;
			break;

		case PCD_TAGWAIT:
			state = SCRIPT_TagWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_TAGWAITDIRECT:
			state = SCRIPT_TagWait;
			statedata = NEXTWORD;
			break;

		case PCD_POLYWAIT:
			state = SCRIPT_PolyWait;
			statedata = STACK(1);
			sp--;
			break;

		case PCD_POLYWAITDIRECT:
			state = SCRIPT_PolyWait;
			statedata = NEXTWORD;
			break;

		case PCD_CHANGEFLOOR:
			ChangeFlat (STACK(2), STACK(1), 0);
			sp -= 2;
			break;

		case PCD_CHANGEFLOORDIRECT:
			ChangeFlat (pc[0], pc[1], 0);
			pc += 2;
			break;

		case PCD_CHANGECEILING:
			ChangeFlat (STACK(2), STACK(1), 1);
			sp -= 2;
			break;

		case PCD_CHANGECEILINGDIRECT:
			ChangeFlat (pc[0], pc[1], 1);
			pc += 2;
			break;

		case PCD_RESTART:
			pc = activeBehavior->FindScript (script);
			break;

		case PCD_ANDLOGICAL:
			STACK(2) = (STACK(2) && STACK(1));
			sp--;
			break;

		case PCD_ORLOGICAL:
			STACK(2) = (STACK(2) || STACK(1));
			sp--;
			break;

		case PCD_ANDBITWISE:
			STACK(2) = (STACK(2) & STACK(1));
			sp--;
			break;

		case PCD_ORBITWISE:
			STACK(2) = (STACK(2) | STACK(1));
			sp--;
			break;

		case PCD_EORBITWISE:
			STACK(2) = (STACK(2) ^ STACK(1));
			sp--;
			break;

		case PCD_NEGATELOGICAL:
			STACK(1) = !STACK(1);
			break;

		case PCD_LSHIFT:
			STACK(2) = (STACK(2) << STACK(1));
			sp--;
			break;

		case PCD_RSHIFT:
			STACK(2) = (STACK(2) >> STACK(1));
			sp--;
			break;

		case PCD_UNARYMINUS:
			STACK(1) = -STACK(1);
			break;

		case PCD_IFNOTGOTO:
			if (!STACK(1))
				pc = activeBehavior->Ofs2PC (*pc);
			else
				pc++;
			sp--;
			break;

		case PCD_LINESIDE:
			PushToStack (lineSide);
			break;

		case PCD_SCRIPTWAIT:
			statedata = STACK(1);
			if (controller->RunningScripts[statedata])
				state = SCRIPT_ScriptWait;
			else
				state = SCRIPT_ScriptWaitPre;
			sp--;
			PutLast ();
			break;

		case PCD_SCRIPTWAITDIRECT:
			state = SCRIPT_ScriptWait;
			statedata = NEXTWORD;
			PutLast ();
			break;

		case PCD_CLEARLINESPECIAL:
			if (activationline)
				activationline->special = 0;
			break;

		case PCD_CASEGOTO:
			if (STACK(1) == NEXTWORD)
			{
				pc = activeBehavior->Ofs2PC (*pc);
				sp--;
			}
			else
			{
				pc++;
			}
			break;

		case PCD_BEGINPRINT:
			workwhere = work;
			work[0] = 0;
			break;

		case PCD_PRINTSTRING:
		case PCD_PRINTLOCALIZED:
			lookup = (pcd == PCD_PRINTSTRING ?
				FBehavior::StaticLookupString (STACK(1)) :
				FBehavior::StaticLocalizeString (STACK(1)));
			if (lookup != NULL)
			{
				workwhere += sprintf (workwhere, "%s", lookup);
			}
			--sp;
			break;

		case PCD_PRINTNUMBER:
			workwhere += sprintf (workwhere, "%ld", STACK(1));
			--sp;
			break;

		case PCD_PRINTCHARACTER:
			workwhere[0] = STACK(1);
			workwhere[1] = 0;
			workwhere++;
			--sp;
			break;

		case PCD_PRINTFIXED:
			workwhere += sprintf (workwhere, "%g", FIXED2FLOAT(STACK(1)));
			--sp;
			break;

		// [BC] Print activator's name
		// [RH] Fancied up a bit
		case PCD_PRINTNAME:
			{
				player_t *player = NULL;

				if (STACK(1) == 0 || (unsigned)STACK(1) > MAXPLAYERS)
				{
					if (activator)
					{
						player = activator->player;
					}
				}
				else if (playeringame[STACK(1)])
				{
					player = &players[STACK(1)];
				}
				else
				{
					workwhere += sprintf (workwhere, "Player %ld\n", STACK(1));
					sp--;
					break;
				}
				if (player)
				{
					workwhere += sprintf (workwhere, "%s", activator->player->userinfo.netname);
				}
				else if (activator)
				{
					workwhere += sprintf (workwhere, "%s", RUNTIME_TYPE(activator)->Name+1);
				}
				else
				{
					workwhere += sprintf (workwhere, " ");
				}
				sp--;
			}
			break;

		case PCD_ENDPRINT:
		case PCD_ENDPRINTBOLD:
		case PCD_MOREHUDMESSAGE:
			strbin (work);
			if (pcd != PCD_MOREHUDMESSAGE)
			{
				AActor *screen = activator;
				// If a missile is the activator, make the thing that
				// launched the missile the target of the print command.
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDPRINTBOLD || screen == NULL ||
					players[consoleplayer].mo == screen)
				{
					strbin (work);
					C_MidPrint (work);
				}
			}
			else
			{
				optstart = -1;
			}
			break;

		case PCD_OPTHUDMESSAGE:
			optstart = sp;
			break;

		case PCD_ENDHUDMESSAGE:
		case PCD_ENDHUDMESSAGEBOLD:
			if (optstart == -1)
			{
				optstart = sp;
			}
			{
				AActor *screen = activator;
				if (screen != NULL &&
					screen->player == NULL &&
					(screen->flags & MF_MISSILE) &&
					screen->target != NULL)
				{
					screen = screen->target;
				}
				if (pcd == PCD_ENDHUDMESSAGEBOLD || screen == NULL ||
					players[consoleplayer].mo == screen)
				{
					int type = Stack[optstart-6];
					int id = Stack[optstart-5];
					EColorRange color = CLAMPCOLOR(Stack[optstart-4]);
					float x = FIXED2FLOAT(Stack[optstart-3]);
					float y = FIXED2FLOAT(Stack[optstart-2]);
					float holdTime = FIXED2FLOAT(Stack[optstart-1]);
					DHUDMessage *msg;

					switch (type & ~HUDMSG_LOG)
					{
					default:	// normal
						msg = new DHUDMessage (work, x, y, color, holdTime);
						break;
					case 1:		// fade out
						{
							float fadeTime = (optstart < sp) ?
								FIXED2FLOAT(Stack[optstart]) : 0.5f;
							msg = new DHUDMessageFadeOut (work, x, y, color, holdTime, fadeTime);
						}
						break;
					case 2:		// type on, then fade out
						{
							float typeTime = (optstart < sp) ?
								FIXED2FLOAT(Stack[optstart]) : 0.05f;
							float fadeTime = (optstart < sp-1) ?
								FIXED2FLOAT(Stack[optstart+1]) : 0.5f;
							msg = new DHUDMessageTypeOnFadeOut (work, x, y, color, typeTime, holdTime, fadeTime);
						}
						break;
					}
					StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0);
					if (type & HUDMSG_LOG)
					{
						static char bar[] = TEXTCOLOR_ORANGE "\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
					"\36\36\36\36\36\36\36\36\36\36\36\36\37" TEXTCOLOR_NORMAL "\n";

						workreal[0] = '\x1c';
						workreal[1] = color >= CR_BRICK && color <= CR_YELLOW ? 'A' + color : '-';
						AddToConsole (-1, bar);
						AddToConsole (-1, workreal);
						AddToConsole (-1, bar);
					}
				}
			}
			sp = optstart-6;
			break;

		case PCD_SETFONT:
			DoSetFont (STACK(1));
			sp--;
			break;

		case PCD_SETFONTDIRECT:
			DoSetFont (pc[0]);
			pc++;
			break;

		case PCD_PLAYERCOUNT:
			PushToStack (CountPlayers ());
			break;

		case PCD_GAMETYPE:
			if (deathmatch)
				PushToStack (GAME_NET_DEATHMATCH);
			else if (multiplayer)
				PushToStack (GAME_NET_COOPERATIVE);
			else
				PushToStack (GAME_SINGLE_PLAYER);
			break;

		case PCD_GAMESKILL:
			PushToStack (gameskill);
			break;

// [BC] Start ST PCD's
		case PCD_PLAYERHEALTH:
			if (activator)
				PushToStack (activator->health);
			break;

		case PCD_PLAYERARMORPOINTS:
			if (activator && activator->player)
				PushToStack (activator->player->armorpoints[0]);
			break;

		case PCD_PLAYERFRAGS:
			if (activator && activator->player)
				PushToStack (activator->player->fragcount);
			break;

		case PCD_MUSICCHANGE:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_ChangeMusic (lookup, STACK(1));
			}
			sp -= 2;
			break;

		case PCD_SINGLEPLAYER:
			PushToStack (!netgame);
			break;
// [BC] End ST PCD's

		case PCD_TIMER:
			PushToStack (level.time);
			break;

		case PCD_SECTORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				if (activationline)
				{
					S_Sound (
						activationline->frontsector->soundorg,
						CHAN_AUTO,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
				else
				{
					S_Sound (
						CHAN_AUTO,
						lookup,
						(float)(STACK(1)) / 127.f,
						ATTN_NORM);
				}
			}
			sp -= 2;
			break;

		case PCD_AMBIENTSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_LOCALAMBIENTSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL && players[consoleplayer].camera == activator)
			{
				S_Sound (CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NONE);
			}
			sp -= 2;
			break;

		case PCD_ACTIVATORSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				S_Sound (activator, CHAN_AUTO,
						 lookup,
						 (float)(STACK(1)) / 127.f, ATTN_NORM);
			}
			sp -= 2;
			break;

		case PCD_SOUNDSEQUENCE:
			lookup = FBehavior::StaticLookupString (STACK(1));
			if (lookup != NULL)
			{
				if (activationline)
				{
					SN_StartSequence (
						activationline->frontsector,
						lookup);
				}
			}
			sp--;
			break;

		case PCD_SETLINETEXTURE:
			SetLineTexture (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 4;
			break;

		case PCD_SETLINEBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					switch (STACK(1))
					{
					case BLOCK_NOTHING:
						lines[line].flags &= ~(ML_BLOCKING|ML_BLOCKEVERYTHING);
						break;
					case BLOCK_CREATURES:
					default:
						lines[line].flags &= ~ML_BLOCKEVERYTHING;
						lines[line].flags |= ML_BLOCKING;
						break;
					case BLOCK_EVERYTHING:
						lines[line].flags |= ML_BLOCKING|ML_BLOCKEVERYTHING;
						break;
					}
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINEMONSTERBLOCKING:
			{
				int line = -1;

				while ((line = P_FindLineFromID (STACK(2), line)) >= 0)
				{
					if (STACK(1))
						lines[line].flags |= ML_BLOCKMONSTERS;
					else
						lines[line].flags &= ~ML_BLOCKMONSTERS;
				}

				sp -= 2;
			}
			break;

		case PCD_SETLINESPECIAL:
			{
				int linenum = -1;

				while ((linenum = P_FindLineFromID (STACK(7), linenum)) >= 0) {
					line_t *line = &lines[linenum];

					line->special = STACK(6);
					line->args[0] = STACK(5);
					line->args[1] = STACK(4);
					line->args[2] = STACK(3);
					line->args[3] = STACK(2);
					line->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_SETTHINGSPECIAL:
			{
				FActorIterator iterator (STACK(7));
				AActor *actor;

				while ( (actor = iterator.Next ()) )
				{
					actor->special = STACK(6);
					actor->args[0] = STACK(5);
					actor->args[1] = STACK(4);
					actor->args[2] = STACK(3);
					actor->args[3] = STACK(2);
					actor->args[4] = STACK(1);
				}
				sp -= 7;
			}
			break;

		case PCD_THINGSOUND:
			lookup = FBehavior::StaticLookupString (STACK(2));
			if (lookup != NULL)
			{
				FActorIterator iterator (STACK(3));
				AActor *spot;

				while ( (spot = iterator.Next ()) )
				{
					S_Sound (spot, CHAN_BODY,
							 lookup,
							 (float)(STACK(1))/127.f, ATTN_NORM);
				}
			}
			sp -= 3;
			break;

		case PCD_FIXEDMUL:
			STACK(2) = FixedMul (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_FIXEDDIV:
			STACK(2) = FixedDiv (STACK(2), STACK(1));
			sp--;
			break;

		case PCD_SETGRAVITY:
			level.gravity = (float)STACK(1) / 65536.f;
			sp--;
			break;

		case PCD_SETGRAVITYDIRECT:
			level.gravity = (float)pc[0] / 65536.f;
			pc++;
			break;

		case PCD_SETAIRCONTROL:
			level.aircontrol = STACK(1);
			sp--;
			G_AirControlChanged ();
			break;

		case PCD_SETAIRCONTROLDIRECT:
			level.aircontrol = pc[0];
			pc++;
			G_AirControlChanged ();
			break;

		case PCD_SPAWN:
			STACK(6) = DoSpawn (STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_SPAWNDIRECT:
			PushToStack (DoSpawn (pc[0], pc[1], pc[2], pc[3], pc[4], pc[5]));
			pc += 6;
			break;

		case PCD_SPAWNSPOT:
			STACK(4) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_SPAWNSPOTDIRECT:
			PushToStack (DoSpawnSpot (pc[0], pc[1], pc[2], pc[3]));
			pc += 4;
			break;

		case PCD_CLEARINVENTORY:
			ClearInventory (activator);
			break;

		case PCD_GIVEINVENTORY:
			GiveInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_GIVEINVENTORYDIRECT:
			GiveInventory (activator, FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_TAKEINVENTORY:
			TakeInventory (activator, FBehavior::StaticLookupString (STACK(2)), STACK(1));
			sp -= 2;
			break;

		case PCD_TAKEINVENTORYDIRECT:
			TakeInventory (activator, FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 2;
			break;

		case PCD_CHECKINVENTORY:
			STACK(1) = CheckInventory (activator, FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_CHECKINVENTORYDIRECT:
			PushToStack (CheckInventory (activator, FBehavior::StaticLookupString (pc[0])));
			pc += 1;
			break;

		case PCD_SETMUSIC:
			S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			sp -= 3;
			break;

		case PCD_SETMUSICDIRECT:
			S_ChangeMusic (FBehavior::StaticLookupString (pc[0]), pc[1]);
			pc += 3;
			break;

		case PCD_LOCALSETMUSIC:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (STACK(3)), STACK(2));
			}
			sp -= 3;
			break;

		case PCD_LOCALSETMUSICDIRECT:
			if (activator == players[consoleplayer].mo)
			{
				S_ChangeMusic (FBehavior::StaticLookupString (pc[0]), pc[1]);
			}
			pc += 3;
			break;

		case PCD_FADETO:
			DoFadeTo (STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 5;
			break;

		case PCD_FADERANGE:
			DoFadeRange (STACK(9), STACK(8), STACK(7), STACK(6),
						 STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 9;
			break;

		case PCD_CANCELFADE:
			{
				TThinkerIterator<DFlashFader> iterator;
				DFlashFader *fader;

				while ( (fader = iterator.Next()) )
				{
					if (activator == NULL || fader->WhoFor() == activator)
					{
						fader->Cancel ();
					}
				}
			}
			break;

		case PCD_PLAYMOVIE:
			STACK(1) = I_PlayMovie (FBehavior::StaticLookupString (STACK(1)));
			break;

		case PCD_GETACTORX:
		case PCD_GETACTORY:
		case PCD_GETACTORZ:
			{
				AActor *actor;

				if (STACK(1) == 0)
				{
					actor = activator;
				}
				else
				{
					FActorIterator iterator (STACK(1));
					actor = iterator.Next ();
				}
				if (actor == NULL)
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = (&actor->x)[pcd - PCD_GETACTORX];
				}
			}
			break;

		case PCD_SETFLOORTRIGGER:
			new DPlaneWatcher (activator, activationline, lineSide, false, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_SETCEILINGTRIGGER:
			new DPlaneWatcher (activator, activationline, lineSide, true, STACK(8),
				STACK(7), STACK(6), STACK(5), STACK(4), STACK(3), STACK(2), STACK(1));
			sp -= 8;
			break;

		case PCD_STARTTRANSLATION:
			{
				int i = STACK(1);
				sp--;
				if (i >= 1 && i <= MAX_ACS_TRANSLATIONS)
				{
					translation = &translationtables[TRANSLATION_LevelScripted][i*256-256];
					for (i = 0; i < 256; ++i)
					{
						translation[i] = i;
					}
				}
			}
			break;

		case PCD_TRANSLATIONRANGE1:
			{ // translation using palette shifting
				int start = STACK(4);
				int end = STACK(3);
				int pal1 = STACK(2);
				int pal2 = STACK(1);
				fixed_t palcol, palstep;
				sp -= 4;

				if (translation == NULL)
				{
					break;
				}
				if (start > end)
				{
					swap (start, end);
					swap (pal1, pal2);
				}
				else if (start == end)
				{
					translation[start] = pal1;
					break;
				}
				palcol = pal1 << FRACBITS;
				palstep = ((pal2 << FRACBITS) - palcol) / (end - start);
				for (int i = start; i <= end; palcol += palstep, ++i)
				{
					translation[i] = palcol >> FRACBITS;
				}
			}
			break;

		case PCD_TRANSLATIONRANGE2:
			{ // translation using RGB values
			  // (would HSV be a good idea too?)
				int start = STACK(8);
				int end = STACK(7);
				fixed_t r1 = STACK(6) << FRACBITS;
				fixed_t g1 = STACK(5) << FRACBITS;
				fixed_t b1 = STACK(4) << FRACBITS;
				fixed_t r2 = STACK(3) << FRACBITS;
				fixed_t g2 = STACK(2) << FRACBITS;
				fixed_t b2 = STACK(1) << FRACBITS;
				fixed_t r, g, b;
				fixed_t rs, gs, bs;
				sp -= 8;

				if (translation == NULL)
				{
					break;
				}
				if (start > end)
				{
					swap (start, end);
					r = r2;
					g = g2;
					b = b2;
					rs = r1 - r2;
					gs = g1 - g2;
					bs = b1 - b2;
				}
				else
				{
					r = r1;
					g = g1;
					b = b1;
					rs = r2 - r1;
					gs = g2 - g1;
					bs = b2 - b1;
				}
				if (start == end)
				{
					translation[start] = ColorMatcher.Pick
						(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
					break;
				}
				rs /= (end - start);
				gs /= (end - start);
				bs /= (end - start);
				for (int i = start; i <= end; ++i)
				{
					translation[i] = ColorMatcher.Pick
						(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
					r += rs;
					g += gs;
					b += bs;
				}
			}
			break;

		case PCD_ENDTRANSLATION:
			// This might be useful for hardware rendering, but
			// for software it is superfluous.
			translation = NULL;
			break;

		case PCD_SIN:
			STACK(1) = finesine[(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_COS:
			STACK(1) = finecosine[(STACK(1)<<16)>>ANGLETOFINESHIFT];
			break;

		case PCD_VECTORANGLE:
			STACK(2) = R_PointToAngle2 (0, 0, STACK(2), STACK(1)) >> 16;
			sp--;
			break;

		case PCD_CHECKWEAPON:
			if (activator == NULL || activator->player == NULL)
			{ // Non-players do not have ready weapons
				STACK(1) = 0;
			}
			else
			{
				STACK(1) = 0 == strcmp (FBehavior::StaticLookupString (STACK(1)),
					wpnlev1info[activator->player->readyweapon]->type->Name+1);
			}
			break;

		case PCD_SETWEAPON:
			if (activator == NULL || activator->player == NULL)
			{
				STACK(1) = 0;
			}
			else
			{
				int i;

				for (i = 0; i < NUMWEAPONS; ++i)
				{
					if (wpnlev1info[i] != NULL &&
						wpnlev1info[i]->type != NULL &&
						0 == strcmp (FBehavior::StaticLookupString (STACK(1)),
									 wpnlev1info[i]->type->Name+1))
					{
						break;
					}
				}
				if (i >= NUMWEAPONS || !activator->player->weaponowned[i])
				{
					STACK(1) = 0;
				}
				else
				{
					STACK(1) = 1;
					if (activator->player->readyweapon != i)
					{
						weapontype_t oldpend = activator->player->pendingweapon;
						weapontype_t oldready = activator->player->readyweapon;

						activator->player->readyweapon = (weapontype_t)i;
						if (P_CheckAmmo (activator->player))
						{
							activator->player->pendingweapon = (weapontype_t)i;
						}
						else
						{ // Not enough ammo for the new weapon
							STACK(1) = 0;
							activator->player->pendingweapon = oldpend;
						}
						activator->player->readyweapon = oldready;
					}
				}
			}
			break;
			
		case PCD_SETMARINEWEAPON:
			if (STACK(2) != 0)
			{
				AScriptedMarine *marine;
				TActorIterator<AScriptedMarine> iterator (STACK(2));

				while ((marine = iterator.Next()) != NULL)
				{
					marine->SetWeapon ((AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			else
			{
				if (activator->IsKindOf (RUNTIME_CLASS(AScriptedMarine)))
				{
					static_cast<AScriptedMarine *>(activator)->SetWeapon (
						(AScriptedMarine::EMarineWeapon)STACK(1));
				}
			}
			sp -= 2;
			break;

		case PCD_SETACTORPROPERTY:
			SetActorProperty (STACK(3), STACK(2), STACK(1));
			sp -= 3;
			break;

		case PCD_GETACTORPROPERTY:
			STACK(2) = GetActorProperty (STACK(2), STACK(1));
			sp -= 1;
			break;
		}
	}

	this->pc = pc;
	assert (sp == 0);

	if (screen != NULL)
	{
		screen->SetFont (SmallFont);
	}

	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		if (controller->RunningScripts[script] == this)
			controller->RunningScripts[script] = NULL;
		this->Destroy ();
	}
}

#undef PushtoStack

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code, FBehavior *module,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller && !always && controller->RunningScripts[num])
	{
		if (controller->RunningScripts[num]->GetState () == DLevelScript::SCRIPT_Suspended)
		{
			controller->RunningScripts[num]->SetState (DLevelScript::SCRIPT_Running);
			return true;
		}
		return false;
	}

	new DLevelScript (who, where, num, code, module, lineSide, arg0, arg1, arg2, always, delay);

	return true;
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, int *code, FBehavior *module,
							int lineside, int arg0, int arg1, int arg2, int always, bool delay)
	: activeBehavior (module)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	localvars[0] = arg0;
	localvars[1] = arg1;
	localvars[2] = arg2;
	memset (localvars+3, 0, sizeof(localvars)-3*sizeof(int));
	pc = code;
	activator = who;
	activationline = where;
	lineSide = lineside;
	activefont = SmallFont;
	if (delay)
	{
		// From Hexen: Give the world some time to set itself up before running open scripts.
		//script->state = SCRIPT_Delayed;
		//script->statedata = TICRATE;
		state = SCRIPT_Running;
	}
	else
	{
		state = SCRIPT_Running;
	}

	if (!always)
		DACSThinker::ActiveThinker->RunningScripts[num] = this;

	Link ();

	DPrintf ("Script %d started.\n", num);
}

static void SetScriptState (int script, DLevelScript::EScriptState state)
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

	if (controller->RunningScripts[script])
		controller->RunningScripts[script]->SetState (state);
}

void P_DoDeferedScripts ()
{
	acsdefered_t *def;
	int *scriptdata;
	FBehavior *module;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = FBehavior::StaticFindScript (def->script, module);
			if (scriptdata)
			{
				P_GetScriptGoing ((unsigned)def->playernum < MAXPLAYERS &&
					playeringame[def->playernum] ? players[def->playernum].mo : NULL,
					NULL, def->script,
					scriptdata, module,
					0, def->arg0, def->arg1, def->arg2,
					def->type == acsdefered_t::defexealways, true);
			}
			else
			{
				Printf ("P_DoDeferredScripts: Unknown script %d\n", def->script);
			}
			break;

		case acsdefered_t::defsuspend:
			SetScriptState (def->script, DLevelScript::SCRIPT_Suspended);
			DPrintf ("Defered suspend of script %d\n", def->script);
			break;

		case acsdefered_t::defterminate:
			SetScriptState (def->script, DLevelScript::SCRIPT_PleaseRemove);
			DPrintf ("Defered terminate of script %d\n", def->script);
			break;
		}
		delete def;
		def = next;
	}
	level.info->defered = NULL;
}

static void addDefered (level_info_t *i, acsdefered_t::EType type, int script, int arg0, int arg1, int arg2, AActor *who)
{
	if (i)
	{
		acsdefered_t *def = new acsdefered_s;

		def->next = i->defered;
		def->type = type;
		def->script = script;
		def->arg0 = arg0;
		def->arg1 = arg1;
		def->arg2 = arg2;
		if (who != NULL && who->player != NULL)
		{
			def->playernum = who->player - players;
		}
		else
		{
			def->playernum = -1;
		}
		i->defered = def;
		DPrintf ("Script %d on map %s defered\n", script, i->mapname);
	}
}

bool P_StartScript (AActor *who, line_t *where, int script, char *map, int lineSide,
					int arg0, int arg1, int arg2, int always)
{
	if (map == NULL || 0 == strnicmp (level.mapname, map, 8))
	{
		FBehavior *module = NULL;
		int *scriptdata;

		if ((scriptdata = FBehavior::StaticFindScript (script, module)) != NULL)
		{
			return P_GetScriptGoing (who, where, script,
									 scriptdata, module,
									 lineSide, arg0, arg1, arg2, always, false);
		}
		else
		{
			Printf ("P_StartScript: Unknown script %d\n", script);
		}
	}
	else
	{
		addDefered (FindLevelInfo (map),
					always ? acsdefered_t::defexealways : acsdefered_t::defexecute,
					script, arg0, arg1, arg2, who);
	}
	return false;
}

void P_SuspendScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defsuspend, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_Suspended);
}

void P_TerminateScript (int script, char *map)
{
	if (strnicmp (level.mapname, map, 8))
		addDefered (FindLevelInfo (map), acsdefered_t::defterminate, script, 0, 0, 0, NULL);
	else
		SetScriptState (script, DLevelScript::SCRIPT_PleaseRemove);
}

void strbin (char *str)
{
	char *p = str, c;
	int i;

	while ( (c = *p++) ) {
		if (c != '\\') {
			*str++ = c;
		} else {
			switch (*p) {
				case 'c':
					*str++ = TEXTCOLOR_ESCAPE;
					break;
				case 'n':
					*str++ = '\n';
					break;
				case 't':
					*str++ = '\t';
					break;
				case 'r':
					*str++ = '\r';
					break;
				case '\n':
					break;
				case 'x':
				case 'X':
					c = 0;
					p++;
					for (i = 0; i < 2; i++) {
						c <<= 4;
						if (*p >= '0' && *p <= '9')
							c += *p-'0';
						else if (*p >= 'a' && *p <= 'f')
							c += 10 + *p-'a';
						else if (*p >= 'A' && *p <= 'F')
							c += 10 + *p-'A';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
					c = 0;
					for (i = 0; i < 3; i++) {
						c <<= 3;
						if (*p >= '0' && *p <= '7')
							c += *p-'0';
						else
							break;
						p++;
					}
					*str++ = c;
					break;
				default:
					*str++ = *p;
					break;
			}
			p++;
		}
	}
	*str = 0;
}

FArchive &operator<< (FArchive &arc, acsdefered_s *&defertop)
{
	BYTE more;

	if (arc.IsStoring ())
	{
		acsdefered_s *defer = defertop;
		more = 1;
		while (defer)
		{
			BYTE type;
			arc << more;
			type = (BYTE)defer->type;
			arc << type << defer->script << defer->playernum
				<< defer->arg0 << defer->arg1 << defer->arg2;
			defer = defer->next;
		}
		more = 0;
		arc << more;
	}
	else
	{
		acsdefered_s **defer = &defertop;

		arc << more;
		while (more)
		{
			*defer = new acsdefered_s;
			arc << more;
			(*defer)->type = (acsdefered_s::EType)more;
			arc << (*defer)->script << (*defer)->playernum
				<< (*defer)->arg0 << (*defer)->arg1 << (*defer)->arg2;
			defer = &((*defer)->next);
			arc << more;
		}
		*defer = NULL;
	}
	return arc;
}

CCMD (scriptstat)
{
	if (DACSThinker::ActiveThinker == NULL)
	{
		Printf ("No scripts are running.\n");
	}
	else
	{
		DACSThinker::ActiveThinker->DumpScriptStatus ();
	}
}

void DACSThinker::DumpScriptStatus ()
{
	static const char *stateNames[] =
	{
		"Running",
		"Suspended",
		"Delayed",
		"TagWait",
		"PolyWait",
		"ScriptWaitPre",
		"ScriptWait",
		"PleaseRemove"
	};
	DLevelScript *script = Scripts;

	while (script != NULL)
	{
		Printf ("%d: %s\n", script->script, stateNames[script->state]);
		script = script->next;
	}
}
