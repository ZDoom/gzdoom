// [RH] p_acs.c: New file to handle ACS scripts

#include "z_zone.h"
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
#include "s_sndseq.h"
#include "i_system.h"
#include "sbar.h"
#include "vectors.h"
#include "m_swap.h"

#define CLAMPCOLOR(c)	(EColorRange)((unsigned)(c)>CR_UNTRANSLATED?CR_UNTRANSLATED:(c))
#define LANGREGIONMASK	MAKE_ID(0,0,0xff,0xff)	

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code,
	int lineSide, int arg0, int arg1, int arg2, int always, bool delay);

//---- Temporary inventory functions -- to be removed ----//
#include "gi.h"

static void ClearInventory (AActor *activator)
{
	if (!activator->player)
		return;

	player_t *player = activator->player;
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
}

static const char *DoomAmmoNames[4] =
{
	"Clip", "Shell", "RocketAmmo", "Cell"
};

static const char *DoomWeaponNames[9] =
{
	"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher",
	"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun"
};

static const char *HereticAmmoNames[6] =
{
	"GoldWandAmmo", "CrossbowAmmo", "BlasterAmmo",
	"SkullRodAmmo", "PhoenixRodAmmo", "MaceAmmo"
};

static const char *HereticWeaponNames[9] =
{
	"Staff", "GoldWand", "Crossbow", "Blaster", "SkullRod",
	"PhoenixRod", "Mace", "Gauntlets", "Beak"
};

static const char *HereticArtifactNames[11] =
{
	"ArtiInvulnerability", "ArtiInvisibility", "ArtiHealth",
	"ArtiSuperHealth", "ArtiTomeOfPower", NULL, NULL, "ArtiTorch",
	"ArtiTimeBomb", "ArtiEgg", "ArtiFly"
};

static void GiveInventory (AActor *activator, const char *type, int amount)
{
	if (!activator->player)
		return;

	player_t *player = activator->player;
	int i;

	if (type[0] == 'H' && strcmp (type+1, "ealth") == 0)
	{
		player->health = amount;
		if (player->mo)
		{
			player->mo->health = amount;
		}
	}
	else if (amount > 0)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			for (i = 0; i < 4; i++)
			{
				if (strcmp (DoomAmmoNames[i], type) == 0)
				{
					player->ammo[i] = MIN(player->ammo[i]+amount, player->ammo[i]);
					return;
				}
			}
			for (i = 0; i < 9; i++)
			{
				if (strcmp (DoomWeaponNames[i], type) == 0)
				{
					player->weaponowned[i] = true;
					return;
				}
			}
		}
		else
		{
			for (i = 0; i < 6; i++)
			{
				if (strcmp (HereticAmmoNames[i], type) == 0)
				{
					player->ammo[i] = MIN(player->ammo[i]+amount, player->ammo[i]);
					return;
				}
			}
			for (i = 0; i < 9; i++)
			{
				if (strcmp (HereticWeaponNames[i], type) == 0)
				{
					player->weaponowned[i] = true;
					return;
				}
			}
			for (i = 0; i < 11; i++)
			{
				if (strcmp (HereticArtifactNames[i], type) == 0)
				{
					player->inventory[i] = MIN(player->inventory[i]+amount, 16);
					return;
				}
			}
		}
		const TypeInfo *info = TypeInfo::FindType (type);
		do
		{
			AInventory *item = static_cast<AInventory *>(Spawn (info, activator->x, activator->y, activator->z));
			item->Touch (activator);
			if (!(item->ObjectFlags & OF_MassDestruction))
			{
				item->Destroy ();
			}
		} while (--amount > 0);
	}
}

