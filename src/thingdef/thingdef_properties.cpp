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
#include "a_morph.h"

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_FLAG2(symbol, name, type, variable) { symbol, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_DEPRECATED_FLAG(name) { DEPF_##name, #name, -1 }
#define DEFINE_DUMMY_FLAG(name) { DEPF_UNUSED, #name, -1 }

struct flagdef
{
	int flagbit;
	const char *name;
	int structoffset;
};

enum 
{
	DEPF_UNUSED,
	DEPF_FIREDAMAGE,
	DEPF_ICEDAMAGE,
	DEPF_LOWGRAVITY,
	DEPF_LONGMELEERANGE,
	DEPF_SHORTMISSILERANGE,
	DEPF_PICKUPFLASH,
	DEPF_QUARTERGRAVITY,
};

static flagdef ActorFlags[]=
{
	DEFINE_FLAG(MF, PICKUP, APlayerPawn, flags),
	DEFINE_FLAG(MF, SPECIAL, APlayerPawn, flags),
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
	DEFINE_FLAG(MF3, SPECIALFLOORCLIP, AActor, flags3),
	DEFINE_FLAG(MF3, ALWAYSPUFF, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSPLASH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTOVERLAP, AActor, flags3),
	DEFINE_FLAG(MF3, DONTMORPH, AActor, flags3),
	DEFINE_FLAG(MF3, DONTSQUASH, AActor, flags3),
	DEFINE_FLAG(MF3, EXPLOCOUNT, AActor, flags3),
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
	DEFINE_FLAG(MF5, ALWAYSRESPAWN, AActor, flags5),
	DEFINE_FLAG(MF5, NEVERRESPAWN, AActor, flags5),
	DEFINE_FLAG(MF5, DONTRIP, AActor, flags5),
	DEFINE_FLAG(MF5, NOINFIGHTING, AActor, flags5),
	DEFINE_FLAG(MF5, NOINTERACTION, AActor, flags5),
	DEFINE_FLAG(MF5, NOTIMEFREEZE, AActor, flags5),
	DEFINE_FLAG(MF5, PUFFGETSOWNER, AActor, flags5), // [BB] added PUFFGETSOWNER
	DEFINE_FLAG(MF5, SPECIALFIREDAMAGE, AActor, flags5),
	DEFINE_FLAG(MF5, SUMMONEDMONSTER, AActor, flags5),
	DEFINE_FLAG(MF5, NOVERTICALMELEERANGE, AActor, flags5),

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEXYBILLBOARD, AActor, renderflags),

	// Deprecated flags. Handling must be performed in HandleDeprecatedFlags
	DEFINE_DEPRECATED_FLAG(FIREDAMAGE),
	DEFINE_DEPRECATED_FLAG(ICEDAMAGE),
	DEFINE_DEPRECATED_FLAG(LOWGRAVITY),
	DEFINE_DEPRECATED_FLAG(SHORTMISSILERANGE),
	DEFINE_DEPRECATED_FLAG(LONGMELEERANGE),
	DEFINE_DEPRECATED_FLAG(QUARTERGRAVITY),
	DEFINE_DUMMY_FLAG(NONETID),
	DEFINE_DUMMY_FLAG(ALLOWCLIENTSPAWN),
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
	DEFINE_FLAG(IF, ALWAYSPICKUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, FANCYPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, BIGPOWERUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, KEEPDEPLETED, AInventory, ItemFlags),
	DEFINE_FLAG(IF, IGNORESKILL, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ADDITIVETIME, AInventory, ItemFlags),

	DEFINE_DEPRECATED_FLAG(PICKUPFLASH),

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
	DEFINE_FLAG(WIF, STAFF2_KICKBACK, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, EXPLOSIVE, AWeapon, WeaponFlags),
	DEFINE_FLAG2(WIF_BOT_MELEE, MELEEWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, BFG, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, CHEATNOTWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NO_AUTO_SWITCH, AWeapon, WeaponFlags),
	
	DEFINE_DUMMY_FLAG(NOLMS),
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
	case DEPF_FIREDAMAGE:
		defaults->DamageType = set? NAME_Fire : NAME_None;
		break;
	case DEPF_ICEDAMAGE:
		defaults->DamageType = set? NAME_Ice : NAME_None;
		break;
	case DEPF_LOWGRAVITY:
		defaults->gravity = set? FRACUNIT/8 : FRACUNIT;
		break;
	case DEPF_SHORTMISSILERANGE:
		defaults->maxtargetrange = set? 896*FRACUNIT : 0;
		break;
	case DEPF_LONGMELEERANGE:
		defaults->meleethreshold = set? 196*FRACUNIT : 0;
		break;
	case DEPF_QUARTERGRAVITY:
		defaults->gravity = set? FRACUNIT/4 : FRACUNIT;
		break;
	case DEPF_PICKUPFLASH:
		if (set)
		{
			static_cast<AInventory*>(defaults)->PickupFlash = fuglyname("PickupFlash");
		}
		else
		{
			static_cast<AInventory*>(defaults)->PickupFlash = NULL;
		}
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
void ParseActorFlag (FScanner &sc, Baggage &bag, int mod)
{
	flagdef *fd;

	sc.MustGetString ();

	FString part1 = sc.String;
	const char *part2 = NULL;
	if (sc.CheckString ("."))
	{
		sc.MustGetString ();
		part2 = sc.String;
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
			sc.ScriptError("\"%s\" is an unknown flag\n", part1.GetChars());
		}
		else
		{
			sc.ScriptError("\"%s.%s\" is an unknown flag\n", part1.GetChars(), part2);
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
		//sc.ScriptError("Invalid syntax in translation specification: '%c' expected", c);
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

static int StoreTranslation(FScanner &sc)
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
		sc.ScriptError("Too many translations in DECORATE");
	}
	FRemapTable *newtrans = new FRemapTable;
	*newtrans = CurrentTranslation;
	i = translationtables[TRANSLATION_Decorate].Push(newtrans);
	return TRANSLATION(TRANSLATION_Decorate, i);
}

static int CreateBloodTranslation(FScanner &sc, PalEntry color)
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
		sc.ScriptError("Too many blood colors in DECORATE");
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

typedef void (*ActorPropFunction) (FScanner &sc, AActor *defaults, Baggage &bag);

struct ActorProps 
{ 
	const char *name; 
	ActorPropFunction Handler; 
	const PClass * type; 
};

typedef ActorProps (*ActorPropHandler) (const char *str, unsigned int len);

static const ActorProps *is_actorprop (const char *str);


//==========================================================================
//
// Checks for a numeric parameter which may or may not be preceded by a comma
//
//==========================================================================
static bool CheckNumParm(FScanner &sc)
{
	if (sc.CheckString(","))
	{
		sc.MustGetNumber();
		return true;
	}
	else
	{
		return sc.CheckNumber();
	}
}

static bool CheckFloatParm(FScanner &sc)
{
	if (sc.CheckString(","))
	{
		sc.MustGetFloat();
		return true;
	}
	else
	{
		return sc.CheckFloat();
	}
}

// [MH]
static int ParseMorphStyle (FScanner &sc)
{
 	static const char * morphstyles[]={
		"MRF_ADDSTAMINA", "MRF_FULLHEALTH", "MRF_UNDOBYTOMEOFPOWER", "MRF_UNDOBYCHAOSDEVICE",
		"MRF_FAILNOTELEFRAG", "MRF_FAILNOLAUGH", "MRF_WHENINVULNERABLE", "MRF_LOSEACTUALWEAPON",
		"MRF_NEWTIDBEHAVIOUR", "MRF_UNDOBYDEATH", "MRF_UNDOBYDEATHFORCED", "MRF_UNDOBYDEATHSAVES", NULL};

 	static const int morphstyle_values[]={
		MORPH_ADDSTAMINA, MORPH_FULLHEALTH, MORPH_UNDOBYTOMEOFPOWER, MORPH_UNDOBYCHAOSDEVICE,
		MORPH_FAILNOTELEFRAG, MORPH_FAILNOLAUGH, MORPH_WHENINVULNERABLE, MORPH_LOSEACTUALWEAPON,
		MORPH_NEWTIDBEHAVIOUR, MORPH_UNDOBYDEATH, MORPH_UNDOBYDEATHFORCED, MORPH_UNDOBYDEATHSAVES};

	// May be given flags by number...
	if (sc.CheckNumber())
	{
		sc.MustGetNumber();
		return sc.Number;
	}

	// ... else should be flags by name.
	// NOTE: Later this should be removed and a normal expression used.
	// The current DECORATE parser can't handle this though.
	bool gotparen = sc.CheckString("(");
	int style = 0;
	do
	{
		sc.MustGetString();
		style |= morphstyle_values[sc.MustMatchString(morphstyles)];
	}
	while (sc.CheckString("|"));
	if (gotparen)
	{
		sc.MustGetStringName(")");
	}

	return style;
}

//==========================================================================
//
// Property parsers
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void ActorSkipSuper (FScanner &sc, AActor *defaults, Baggage &bag)
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
static void ActorGame (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString ();
	if (sc.Compare ("Doom"))
	{
		bag.Info->GameFilter |= GAME_Doom;
	}
	else if (sc.Compare ("Heretic"))
	{
		bag.Info->GameFilter |= GAME_Heretic;
	}
	else if (sc.Compare ("Hexen"))
	{
		bag.Info->GameFilter |= GAME_Hexen;
	}
	else if (sc.Compare ("Raven"))
	{
		bag.Info->GameFilter |= GAME_Raven;
	}
	else if (sc.Compare ("Strife"))
	{
		bag.Info->GameFilter |= GAME_Strife;
	}
	else if (sc.Compare ("Any"))
	{
		bag.Info->GameFilter = GAME_Any;
	}
	else
	{
		sc.ScriptError ("Unknown game type %s", sc.String);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnID (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	if (sc.Number<0 || sc.Number>255)
	{
		sc.ScriptError ("SpawnID must be in the range [0,255]");
	}
	else bag.Info->SpawnID=(BYTE)sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorConversationID (FScanner &sc, AActor *defaults, Baggage &bag)
{
	int convid;

	sc.MustGetNumber();
	convid = sc.Number;

	// Handling for Strife teaser IDs - only of meaning for the standard items
	// as PWADs cannot be loaded with the teasers.
	if (sc.CheckString(","))
	{
		sc.MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE))
			convid=sc.Number;

		sc.MustGetStringName(",");
		sc.MustGetNumber();
		if ((gameinfo.flags & (GI_SHAREWARE|GI_TEASER2)) == (GI_SHAREWARE|GI_TEASER2))
			convid=sc.Number;

		if (convid==-1) return;
	}
	if (convid<0 || convid>1000)
	{
		sc.ScriptError ("ConversationID must be in the range [0,1000]");
	}
	else StrifeTypes[convid] = bag.Info->Class;
}

//==========================================================================
//
//==========================================================================
static void ActorTag (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaString(AMETA_StrifeName, sc.String);
}

//==========================================================================
//
//==========================================================================
static void ActorHealth (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->health=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorGibHealth (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_GibHealth, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorWoundHealth (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_WoundHealth, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorReactionTime (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->reactiontime=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorPainChance (FScanner &sc, AActor *defaults, Baggage &bag)
{
	if (!sc.CheckNumber())
	{
		FName painType;
		sc.MustGetString();
		if (sc.Compare("Normal")) painType = NAME_None;
		else painType=sc.String;
		sc.MustGetToken(',');
		sc.MustGetNumber();
	
		if (bag.Info->PainChances == NULL) bag.Info->PainChances=new PainChanceList;
		(*bag.Info->PainChances)[painType] = (BYTE)sc.Number;
		return;
	}
	defaults->PainChance=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamage (FScanner &sc, AActor *defaults, Baggage &bag)
{
	// Damage can either be a single number, in which case it is subject
	// to the original damage calculation rules. Or, it can be an expression
	// and will be calculated as-is, ignoring the original rules. For
	// compatibility reasons, expressions must be enclosed within
	// parentheses.

	if (sc.CheckString ("("))
	{
		defaults->Damage = 0x40000000 | ParseExpression (sc, false, bag.Info->Class);
		sc.MustGetStringName(")");
	}
	else
	{
		sc.MustGetNumber ();
		defaults->Damage = sc.Number;
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSpeed (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->Speed=fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorFloatSpeed (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->FloatSpeed=fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorRadius (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->radius=fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->height=fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMass (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->Mass=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorXScale (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->scaleX = FLOAT2FIXED(sc.Float);
}

//==========================================================================
//
//==========================================================================
static void ActorYScale (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->scaleY = FLOAT2FIXED(sc.Float);
}

//==========================================================================
//
//==========================================================================
static void ActorScale (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->scaleX= defaults->scaleY = FLOAT2FIXED(sc.Float);
}

//==========================================================================
//
//==========================================================================
static void ActorArgs (FScanner &sc, AActor *defaults, Baggage &bag)
{
	for (int i = 0; i < 5; i++)
	{
		sc.MustGetNumber();
		defaults->args[i] = sc.Number;
		if (i < 4 && !sc.CheckToken(',')) break;
	}
	defaults->flags2|=MF2_ARGSDEFINED;
}

//==========================================================================
//
//==========================================================================
static void ActorSeeSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->SeeSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorAttackSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->AttackSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorPainSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->PainSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorDeathSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->DeathSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorActiveSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->ActiveSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorHowlSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaInt (AMETA_HowlSound, S_FindSound(sc.String));
}

//==========================================================================
//
//==========================================================================
static void ActorDropItem (FScanner &sc, AActor *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem *di = new FDropItem;

	sc.MustGetString();
	di->Name=sc.String;
	di->probability=255;
	di->amount=-1;

	if (CheckNumParm(sc))
	{
		di->probability = sc.Number;
		if (CheckNumParm(sc))
		{
			di->amount = sc.Number;
		}
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static void ActorSpawnState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Spawn", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorSeeState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("See", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Melee", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Missile", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorPainState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Pain", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDeathState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Death", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorXDeathState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("XDeath", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorBurnState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Burn", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorIceState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Ice", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorRaiseState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Raise", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrashState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Crash", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrushState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Crush", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorWoundState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Wound", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDisintegrateState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Disintegrate", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorHealState (FScanner &sc, AActor *defaults, Baggage &bag)
{
	AddState("Heal", CheckState (sc, bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorStates (FScanner &sc, AActor *defaults, Baggage &bag)
{
	if (!bag.StateSet) ParseStates(sc, bag.Info, defaults, bag);
	else sc.ScriptError("Multiple state declarations not allowed");
	bag.StateSet=true;
}

//==========================================================================
//
//==========================================================================
static void ActorRenderStyle (FScanner &sc, AActor *defaults, Baggage &bag)
{
	static const char * renderstyles[]={
		"NONE","NORMAL","FUZZY","SOULTRANS","OPTFUZZY","STENCIL","TRANSLUCENT", "ADD","SHADED", NULL};

	static const int renderstyle_values[]={
		STYLE_None, STYLE_Normal, STYLE_Fuzzy, STYLE_SoulTrans, STYLE_OptFuzzy,
			STYLE_TranslucentStencil, STYLE_Translucent, STYLE_Add, STYLE_Shaded};

	sc.MustGetString();
	defaults->RenderStyle = LegacyRenderStyles[renderstyle_values[sc.MustMatchString(renderstyles)]];
}

//==========================================================================
//
//==========================================================================
static void ActorAlpha (FScanner &sc, AActor *defaults, Baggage &bag)
{
	if (sc.CheckString("DEFAULT"))
	{
		defaults->alpha = gameinfo.gametype == GAME_Heretic ? HR_SHADOW : HX_SHADOW;
	}
	else
	{
		sc.MustGetFloat();
		defaults->alpha=fixed_t(sc.Float*FRACUNIT);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorObituary (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_Obituary, sc.String);
}

//==========================================================================
//
//==========================================================================
static void ActorHitObituary (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaString (AMETA_HitObituary, sc.String);
}

//==========================================================================
//
//==========================================================================
static void ActorDontHurtShooter (FScanner &sc, AActor *defaults, Baggage &bag)
{
	bag.Info->Class->Meta.SetMetaInt (ACMETA_DontHurtShooter, true);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionRadius (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionRadius, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorExplosionDamage (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_ExplosionDamage, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorDeathHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	fixed_t h = fixed_t(sc.Float * FRACUNIT);
	// AActor::Die() uses a height of 0 to mean "cut the height to 1/4",
	// so if a height of 0 is desired, store it as -1.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorBurnHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	fixed_t h = fixed_t(sc.Float * FRACUNIT);
	// The note above for AMETA_DeathHeight also applies here.
	bag.Info->Class->Meta.SetMetaFixed (AMETA_BurnHeight, h <= 0 ? -1 : h);
}

//==========================================================================
//
//==========================================================================
static void ActorMaxTargetRange (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->maxtargetrange = fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeThreshold (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->meleethreshold = fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeDamage (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MeleeDamage, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeRange (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->meleerange = fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeSound (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MeleeSound, S_FindSound(sc.String));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileType (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaInt (ACMETA_MissileName, FName(sc.String));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (ACMETA_MissileHeight, fixed_t(sc.Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorTranslation (FScanner &sc, AActor *defaults, Baggage &bag)
{
	if (sc.CheckNumber())
	{
		int max = (gameinfo.gametype==GAME_Strife || (bag.Info->GameFilter&GAME_Strife)) ? 6:2;
		if (sc.Number < 0 || sc.Number > max)
		{
			sc.ScriptError ("Translation must be in the range [0,%d]", max);
		}
		defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc.Number);
	}
	else if (sc.CheckString("Ice"))
	{
		defaults->Translation = TRANSLATION(TRANSLATION_Standard, 7);
	}
	else
	{
		CurrentTranslation.MakeIdentity();
		do
		{
			sc.GetString();
			AddToTranslation(sc.String);
		}
		while (sc.CheckString(","));
		defaults->Translation = StoreTranslation (sc);
	}
}

//==========================================================================
//
//==========================================================================
static void ActorStencilColor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	int r,g,b;

	if (sc.CheckNumber())
	{
		sc.MustGetNumber();
		r=clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		g=clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		b=clamp<int>(sc.Number, 0, 255);
	}
	else
	{
		sc.MustGetString();
		int c = V_GetColor(NULL, sc.String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	defaults->fillcolor = MAKERGB(r,g,b) | (ColorMatcher.Pick (r, g, b) << 24);
}


//==========================================================================
//
//==========================================================================
static void ActorBloodColor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	int r,g,b;

	if (sc.CheckNumber())
	{
		sc.MustGetNumber();
		r = clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		g = clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		b = clamp<int>(sc.Number, 0, 255);
	}
	else
	{
		sc.MustGetString();
		int c = V_GetColor(NULL, sc.String);
		r = RPART(c);
		g = GPART(c);
		b = BPART(c);
	}
	PalEntry pe = MAKERGB(r,g,b);
	pe.a = CreateBloodTranslation(sc, pe);
	if (DWORD(pe) == 0)
	{
		// If black is the first color being created it will create a value of 0
		// which stands for 'no translation'
		// Using (1,1,1) instead of (0,0,0) won't be noticable.
		pe = MAKERGB(1,1,1);
	}
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodColor, pe);
}


//==========================================================================
//
//==========================================================================
static void ActorBloodType (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	FName blood = sc.String;
	// normal blood
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType, blood);

	if (sc.CheckString(",")) 
	{
		sc.MustGetString();
		blood = sc.String;
	}
	// blood splatter
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType2, blood);

	if (sc.CheckString(",")) 
	{
		sc.MustGetString();
		blood = sc.String;
	}
	// axe blood
	bag.Info->Class->Meta.SetMetaInt (AMETA_BloodType3, blood);
}

//==========================================================================
//
//==========================================================================
static void ActorBounceFactor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->bouncefactor = clamp<fixed_t>(fixed_t(sc.Float * FRACUNIT), 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorWallBounceFactor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->wallbouncefactor = clamp<fixed_t>(fixed_t(sc.Float * FRACUNIT), 0, FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorBounceCount (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->bouncecount = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorMinMissileChance (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->MinMissileChance=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ActorDamageType (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString ();   
	if (sc.Compare("Normal")) defaults->DamageType = NAME_None;
	else defaults->DamageType=sc.String;
}

//==========================================================================
//
//==========================================================================
static void ActorDamageFactor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString ();   
	if (bag.Info->DamageFactors == NULL) bag.Info->DamageFactors=new DmgFactors;

	FName dmgType;
	if (sc.Compare("Normal")) dmgType = NAME_None;
	else dmgType=sc.String;

	sc.MustGetToken(',');
	sc.MustGetFloat();
	(*bag.Info->DamageFactors)[dmgType]=(fixed_t)(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorDecal (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->DecalGenerator = (FDecalBase *)intptr_t(int(FName(sc.String)));
}

//==========================================================================
//
//==========================================================================
static void ActorMaxStepHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->MaxStepHeight=sc.Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorMaxDropoffHeight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->MaxDropOffHeight=sc.Number * FRACUNIT;
}

//==========================================================================
//
//==========================================================================
static void ActorPoisonDamage (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AMETA_PoisonDamage, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ActorFastSpeed (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_FastSpeed, fixed_t(sc.Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorRadiusDamageFactor (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_RDFactor, fixed_t(sc.Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorCameraheight (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	bag.Info->Class->Meta.SetMetaFixed (AMETA_CameraHeight, fixed_t(sc.Float*FRACUNIT));
}

//==========================================================================
//
//==========================================================================
static void ActorVSpeed (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->momz = fixed_t(sc.Float*FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void ActorGravity (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat ();

	if (sc.Float < 0.f)
		sc.ScriptError ("Gravity must not be negative.");

	defaults->gravity = FLOAT2FIXED (sc.Float);

	if (sc.Float == 0.f)
		defaults->flags |= MF_NOGRAVITY;
}

//==========================================================================
//
//==========================================================================
static void ActorClearFlags (FScanner &sc, AActor *defaults, Baggage &bag)
{
	defaults->flags=defaults->flags3=defaults->flags4=defaults->flags5=0;
	defaults->flags2&=MF2_ARGSDEFINED;	// this flag must not be cleared
}

//==========================================================================
//
//==========================================================================
static void ActorMonster (FScanner &sc, AActor *defaults, Baggage &bag)
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
static void ActorProjectile (FScanner &sc, AActor *defaults, Baggage &bag)
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
static void AmmoBackpackAmount (FScanner &sc, AAmmo *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->BackpackAmount=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoBackpackMaxAmount (FScanner &sc, AAmmo *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->BackpackMaxAmount=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void AmmoDropAmount (FScanner &sc, AAmmo *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt (AIMETA_DropAmount, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxSaveAmount (FScanner &sc, ABasicArmorBonus *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->MaxSaveAmount = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxBonus (FScanner &sc, ABasicArmorBonus *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->BonusCount = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorMaxBonusMax (FScanner &sc, ABasicArmorBonus *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->BonusMax = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void ArmorSaveAmount (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SaveAmount=sc.Number;
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SaveAmount=sc.Number;
	}
	else
	{
		sc.ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc.String);
	}
}

//==========================================================================
//
//==========================================================================
static void ArmorSavePercent (FScanner &sc, AActor *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	if (sc.Float<0.0f) sc.Float=0.0f;
	if (sc.Float>100.0f) sc.Float=100.0f;
	// Special case here because this property has to work for 2 unrelated classes
	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorPickup)))
	{
		((ABasicArmorPickup*)defaults)->SavePercent=fixed_t(sc.Float*FRACUNIT/100.0f);
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(ABasicArmorBonus)))
	{
		((ABasicArmorBonus*)defaults)->SavePercent=fixed_t(sc.Float*FRACUNIT/100.0f);
	}
	else
	{
		sc.ScriptError("\"%s\" requires an actor of type \"Armor\"\n", sc.String);
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryAmount (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->Amount=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryIcon (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->Icon = TexMan.AddPatch (sc.String);
	if (!defaults->Icon.isValid())
	{
		defaults->Icon = TexMan.AddPatch (sc.String, ns_sprites);
		if (!defaults->Icon.isValid())
		{
			// Don't print warnings if the item is for another game or if this is a shareware IWAD. 
			// Strife's teaser doesn't contain all the icon graphics of the full game.
			if ((bag.Info->GameFilter == GAME_Any || bag.Info->GameFilter & gameinfo.gametype) &&
				!(gameinfo.flags&GI_SHAREWARE) && Wads.GetLumpFile(sc.LumpNum) != 0)
			{
				Printf("Icon '%s' for '%s' not found\n", sc.String, bag.Info->Class->TypeName.GetChars());
			}
		}
	}
}

//==========================================================================
//
//==========================================================================
static void InventoryMaxAmount (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->MaxAmount=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryDefMaxAmount (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	defaults->MaxAmount = gameinfo.gametype == GAME_Heretic ? 16 : 25;
}


//==========================================================================
//
//==========================================================================
static void InventoryPickupflash (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->PickupFlash = fuglyname(sc.String);
}

//==========================================================================
//
//==========================================================================
static void InventoryPickupmsg (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	// allow game specific pickup messages
	const char * games[] = {"Doom", "Heretic", "Hexen", "Raven", "Strife", NULL};
	int gamemode[]={GAME_Doom, GAME_Heretic, GAME_Hexen, GAME_Raven, GAME_Strife};

	sc.MustGetString();
	int game = sc.MatchString(games);

	if (game!=-1 && sc.CheckString(","))
	{
		sc.MustGetString();
		if (!(gameinfo.gametype&gamemode[game])) return;
	}
	bag.Info->Class->Meta.SetMetaString(AIMETA_PickupMessage, sc.String);
}

//==========================================================================
//
//==========================================================================
static void InventoryPickupsound (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->PickupSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void InventoryRespawntics (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->RespawnTics=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void InventoryUsesound (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->UseSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void InventoryGiveQuest (FScanner &sc, AInventory *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_GiveQuest, sc.Number);
}

//==========================================================================
//
//==========================================================================
static void HealthLowMessage (FScanner &sc, AHealth *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	bag.Info->Class->Meta.SetMetaInt(AIMETA_LowHealth, sc.Number);
	sc.MustGetStringName(",");
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaString(AIMETA_LowHealthMessage, sc.String);
}

//==========================================================================
//
//==========================================================================
static void PuzzleitemNumber (FScanner &sc, APuzzleItem *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->PuzzleItemNumber=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void PuzzleitemFailMsg (FScanner &sc, APuzzleItem *defaults, Baggage &bag)
{
	sc.MustGetString();
	bag.Info->Class->Meta.SetMetaString(AIMETA_PuzzFailMessage, sc.String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive1 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->AmmoGive1=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoGive2 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->AmmoGive2=sc.Number;
}

//==========================================================================
//
// Passing these parameters is really tricky to allow proper inheritance
// and forward declarations. Here only a name is
// stored which must be resolved after everything has been declared
//
//==========================================================================

static void WeaponAmmoType1 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->AmmoType1 = fuglyname(sc.String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoType2 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->AmmoType2 = fuglyname(sc.String);
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse1 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->AmmoUse1=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponAmmoUse2 (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->AmmoUse2=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponKickback (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->Kickback=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponDefKickback (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	defaults->Kickback = gameinfo.defKickback;
}

//==========================================================================
//
//==========================================================================
static void WeaponReadySound (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->ReadySound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void WeaponSelectionOrder (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->SelectionOrder=sc.Number;
}

//==========================================================================
//
//==========================================================================
static void WeaponSisterWeapon (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->SisterWeaponType=fuglyname(sc.String);
}

//==========================================================================
//
//==========================================================================
static void WeaponUpSound (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->UpSound = sc.String;
}

//==========================================================================
//
//==========================================================================
static void WeaponYAdjust (FScanner &sc, AWeapon *defaults, Baggage &bag)
{
	sc.MustGetFloat();
	defaults->YAdjust=fixed_t(sc.Float * FRACUNIT);
}

//==========================================================================
//
//==========================================================================
static void WPieceValue (FScanner &sc, AWeaponPiece *defaults, Baggage &bag)
{
	sc.MustGetNumber();
	defaults->PieceValue = 1 << (sc.Number-1);
}

//==========================================================================
//
//==========================================================================
static void WPieceWeapon (FScanner &sc, AWeaponPiece *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->WeaponClass = fuglyname(sc.String);
}

//==========================================================================
//
//==========================================================================
static void PowerupColor (FScanner &sc, APowerupGiver *defaults, Baggage &bag)
{
	int r;
	int g;
	int b;
	int alpha;
	PalEntry * pBlendColor;

	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		pBlendColor = &((APowerup*)defaults)->BlendColor;
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)))
	{
		pBlendColor = &((APowerupGiver*)defaults)->BlendColor;
	}
	else
	{
		sc.ScriptError("\"%s\" requires an actor of type \"Powerup\"\n", sc.String);
	}

	if (sc.CheckNumber())
	{
		r=clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		g=clamp<int>(sc.Number, 0, 255);
		sc.CheckString(",");
		sc.MustGetNumber();
		b=clamp<int>(sc.Number, 0, 255);
	}
	else
	{
		sc.MustGetString();

		if (sc.Compare("INVERSEMAP"))
		{
			defaults->BlendColor = INVERSECOLOR;
			return;
		}
		else if (sc.Compare("GOLDMAP"))
		{
			defaults->BlendColor = GOLDCOLOR;
			return;
		}
		// [BC] Yay, more hacks.
		else if ( sc.Compare( "REDMAP" ))
		{
			defaults->BlendColor = REDCOLOR;
			return;
		}
		else if ( sc.Compare( "GREENMAP" ))
		{
			defaults->BlendColor = GREENCOLOR;
			return;
		}

		int c = V_GetColor(NULL, sc.String);
		r=RPART(c);
		g=GPART(c);
		b=BPART(c);
	}
	sc.CheckString(",");
	sc.MustGetFloat();
	alpha=int(sc.Float*255);
	alpha=clamp<int>(alpha, 0, 255);
	if (alpha!=0) *pBlendColor = MAKEARGB(alpha, r, g, b);
	else *pBlendColor = 0;
}

//==========================================================================
//
//==========================================================================
static void PowerupDuration (FScanner &sc, APowerupGiver *defaults, Baggage &bag)
{
	int *pEffectTics;

	if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(APowerup)))
	{
		pEffectTics = &((APowerup*)defaults)->EffectTics;
	}
	else if (bag.Info->Class->IsDescendantOf(RUNTIME_CLASS(APowerupGiver)))
	{
		pEffectTics = &((APowerupGiver*)defaults)->EffectTics;
	}
	else
	{
		sc.ScriptError("\"%s\" requires an actor of type \"Powerup\"\n", sc.String);
	}

	sc.MustGetNumber();
	*pEffectTics = sc.Number>=0? sc.Number : -sc.Number*TICRATE;
}

//==========================================================================
//
//==========================================================================
static void PowerupMode (FScanner &sc, APowerupGiver *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->mode = (FName)sc.String;
}

//==========================================================================
//
//==========================================================================
static void PowerupType (FScanner &sc, APowerupGiver *defaults, Baggage &bag)
{
	sc.MustGetString();
	defaults->PowerupType = fuglyname(sc.String);
}

//==========================================================================
//
// [GRB] Special player properties
//
//==========================================================================

//==========================================================================
//
//==========================================================================
static void PlayerDisplayName (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	bag.Info->Class->Meta.SetMetaString (APMETA_DisplayName, sc.String);
}

//==========================================================================
//
//==========================================================================
static void PlayerSoundClass (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	FString tmp;

	sc.MustGetString ();
	tmp = sc.String;
	tmp.ReplaceChars (' ', '_');
	bag.Info->Class->Meta.SetMetaString (APMETA_SoundClass, tmp);
}

//==========================================================================
//
//==========================================================================
static void PlayerFace (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	FString tmp;

	sc.MustGetString ();
	tmp = sc.String;
	if (tmp.Len() != 3)
	{
		Printf("Invalid face '%s' for '%s';\nSTF replacement codes must be 3 characters.\n",
			sc.String, bag.Info->Class->TypeName.GetChars ());
	}

	tmp.ToUpper();
	bool valid = (
		(((tmp[0] >= 'A') && (tmp[0] <= 'Z')) || ((tmp[0] >= '0') && (tmp[0] <= '9'))) &&
		(((tmp[1] >= 'A') && (tmp[1] <= 'Z')) || ((tmp[1] >= '0') && (tmp[1] <= '9'))) &&
		(((tmp[2] >= 'A') && (tmp[2] <= 'Z')) || ((tmp[2] >= '0') && (tmp[2] <= '9')))
		);
	if (!valid)
	{
		Printf("Invalid face '%s' for '%s';\nSTF replacement codes must be alphanumeric.\n",
			sc.String, bag.Info->Class->TypeName.GetChars ());
	}
	
	bag.Info->Class->Meta.SetMetaString (APMETA_Face, tmp);
}

//==========================================================================
//
//==========================================================================
static void PlayerColorRange (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	int start, end;

	sc.MustGetNumber ();
	start = sc.Number;
	sc.CheckString(",");
	sc.MustGetNumber ();
	end = sc.Number;

	if (start > end)
		swap (start, end);

	bag.Info->Class->Meta.SetMetaInt (APMETA_ColorRange, (start & 255) | ((end & 255) << 8));
}

//==========================================================================
//
//==========================================================================
static void PlayerAttackZOffset (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->AttackZOffset = FLOAT2FIXED (sc.Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerJumpZ (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->JumpZ = FLOAT2FIXED (sc.Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSpawnClass (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	if (sc.Compare ("Any"))
		defaults->SpawnMask = 0;
	else if (sc.Compare ("Fighter"))
		defaults->SpawnMask |= 1;
	else if (sc.Compare ("Cleric"))
		defaults->SpawnMask |= 2;
	else if (sc.Compare ("Mage"))
		defaults->SpawnMask |= 4;
	else if (IsNum(sc.String))
	{
		int val = strtol(sc.String, NULL, 0);
		if (val > 0) defaults->SpawnMask |= 1<<(val-1);
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerViewHeight (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->ViewHeight = FLOAT2FIXED (sc.Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerForwardMove (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->ForwardMove1 = defaults->ForwardMove2 = FLOAT2FIXED (sc.Float);
	if (CheckFloatParm (sc))
		defaults->ForwardMove2 = FLOAT2FIXED (sc.Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerSideMove (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetFloat ();
	defaults->SideMove1 = defaults->SideMove2 = FLOAT2FIXED (sc.Float);
	if (CheckFloatParm (sc))
		defaults->SideMove2 = FLOAT2FIXED (sc.Float);
}

//==========================================================================
//
//==========================================================================
static void PlayerMaxHealth (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->MaxHealth = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void PlayerRunHealth (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->RunHealth = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void PlayerMorphWeapon (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->MorphWeapon = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void PlayerScoreIcon (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->ScoreIcon = TexMan.AddPatch (sc.String);
	if (!defaults->ScoreIcon.isValid())
	{
		defaults->ScoreIcon = TexMan.AddPatch (sc.String, ns_sprites);
		if (!defaults->ScoreIcon.isValid())
		{
			Printf("Icon '%s' for '%s' not found\n", sc.String, bag.Info->Class->TypeName.GetChars ());
		}
	}
}

//==========================================================================
//
//==========================================================================
static void PlayerCrouchSprite (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	for (int i = 0; i < sc.StringLen; i++)
	{
		sc.String[i] = toupper (sc.String[i]);
	}
	defaults->crouchsprite = GetSpriteIndex (sc.String);
}

//==========================================================================
//
//==========================================================================
static void PlayerDmgScreenColor (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	defaults->HasDamageFade = true;

	if (sc.CheckNumber ())
	{
		sc.MustGetNumber ();
		defaults->RedDamageFade = clamp <float> (sc.Number, 0, 255);
		sc.CheckString (",");
		sc.MustGetNumber ();
		defaults->GreenDamageFade = clamp <float> (sc.Number, 0, 255);
		sc.CheckString (",");
		sc.MustGetNumber ();
		defaults->BlueDamageFade = clamp <float> (sc.Number, 0, 255);
	}
	else
	{
		sc.MustGetString ();
		int c = V_GetColor (NULL, sc.String);
		defaults->RedDamageFade = RPART (c);
		defaults->GreenDamageFade = GPART (c);
		defaults->BlueDamageFade = BPART (c);
	}
}

//==========================================================================
//
// [GRB] Store start items in drop item list
//
//==========================================================================
static void PlayerStartItem (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	// create a linked list of dropitems
	if (!bag.DropItemSet)
	{
		bag.DropItemSet = true;
		bag.DropItemList = NULL;
	}

	FDropItem * di=new FDropItem;

	sc.MustGetString();
	di->Name = sc.String;
	di->probability = 255;
	di->amount = 1;
	if (CheckNumParm(sc))
	{
		di->amount = sc.Number;
	}
	di->Next = bag.DropItemList;
	bag.DropItemList = di;
}

//==========================================================================
//
//==========================================================================
static void PlayerInvulMode (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	bag.Info->Class->Meta.SetMetaInt (APMETA_InvulMode, (FName)sc.String);
}

//==========================================================================
//
//==========================================================================
static void PlayerHealRadius (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	sc.MustGetString ();
	bag.Info->Class->Meta.SetMetaInt (APMETA_HealingRadius, (FName)sc.String);
}

//==========================================================================
//
//==========================================================================
static void PlayerHexenArmor (FScanner &sc, APlayerPawn *defaults, Baggage &bag)
{
	for (int i=0;i<5;i++)
	{
		sc.MustGetFloat ();
		bag.Info->Class->Meta.SetMetaFixed (APMETA_Hexenarmor0+i, FLOAT2FIXED (sc.Float));
		if (i!=4) sc.MustGetStringName(",");
	}
}

//==========================================================================
//
//==========================================================================
static void EggFXPlayerClass (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->PlayerClass = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void EggFXMonsterClass (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->MonsterClass = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void EggFXDuration (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	sc.MustGetNumber ();
	defaults->Duration = sc.Number;
}

//==========================================================================
//
//==========================================================================
static void EggFXMorphStyle (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	defaults->MorphStyle = ParseMorphStyle(sc);
}

//==========================================================================
//
//==========================================================================
static void EggFXMorphFlash (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->MorphFlash = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void EggFXUnMorphFlash (FScanner &sc, AMorphProjectile *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->UnMorphFlash = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void PowerMorphPlayerClass (FScanner &sc, APowerMorph *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->PlayerClass = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void PowerMorphMorphStyle (FScanner &sc, APowerMorph *defaults, Baggage &bag)
{
	defaults->MorphStyle = ParseMorphStyle(sc);
}

//==========================================================================
//
//==========================================================================
static void PowerMorphMorphFlash (FScanner &sc, APowerMorph *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->MorphFlash = FName(sc.String);
}

//==========================================================================
//
//==========================================================================
static void PowerMorphUnMorphFlash (FScanner &sc, APowerMorph *defaults, Baggage &bag)
{
	sc.MustGetString ();
	defaults->UnMorphFlash = FName(sc.String);
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
	{ "activesound",					ActorActiveSound,				RUNTIME_CLASS(AActor) },
	{ "alpha",							ActorAlpha,						RUNTIME_CLASS(AActor) },
	{ "ammo.backpackamount",			(apf)AmmoBackpackAmount,		RUNTIME_CLASS(AAmmo) },
	{ "ammo.backpackmaxamount",			(apf)AmmoBackpackMaxAmount,		RUNTIME_CLASS(AAmmo) },
	{ "ammo.dropamount",				(apf)AmmoDropAmount,			RUNTIME_CLASS(AAmmo) },
	{ "args",							ActorArgs,						RUNTIME_CLASS(AActor) },
	{ "armor.maxbonus",					(apf)ArmorMaxBonus,				RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.maxbonusmax",				(apf)ArmorMaxBonusMax,			RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.maxsaveamount",			(apf)ArmorMaxSaveAmount,		RUNTIME_CLASS(ABasicArmorBonus) },
	{ "armor.saveamount",				(apf)ArmorSaveAmount,			RUNTIME_CLASS(AActor) },
	{ "armor.savepercent",				(apf)ArmorSavePercent,			RUNTIME_CLASS(AActor) },
	{ "attacksound",					ActorAttackSound,				RUNTIME_CLASS(AActor) },
	{ "bloodcolor",						ActorBloodColor,				RUNTIME_CLASS(AActor) },
	{ "bloodtype",						ActorBloodType,					RUNTIME_CLASS(AActor) },
	{ "bouncecount",					ActorBounceCount,				RUNTIME_CLASS(AActor) },
	{ "bouncefactor",					ActorBounceFactor,				RUNTIME_CLASS(AActor) },
	{ "burn",							ActorBurnState,					RUNTIME_CLASS(AActor) },
	{ "burnheight",						ActorBurnHeight,				RUNTIME_CLASS(AActor) },
	{ "cameraheight",					ActorCameraheight,				RUNTIME_CLASS(AActor) },
	{ "clearflags",						ActorClearFlags,				RUNTIME_CLASS(AActor) },
	{ "conversationid",					ActorConversationID,			RUNTIME_CLASS(AActor) },
	{ "crash",							ActorCrashState,				RUNTIME_CLASS(AActor) },
	{ "crush",							ActorCrushState,				RUNTIME_CLASS(AActor) },
	{ "damage",							ActorDamage,					RUNTIME_CLASS(AActor) },
	{ "damagefactor",					ActorDamageFactor,				RUNTIME_CLASS(AActor) },
	{ "damagetype",						ActorDamageType,				RUNTIME_CLASS(AActor) },
	{ "death",							ActorDeathState,				RUNTIME_CLASS(AActor) },
	{ "deathheight",					ActorDeathHeight,				RUNTIME_CLASS(AActor) },
	{ "deathsound",						ActorDeathSound,				RUNTIME_CLASS(AActor) },
	{ "decal",							ActorDecal,						RUNTIME_CLASS(AActor) },
	{ "disintegrate",					ActorDisintegrateState,			RUNTIME_CLASS(AActor) },
	{ "donthurtshooter",				ActorDontHurtShooter,			RUNTIME_CLASS(AActor) },
	{ "dropitem",						ActorDropItem,					RUNTIME_CLASS(AActor) },
	{ "explosiondamage",				ActorExplosionDamage,			RUNTIME_CLASS(AActor) },
	{ "explosionradius",				ActorExplosionRadius,			RUNTIME_CLASS(AActor) },
	{ "fastspeed",						ActorFastSpeed,					RUNTIME_CLASS(AActor) },
	{ "floatspeed",						ActorFloatSpeed,				RUNTIME_CLASS(AActor) },
	{ "game",							ActorGame,						RUNTIME_CLASS(AActor) },
	{ "gibhealth",						ActorGibHealth,					RUNTIME_CLASS(AActor) },
	{ "gravity",						ActorGravity,					RUNTIME_CLASS(AActor) },
	{ "heal",							ActorHealState,					RUNTIME_CLASS(AActor) },
	{ "health",							ActorHealth,					RUNTIME_CLASS(AActor) },
	{ "health.lowmessage",				(apf)HealthLowMessage,			RUNTIME_CLASS(AHealth) },
	{ "height",							ActorHeight,					RUNTIME_CLASS(AActor) },
	{ "hitobituary",					ActorHitObituary,				RUNTIME_CLASS(AActor) },
	{ "howlsound",						ActorHowlSound,					RUNTIME_CLASS(AActor) },
	{ "ice",							ActorIceState,					RUNTIME_CLASS(AActor) },
	{ "inventory.amount",				(apf)InventoryAmount,			RUNTIME_CLASS(AInventory) },
	{ "inventory.defmaxamount",			(apf)InventoryDefMaxAmount,		RUNTIME_CLASS(AInventory) },
	{ "inventory.givequest",			(apf)InventoryGiveQuest,		RUNTIME_CLASS(AInventory) },
	{ "inventory.icon",					(apf)InventoryIcon,				RUNTIME_CLASS(AInventory) },
	{ "inventory.maxamount",			(apf)InventoryMaxAmount,		RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupflash",			(apf)InventoryPickupflash,		RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupmessage",		(apf)InventoryPickupmsg,		RUNTIME_CLASS(AInventory) },
	{ "inventory.pickupsound",			(apf)InventoryPickupsound,		RUNTIME_CLASS(AInventory) },
	{ "inventory.respawntics",			(apf)InventoryRespawntics,		RUNTIME_CLASS(AInventory) },
	{ "inventory.usesound",				(apf)InventoryUsesound,			RUNTIME_CLASS(AInventory) },
	{ "mass",							ActorMass,						RUNTIME_CLASS(AActor) },
	{ "maxdropoffheight",				ActorMaxDropoffHeight,			RUNTIME_CLASS(AActor) },
	{ "maxstepheight",					ActorMaxStepHeight,				RUNTIME_CLASS(AActor) },
	{ "maxtargetrange",					ActorMaxTargetRange,			RUNTIME_CLASS(AActor) },
	{ "melee",							ActorMeleeState,				RUNTIME_CLASS(AActor) },
	{ "meleedamage",					ActorMeleeDamage,				RUNTIME_CLASS(AActor) },
	{ "meleerange",						ActorMeleeRange,				RUNTIME_CLASS(AActor) },
	{ "meleesound",						ActorMeleeSound,				RUNTIME_CLASS(AActor) },
	{ "meleethreshold",					ActorMeleeThreshold,			RUNTIME_CLASS(AActor) },
	{ "minmissilechance",				ActorMinMissileChance,			RUNTIME_CLASS(AActor) },
	{ "missile",						ActorMissileState,				RUNTIME_CLASS(AActor) },
	{ "missileheight",					ActorMissileHeight,				RUNTIME_CLASS(AActor) },
	{ "missiletype",					ActorMissileType,				RUNTIME_CLASS(AActor) },
	{ "monster",						ActorMonster,					RUNTIME_CLASS(AActor) },
	{ "morphprojectile.duration",		(apf)EggFXDuration,				RUNTIME_CLASS(AMorphProjectile) },
 	{ "morphprojectile.monsterclass",	(apf)EggFXMonsterClass,			RUNTIME_CLASS(AMorphProjectile) },
	{ "morphprojectile.morphflash",		(apf)EggFXMorphFlash,			RUNTIME_CLASS(AMorphProjectile) },
	{ "morphprojectile.morphstyle",		(apf)EggFXMorphStyle,			RUNTIME_CLASS(AMorphProjectile) },
 	{ "morphprojectile.playerclass",	(apf)EggFXPlayerClass,			RUNTIME_CLASS(AMorphProjectile) },
	{ "morphprojectile.unmorphflash",	(apf)EggFXUnMorphFlash,			RUNTIME_CLASS(AMorphProjectile) },
	{ "obituary",						ActorObituary,					RUNTIME_CLASS(AActor) },
	{ "pain",							ActorPainState,					RUNTIME_CLASS(AActor) },
	{ "painchance",						ActorPainChance,				RUNTIME_CLASS(AActor) },
	{ "painsound",						ActorPainSound,					RUNTIME_CLASS(AActor) },
	{ "player.attackzoffset",			(apf)PlayerAttackZOffset,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.colorrange",				(apf)PlayerColorRange,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.crouchsprite",			(apf)PlayerCrouchSprite,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.damagescreencolor",		(apf)PlayerDmgScreenColor,		RUNTIME_CLASS(APlayerPawn) },
	{ "player.displayname",				(apf)PlayerDisplayName,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.face",					(apf)PlayerFace,				RUNTIME_CLASS(APlayerPawn) },
	{ "player.forwardmove",				(apf)PlayerForwardMove,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.healradiustype",			(apf)PlayerHealRadius,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.hexenarmor",				(apf)PlayerHexenArmor,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.invulnerabilitymode",		(apf)PlayerInvulMode,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.jumpz",					(apf)PlayerJumpZ,				RUNTIME_CLASS(APlayerPawn) },
	{ "player.maxhealth",				(apf)PlayerMaxHealth,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.morphweapon",				(apf)PlayerMorphWeapon,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.runhealth",				(apf)PlayerRunHealth,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.scoreicon",				(apf)PlayerScoreIcon,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.sidemove",				(apf)PlayerSideMove,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.soundclass",				(apf)PlayerSoundClass,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.spawnclass",				(apf)PlayerSpawnClass,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.startitem",				(apf)PlayerStartItem,			RUNTIME_CLASS(APlayerPawn) },
	{ "player.viewheight",				(apf)PlayerViewHeight,			RUNTIME_CLASS(APlayerPawn) },
	{ "poisondamage",					ActorPoisonDamage,				RUNTIME_CLASS(AActor) },
	{ "powermorph.morphflash",			(apf)PowerMorphMorphFlash,		RUNTIME_CLASS(APowerMorph) },
	{ "powermorph.morphstyle",			(apf)PowerMorphMorphStyle,		RUNTIME_CLASS(APowerMorph) },
	{ "powermorph.playerclass",			(apf)PowerMorphPlayerClass,		RUNTIME_CLASS(APowerMorph) },
	{ "powermorph.unmorphflash",		(apf)PowerMorphUnMorphFlash,	RUNTIME_CLASS(APowerMorph) },
	{ "powerup.color",					(apf)PowerupColor,				RUNTIME_CLASS(AInventory) },
	{ "powerup.duration",				(apf)PowerupDuration,			RUNTIME_CLASS(AInventory) },
	{ "powerup.mode",					(apf)PowerupMode,				RUNTIME_CLASS(APowerupGiver) },
	{ "powerup.type",					(apf)PowerupType,				RUNTIME_CLASS(APowerupGiver) },
	{ "projectile",						ActorProjectile,				RUNTIME_CLASS(AActor) },
	{ "puzzleitem.failmessage",			(apf)PuzzleitemFailMsg,			RUNTIME_CLASS(APuzzleItem) },
	{ "puzzleitem.number",				(apf)PuzzleitemNumber,			RUNTIME_CLASS(APuzzleItem) },
	{ "radius",							ActorRadius,					RUNTIME_CLASS(AActor) },
	{ "radiusdamagefactor",				ActorRadiusDamageFactor,		RUNTIME_CLASS(AActor) },
	{ "raise",							ActorRaiseState,				RUNTIME_CLASS(AActor) },
	{ "reactiontime",					ActorReactionTime,				RUNTIME_CLASS(AActor) },
	{ "renderstyle",					ActorRenderStyle,				RUNTIME_CLASS(AActor) },
	{ "scale",							ActorScale,						RUNTIME_CLASS(AActor) },
	{ "see",							ActorSeeState,					RUNTIME_CLASS(AActor) },
	{ "seesound",						ActorSeeSound,					RUNTIME_CLASS(AActor) },
	{ "skip_super",						ActorSkipSuper,					RUNTIME_CLASS(AActor) },
	{ "spawn",							ActorSpawnState,				RUNTIME_CLASS(AActor) },
	{ "spawnid",						ActorSpawnID,					RUNTIME_CLASS(AActor) },
	{ "speed",							ActorSpeed,						RUNTIME_CLASS(AActor) },
	{ "states",							ActorStates,					RUNTIME_CLASS(AActor) },
	{ "stencilcolor",					ActorStencilColor,				RUNTIME_CLASS(AActor) },
	{ "tag",							ActorTag,						RUNTIME_CLASS(AActor) },
	{ "translation",					ActorTranslation,				RUNTIME_CLASS(AActor) },
	{ "vspeed",							ActorVSpeed,					RUNTIME_CLASS(AActor) },
	{ "wallbouncefactor",				ActorWallBounceFactor,			RUNTIME_CLASS(AActor) },
	{ "weapon.ammogive",				(apf)WeaponAmmoGive1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive1",				(apf)WeaponAmmoGive1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammogive2",				(apf)WeaponAmmoGive2,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype",				(apf)WeaponAmmoType1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype1",				(apf)WeaponAmmoType1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammotype2",				(apf)WeaponAmmoType2,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse",					(apf)WeaponAmmoUse1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse1",				(apf)WeaponAmmoUse1,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.ammouse2",				(apf)WeaponAmmoUse2,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.defaultkickback",			(apf)WeaponDefKickback,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.kickback",				(apf)WeaponKickback,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.readysound",				(apf)WeaponReadySound,			RUNTIME_CLASS(AWeapon) },
	{ "weapon.selectionorder",			(apf)WeaponSelectionOrder,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.sisterweapon",			(apf)WeaponSisterWeapon,		RUNTIME_CLASS(AWeapon) },
	{ "weapon.upsound",					(apf)WeaponUpSound,				RUNTIME_CLASS(AWeapon) },
	{ "weapon.yadjust",					(apf)WeaponYAdjust,				RUNTIME_CLASS(AWeapon) },
	{ "weaponpiece.number",				(apf)WPieceValue,				RUNTIME_CLASS(AWeaponPiece) },
	{ "weaponpiece.weapon",				(apf)WPieceWeapon,				RUNTIME_CLASS(AWeaponPiece) },
	{ "wound",							ActorWoundState,				RUNTIME_CLASS(AActor) },
	{ "woundhealth",					ActorWoundHealth,				RUNTIME_CLASS(AActor) },
	{ "xdeath",							ActorXDeathState,				RUNTIME_CLASS(AActor) },
	{ "xscale",							ActorXScale,					RUNTIME_CLASS(AActor) },
	{ "yscale",							ActorYScale,					RUNTIME_CLASS(AActor) },
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

void ParseActorProperty(FScanner &sc, Baggage &bag)
{
	strlwr (sc.String);

	FString propname = sc.String;

	if (sc.CheckString ("."))
	{
		sc.MustGetString ();
		propname += '.';
		strlwr (sc.String);
		propname += sc.String;
	}
	else
	{
		sc.UnGet ();
	}

	const ActorProps *prop = is_actorprop (propname.GetChars());

	if (prop != NULL)
	{
		if (!bag.Info->Class->IsDescendantOf(prop->type))
		{
			sc.ScriptError("\"%s\" requires an actor of type \"%s\"\n", propname.GetChars(), prop->type->TypeName.GetChars());
		}
		else
		{
			prop->Handler (sc, (AActor *)bag.Info->Class->Defaults, bag);
		}
	}
	else
	{
		sc.ScriptError("\"%s\" is an unknown actor property\n", propname.GetChars());
	}
}


//==========================================================================
//
// Finalizes an actor definition
//
//==========================================================================

void FinishActor(FScanner &sc, FActorInfo *info, Baggage &bag)
{
	AActor *defaults = (AActor*)info->Class->Defaults;

	FinishStates (sc, info, defaults, bag);
	InstallStates (info, defaults);
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
