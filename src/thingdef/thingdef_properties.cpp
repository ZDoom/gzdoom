/*
** thingdef-properties.cpp
**
** Actor definitions - properties and flags parser
**
**---------------------------------------------------------------------------
** Copyright 2002-2007 Christoph Oelckers
** Copyright 2004-2007 Randy Heit
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
** 4. When not used as part of ZDoom or a ZDoom derivative, this code will be
**    covered by the terms of the GNU General Public License as published by
**    the Free Software Foundation; either version 2 of the License, or (at
**    your option) any later version.
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
*/

#include "gi.h"
#include "actor.h"
#include "info.h"
#include "sc_man.h"
#include "tarray.h"
#include "w_wad.h"
#include "templates.h"
#include "r_defs.h"
#include "r_draw.h"
#include "a_pickups.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "a_action.h"
#include "decallib.h"
#include "m_random.h"
#include "autosegs.h"
#include "i_system.h"
#include "p_local.h"
#include "p_effect.h"
#include "v_palette.h"
#include "doomerrors.h"
#include "a_doomglobal.h"
#include "a_hexenglobal.h"
#include "a_weaponpiece.h"
#include "p_conversation.h"
#include "v_text.h"
#include "thingdef.h"
#include "a_sharedglobal.h"
#include "r_translate.h"

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_FLAG2(symbol, name, type, variable) { symbol, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_DEPRECATED_FLAG(name, type, index) { index, #name, -1 }

struct flagdef
{
	int flagbit;
	const char *name;
	int structoffset;
};


static flagdef ActorFlags[]=
{
	DEFINE_FLAG(MF, SOLID, AActor, flags),
	DEFINE_FLAG(MF, SHOOTABLE, AActor, flags),
	DEFINE_FLAG(MF, NOSECTOR, AActor, flags),
	DEFINE_FLAG(MF, NOBLOCKMAP, AActor, flags),
	DEFINE_FLAG(MF, AMBUSH, AActor, flags),
	DEFINE_FLAG(MF, JUSTHIT, AActor, flags),
	DEFINE_FLAG(MF, JUSTATTACKED, AActor, flags),
	DEFINE_FLAG(MF, SPAWNCEILING, AActor, flags),
	DEFINE_FLAG(MF, NOGRAVITY, AActor, flags),
	DEFINE_FLAG(MF, DROPOFF, AActor, flags),
	DEFINE_FLAG(MF, NOCLIP, AActor, flags),
	DEFINE_FLAG(MF, FLOAT, AActor, flags),
	DEFINE_FLAG(MF, TELEPORT, AActor, flags),
	DEFINE_FLAG(MF, MISSILE, AActor, flags),
	DEFINE_FLAG(MF, DROPPED, AActor, flags),
	DEFINE_FLAG(MF, SHADOW, AActor, flags),
	DEFINE_FLAG(MF, NOBLOOD, AActor, flags),
	DEFINE_FLAG(MF, CORPSE, AActor, flags),
	DEFINE_FLAG(MF, INFLOAT, AActor, flags),
	DEFINE_FLAG(MF, COUNTKILL, AActor, flags),
	DEFINE_FLAG(MF, COUNTITEM, AActor, flags),
	DEFINE_FLAG(MF, SKULLFLY, AActor, flags),
	DEFINE_FLAG(MF, NOTDMATCH, AActor, flags),
	DEFINE_FLAG(MF, SPAWNSOUNDSOURCE, AActor, flags),
	DEFINE_FLAG(MF, FRIENDLY, AActor, flags),
	DEFINE_FLAG(MF, NOLIFTDROP, AActor, flags),
	DEFINE_FLAG(MF, STEALTH, AActor, flags),
	DEFINE_FLAG(MF, ICECORPSE, AActor, flags),
	DEFINE_FLAG(MF2, DONTREFLECT, AActor, flags2),
	DEFINE_FLAG(MF2, WINDTHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, HERETICBOUNCE , AActor, flags2),
	DEFINE_FLAG(MF2, BLASTED, AActor, flags2),
	DEFINE_FLAG(MF2, FLOORCLIP, AActor, flags2),
	DEFINE_FLAG(MF2, SPAWNFLOAT, AActor, flags2),
	DEFINE_FLAG(MF2, NOTELEPORT, AActor, flags2),
	DEFINE_FLAG2(MF2_RIP, RIPPER, AActor, flags2),
	DEFINE_FLAG(MF2, PUSHABLE, AActor, flags2),
	DEFINE_FLAG2(MF2_SLIDE, SLIDESONWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_PASSMOBJ, CANPASS, AActor, flags2),
	DEFINE_FLAG(MF2, CANNOTPUSH, AActor, flags2),
	DEFINE_FLAG(MF2, THRUGHOST, AActor, flags2),
	DEFINE_FLAG(MF2, BOSS, AActor, flags2),
	DEFINE_FLAG2(MF2_NODMGTHRUST, NODAMAGETHRUST, AActor, flags2),
	DEFINE_FLAG(MF2, DONTTRANSLATE, AActor, flags2),
	DEFINE_FLAG(MF2, TELESTOMP, AActor, flags2),
	DEFINE_FLAG(MF2, FLOATBOB, AActor, flags2),
	DEFINE_FLAG(MF2, HEXENBOUNCE, AActor, flags2),
	DEFINE_FLAG(MF2, DOOMBOUNCE, AActor, flags2),
	DEFINE_FLAG2(MF2_IMPACT, ACTIVATEIMPACT, AActor, flags2),
	DEFINE_FLAG2(MF2_PUSHWALL, CANPUSHWALLS, AActor, flags2),
	DEFINE_FLAG2(MF2_MCROSS, ACTIVATEMCROSS, AActor, flags2),
	DEFINE_FLAG2(MF2_PCROSS, ACTIVATEPCROSS, AActor, flags2),
	DEFINE_FLAG(MF2, CANTLEAVEFLOORPIC, AActor, flags2),
	DEFINE_FLAG(MF2, NONSHOOTABLE, AActor, flags2),
	DEFINE_FLAG(MF2, INVULNERABLE, AActor, flags2),
	DEFINE_FLAG(MF2, DORMANT, AActor, flags2),
	DEFINE_FLAG(MF2, SEEKERMISSILE, AActor, flags2),
	DEFINE_FLAG(MF2, REFLECTIVE, AActor, flags2),
	DEFINE_FLAG(MF3, FLOORHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, CEILINGHUGGER, AActor, flags3),
	DEFINE_FLAG(MF3, NORADIUSDMG, AActor, flags3),
	DEFINE_FLAG(MF3, GHOST, AActor, flags3),
	DEFINE_FLAG(MF3, ALWAYSPUFF, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSPLASH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTOVERLAP, AActor, flags3),
	DEFINE_FLAG(MF3, DONTMORPH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSQUASH, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLACTIVE, AActor, flags3),
	DEFINE_FLAG(MF3, ISMONSTER, AActor, flags3),
	DEFINE_FLAG(MF3, SKYEXPLODE, AActor, flags3),
	DEFINE_FLAG(MF3, STAYMORPHED, AActor, flags3),
	DEFINE_FLAG(MF3, DONTBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, CANBLAST, AActor, flags3),
	DEFINE_FLAG(MF3, NOTARGET, AActor, flags3),
	DEFINE_FLAG(MF3, DONTGIB, AActor, flags3),
	DEFINE_FLAG(MF3, NOBLOCKMONST, AActor, flags3),
	DEFINE_FLAG(MF3, FULLVOLDEATH, AActor, flags3),
	DEFINE_FLAG(MF3, CANBOUNCEWATER, AActor, flags3),
	DEFINE_FLAG(MF3, NOWALLBOUNCESND, AActor, flags3),
	DEFINE_FLAG(MF3, FOILINVUL, AActor, flags3),
	DEFINE_FLAG(MF3, NOTELEOTHER, AActor, flags3),
	DEFINE_FLAG(MF3, BLOODLESSIMPACT, AActor, flags3),
	DEFINE_FLAG(MF3, NOEXPLODEFLOOR, AActor, flags3),
	DEFINE_FLAG(MF3, PUFFONACTORS, AActor, flags3),
	DEFINE_FLAG(MF4, QUICKTORETALIATE, AActor, flags4),
	DEFINE_FLAG(MF4, NOICEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, RANDOMIZE, AActor, flags4),
	DEFINE_FLAG(MF4, FIXMAPTHINGPOS , AActor, flags4),
	DEFINE_FLAG(MF4, ACTLIKEBRIDGE, AActor, flags4),
	DEFINE_FLAG(MF4, STRIFEDAMAGE, AActor, flags4),
	DEFINE_FLAG(MF4, CANUSEWALLS, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEMORE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEEVENMORE, AActor, flags4),
	DEFINE_FLAG(MF4, FORCERADIUSDMG, AActor, flags4),
	DEFINE_FLAG(MF4, DONTFALL, AActor, flags4),
	DEFINE_FLAG(MF4, SEESDAGGERS, AActor, flags4),
	DEFINE_FLAG(MF4, INCOMBAT, AActor, flags4),
	DEFINE_FLAG(MF4, LOOKALLAROUND, AActor, flags4),
	DEFINE_FLAG(MF4, STANDSTILL, AActor, flags4),
	DEFINE_FLAG(MF4, SPECTRAL, AActor, flags4),
	DEFINE_FLAG(MF4, FIRERESIST, AActor, flags4),
	DEFINE_FLAG(MF4, NOSPLASHALERT, AActor, flags4),
	DEFINE_FLAG(MF4, SYNCHRONIZED, AActor, flags4),
	DEFINE_FLAG(MF4, NOTARGETSWITCH, AActor, flags4),
	DEFINE_FLAG(MF4, DONTHURTSPECIES, AActor, flags4),
	DEFINE_FLAG(MF4, SHIELDREFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, DEFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, ALLOWPARTICLES, AActor, flags4),
	DEFINE_FLAG(MF4, EXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, NOEXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, FRIGHTENED, AActor, flags4),
	DEFINE_FLAG(MF4, NOBOUNCESOUND, AActor, flags4),
	DEFINE_FLAG(MF4, NOSKIN, AActor, flags4),
	DEFINE_FLAG(MF4, BOSSDEATH, AActor, flags4),

	DEFINE_FLAG(MF5, FASTER, AActor, flags5),
	DEFINE_FLAG(MF5, FASTMELEE, AActor, flags5),
	DEFINE_FLAG(MF5, NODROPOFF, AActor, flags5),
	DEFINE_FLAG(MF5, BOUNCEONACTORS, AActor, flags5),
	DEFINE_FLAG(MF5, EXPLODEONWATER, AActor, flags5),
	DEFINE_FLAG(MF5, NODAMAGE, AActor, flags5),
	DEFINE_FLAG(MF5, BLOODSPLATTER, AActor, flags5),
	DEFINE_FLAG(MF5, OLDRADIUSDMG, AActor, flags5),
	DEFINE_FLAG(MF5, DEHEXPLOSION, AActor, flags5),
	DEFINE_FLAG(MF5, PIERCEARMOR, AActor, flags5),
	DEFINE_FLAG(MF5, NOBLOODDECALS, AActor, flags5),
	DEFINE_FLAG(MF5, USESPECIAL, AActor, flags5),
	DEFINE_FLAG(MF5, NOPAIN, AActor, flags5),
	DEFINE_FLAG(MF5, ALWAYSFAST, AActor, flags5),
	DEFINE_FLAG(MF5, NEVERFAST, AActor, flags5),

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEXYBILLBOARD, AActor, renderflags),

	// Deprecated flags. Handling must be performed in HandleDeprecatedFlags
	DEFINE_DEPRECATED_FLAG(FIREDAMAGE, AActor, 0),
	DEFINE_DEPRECATED_FLAG(ICEDAMAGE, AActor, 1),
	DEFINE_DEPRECATED_FLAG(LOWGRAVITY, AActor, 2),
	DEFINE_DEPRECATED_FLAG(SHORTMISSILERANGE, AActor, 3),
	DEFINE_DEPRECATED_FLAG(LONGMELEERANGE, AActor, 4),
};

static flagdef InventoryFlags[] =
{
	// Inventory flags
	DEFINE_FLAG(IF, QUIET, AInventory, ItemFlags),
	DEFINE_FLAG(IF, AUTOACTIVATE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNDROPPABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INVBAR, AInventory, ItemFlags),
	DEFINE_FLAG(IF, HUBPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INTERHUBSTRIP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, PICKUPFLASH, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ALWAYSPICKUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, FANCYPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, BIGPOWERUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, KEEPDEPLETED, AInventory, ItemFlags),
	DEFINE_FLAG(IF, IGNORESKILL, AInventory, ItemFlags),
};

static flagdef WeaponFlags[] =
{
	// Weapon flags
	DEFINE_FLAG(WIF, NOAUTOFIRE, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, READYSNDHALF, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, DONTBOB, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AXEBLOOD, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOALERT, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, ALT_AMMO_OPTIONAL, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, PRIMARY_USES_BOTH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, WIMPY_WEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, POWERED_UP, AWeapon, WeaponFlags),
	//DEFINE_FLAG(WIF, EXTREME_DEATH, AWeapon, WeaponFlags),	// this should be removed now!
	DEFINE_FLAG(WIF, STAFF2_KICKBACK, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, EXPLOSIVE, AWeapon, WeaponFlags),
	DEFINE_FLAG2(WIF_BOT_MELEE, MELEEWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, BFG, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, CHEATNOTWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NO_AUTO_SWITCH, AWeapon, WeaponFlags),
	//WIF_BOT_REACTION_SKILL_THING = 1<<31, // I don't understand this
};

static const struct { const PClass *Type; flagdef *Defs; int NumDefs; } FlagLists[] =
{
	{ RUNTIME_CLASS(AActor), 		ActorFlags,		sizeof(ActorFlags)/sizeof(flagdef) },
	{ RUNTIME_CLASS(AInventory), 	InventoryFlags,	sizeof(InventoryFlags)/sizeof(flagdef) },
	{ RUNTIME_CLASS(AWeapon), 		WeaponFlags,	sizeof(WeaponFlags)/sizeof(flagdef) }
};
#define NUM_FLAG_LISTS 3

//==========================================================================
//
// Find a flag by name using a binary search
//
//==========================================================================
static int STACK_ARGS flagcmp (const void * a, const void * b)
{
	return stricmp( ((flagdef*)a)->name, ((flagdef*)b)->name);
}

static flagdef *FindFlag (flagdef *flags, int numflags, const char *flag)
{
	int min = 0, max = numflags - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (flag, flags[mid].name);
		if (lexval == 0)
		{
			return &flags[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

static flagdef *FindFlag (const PClass *type, const char *part1, const char *part2)
{
	static bool flagsorted = false;
	flagdef *def;
	int i;

	if (!flagsorted) 
	{
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			qsort (FlagLists[i].Defs, FlagLists[i].NumDefs, sizeof(flagdef), flagcmp);
		}
		flagsorted = true;
	}
	if (part2 == NULL)
	{ // Search all lists
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (type->IsDescendantOf (FlagLists[i].Type))
			{
				def = FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part1);
				if (def != NULL)
				{
					return def;
				}
			}
		}
	}
	else
	{ // Search just the named list
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (stricmp (FlagLists[i].Type->TypeName.GetChars(), part1) == 0)
			{
				if (type->IsDescendantOf (FlagLists[i].Type))
				{
					return FindFlag (FlagLists[i].Defs, FlagLists[i].NumDefs, part2);
				}
				else
				{
					return NULL;
				}
			}
		}
	}
	return NULL;
}