static void TakeInventory (AActor *activator, const char *type, int amount)
{
	if (!activator->player || amount <= 0)
		return;

	player_t *player = activator->player;
	int i;

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 4; i++)
		{
			if (strcmp (DoomAmmoNames[i], type) == 0)
			{
				player->ammo[i] = MAX(player->ammo[i]-amount, 0);
				return;
			}
		}
		for (i = 0; i < 9; i++)
		{
			if (strcmp (DoomWeaponNames[i], type) == 0)
			{
				player->weaponowned[i] = false;
				return;
			}
		}
	}
	else
	{
		for (i = 0; i < 6; i++)
		{
			if (strcmp (HereticAmmoNames[i], type) == 0)
			{
				player->ammo[i] = MAX(player->ammo[i]-amount, 0);
				return;
			}
		}
		for (i = 0; i < 9; i++)
		{
			if (strcmp (HereticWeaponNames[i], type) == 0)
			{
				player->weaponowned[i] = true;
				return;
			}
		}
		for (i = 0; i < 11; i++)
		{
			if (strcmp (HereticArtifactNames[i], type) == 0)
			{
				player->inventory[i] = MAX(player->inventory[i]-amount, 0);
				return;
			}
		}
	}
}

static int CheckInventory (AActor *activator, const char *type)
{
	if (!activator->player)
		return 0;

	player_t *player = activator->player;
	int i;

	if (gameinfo.gametype == GAME_Doom)
	{
		for (i = 0; i < 4; i++)
		{
			if (strcmp (DoomAmmoNames[i], type) == 0)
			{
				return player->ammo[i];
			}
		}
		for (i = 0; i < 9; i++)
		{
			if (strcmp (DoomWeaponNames[i], type) == 0)
			{
				return player->weaponowned[i] ? 1 : 0;
			}
		}
	}
	else
	{
		for (i = 0; i < 6; i++)
		{
			if (strcmp (HereticAmmoNames[i], type) == 0)
			{
				return player->ammo[i];
			}
		}
		for (i = 0; i < 9; i++)
		{
			if (strcmp (HereticWeaponNames[i], type) == 0)
			{
				return player->weaponowned[i] ? 1 : 0;
			}
		}
		for (i = 0; i < 11; i++)
		{
			if (strcmp (HereticArtifactNames[i], type) == 0)
			{
				return player->inventory[i];
			}
		}
	}
	return 0;
}

//---- ACS lump manager ----//