//===========================================================================
//
// HandleDeprecatedFlags
//
// Handles the deprecated flags and sets the respective properties
// to appropriate values. This is solely intended for backwards
// compatibility so mixing this with code that is aware of the real
// properties is not recommended
//
//===========================================================================
static void HandleDeprecatedFlags(AActor *defaults, bool set, int index)
{
	switch (index)
	{
	case 0:	// FIREDAMAGE
		defaults->DamageType = set? NAME_Fire : NAME_None;
		break;
	case 1:	// ICEDAMAGE
		defaults->DamageType = set? NAME_Ice : NAME_None;
		break;
	case 2:	// LOWGRAVITY
		defaults->gravity = set? FRACUNIT/8 : FRACUNIT;
		break;
	case 3:	// SHORTMISSILERANGE
		defaults->maxtargetrange = set? 896*FRACUNIT : 0;
		break;
	case 4:	// LONGMELEERANGE
		defaults->meleethreshold = set? 196*FRACUNIT : 0;
		break;
	default:
		break;	// silence GCC
	}
}

//===========================================================================
//
// A_ChangeFlag
//
// This cannot be placed in thingdef_codeptr because it needs the flag table
//
//===========================================================================
void A_ChangeFlag(AActor * self)
{
	int index=CheckIndex(2);
	const char * flagname = FName((ENamedName)StateParameters[index]).GetChars();	
	int expression = EvalExpressionI (StateParameters[index+1], self);

	const char *dot = strchr (flagname, '.');
	flagdef *fd;

	if (dot != NULL)
	{
		FString part1(flagname, dot-flagname);
		fd = FindFlag (self->GetClass(), part1, dot+1);
	}
	else
	{
		fd = FindFlag (self->GetClass(), flagname, NULL);
	}

	if (fd != NULL)
	{
		if (fd->structoffset == -1)
		{
			HandleDeprecatedFlags(self, !!expression, fd->flagbit);
		}
		else
		{
			int * flagp = (int*) (((char*)self) + fd->structoffset);

			if (expression) *flagp |= fd->flagbit;
			else *flagp &= ~fd->flagbit;
		}
	}
	else
	{
		Printf("Unknown flag '%s' in '%s'\n", flagname, self->GetClass()->TypeName.GetChars());
	}
}

//==========================================================================
//
//==========================================================================
void ParseActorFlag (Baggage &bag, int mod)
{
	flagdef *fd;

	SC_MustGetString ();

	FString part1 = sc_String;
	const char *part2 = NULL;
	if (SC_CheckString ("."))
	{
		SC_MustGetString ();
		part2 = sc_String;
	}
	if ( (fd = FindFlag (bag.Info->Class, part1.GetChars(), part2)) )
	{
		AActor *defaults = (AActor*)bag.Info->Class->Defaults;
		if (fd->structoffset == -1)	// this is a deprecated flag that has been changed into a real property
		{
			HandleDeprecatedFlags(defaults, mod=='+', fd->flagbit);
		}
		else
		{
			DWORD * flagvar = (DWORD*) ((char*)defaults + fd->structoffset);
			if (mod == '+')
			{
				*flagvar |= fd->flagbit;
			}
			else
			{
				*flagvar &= ~fd->flagbit;
			}
		}
	}
	else
	{
		if (part2 == NULL)
		{
			SC_ScriptError("\"%s\" is an unknown flag\n", part1.GetChars());
		}
		else
		{
			SC_ScriptError("\"%s.%s\" is an unknown flag\n", part1.GetChars(), part2);
		}
	}
}



//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// Translation parsing
//
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
FRemapTable CurrentTranslation;
TArray<PalEntry> BloodTranslationColors;

PalEntry BloodTranslations[256];

static bool Check(char *& range,  char c, bool error=true)
{
	while (isspace(*range)) range++;
	if (*range==c)
	{
		range++;
		return true;
	}
	if (error)
	{
		//SC_ScriptError("Invalid syntax in translation specification: '%c' expected", c);
	}
	return false;
}


static void AddToTranslation(char * range)
{
	int start,end;

	start=strtol(range, &range, 10);
	if (!Check(range, ':')) return;
	end=strtol(range, &range, 10);
	if (!Check(range, '=')) return;
	if (!Check(range, '[', false))
	{
		int pal1,pal2;

		pal1=strtol(range, &range, 10);
		if (!Check(range, ':')) return;
		pal2=strtol(range, &range, 10);

		CurrentTranslation.AddIndexRange(start, end, pal1, pal2);
	}
	else
	{ 
		// translation using RGB values
		int r1,g1,b1,r2,g2,b2;

		r1=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		g1=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		b1=strtol(range, &range, 10);
		if (!Check(range, ']')) return;
		if (!Check(range, ':')) return;
		if (!Check(range, '[')) return;
		r2=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		g2=strtol(range, &range, 10);
		if (!Check(range, ',')) return;
		b2=strtol(range, &range, 10);
		if (!Check(range, ']')) return;

		CurrentTranslation.AddColorRange(start, end, r1, g1, b1, r2, g2, b2);
	}
}

static int StoreTranslation()
{
	unsigned int i;

	for (i = 0; i < translationtables[TRANSLATION_Decorate].Size(); i++)
	{
		if (CurrentTranslation == *translationtables[TRANSLATION_Decorate][i])
		{
			// A duplicate of this translation already exists
			return TRANSLATION(TRANSLATION_Decorate, i);
		}
	}
	if (translationtables[TRANSLATION_Decorate].Size() >= MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many translations in DECORATE");
	}
	FRemapTable *newtrans = new FRemapTable;
	*newtrans = CurrentTranslation;
	i = translationtables[TRANSLATION_Decorate].Push(newtrans);
	return TRANSLATION(TRANSLATION_Decorate, i);
}

static int CreateBloodTranslation(PalEntry color)
{
	unsigned int i;

	for (i = 0; i < BloodTranslationColors.Size(); i++)
	{
		if (color.r == BloodTranslationColors[i].r &&
			color.g == BloodTranslationColors[i].g &&
			color.b == BloodTranslationColors[i].b)
		{
			// A duplicate of this translation already exists
			return i;
		}
	}
	if (BloodTranslationColors.Size() >= MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many blood colors in DECORATE");
	}
	FRemapTable *trans = new FRemapTable;
	for (i = 0; i < 256; i++)
	{
		int bright = MAX(MAX(GPalette.BaseColors[i].r, GPalette.BaseColors[i].g), GPalette.BaseColors[i].b);
		PalEntry pe = PalEntry(color.r*bright/255, color.g*bright/255, color.b*bright/255);
		int entry = ColorMatcher.Pick(pe.r, pe.g, pe.b);

		trans->Palette[i] = pe;
		trans->Remap[i] = entry;
	}
	translationtables[TRANSLATION_Blood].Push(trans);
	return BloodTranslationColors.Push(color);
}

//----------------------------------------------------------------------------
//
// DropItem handling
//
//----------------------------------------------------------------------------

static void FreeDropItemChain(FDropItem *chain)
{
	while (chain != NULL)
	{
		FDropItem *next = chain->Next;
		delete chain;
		chain = next;
	}
}

class FDropItemPtrArray : public TArray<FDropItem *>
{
public:
	~FDropItemPtrArray()
	{
		for (unsigned int i = 0; i < Size(); ++i)
		{
			FreeDropItemChain ((*this)[i]);
		}
	}
};

static FDropItemPtrArray DropItemList;

FDropItem *GetDropItems(const PClass *cls)
{
	unsigned int index = cls->Meta.GetMetaInt (ACMETA_DropItems) - 1;

	if (index >= 0 && index < DropItemList.Size())
	{
		return DropItemList[index];
	}
	return NULL;
}

//==========================================================================
//
//
//==========================================================================

typedef void (*ActorPropFunction) (AActor *defaults, Baggage &bag);

struct ActorProps 
{ 
	const char *name; 
	ActorPropFunction Handler; 
	const PClass * type; 
};

typedef ActorProps (*ActorPropHandler) (register const char *str, register unsigned int len);

static const ActorProps *is_actorprop (const char *str);


//==========================================================================
//
// Checks for a numeric parameter which may or may not be preceded by a comma
//
//==========================================================================
static bool CheckNumParm()
{
	if (SC_CheckString(","))
	{
		SC_MustGetNumber();
		return true;
	}
	else
	{
		return !!SC_CheckNumber();
	}
}