FBehavior::FBehavior (BYTE *object, int len)
{
	int i;

	NumScripts = 0;
	Scripts = NULL;
	Chunks = NULL;

	if (object[0] != 'A' || object[1] != 'C' || object[2] != 'S')
	{
		Format = ACS_Unknown;
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
		Format = ACS_Unknown;
		return;
	}

	Data = object;
	DataSize = len;
	
	if (Format == ACS_Old)
	{
		Chunks = object + len;
		Scripts = object + ((DWORD *)object)[1];
		NumScripts = ((DWORD *)Scripts)[0];
		Scripts += 4;
		for (i = 0; i < NumScripts; i++)
		{
			ScriptPtr *ptr = (ScriptPtr *)(Scripts + 12*i);
			DWORD numtype = ((DWORD *)ptr)[0];
			ptr->Number = numtype % 1000;
			ptr->Type = numtype / 1000;
		}
	}
	else
	{
		Chunks = object + ((DWORD *)object)[1];
		Scripts = FindChunk (MAKE_ID('S','P','T','R'));
		NumScripts = ((DWORD *)Scripts)[1] / 12;
		Scripts += 8;
	}

	// Sort scripts, so we can use a binary search to find them
	if (NumScripts > 0)
	{
		qsort (Scripts, NumScripts, 12, SortScripts);
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

	DPrintf ("Loaded %d scripts\n", NumScripts);
}

FBehavior::~FBehavior ()
{
	// Data is freed by the zone heap
}

int STACK_ARGS FBehavior::SortScripts (const void *a, const void *b)
{
	ScriptPtr *ptr1 = (ScriptPtr *)a;
	ScriptPtr *ptr2 = (ScriptPtr *)b;
	return ptr1->Number - ptr2->Number;
}

bool FBehavior::IsGood ()
{
	return Format != ACS_Unknown;
}

int *FBehavior::FindScript (int script) const
{
	if (NumScripts == 0)
		return NULL;

	int min = 0;
	int max = NumScripts - 1;
	int probe = NumScripts/2;
	ScriptPtr *thisone;

	while (min <= max)
	{
		thisone = (ScriptPtr *)(Scripts + 12*probe);
		if (thisone->Number == script)
			return (int *)(thisone->Address + Data);
		else if (thisone->Number < script)
			min = probe + 1;
		else
			max = probe - 1;
		probe = (max + min) / 2;
	}

	return NULL;
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

void FBehavior::StartTypedScripts (WORD type, AActor *activator) const
{
	ScriptPtr *ptr;
	DWORD i;

	for (i = 0; i < NumScripts; i++)
	{
		ptr = (ScriptPtr *)(Scripts + 12*i);
		if (ptr->Type == type)
		{
			P_GetScriptGoing (activator, NULL, ptr->Number,
				(int *)(ptr->Address + Data), 0, 0, 0, 0, 0, true);
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

void DACSThinker::RunThink ()
{
	DLevelScript *script = Scripts;

	while (script)
	{
		DLevelScript *next = script->next;
		script->RunScript ();
		script = next;
	}
}

IMPLEMENT_CLASS (DLevelScript)

void *DLevelScript::operator new (size_t size)
{
	return Z_Malloc (sizeof(DLevelScript), PU_LEVACS, 0);
}

void DLevelScript::operator delete (void *block)
{
	Z_Free (block);
}

void DLevelScript::Serialize (FArchive &arc)
{
	DWORD i;

	Super::Serialize (arc);
	arc << next << prev
		<< script
		<< sp
		<< state
		<< statedata
		<< activator
		<< activationline
		<< lineSide;
	for (i = 0; i < STACK_SIZE; i++)
		arc << stack[i];
	for (i = 0; i < LOCAL_SIZE; i++)
		arc << locals[i];

	if (arc.IsStoring ())
	{
		i = level.behavior->PC2Ofs (pc);
		arc << i;
	}
	else
	{
		arc << i;
		pc = level.behavior->Ofs2PC (i);
	}
}

DLevelScript::DLevelScript ()
{
	next = prev = NULL;
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;
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
		num1 = P_Random (pr_acs);
		num2 = P_Random (pr_acs);
		num3 = P_Random (pr_acs);
		num4 = P_Random (pr_acs);

		num = ((num1 << 24) | (num2 << 16) | (num3 << 8) | num4);
	}
	else
	{
		num = P_Random (pr_acs);
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
	
	if (tid)
	{
		FActorIterator iterator (tid);
		while ( (actor = iterator.Next ()) )
		{
			if (actor->health > 0 &&
				(type == 0 || actor->IsA (kind)))
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
				(type == 0 || actor->IsA (kind)))
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
	const char *flatname = level.behavior->LookupString (name);

	if (flatname == NULL)
		return;

	flat = R_FlatNumForName (flatname);

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0)
	{
		if (floorOrCeiling == false)
			sectors[secnum].floorpic = flat;
		else
			sectors[secnum].ceilingpic = flat;
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
	const char *texname = level.behavior->LookupString (name);

	if (texname == NULL)
		return;

	side = !!side;

	texture = R_TextureNumForName (texname);

	while ((linenum = P_FindLineFromID (lineid, linenum)) >= 0)
	{
		side_t *sidedef;

		if (lines[linenum].sidenum[side] == -1)
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
	const char *typestr = level.behavior->LookupString (type);
	if (typestr == NULL)
		return 0;
	const TypeInfo *info = TypeInfo::FindType (typestr);
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
				actor->flags |= MF_DROPPED;  // Don't respawn
				if (actor->flags2 & MF2_FLOATBOB)
				{
					actor->special1 = actor->z - actor->floorz;
				}
			}
			else
			{
				actor->Destroy ();
				actor = NULL;
			}
		}
	}
	return (int)actor;
}

int DLevelScript::DoSpawnSpot (int type, int spot, int tid, int angle)
{
	FActorIterator iterator (tid);
	AActor *aspot;
	int spawned = 0;

	while ( (aspot = iterator.Next ()) )
	{
		spawned = DoSpawn (type, aspot->x, aspot->y, aspot->z, tid, angle);
	}
	return spawned;
}

#define NEXTWORD	(LONG(*pc++))
#define NEXTBYTE	(fmt==ACS_LittleEnhanced?getbyte(pc):NEXTWORD)
#define STACK(a)	(stack[sp - (a)])
#define PushToStack(a)	(stack[sp++] = (a))

inline int getbyte (int *&pc)
{
	int res = *(BYTE *)pc;
	pc = (int *)((BYTE *)pc+1);
	return res;
}

void DLevelScript::RunScript ()
{
	DACSThinker *controller = DACSThinker::ActiveThinker;

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
	int sp = this->sp;
	const ACSFormat fmt = level.behavior->GetFormat();
	int runaway = 0;	// used to prevent infinite loops
	int pcd;
	char work[4096], *workwhere;
	const char *lookup;
	int optstart;
	int temp;

	while (state == SCRIPT_Running)
	{
		if (++runaway > 500000)
		{
			Printf (PRINT_HIGH, "Runaway script %d terminated\n", script);
			state = SCRIPT_PleaseRemove;
			break;
		}

		pcd = NEXTBYTE;
		switch (pcd)
		{
			default:
				Printf (PRINT_HIGH, "Unknown P-Code %d in script %d\n", pcd, script);
				// fall through
			case PCD_TERMINATE:
				state = SCRIPT_PleaseRemove;
				break;

			case PCD_NOP:
				break;

			case PCD_SUSPEND:
				state = SCRIPT_Suspended;
				break;

			case PCD_PUSHNUMBER:
				PushToStack (NEXTWORD);
				break;

			case PCD_PUSHBYTE:
				PushToStack (*(BYTE *)pc);
				pc = (int *)((BYTE *)pc + 1);
				break;

			case PCD_PUSH2BYTES:
				stack[sp] = ((BYTE *)pc)[0];
				stack[sp+1] = ((BYTE *)pc)[1];
				sp += 2;
				pc = (int *)((BYTE *)pc + 2);
				break;

			case PCD_PUSH3BYTES:
				stack[sp] = ((BYTE *)pc)[0];
				stack[sp+1] = ((BYTE *)pc)[1];
				stack[sp+2] = ((BYTE *)pc)[2];
				sp += 3;
				pc = (int *)((BYTE *)pc + 3);
				break;

			case PCD_PUSH4BYTES:
				stack[sp] = ((BYTE *)pc)[0];
				stack[sp+1] = ((BYTE *)pc)[1];
				stack[sp+2] = ((BYTE *)pc)[2];
				stack[sp+3] = ((BYTE *)pc)[3];
				sp += 4;
				pc = (int *)((BYTE *)pc + 4);
				break;

			case PCD_PUSH5BYTES:
				stack[sp] = ((BYTE *)pc)[0];
				stack[sp+1] = ((BYTE *)pc)[1];
				stack[sp+2] = ((BYTE *)pc)[2];
				stack[sp+3] = ((BYTE *)pc)[3];
				stack[sp+4] = ((BYTE *)pc)[4];
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
				level.vars[NEXTBYTE] = STACK(1);
				sp--;
				break;

			case PCD_ASSIGNWORLDVAR:
				WorldVars[NEXTBYTE] = STACK(1);
				sp--;
				break;

			case PCD_PUSHSCRIPTVAR:
				PushToStack (locals[NEXTBYTE]);
				break;

			case PCD_PUSHMAPVAR:
				PushToStack (level.vars[NEXTBYTE]);
				break;

			case PCD_PUSHWORLDVAR:
				PushToStack (WorldVars[NEXTBYTE]);
				break;

			case PCD_ADDSCRIPTVAR:
				locals[NEXTBYTE] += STACK(1);
				sp--;
				break;

			case PCD_ADDMAPVAR:
				level.vars[NEXTBYTE] += STACK(1);
				sp--;
				break;

			case PCD_ADDWORLDVAR:
				WorldVars[NEXTBYTE] += STACK(1);
				sp--;
				break;

			case PCD_SUBSCRIPTVAR:
				locals[NEXTBYTE] -= STACK(1);
				sp--;
				break;

			case PCD_SUBMAPVAR:
				level.vars[NEXTBYTE] -= STACK(1);
				sp--;
				break;

			case PCD_SUBWORLDVAR:
				WorldVars[NEXTBYTE] -= STACK(1);
				sp--;
				break;

			case PCD_MULSCRIPTVAR:
				locals[NEXTBYTE] *= STACK(1);
				sp--;
				break;

			case PCD_MULMAPVAR:
				level.vars[NEXTBYTE] *= STACK(1);
				sp--;
				break;

			case PCD_MULWORLDVAR:
				WorldVars[NEXTBYTE] *= STACK(1);
				sp--;
				break;

			case PCD_DIVSCRIPTVAR:
				locals[NEXTBYTE] /= STACK(1);
				sp--;
				break;

			case PCD_DIVMAPVAR:
				level.vars[NEXTBYTE] /= STACK(1);
				sp--;
				break;

			case PCD_DIVWORLDVAR:
				WorldVars[NEXTBYTE] /= STACK(1);
				sp--;
				break;

			case PCD_MODSCRIPTVAR:
				locals[NEXTBYTE] %= STACK(1);
				sp--;
				break;

			case PCD_MODMAPVAR:
				level.vars[NEXTBYTE] %= STACK(1);
				sp--;
				break;

			case PCD_MODWORLDVAR:
				WorldVars[NEXTBYTE] %= STACK(1);
				sp--;
				break;

			case PCD_INCSCRIPTVAR:
				locals[NEXTBYTE]++;
				break;

			case PCD_INCMAPVAR:
				level.vars[NEXTBYTE]++;
				break;

			case PCD_INCWORLDVAR:
				WorldVars[NEXTBYTE]++;
				break;

			case PCD_DECSCRIPTVAR:
				locals[NEXTBYTE]--;
				break;

			case PCD_DECMAPVAR:
				level.vars[NEXTBYTE]--;
				break;

			case PCD_DECWORLDVAR:
				WorldVars[NEXTBYTE]--;
				break;

			case PCD_GOTO:
				pc = level.behavior->Ofs2PC (*pc);
				break;

			case PCD_IFGOTO:
				if (STACK(1))
					pc = level.behavior->Ofs2PC (*pc);
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
				pc = level.behavior->FindScript (script);
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
					pc = level.behavior->Ofs2PC (*pc);
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
					pc = level.behavior->Ofs2PC (*pc);
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
					level.behavior->LookupString (STACK(1)) :
					level.behavior->LocalizeString (STACK(1)));
				if (lookup != NULL)
				{
					workwhere += sprintf (workwhere, "%s", lookup);
				}
				sp--;
				break;

			case PCD_PRINTNUMBER:
				workwhere += sprintf (workwhere, "%d", STACK(1));
				break;

			case PCD_PRINTCHARACTER:
				workwhere[0] = STACK(1);
				workwhere[1] = 0;
				workwhere++;
				sp--;
				break;

			case PCD_PRINTFIXED:
				workwhere += sprintf (workwhere, "%g", FIXED2FLOAT(STACK(1)));
				sp--;
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
						workwhere += sprintf (workwhere, "Player %d\n",
							STACK(1));
						sp--;
						break;
					}
					if (player)
					{
						workwhere += sprintf (workwhere, "%s",
							activator->player->userinfo.netname);
					}
					else if (activator)
					{
						workwhere += sprintf (workwhere, "%s",
							RUNTIME_TYPE(activator)->Name+1);
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
					if (pcd == PCD_ENDPRINTBOLD || activator == NULL ||
						(activator->player - players == consoleplayer))
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
				if (pcd == PCD_ENDHUDMESSAGEBOLD || activator == NULL ||
					(activator->player - players == consoleplayer))
				{
					int type = stack[optstart-6];
					int id = stack[optstart-5];
					EColorRange color = CLAMPCOLOR(stack[optstart-4]);
					float x = FIXED2FLOAT(stack[optstart-3]);
					float y = FIXED2FLOAT(stack[optstart-2]);
					float holdTime = FIXED2FLOAT(stack[optstart-1]);
					FHUDMessage *msg;

					switch (type)
					{
					default:	// normal
						msg = new FHUDMessage (work, x, y, color, holdTime);
						break;
					case 1:		// fade out
						{
							float fadeTime = (optstart < sp) ?
								FIXED2FLOAT(stack[optstart]) : 0.5f;
							msg = new FHUDMessageFadeOut (work, x, y, color, holdTime, fadeTime);
						}
						break;
					case 2:		// type on, then fade out
						{
							float typeTime = (optstart < sp) ?
								FIXED2FLOAT(stack[optstart]) : 0.05f;
							float fadeTime = (optstart < sp-1) ?
								FIXED2FLOAT(stack[optstart+1]) : 0.5f;
							msg = new FHUDMessageTypeOnFadeOut (work, x, y, color, typeTime, holdTime, fadeTime);
						}
						break;
					}
					StatusBar->AttachMessage (msg, id ? 0xff000000|id : 0);
				}
				sp = optstart-6;
				break;

			case PCD_PLAYERCOUNT:
				PushToStack (CountPlayers ());
				break;

			case PCD_GAMETYPE:
				if (*deathmatch)
					PushToStack (GAME_NET_DEATHMATCH);
				else if (multiplayer)
					PushToStack (GAME_NET_COOPERATIVE);
				else
					PushToStack (GAME_SINGLE_PLAYER);
				break;

			case PCD_GAMESKILL:
				PushToStack (*gameskill);
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
				lookup = level.behavior->LookupString (STACK(2));
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
				lookup = level.behavior->LookupString (STACK(2));
				if (lookup != NULL)
				{
					if (activationline)
					{
						S_Sound (
							activationline->frontsector->soundorg,
							CHAN_BODY,
							lookup,
							(float)(STACK(1)) / 127.f,
							ATTN_NORM);
					}
					else
					{
						S_Sound (
							CHAN_BODY,
							lookup,
							(float)(STACK(1)) / 127.f,
							ATTN_NORM);
					}
				}
				sp -= 2;
				break;

			case PCD_AMBIENTSOUND:
				lookup = level.behavior->LookupString (STACK(2));
				if (lookup != NULL)
				{
					S_Sound (CHAN_BODY,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NONE);
				}
				sp -= 2;
				break;

			case PCD_LOCALAMBIENTSOUND:
				lookup = level.behavior->LookupString (STACK(2));
				if (lookup != NULL && players[consoleplayer].camera == activator)
				{
					S_Sound (CHAN_BODY,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NONE);
				}
				sp -= 2;
				break;

			case PCD_ACTIVATORSOUND:
				lookup = level.behavior->LookupString (STACK(2));
				if (lookup != NULL)
				{
					S_Sound (activator, CHAN_AUTO,
							 lookup,
							 (float)(STACK(1)) / 127.f, ATTN_NORM);
				}
				sp -= 2;
				break;

			case PCD_SOUNDSEQUENCE:
				lookup = level.behavior->LookupString (STACK(1));
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

			case PCD_THINGSOUND:
				lookup = level.behavior->LookupString (STACK(2));
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
				break;

			case PCD_SETAIRCONTROLDIRECT:
				level.aircontrol = pc[0];
				pc++;
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
				STACK(6) = DoSpawnSpot (STACK(4), STACK(3), STACK(2), STACK(1));
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
				GiveInventory (activator, level.behavior->LookupString (STACK(2)), STACK(1));
				break;

			case PCD_GIVEINVENTORYDIRECT:
				GiveInventory (activator, level.behavior->LookupString (pc[0]), pc[1]);
				pc += 2;
				break;

			case PCD_TAKEINVENTORY:
				TakeInventory (activator, level.behavior->LookupString (STACK(2)), STACK(1));
				break;

			case PCD_TAKEINVENTORYDIRECT:
				TakeInventory (activator, level.behavior->LookupString (pc[0]), pc[1]);
				pc += 2;
				break;

			case PCD_CHECKINVENTORY:
				STACK(1) = CheckInventory (activator, level.behavior->LookupString (STACK(1)));
				break;

			case PCD_CHECKINVENTORYDIRECT:
				PushToStack (CheckInventory (activator, level.behavior->LookupString (pc[0])));
				pc += 1;
				break;

			case PCD_SETMUSIC:
				S_ChangeMusic (level.behavior->LookupString (STACK(3)), STACK(2));
				break;

			case PCD_SETMUSICDIRECT:
				S_ChangeMusic (level.behavior->LookupString (pc[0]), pc[1]);
				pc += 3;
				break;

			case PCD_LOCALSETMUSIC:
				if (activator == players[consoleplayer].mo)
				{
					S_ChangeMusic (level.behavior->LookupString (STACK(3)), STACK(2));
				}
				break;

			case PCD_LOCALSETMUSICDIRECT:
				if (activator == players[consoleplayer].mo)
				{
					S_ChangeMusic (level.behavior->LookupString (pc[0]), pc[1]);
				}
				pc += 3;
				break;
		}
	}

	this->pc = pc;
	this->sp = sp;

	if (state == SCRIPT_PleaseRemove)
	{
		Unlink ();
		if (controller->RunningScripts[script] == this)
			controller->RunningScripts[script] = NULL;
		this->Destroy ();
	}
}

#undef PushtoStack

static bool P_GetScriptGoing (AActor *who, line_t *where, int num, int *code,
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

	new DLevelScript (who, where, num, code, lineSide, arg0, arg1, arg2, always, delay);

	return true;
}

DLevelScript::DLevelScript (AActor *who, line_t *where, int num, int *code, int lineside,
							int arg0, int arg1, int arg2, int always, bool delay)
{
	if (DACSThinker::ActiveThinker == NULL)
		new DACSThinker;

	script = num;
	sp = 0;
	memset (locals, 0, sizeof(locals));
	locals[0] = arg0;
	locals[1] = arg1;
	locals[2] = arg2;
	pc = code;
	activator = who;
	activationline = where;
	lineSide = lineside;
	if (delay) {
		// From Hexen: Give the world some time to set itself up before
		// running open scripts.
		//script->state = SCRIPT_Delayed;
		//script->statedata = TICRATE;
		state = SCRIPT_Running;
	} else {
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

void P_DoDeferedScripts (void)
{
	acsdefered_t *def;
	int *scriptdata;

	// Handle defered scripts in this step, too
	def = level.info->defered;
	while (def)
	{
		acsdefered_t *next = def->next;
		switch (def->type)
		{
		case acsdefered_t::defexecute:
		case acsdefered_t::defexealways:
			scriptdata = level.behavior->FindScript (def->script);
			if (scriptdata)
			{
				P_GetScriptGoing ((unsigned)def->playernum < MAXPLAYERS &&
					playeringame[def->playernum] ? players[def->playernum].mo : NULL,
					NULL, def->script,
					scriptdata,
					0, def->arg0, def->arg1, def->arg2,
					def->type == acsdefered_t::defexealways, true);
			} else
				Printf (PRINT_HIGH, "P_DoDeferredScripts: Unknown script %d\n", def->script);
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
	if (!strnicmp (level.mapname, map, 8))
	{
		int *scriptdata;

		if (level.behavior != NULL &&
			(scriptdata = level.behavior->FindScript (script)) != NULL)
		{
			return P_GetScriptGoing (who, where, script,
									 scriptdata,
									 lineSide, arg0, arg1, arg2, always, false);
		}
		else
		{
			Printf (PRINT_HIGH, "P_StartScript: Unknown script %d\n", script);
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
					*str++ = '\x81';
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