static bool CheckFloatParm()
{
	if (SC_CheckString(","))
	{
		SC_MustGetFloat();
		return true;
	}
	else
	{
		return !!SC_CheckFloat();
	}
}

//==========================================================================
//
// Property parsers
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void ActorSkipSuper (AActor *defaults, Baggage &bag)
{
	memcpy (defaults, GetDefault<AActor>(), sizeof(AActor));
	if (bag.DropItemList != NULL)
	{
		FreeDropItemChain (bag.DropItemList);
	}
	ResetBaggage (&bag);
	MakeStateDefines(NULL);
}

//==========================================================================
//
//==========================================================================
static void ActorGame (AActor *defaults, Baggage &bag)
{
	SC_MustGetString ();
	if (SC_Compare ("Doom"))
	{
		bag.Info->GameFilter |= GAME_Doom;
	}
	else if (SC_Compare ("Heretic"))
	{
		bag.Info->GameFilter |= GAME_Heretic;
	}
	else if (SC_Compare ("Hexen"))
	{
		bag.Info->GameFilter |= GAME_Hexen;
	}
	else if (SC_Compare ("Raven"))
	{
		bag.Info->GameFilter |= GAME_Raven;
	}
	else if (SC_Compare ("Strife"))
	{
		bag.Info->GameFilter |= GAME_Strife;
	}
	else if (SC_Compare ("Any"))
	{
		bag.Info->GameFilter = GAME_Any;
	}
	else
	{
		SC_ScriptError ("Unknown game type %s", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnID (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	if (sc_Number<0 || sc_Number>255)
	{
		SC_ScriptError ("SpawnID must be in the range [0,255]");
	}
	else bag.Info->SpawnID=(BYTE)sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorConversationID (AActor *defaults, Baggage &bag)
{
	int convid;

	SC_MustGetNumber();
	convid = sc_Number;

	// Handling for Strife teaser IDs - only of meaning for the standard items
	// as PWADs cannot be loaded with the teasers.
	if (SC_CheckString(","))
	{
		SC_MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE))
			convid=sc_Number;

		SC_MustGetStringName(",");
		SC_MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE|GI_TEASER2))
			convid=sc_Number;

		if (convid==-1) return;
	}
	if (convid<0 || convid>1000)
	{
		SC_ScriptError ("ConversationID must be in the range [0,1000]");
	}
	else StrifeTypes[convid] = bag.Info->Class;
}

//==========================================================================
//
//==========================================================================
static void ActorTag (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString(AMETA_StrifeName, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->health=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorGibHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_GibHealth, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorWoundHealth (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_WoundHealth, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorReactionTime (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->reactiontime=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorPainChance (AActor *defaults, Baggage &bag)
{
	if (!SC_CheckNumber())
	{
		FName painType;
		SC_MustGetString();
		if (SC_Compare("Normal")) painType = NAME_None;
		else painType=sc_String;
		SC_MustGetToken(',');
		SC_MustGetNumber();
	
		if (bag.Info->PainChances == NULL) bag.Info->PainChances=new PainChanceList;
		(*bag.Info->PainChances)[painType] = (BYTE)sc_Number;
		return;
	}
	defaults->PainChance=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamage (AActor *defaults, Baggage &bag)
{
	// Damage can either be a single number, in which case it is subject
	// to the original damage calculation rules. Or, it can be an expression
	// and will be calculated as-is, ignoring the original rules. For
	// compatibility reasons, expressions must be enclosed within
	// parentheses.

	if (SC_CheckString ("("))
	{
		defaults->Damage = 0x40000000 | ParseExpression (false, bag.Info->Class);
		SC_MustGetStringName(")");
	}
	else
	{
		SC_MustGetNumber ();
		defaults->Damage = sc_Number;
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->Speed=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorFloatSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->FloatSpeed=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->radius=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->height=fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMass (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Mass=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorXScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->scaleX = FLOAT2FIXED(sc_Float);
}

//==========================================================================
//
//==========================================================================
static void ActorYScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->scaleY = FLOAT2FIXED(sc_Float);
}

//==========================================================================
//
//==========================================================================
static void ActorScale (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->scaleX= defaults->scaleY = FLOAT2FIXED(sc_Float);
}

//==========================================================================
//
//==========================================================================
static void ActorArgs (AActor *defaults, Baggage &bag)
{
	for (int i=0;i<5;i++)
	{
		SC_MustGetNumber();
		defaults->args[i] = sc_Number;
		if (i < 4 && !SC_CheckToken(',')) break;
	}
	defaults->flags2|=MF2_ARGSDEFINED;
}

//==========================================================================
//
//==========================================================================
static void ActorSeeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->SeeSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorAttackSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AttackSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorPainSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->PainSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DeathSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorActiveSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->ActiveSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorHowlSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaInt (AMETA_HowlSound, S_FindSound(sc_String));
}

//==========================================================================
//
//==========================================================================
static void ActorDropItem (AActor *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	SC_MustGetString();
	di->Name=sc_String;
	di->probability=255;
	di->amount=-1;

	if (CheckNumParm())
	{
		di->probability=sc_Number;
		if (CheckNumParm())
		{
			di->amount=sc_Number;
		}
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnState (AActor *defaults, Baggage &bag)
{
	AddState("Spawn", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorSeeState (AActor *defaults, Baggage &bag)
{
	AddState("See", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeState (AActor *defaults, Baggage &bag)
{
	AddState("Melee", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileState (AActor *defaults, Baggage &bag)
{
	AddState("Missile", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorPainState (AActor *defaults, Baggage &bag)
{
	AddState("Pain", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDeathState (AActor *defaults, Baggage &bag)
{
	AddState("Death", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorXDeathState (AActor *defaults, Baggage &bag)
{
	AddState("XDeath", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorBurnState (AActor *defaults, Baggage &bag)
{
	AddState("Burn", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorIceState (AActor *defaults, Baggage &bag)
{
	AddState("Ice", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorRaiseState (AActor *defaults, Baggage &bag)
{
	AddState("Raise", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrashState (AActor *defaults, Baggage &bag)
{
	AddState("Crash", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrushState (AActor *defaults, Baggage &bag)
{
	AddState("Crush", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorWoundState (AActor *defaults, Baggage &bag)
{
	AddState("Wound", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDisintegrateState (AActor *defaults, Baggage &bag)
{
	AddState("Disintegrate", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorHealState (AActor *defaults, Baggage &bag)
{
	AddState("Heal", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorStates (AActor *defaults, Baggage &bag)
{
	if (!bag.StateSet) ParseStates(bag.Info, defaults, bag);
	else SC_ScriptError("Multiple state declarations not allowed");
	bag.StateSet=true;
}

//==========================================================================
//
//==========================================================================
static void ActorRenderStyle (AActor *defaults, Baggage &bag)
{
	static const char * renderstyles[]={
		"NONE","NORMAL","FUZZY","SOULTRANS","OPTFUZZY","STENCIL","TRANSLUCENT", "ADD",NULL};

	static const int renderstyle_values[]={
		STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
			STYLE_Stencil, STYLE_Translucent, STYLE_Add};

	SC_MustGetString();
	defaults->RenderStyle=renderstyle_values[SC_MustMatchString(renderstyles)];
}

//==========================================================================
//
//==========================================================================
static void ActorAlpha (AActor *defaults, Baggage &bag)
{
	if (SC_CheckString("DEFAULT"))
	{
		defaults->alpha = gameinfo.gametype==GAME_Heretic? HR_SHADOW : HX_SHADOW;
	}
	else
	{
		SC_MustGetFloat();
		defaults->alpha=fixed_t(sc_Float*FRACUNIT);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_Obituary, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorHitObituary (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_HitObituary, sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorDontHurtShooter (AActor *defaults, Baggage &bag)
{
	bag.Info->Class->Meta.SetMetaInt (ACMETA_DontHurtShooter, true);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionRadius (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionRadius, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionDamage, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// AActor::Die() uses a height of 0 to mean "cut the height to 1/4",
	// so if a height of 0 is desired, store it as -1.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorBurnHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	fixed_t h = fixed_t(sc_Float * FRACUNIT);
	// The note above for AMETA_DeathHeight also applies here.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_BurnHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorMaxTargetRange (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->maxtargetrange = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeThreshold (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->meleethreshold = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MeleeDamage, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeRange (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->meleerange = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeSound (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MeleeSound, S_FindSound(sc_String));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MissileName, FName(sc_String));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (ACMETA_MissileHeight, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorTranslation (AActor *defaults, Baggage &bag)
{
	if (SC_CheckNumber())
	{
		int max = (gameinfo.gametype==GAME_Strife || (bag.Info->GameFilter&GAME_Strife)) ? 6:2;
		if (sc_Number < 0 || sc_Number > max)
		{
			SC_ScriptError ("Translation must be in the range [0,%d]", max);
		}
		defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc_Number);
	}
	else
	{
		CurrentTranslation.MakeIdentity();
		do
		{
			SC_GetString();
			AddToTranslation(sc_String);
		}
		while (SC_CheckString(","));
		defaults->Translation = StoreTranslation ();
	}
}

//==========================================================================
//
//==========================================================================
static void ActorBloodColor (AActor *defaults, Baggage &bag)
{
	int r,g,b;

	if (SC_CheckNumber())
	{
		SC_MustGetNumber();
		r=clamp<int>(sc_Number, 0, 255);
		SC_CheckString(",");
		SC_MustGetNumber();
		g=clamp<int>(sc_Number, 0, 255);
		SC_CheckString(",");
		SC_MustGetNumber();
		b=clamp<int>(sc_Number, 0, 255);
	}
	else
	{
		SC_MustGetString();
		int c = V_GetColor(NULL, sc_String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	PalEntry pe = MAKERGB(r,g,b);
	pe.a = CreateBloodTranslation(pe);
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodColor, pe);
}


//==========================================================================
//
//==========================================================================
static void ActorBloodType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	FName blood = sc_String;
	// normal blood
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType, blood);

	if (SC_CheckString(",")) 
	{
		SC_MustGetString();
		blood = sc_String;
	}
	// blood splatter
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType2, blood);

	if (SC_CheckString(",")) 
	{
		SC_MustGetString();
		blood = sc_String;
	}
	// axe blood
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType3, blood);
}

//==========================================================================
//
//==========================================================================
static void ActorBounceFactor (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->bouncefactor = clamp<fixed_t>(fixed_t(sc_Float * FRACUNIT), 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorBounceCount (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->bouncecount = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorMinMissileChance (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MinMissileChance=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamageType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString ();   
	if (SC_Compare("Normal")) defaults->DamageType = NAME_None;
	else defaults->DamageType=sc_String;
}

//==========================================================================
//
//==========================================================================
static void ActorDamageFactor (AActor *defaults, Baggage &bag)
{
	SC_MustGetString ();   
	if (bag.Info->DamageFactors == NULL) bag.Info->DamageFactors=new DmgFactors;

	FName dmgType;
	if (SC_Compare("Normal")) dmgType = NAME_None;
	else dmgType=sc_String;

	SC_MustGetToken(',');
	SC_MustGetFloat();
	(*bag.Info->DamageFactors)[dmgType]=(fixed_t)(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorDecal (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DecalGenerator = (FDecalBase *)intptr_t(int(FName(sc_String)));
}

//==========================================================================
//
//==========================================================================
static void ActorMaxStepHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxStepHeight=sc_Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorMaxDropoffHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxDropOffHeight=sc_Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorPoisonDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_PoisonDamage, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ActorFastSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_FastSpeed, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorRadiusDamageFactor (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_RDFactor, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorCameraheight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_CameraHeight, fixed_t(sc_Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorVSpeed (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->momz = fixed_t(sc_Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorGravity (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat ();

	if (sc_Float < 0.f)
		SC_ScriptError ("Gravity must not be negative.");

	defaults->gravity = FLOAT2FIXED (sc_Float);

	if (sc_Float == 0.f)
		defaults->flags |= MF_NOGRAVITY;
}

//==========================================================================
//
//==========================================================================
static void ActorClearFlags (AActor *defaults, Baggage &bag)
{
	defaults->flags=defaults->flags3=defaults->flags4=defaults->flags5=0;
	defaults->flags2&=MF2_ARGSDEFINED;	// this flag must not be cleared
}

//==========================================================================
//
//==========================================================================
static void ActorMonster (AActor *defaults, Baggage &bag)
{
	// sets the standard flag for a monster
	defaults->flags|=MF_SHOOTABLE|MF_COUNTKILL|MF_SOLID; 
	defaults->flags2|=MF2_PUSHWALL|MF2_MCROSS|MF2_PASSMOBJ;
	defaults->flags3|=MF3_ISMONSTER;
	defaults->flags4|=MF4_CANUSEWALLS;
}

//==========================================================================
//
//==========================================================================
static void ActorProjectile (AActor *defaults, Baggage &bag)
{
	// sets the standard flags for a projectile
	defaults->flags|=MF_NOBLOCKMAP|MF_NOGRAVITY|MF_DROPOFF|MF_MISSILE; 
	defaults->flags2|=MF2_IMPACT|MF2_PCROSS|MF2_NOTELEPORT;
	if (gameinfo.gametype&GAME_Raven) defaults->flags5|=MF5_BLOODSPLATTER;
}

//==========================================================================
//
// Special inventory properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void AmmoBackpackAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BackpackAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoBackpackMaxAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BackpackMaxAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoDropAmount (AAmmo *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AIMETA_DropAmount, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxSaveAmount (ABasicArmorBonus *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->MaxSaveAmount = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxBonus (ABasicArmorBonus *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BonusCount = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxBonusMax (ABasicArmorBonus *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->BonusMax = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorSaveAmount (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SaveAmount=sc_Number;
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SaveAmount=sc_Number;
	}
	else
	{
		SC_ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void ArmorSavePercent (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	if (sc_Float<0.0f) sc_Float=0.0f;
	if (sc_Float>100.0f) sc_Float=100.0f;
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SavePercent=fixed_t(sc_Float*FRACUNIT/100.0f);
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SavePercent=fixed_t(sc_Float*FRACUNIT/100.0f);
	}
	else
	{
		SC_ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc_String);
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryAmount (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Amount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryIcon (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->Icon = TexMan.AddPatch (sc_String);
	if (defaults->Icon <= 0)
	{
		defaults->Icon = TexMan.AddPatch (sc_String, ns_sprites);
		if (defaults->Icon<=0)
		{
			// Don't print warnings if the item is for another game or if this is a shareware IWAD. 
			// Strife's teaser doesn't contain all the icon graphics of the full game.
			if ((bag.Info->GameFilter == GAME_Any || bag.Info->GameFilter & gameinfo.gametype) &&
				!(gameinfo.flags&GI_SHAREWARE))
			{
				Printf("Icon '%s' for '%s' not found\n", sc_String, bag.Info->Class->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryMaxAmount (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->MaxAmount=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryDefMaxAmount (AInventory *defaults, Baggage &bag)
{
	defaults->MaxAmount = gameinfo.gametype == GAME_Heretic ? 16 : 25;
}


//==========================================================================
//
//==========================================================================
static void InventoryPickupmsg (AInventory *defaults, Baggage &bag)
{
	// allow game specific pickup messages
	const char * games[] = {"Doom", "Heretic", "Hexen", "Raven", "Strife", NULL};
	int gamemode[]={GAME_Doom, GAME_Heretic, GAME_Hexen, GAME_Raven, GAME_Strife};

	SC_MustGetString();
	int game = SC_MatchString(games);

	if (game!=-1 && SC_CheckString(","))
	{
		SC_MustGetString();
		if (!(gameinfo.gametype&gamemode[game])) return;
	}
	bag.Info->Class->Meta.SetMetaString(AIMETA_PickupMessage, sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryPickupsound (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->PickupSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryRespawntics (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->RespawnTics=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryUsesound (AInventory *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->UseSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void InventoryGiveQuest (AInventory *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_GiveQuest, sc_Number);
}

//==========================================================================
//
//==========================================================================
static void HealthLowMessage (AHealth *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_LowHealth, sc_Number);
	SC_MustGetStringName(",");
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString(AIMETA_LowHealthMessage, sc_String);
}

//==========================================================================
//
//==========================================================================
static void PuzzleitemNumber (APuzzleItem *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PuzzleItemNumber=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PuzzleitemFailMsg (APuzzleItem *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.Info->Class->Meta.SetMetaString(AIMETA_PuzzFailMessage, sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoGive1=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoGive2=sc_Number;
}

//==========================================================================
//
// Passing these parameters is really tricky to allow proper inheritance
// and forward declarations. Here only a name is
// stored which must be resolved after everything has been declared
//
//==========================================================================

static void WeaponAmmoType1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AmmoType1 = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoType2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->AmmoType2 = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse1 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoUse1=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse2 (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->AmmoUse2=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponKickback (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->Kickback=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponReadySound (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->ReadySound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponSelectionOrder (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->SelectionOrder=sc_Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponSisterWeapon (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->SisterWeaponType=fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponUpSound (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->UpSound=S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void WeaponYAdjust (AWeapon *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	defaults->YAdjust=fixed_t(sc_Float * FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void WPieceValue (AWeaponPiece *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->PieceValue = 1 << (sc_Number-1);
}

//==========================================================================
//
//==========================================================================
static void WPieceWeapon (AWeaponPiece *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->WeaponClass = fuglyname(sc_String);
}

//==========================================================================
//
//==========================================================================
static void PowerupColor (APowerupGiver *defaults, Baggage &bag)
{
	int r;
	int g;
	int b;
	int alpha;

	if (SC_CheckNumber())
	{
		r=clamp<int>(sc_Number, 0, 255);
		SC_CheckString(",");
		SC_MustGetNumber();
		g=clamp<int>(sc_Number, 0, 255);
		SC_CheckString(",");
		SC_MustGetNumber();
		b=clamp<int>(sc_Number, 0, 255);
	}
	else
	{
		SC_MustGetString();

		if (SC_Compare("INVERSEMAP"))
		{
			defaults->BlendColor = INVERSECOLOR;
			return;
		}
		else if (SC_Compare("GOLDMAP"))
		{
			defaults->BlendColor = GOLDCOLOR;
			return;
		}
		// [BC] Yay, more hacks.
		else if ( SC_Compare( "REDMAP" ))
		{
			defaults->BlendColor = REDCOLOR;
			return;
		}
		else if ( SC_Compare( "GREENMAP" ))
		{
			defaults->BlendColor = GREENCOLOR;
			return;
		}

		int c = V_GetColor(NULL, sc_String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	SC_CheckString(",");
	SC_MustGetFloat();
	alpha=int(sc_Float*255);
	alpha=clamp<int>(alpha, 0, 255);
	if (alpha!=0) defaults->BlendColor = MAKEARGB(alpha, r, g, b);
	else defaults->BlendColor = 0;
}

//==========================================================================
//
//==========================================================================
static void PowerupDuration (APowerupGiver *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	defaults->EffectTics = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PowerupMode (APowerupGiver *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->mode = (FName)sc_String;
}

//==========================================================================
//
//==========================================================================
static void PowerupType (APowerupGiver *defaults, Baggage &bag)
{
	FString typestr;

	SC_MustGetString();
	typestr.Format ("Power%s", sc_String);
	const PClass * powertype=PClass::FindClass(typestr);
	if (!powertype)
	{
		SC_ScriptError("Unknown powerup type '%s' in '%s'\n", sc_String, bag.Info->Class->TypeName.GetChars());
	}
	else if (!powertype->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		SC_ScriptError("Invalid powerup type '%s' in '%s'\n", sc_String, bag.Info->Class->TypeName.GetChars());
	}
	else
	{
		defaults->PowerupType=powertype;
	}
}

//==========================================================================
//
// [GRB] Special player properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void PlayerDisplayName (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	bag.Info->Class->Meta.SetMetaString (APMETA_DisplayName, sc_String);
}

//==========================================================================
//
//==========================================================================
static void PlayerSoundClass (APlayerPawn *defaults, Baggage &bag)
{
	FString tmp;

	SC_MustGetString ();
	tmp = sc_String;
	tmp.ReplaceChars (' ', '_');
	bag.Info->Class->Meta.SetMetaString (APMETA_SoundClass, tmp);
}

//==========================================================================
//
//==========================================================================
static void PlayerColorRange (APlayerPawn *defaults, Baggage &bag)
{
	int start, end;

	SC_MustGetNumber ();
	start = sc_Number;
	SC_CheckString(",");
	SC_MustGetNumber ();
	end = sc_Number;

	if (start > end)
		swap (start, end);

	bag.Info->Class->Meta.SetMetaInt (APMETA_ColorRange, (start & 255) | ((end & 255) << 8));
}

//==========================================================================
//
//==========================================================================
static void PlayerAttackZOffset (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->AttackZOffset = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerJumpZ (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->JumpZ = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSpawnClass (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	if (SC_Compare ("Any"))
		defaults->SpawnMask = 0;
	else if (SC_Compare ("Fighter"))
		defaults->SpawnMask |= MTF_FIGHTER;
	else if (SC_Compare ("Cleric"))
		defaults->SpawnMask |= MTF_CLERIC;
	else if (SC_Compare ("Mage"))
		defaults->SpawnMask |= MTF_MAGE;
	else if (IsNum(sc_String))
	{
		int val = strtol(sc_String, NULL, 0);
		defaults->SpawnMask = (val*MTF_FIGHTER) & (MTF_FIGHTER|MTF_CLERIC|MTF_MAGE);
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerViewHeight (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->ViewHeight = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerForwardMove (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->ForwardMove1 = defaults->ForwardMove2 = FLOAT2FIXED (sc_Float);
	if (CheckFloatParm ())
		defaults->ForwardMove2 = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSideMove (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetFloat ();
	defaults->SideMove1 = defaults->SideMove2 = FLOAT2FIXED (sc_Float);
	if (CheckFloatParm ())
		defaults->SideMove2 = FLOAT2FIXED (sc_Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerMaxHealth (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->MaxHealth = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PlayerRunHealth (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetNumber ();
	defaults->RunHealth = sc_Number;
}

//==========================================================================
//
//==========================================================================
static void PlayerMorphWeapon (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	defaults->MorphWeapon = FName(sc_String);
}

//==========================================================================
//
//==========================================================================
static void PlayerScoreIcon (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	defaults->ScoreIcon = TexMan.AddPatch (sc_String);
	if (defaults->ScoreIcon <= 0)
	{
		defaults->ScoreIcon = TexMan.AddPatch (sc_String, ns_sprites);
		if (defaults->ScoreIcon <= 0)
		{
			Printf("Icon '%s' for '%s' not found\n", sc_String, bag.Info->Class->TypeName.GetChars ());
		}
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerCrouchSprite (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	for (unsigned int i = 0; i < sc_StringLen; i++) sc_String[i] = toupper (sc_String[i]);
	defaults->crouchsprite = GetSpriteIndex (sc_String);
}

//==========================================================================
//
// [GRB] Store start items in drop item list
//
//==========================================================================
static void PlayerStartItem (APlayerPawn *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	SC_MustGetString();
	di->Name = sc_String;
	di->probability = 255;
	di->amount = 1;
	if (CheckNumParm())
	{
		di->amount = sc_Number;
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static void PlayerInvulMode (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	bag.Info->Class->Meta.SetMetaInt (APMETA_InvulMode, (FName)sc_String);
}

//==========================================================================
//
//==========================================================================
static void PlayerHealRadius (APlayerPawn *defaults, Baggage &bag)
{
	SC_MustGetString ();
	bag.Info->Class->Meta.SetMetaInt (APMETA_HealingRadius, (FName)sc_String);
}

//==========================================================================
//
//==========================================================================
static void PlayerHexenArmor (APlayerPawn *defaults, Baggage &bag)
{
	for (int i=0;i<5;i++)
	{
		SC_MustGetFloat ();
		bag.Info->Class->Meta.SetMetaFixed (APMETA_Hexenarmor0+i, FLOAT2FIXED (sc_Float));
		if (i!=4) SC_MustGetStringName(",");
	}
}

//==========================================================================
//
//==========================================================================
static void EggFXMonsterClass (AMorphProjectile *defaults, Baggage &bag)
{
	SC_MustGetString ();
	defaults->MonsterClass = FName(sc_String);
}

//==========================================================================
//
//==========================================================================
static void EggFXPlayerClass (AMorphProjectile *defaults, Baggage &bag)
{
	SC_MustGetString ();
	defaults->PlayerClass = FName(sc_String);
}

//==========================================================================
//
//==========================================================================
static const ActorProps *APropSearch (const char *str, const ActorProps *props, int numprops)
{
	int min = 0, max = numprops - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = strcmp (str, props[mid].name);
		if (lexval == 0)
		{
			return &props[mid];
		}
		else if (lexval > 0)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return NULL;
}

//==========================================================================
//
// all actor properties
//
//==========================================================================
#define apf ActorPropFunction
static const ActorProps props[] =
{
	{ "activesound",				ActorActiveSound,			RUNTIME_CLASS(AActor) },
	{ "alpha",						ActorAlpha,					RUNTIME_CLASS(AActor) },
	{ "ammo.backpackamount",		(apf)AmmoBackpackAmount,	RUNTIME_CLASS(AAmmo) },
	{ "ammo.backpackmaxamount",		(apf)AmmoBackpackMaxAmount,	RUNTIME_CLASS(AAmmo) },
	{ "ammo.dropamount",			(apf)AmmoDropAmount,		RUNTIME_CLASS(AAmmo) },
	{ "args",						ActorArgs,					RUNTIME_CLASS(AActor) },
	{ "armor.maxbonus",				(apf)ArmorMaxBonus,			RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.maxbonusmax",			(apf)ArmorMaxBonusMax,		RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.maxsaveamount",		(apf)ArmorMaxSaveAmount,	RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.saveamount",			(apf)ArmorSaveAmount,		RUNTIME_CLASS(AActor) },
	{ "armor.savepercent",			(apf)ArmorSavePercent,		RUNTIME_CLASS(AActor) },
	{ "attacksound",				ActorAttackSound,			RUNTIME_CLASS(AActor) },
	{ "bloodcolor",					ActorBloodColor,			RUNTIME_CLASS(AActor) },
	{ "bloodtype",					ActorBloodType,				RUNTIME_CLASS(AActor) },
	{ "bouncecount",				ActorBounceCount,			RUNTIME_CLASS(AActor) },
	{ "bouncefactor",				ActorBounceFactor,			RUNTIME_CLASS(AActor) },
	{ "burn",						ActorBurnState,				RUNTIME_CLASS(AActor) },
	{ "burnheight",					ActorBurnHeight,			RUNTIME_CLASS(AActor) },
	{ "cameraheight",				ActorCameraheight,			RUNTIME_CLASS(AActor) },
	{ "clearflags",					ActorClearFlags,			RUNTIME_CLASS(AActor) },
	{ "conversationid",				ActorConversationID,		RUNTIME_CLASS(AActor) },
	{ "crash",						ActorCrashState,			RUNTIME_CLASS(AActor) },
	{ "crush",						ActorCrushState,			RUNTIME_CLASS(AActor) },
	{ "damage",						ActorDamage,				RUNTIME_CLASS(AActor) },
	{ "damagefactor",				ActorDamageFactor,			RUNTIME_CLASS(AActor) },
	{ "damagetype",					ActorDamageType,			RUNTIME_CLASS(AActor) },
	{ "death",						ActorDeathState,			RUNTIME_CLASS(AActor) },
	{ "deathheight",				ActorDeathHeight,			RUNTIME_CLASS(AActor) },
	{ "deathsound",					ActorDeathSound,			RUNTIME_CLASS(AActor) },
	{ "decal",						ActorDecal,					RUNTIME_CLASS(AActor) },
	{ "disintegrate",				ActorDisintegrateState,		RUNTIME_CLASS(AActor) },
	{ "donthurtshooter",			ActorDontHurtShooter,		RUNTIME_CLASS(AActor) },
	{ "dropitem",					ActorDropItem,				RUNTIME_CLASS(AActor) },
	{ "explosiondamage",			ActorExplosionDamage,		RUNTIME_CLASS(AActor) },
	{ "explosionradius",			ActorExplosionRadius,		RUNTIME_CLASS(AActor) },
	{ "fastspeed",					ActorFastSpeed,				RUNTIME_CLASS(AActor) },
	{ "floatspeed",					ActorFloatSpeed,			RUNTIME_CLASS(AActor) },
	{ "game",						ActorGame,					RUNTIME_CLASS(AActor) },
	{ "gibhealth",					ActorGibHealth,				RUNTIME_CLASS(AActor) },
	{ "gravity",					ActorGravity,				RUNTIME_CLASS(AActor) },
	{ "heal",						ActorHealState,				RUNTIME_CLASS(AActor) },
	{ "health",						ActorHealth,				RUNTIME_CLASS(AActor) },
	{ "health.lowmessage",			(apf)HealthLowMessage,		RUNTIME_CLASS(AHealth) },
	{ "height",						ActorHeight,				RUNTIME_CLASS(AActor) },
	{ "hitobituary",				ActorHitObituary,			RUNTIME_CLASS(AActor) },
	{ "howlsound",					ActorHowlSound,				RUNTIME_CLASS(AActor) },
	{ "ice",						ActorIceState,				RUNTIME_CLASS(AActor) },
	{ "inventory.amount",			(apf)InventoryAmount,		RUNTIME_CLASS(AInventory) },
	{ "inventory.defmaxamount",		(apf)InventoryDefMaxAmount,	RUNTIME_CLASS(AInventory) },
	{ "inventory.givequest",		(apf)InventoryGiveQuest,	RUNTIME_CLASS(AInventory) },
	{ "inventory.icon",				(apf)InventoryIcon,			RUNTIME_CLASS(AInventory) },
	{ "inventory.maxamount",		(apf)InventoryMaxAmount,	RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupmessage",	(apf)InventoryPickupmsg,	RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupsound",		(apf)InventoryPickupsound,	RUNTIME_CLASS(AInventory) },
	{ "inventory.respawntics",		(apf)InventoryRespawntics,	RUNTIME_CLASS(AInventory) },
	{ "inventory.usesound",			(apf)InventoryUsesound,		RUNTIME_CLASS(AInventory) },
	{ "mass",						ActorMass,					RUNTIME_CLASS(AActor) },
	{ "maxdropoffheight",			ActorMaxDropoffHeight,		RUNTIME_CLASS(AActor) },
	{ "maxstepheight",				ActorMaxStepHeight,			RUNTIME_CLASS(AActor) },
	{ "maxtargetrange",				ActorMaxTargetRange,		RUNTIME_CLASS(AActor) },
	{ "melee",						ActorMeleeState,			RUNTIME_CLASS(AActor) },
	{ "meleedamage",				ActorMeleeDamage,			RUNTIME_CLASS(AActor) },
	{ "meleerange",					ActorMeleeRange,			RUNTIME_CLASS(AActor) },
	{ "meleesound",					ActorMeleeSound,			RUNTIME_CLASS(AActor) },
	{ "meleethreshold",				ActorMeleeThreshold,		RUNTIME_CLASS(AActor) },
	{ "minmissilechance",			ActorMinMissileChance,		RUNTIME_CLASS(AActor) },
	{ "missile",					ActorMissileState,			RUNTIME_CLASS(AActor) },
	{ "missileheight",				ActorMissileHeight,			RUNTIME_CLASS(AActor) },
	{ "missiletype",				ActorMissileType,			RUNTIME_CLASS(AActor) },
	{ "monster",					ActorMonster,				RUNTIME_CLASS(AActor) },
	{ "morphprojectile.monsterclass",(apf)EggFXMonsterClass,	RUNTIME_CLASS(AMorphProjectile) },
	{ "morphprojectile.playerclass",(apf)EggFXPlayerClass,		RUNTIME_CLASS(AMorphProjectile) },
	{ "obituary",					ActorObituary,				RUNTIME_CLASS(AActor) },
	{ "pain",						ActorPainState,				RUNTIME_CLASS(AActor) },
	{ "painchance",					ActorPainChance,			RUNTIME_CLASS(AActor) },
	{ "painsound",					ActorPainSound,				RUNTIME_CLASS(AActor) },
	{ "player.attackzoffset",		(apf)PlayerAttackZOffset,	RUNTIME_CLASS(APlayerPawn) },
	{ "player.colorrange",			(apf)PlayerColorRange,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.crouchsprite",		(apf)PlayerCrouchSprite,	RUNTIME_CLASS(APlayerPawn) },
	{ "player.displayname",			(apf)PlayerDisplayName,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.forwardmove",			(apf)PlayerForwardMove,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.healradiustype",		(apf)PlayerHealRadius,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.hexenarmor",			(apf)PlayerHexenArmor,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.invulnerabilitymode",	(apf)PlayerInvulMode,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.jumpz",				(apf)PlayerJumpZ,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.maxhealth",			(apf)PlayerMaxHealth,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.morphweapon",			(apf)PlayerMorphWeapon,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.runhealth",			(apf)PlayerRunHealth,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.scoreicon",			(apf)PlayerScoreIcon,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.sidemove",			(apf)PlayerSideMove,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.soundclass",			(apf)PlayerSoundClass,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.spawnclass",			(apf)PlayerSpawnClass,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.startitem",			(apf)PlayerStartItem,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.viewheight",			(apf)PlayerViewHeight,		RUNTIME_CLASS(APlayerPawn) },
	{ "poisondamage",				ActorPoisonDamage,			RUNTIME_CLASS(AActor) },
	{ "powerup.color",				(apf)PowerupColor,			RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.duration",			(apf)PowerupDuration,		RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.mode",				(apf)PowerupMode,			RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.type",				(apf)PowerupType,			RUNTIME_CLASS(APowerupGiver) },
	{ "projectile",					ActorProjectile,			RUNTIME_CLASS(AActor) },
	{ "puzzleitem.failmessage",		(apf)PuzzleitemFailMsg,		RUNTIME_CLASS(APuzzleItem) },
	{ "puzzleitem.number",			(apf)PuzzleitemNumber,		RUNTIME_CLASS(APuzzleItem) },
	{ "radius",						ActorRadius,				RUNTIME_CLASS(AActor) },
	{ "radiusdamagefactor",			ActorRadiusDamageFactor,	RUNTIME_CLASS(AActor) },
	{ "raise",						ActorRaiseState,			RUNTIME_CLASS(AActor) },
	{ "reactiontime",				ActorReactionTime,			RUNTIME_CLASS(AActor) },
	{ "renderstyle",				ActorRenderStyle,			RUNTIME_CLASS(AActor) },
	{ "scale",						ActorScale,					RUNTIME_CLASS(AActor) },
	{ "see",						ActorSeeState,				RUNTIME_CLASS(AActor) },
	{ "seesound",					ActorSeeSound,				RUNTIME_CLASS(AActor) },
	{ "skip_super",					ActorSkipSuper,				RUNTIME_CLASS(AActor) },
	{ "spawn",						ActorSpawnState,			RUNTIME_CLASS(AActor) },
	{ "spawnid",					ActorSpawnID,				RUNTIME_CLASS(AActor) },
	{ "speed",						ActorSpeed,					RUNTIME_CLASS(AActor) },
	{ "states",						ActorStates,				RUNTIME_CLASS(AActor) },
	{ "tag",						ActorTag,					RUNTIME_CLASS(AActor) },
	{ "translation",				ActorTranslation,			RUNTIME_CLASS(AActor) },
	{ "vspeed",						ActorVSpeed,				RUNTIME_CLASS(AActor) },
	{ "weapon.ammogive",			(apf)WeaponAmmoGive1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive1",			(apf)WeaponAmmoGive1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive2",			(apf)WeaponAmmoGive2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype",			(apf)WeaponAmmoType1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype1",			(apf)WeaponAmmoType1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype2",			(apf)WeaponAmmoType2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse",				(apf)WeaponAmmoUse1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse1",			(apf)WeaponAmmoUse1,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse2",			(apf)WeaponAmmoUse2,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.kickback",			(apf)WeaponKickback,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.readysound",			(apf)WeaponReadySound,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.selectionorder",		(apf)WeaponSelectionOrder,	RUNTIME_CLASS(AWeapon) },
	{ "weapon.sisterweapon",		(apf)WeaponSisterWeapon,	RUNTIME_CLASS(AWeapon) },
	{ "weapon.upsound",				(apf)WeaponUpSound,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.yadjust",				(apf)WeaponYAdjust,			RUNTIME_CLASS(AWeapon) },
	{ "weaponpiece.number",			(apf)WPieceValue,			RUNTIME_CLASS(AWeaponPiece) },
	{ "weaponpiece.weapon",			(apf)WPieceWeapon,			RUNTIME_CLASS(AWeaponPiece) },
	{ "wound",						ActorWoundState,			RUNTIME_CLASS(AActor) },
	{ "woundhealth",				ActorWoundHealth,			RUNTIME_CLASS(AActor) },
	{ "xdeath",						ActorXDeathState,			RUNTIME_CLASS(AActor) },
	{ "xscale",						ActorXScale,				RUNTIME_CLASS(AActor) },
	{ "yscale",						ActorYScale,				RUNTIME_CLASS(AActor) },
	// AWeapon:MinAmmo1 and 2 are never used so there is no point in adding them here!
};
static const ActorProps *is_actorprop (const char *str)
{
	return APropSearch (str, props, sizeof(props)/sizeof(ActorProps));
}


//==========================================================================
//
// Parses an actor property
//
//==========================================================================

void ParseActorProperty(Baggage &bag)
{
	strlwr (sc_String);

	FString propname = sc_String;

	if (SC_CheckString ("."))
	{
		SC_MustGetString ();
		propname += '.';
		strlwr (sc_String);
		propname += sc_String;
	}
	else
	{
		SC_UnGet ();
	}

	const ActorProps *prop = is_actorprop (propname.GetChars());

	if (prop != NULL)
	{
		if (!bag.Info->Class->IsDescendantOf(prop->type))
		{
			SC_ScriptError("\"%s\" requires an actor of type \"%s\"\n", propname.GetChars(), prop->type->TypeName.GetChars());
		}
		else
		{
			prop->Handler ((AActor *)bag.Info->Class->Defaults, bag);
		}
	}
	else
	{
		SC_ScriptError("\"%s\" is an unknown actor property\n", propname.GetChars());
	}
}


//==========================================================================
//
// Finalizes an actor definition
//
//==========================================================================

void FinishActor(FActorInfo *info, Baggage &bag)
{
	AActor *defaults = (AActor*)info->Class->Defaults;

	FinishStates (info, defaults, bag);
	InstallStates (info, defaults);
	ProcessStates (info->OwnedStates, info->NumOwnedStates);
	if (bag.DropItemSet)
	{
		if (bag.DropItemList == NULL)
		{
			if (info->Class->Meta.GetMetaInt (ACMETA_DropItems) != 0)
			{
				info->Class->Meta.SetMetaInt (ACMETA_DropItems, 0);
			}
		}
		else
		{
			info->Class->Meta.SetMetaInt (ACMETA_DropItems,
				DropItemList.Push (bag.DropItemList) + 1);
		}
	}
	if (info->Class->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		defaults->flags |= MF_SPECIAL;
	}
}
