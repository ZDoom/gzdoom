/*
** thingdef.cpp
**
** Actor definitions
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


const PClass *QuestItemClasses[31];


extern TArray<FActorInfo *> Decorations;

// allow decal specifications in DECORATE. Decals are loaded after DECORATE so the names must be stored here.
TArray<char*> DecalNames;
// all state parameters
TArray<int> StateParameters;
TArray<FName> JumpParameters;

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1 }
#define DEFINE_FLAG2(symbol, name, type, variable) { symbol, #name, (int)(size_t)&((type*)1)->variable - 1 }

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
	DEFINE_FLAG(MF4, LONGMELEERANGE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEMORE, AActor, flags4),
	DEFINE_FLAG(MF4, MISSILEEVENMORE, AActor, flags4),
	DEFINE_FLAG(MF4, SHORTMISSILERANGE, AActor, flags4),
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

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
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
		int * flagp = (int*) (((char*)self) + fd->structoffset);

		if (expression) *flagp |= fd->flagbit;
		else *flagp &= ~fd->flagbit;
	}
	else
	{
		Printf("Unknown flag '%s' in '%s'\n", flagname, self->GetClass()->TypeName.GetChars());
	}
}


//==========================================================================
//
// Action functions
//
//==========================================================================


#include "thingdef_specials.h"

struct AFuncDesc
{
	const char *Name;
	actionf_p Function;
};


#define FROM_THINGDEF
// Prototype the code pointers
#define WEAPON(x)	void A_##x(AActor*);	
#define ACTOR(x)	void A_##x(AActor*);
#include "codepointers.h"
#include "d_dehackedactions.h"
void A_ComboAttack(AActor*);
void A_ExplodeParms(AActor*);

AFuncDesc AFTable[] =
{
#define WEAPON(x)	{ "A_" #x, A_##x },
#define ACTOR(x)	{ "A_" #x, A_##x },
#include "codepointers.h"
#include "d_dehackedactions.h"
	{ "A_Fall", A_NoBlocking },
	{ "A_BasicAttack", A_ComboAttack },
	{ "A_Explode", A_ExplodeParms }
};


//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================
static int STACK_ARGS funccmp(const void * a, const void * b)
{
	return stricmp( ((AFuncDesc*)a)->Name, ((AFuncDesc*)b)->Name);
}

static AFuncDesc * FindFunction(const char * string)
{
	static bool funcsorted=false;

	if (!funcsorted) 
	{
		qsort(AFTable, countof(AFTable), sizeof(AFTable[0]), funccmp);
		funcsorted=true;
	}

	int min = 0, max = countof(AFTable)-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, AFTable[mid].Name);
		if (lexval == 0)
		{
			return &AFTable[mid];
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


static const char *BasicAttackNames[4] =
{
	"A_MeleeAttack",
	"A_MissileAttack",
	"A_ComboAttack",
	NULL
};
static const actionf_p BasicAttacks[3] =
{
	A_MeleeAttack,
	A_MissileAttack,
	A_ComboAttack
};



//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// Translation parsing
//
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
static int NumUsedTranslations;
static int NumUsedBloodTranslations;
BYTE decorate_translations[256*256*2];
PalEntry BloodTranslations[256];

void InitDecorateTranslations()
{
	// The translation tables haven't been allocated yet so we may as easily use a static buffer instead!
	NumUsedBloodTranslations = NumUsedTranslations = 0;
	for(int i=0;i<256*256*2;i++) decorate_translations[i]=i&255;
}

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


static void AddToTranslation(unsigned char * translation, char * range)
{
	int start,end;

	start=strtol(range, &range, 10);
	if (!Check(range, ':')) return;
	end=strtol(range, &range, 10);
	if (!Check(range, '=')) return;
	if (!Check(range, '[', false))
	{
		int pal1,pal2;
		fixed_t palcol, palstep;

		pal1=strtol(range, &range, 10);
		if (!Check(range, ':')) return;
		pal2=strtol(range, &range, 10);

		if (start > end)
		{
			swap (start, end);
			swap (pal1, pal2);
		}
		else if (start == end)
		{
			translation[start] = pal1;
			return;
		}
		palcol = pal1 << FRACBITS;
		palstep = ((pal2 << FRACBITS) - palcol) / (end - start);
		for (int i = start; i <= end; palcol += palstep, ++i)
		{
			translation[i] = palcol >> FRACBITS;
		}
	}
	else
	{ 
		// translation using RGB values
		int r1;
		int g1;
		int b1;
		int r2;
		int g2;
		int b2;
		int r,g,b;
		int rs,gs,bs;

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
			return;
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
}

static int StoreTranslation(const unsigned char * translation)
{
	for (int i = 0; i < NumUsedTranslations; i++)
	{
		if (!memcmp(translation, decorate_translations + i*256, 256))
		{
			// A duplicate of this translation already exists
			return TRANSLATION(TRANSLATION_Decorate, i);
		}
	}
	if (NumUsedTranslations>=MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many translations in DECORATE");
	}
	memcpy(decorate_translations + NumUsedTranslations*256, translation, 256);
	NumUsedTranslations++;
	return TRANSLATION(TRANSLATION_Decorate, NumUsedTranslations-1);
}

static int CreateBloodTranslation(PalEntry color)
{
	int i;

	for (i = 0; i < NumUsedBloodTranslations; i++)
	{
		if (color.r == BloodTranslations[i].r &&
			color.g == BloodTranslations[i].g &&
			color.b == BloodTranslations[i].b)
		{
			// A duplicate of this translation already exists
			return i;
		}
	}
	if (NumUsedBloodTranslations>=MAX_DECORATE_TRANSLATIONS)
	{
		SC_ScriptError("Too many blood colors in DECORATE");
	}
	for (i = 0; i < 256; i++)
	{
		int bright = MAX(MAX(GPalette.BaseColors[i].r, GPalette.BaseColors[i].g), GPalette.BaseColors[i].b);
		int entry = ColorMatcher.Pick(color.r*bright/255, color.g*bright/255, color.b*bright/255);

		*(decorate_translations + MAX_DECORATE_TRANSLATIONS*256 + NumUsedBloodTranslations*256 + i)=entry;
	}
	BloodTranslations[NumUsedBloodTranslations]=color;
	return NumUsedBloodTranslations++;
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
// Extra info maintained while defining an actor. The original
// implementation stored these in a CustomActor class. They have all been
// moved into action function parameters so that no special CustomActor
// class is necessary.
//
//==========================================================================

struct FBasicAttack
{
	int MeleeDamage;
	int MeleeSound;
	FName MissileName;
	fixed_t MissileHeight;
};

struct Baggage
{
	FActorInfo *Info;
	bool DropItemSet;
	bool StateSet;
	int CurrentState;

	FDropItem *DropItemList;
	FBasicAttack BAttack;
};


//==========================================================================
//
//
//==========================================================================

static TArray<FState> StateArray;



typedef void (*ActorPropFunction) (AActor *defaults, Baggage &bag);

struct ActorProps { const char *name; ActorPropFunction Handler; const PClass * type; };

typedef ActorProps (*ActorPropHandler) (register const char *str, register unsigned int len);

static const ActorProps *is_actorprop (const char *str);

//==========================================================================
//
// Find a state address
//
//==========================================================================

struct FStateDefine
{
	FName Label;
	TArray<FStateDefine> Children;
	FState *State;
};

static TArray<FStateDefine> StateLabels;

void ClearStateLabels()
{
	StateLabels.Clear();
}

//==========================================================================
//
// Search one list of state definitions for the given name
//
//==========================================================================

static FStateDefine * FindStateLabelInList(TArray<FStateDefine> & list, FName name, bool create)
{
	for(unsigned i = 0; i<list.Size(); i++)
	{
		if (list[i].Label == name) return &list[i];
	}
	if (create)
	{
		FStateDefine def;
		def.Label=name;
		def.State=NULL;
		return &list[list.Push(def)];
	}
	return NULL;
}

//==========================================================================
//
// Creates a list of names from a string. Dots are used as separator
//
//==========================================================================

void MakeStateNameList(const char * fname, TArray<FName> * out)
{
	FName firstpart, secondpart;
	char * c;

	// Handle the old names for the existing death states
	char * name = copystring(fname);
	firstpart = strtok(name, ".");
	switch (firstpart)
	{
	case NAME_Burn:
		firstpart = NAME_Death;
		secondpart = NAME_Fire;
		break;
	case NAME_Ice:
		firstpart = NAME_Death;
		secondpart = NAME_Ice;
		break;
	case NAME_Disintegrate:
		firstpart = NAME_Death;
		secondpart = NAME_Disintegrate;
		break;
	case NAME_XDeath:
		firstpart = NAME_Death;
		secondpart = NAME_Extreme;
		break;
	}

	out->Clear();
	out->Push(firstpart);
	if (secondpart!=NAME_None) out->Push(secondpart);

	while ((c = strtok(NULL, "."))!=NULL)
	{
		FName cc = c;
		out->Push(cc);
	}
	delete [] name;
}

//==========================================================================
//
// Finds the address of a state given by name. 
// Adds the state if it doesn't exist
//
//==========================================================================

static FStateDefine * FindStateAddress(const char * name)
{
	static TArray<FName> namelist(3);
	FStateDefine * statedef=NULL;

	MakeStateNameList(name, &namelist);

	TArray<FStateDefine> * statelist = &StateLabels;
	for(unsigned i=0;i<namelist.Size();i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], true);
		statelist = &statedef->Children;
	}
	return statedef;
}

void AddState (const char * statename, FState * state)
{
	FStateDefine * std = FindStateAddress(statename);
	std->State = state;
}

//==========================================================================
//
// Finds the state associated with the given name
//
//==========================================================================

FState * FindState(AActor * actor, const PClass * type, const char * name)
{
	static TArray<FName> namelist(3);
	FStateDefine * statedef=NULL;

	MakeStateNameList(name, &namelist);

	TArray<FStateDefine> * statelist = &StateLabels;
	for(unsigned i=0;i<namelist.Size();i++)
	{
		statedef = FindStateLabelInList(*statelist, namelist[i], false);
		if (statedef == NULL) return NULL;
		statelist = &statedef->Children;
	}
	return statedef? statedef->State : NULL;
}

//==========================================================================
//
// Finds the state associated with the given name
//
//==========================================================================

FState * FindStateInClass(AActor * actor, const PClass * type, const char * name)
{
	static TArray<FName> namelist(3);

	MakeStateNameList(name, &namelist);
	FActorInfo * info = type->ActorInfo;
	if (info) return info->FindState(namelist.Size(), &namelist[0], true);
	return NULL;
}

//==========================================================================
//
// Checks if a state list is empty
// A list is empty if it doesn't contain any states and no children
// that contain any states
//
//==========================================================================

static bool IsStateListEmpty(TArray<FStateDefine> & statelist)
{
	for(unsigned i=0;i<statelist.Size();i++)
	{
		if (statelist[i].State!=NULL || !IsStateListEmpty(statelist[i].Children)) return false;
	}
	return true;
}

//==========================================================================
//
// Creates the final list of states from the state definitions
//
//==========================================================================

static int STACK_ARGS labelcmp(const void * a, const void * b)
{
	FStateLabel * A = (FStateLabel *)a;
	FStateLabel * B = (FStateLabel *)b;
	return ((int)A->Label - (int)B->Label);
}

static FStateLabels * CreateStateLabelList(TArray<FStateDefine> & statelist)
{
	// First delete all empty labels from the list
	for (int i=statelist.Size()-1;i>=0;i--)
	{
		if (statelist[i].Label == NAME_None || (statelist[i].State == NULL && statelist[i].Children.Size() == 0))
		{
			statelist.Delete(i);
		}
	}

	int count=statelist.Size();

	if (count == 0) return NULL;

	FStateLabels * list = (FStateLabels*)M_Malloc(sizeof(FStateLabels)+(count-1)*sizeof(FStateLabel));
	list->NumLabels = count;

	for (int i=0;i<count;i++)
	{
		list->Labels[i].Label = statelist[i].Label;
		list->Labels[i].State = statelist[i].State;
		list->Labels[i].Children = CreateStateLabelList(statelist[i].Children);
	}
	qsort(list->Labels, count, sizeof(FStateLabel), labelcmp);
	return list;
}

//===========================================================================
//
// InstallStates
//
// Creates the actor's state list from the current definition
//
//===========================================================================

void InstallStates(FActorInfo *info, AActor *defaults)
{
	// First ensure we have a valid spawn state.
	FState * state = FindState(defaults, info->Class, "Spawn");

	// Stateless actors that are direct subclasses of AActor
	// have their spawnstate default to something that won't
	// immediately destroy them.
	if (state == &AActor::States[2] && info->Class->ParentClass == RUNTIME_CLASS(AActor))
	{
		AddState("Spawn", &AActor::States[0]);
	}
	else if (state == NULL)
	{
		// A NULL spawn state will crash the engine so set it to something that will make
		// the actor disappear as quickly as possible.
		AddState("Spawn", &AActor::States[2]);
	}

	if (info->StateList != NULL) 
	{
		info->StateList->Destroy();
		free(info->StateList);
	}
	info->StateList = CreateStateLabelList(StateLabels);

	// Cache these states as member veriables.
	defaults->SpawnState = info->FindState(NAME_Spawn);
	defaults->SeeState = info->FindState(NAME_See);
	// Melee and Missile states are manipulated by the scripted marines so they
	// have to be stored locally
	defaults->MeleeState = info->FindState(NAME_Melee);
	defaults->MissileState = info->FindState(NAME_Missile);
}


//===========================================================================
//
// MakeStateDefines
//
// Creates a list of state definitions from an existing actor
// Used by Dehacked to modify an actor's state list
//
//===========================================================================

static void MakeStateList(const FStateLabels *list, TArray<FStateDefine> &dest)
{
	dest.Clear();
	if (list != NULL) for(int i=0;i<list->NumLabels;i++)
	{
		FStateDefine def;

		def.Label = list->Labels[i].Label;
		def.State = list->Labels[i].State;
		dest.Push(def);
		if (list->Labels[i].Children != NULL)
		{
			MakeStateList(list->Labels[i].Children, dest[dest.Size()-1].Children);
		}
	}
}

void MakeStateDefines(const FStateLabels *list)
{
	MakeStateList(list, StateLabels);
}

//==========================================================================
//
// Sets the default values with which an actor definition starts
//
//==========================================================================

static void ResetBaggage (Baggage *bag)
{
	bag->DropItemList = NULL;
	bag->BAttack.MeleeDamage = 0;
	bag->BAttack.MissileHeight = 32*FRACUNIT;
	bag->BAttack.MeleeSound = 0;
	bag->BAttack.MissileName = NAME_None;
	bag->DropItemSet = false;
	bag->CurrentState = 0;
	bag->StateSet = false;
}

static void ResetActor (AActor *actor, Baggage *bag)
{
	memcpy (actor, GetDefault<AActor>(), sizeof(AActor));
	if (bag->DropItemList != NULL)
	{
		FreeDropItemChain (bag->DropItemList);
	}
	ResetBaggage (bag);
}



//==========================================================================
//
// Starts a new actor definition
//
//==========================================================================
static FActorInfo * CreateNewActor(FActorInfo ** parentc, Baggage *bag)
{
	FName typeName;

	// Get actor name
	SC_MustGetString();
	
	char * colon = strchr(sc_String, ':');
	if (colon != NULL)
	{
		*colon++ = 0;
	}

	if (PClass::FindClass (sc_String) != NULL)
	{
		SC_ScriptError ("Actor %s is already defined.", sc_String);
	}

	typeName = sc_String;

	PClass * parent = RUNTIME_CLASS(AActor);
	if (parentc)
	{
		*parentc = NULL;
		
		// Do some tweaking so that a definition like 'Actor:Parent' which is read as a single token is recognized as well
		// without having resort to C-mode (which disallows periods in actor names.)
		if (colon == NULL)
		{
			SC_MustGetString ();
			if (sc_String[0]==':')
			{
				colon = sc_String + 1;
			}
		}
		
		if (colon != NULL)
		{
			if (colon[0] == 0)
			{
				SC_MustGetString ();
				colon = sc_String;
			}
		}
			
		if (colon != NULL)
		{
			parent = const_cast<PClass *> (PClass::FindClass (colon));

			if (parent == NULL)
			{
				SC_ScriptError ("Parent type '%s' not found", colon);
			}
			else if (parent->ActorInfo == NULL)
			{
				SC_ScriptError ("Parent type '%s' is not an actor", colon);
			}
			else
			{
				*parentc = parent->ActorInfo;
			}
		}
		else SC_UnGet();
	}

	PClass * ti = parent->CreateDerivedClass (typeName, parent->Size);
	FActorInfo * info = ti->ActorInfo;

	Decorations.Push (info);
	MakeStateDefines(parent->ActorInfo->StateList);

	ResetBaggage (bag);
	bag->Info = info;

	info->DoomEdNum = -1;

	// Check for "replaces"
	SC_MustGetString ();
	if (SC_Compare ("replaces"))
	{
		const PClass *replacee;

		// Get actor name
		SC_MustGetString ();
		replacee = PClass::FindClass (sc_String);

		if (replacee == NULL)
		{
			SC_ScriptError ("Replaced type '%s' not found", sc_String);
		}
		else if (replacee->ActorInfo == NULL)
		{
			SC_ScriptError ("Replaced type '%s' is not an actor", sc_String);
		}
		replacee->ActorInfo->Replacement = ti->ActorInfo;
		ti->ActorInfo->Replacee = replacee->ActorInfo;
	}
	else
	{
		SC_UnGet();
	}

	// Now, after the actor names have been parsed, it is time to switch to C-mode 
	// for the rest of the actor definition.
	SC_SetCMode (true);
	if (SC_CheckNumber()) 
	{
		if (sc_Number>=-1 && sc_Number<32768) info->DoomEdNum = sc_Number;
		else SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
	}
	if (parent == RUNTIME_CLASS(AWeapon))
	{
		// preinitialize kickback to the default for the game
		((AWeapon*)(info->Class->Defaults))->Kickback=gameinfo.defKickback;
	}

	return info;
}

//==========================================================================
//
// PrepareStateParameters
// creates an empty parameter list for a parameterized function call
//
//==========================================================================
int PrepareStateParameters(FState * state, int numparams)
{
	int paramindex=StateParameters.Size();
	int i, v;

	v=0;
	for(i=0;i<numparams;i++) StateParameters.Push(v);
	state->ParameterIndex = paramindex+1;
	return paramindex;
}

//==========================================================================
//
// Returns the index of the given line special
//
//==========================================================================
int FindLineSpecial(const char * string)
{
	const ACSspecials *spec;

	spec = is_special(string, (unsigned int)strlen(string));
	if (spec) return spec->Special;
	return 0;
}

//==========================================================================
//
// FindLineSpecialEx
//
// Like FindLineSpecial, but also returns the min and max argument count.
//
//==========================================================================

int FindLineSpecialEx (const char *string, int *min_args, int *max_args)
{
	const ACSspecials *spec;

	spec = is_special(string, (unsigned int)strlen(string));
	if (spec != NULL)
	{
		*min_args = spec->MinArgs;
		*max_args = MAX(spec->MinArgs, spec->MaxArgs);
		return spec->Special;
	}
	*min_args = *max_args = 0;
	return 0;
}

//==========================================================================
//
// DoSpecialFunctions
// handles special functions that can't be dealt with by the generic routine
//
//==========================================================================
bool DoSpecialFunctions(FState & state, bool multistate, int * statecount, Baggage &bag)
{
	int i;
	const ACSspecials *spec;

	if ((spec = is_special (sc_String, sc_StringLen)) != NULL)
	{

		int paramindex=PrepareStateParameters(&state, 6);

		StateParameters[paramindex]=spec->Special;

		// Make this consistent with all other parameter parsing
		if (SC_CheckString("("))
		{
			for (i = 0; i < 5;)
			{
				StateParameters[paramindex+i+1]=ParseExpression (false, bag.Info->Class);
				i++;
				if (!SC_CheckString (",")) break;
			}
			SC_MustGetStringName (")");
		}
		else i=0;

		if (i < spec->MinArgs)
		{
			SC_ScriptError ("Too few arguments to %s", spec->name);
		}
		if (i > MAX (spec->MinArgs, spec->MaxArgs))
		{
			SC_ScriptError ("Too many arguments to %s", spec->name);
		}
		state.Action = A_CallSpecial;
		return true;
	}

	// Check for A_MeleeAttack, A_MissileAttack, or A_ComboAttack
	int batk = SC_MatchString (BasicAttackNames);
	if (batk >= 0)
	{
		int paramindex=PrepareStateParameters(&state, 4);

		StateParameters[paramindex] = bag.BAttack.MeleeDamage;
		StateParameters[paramindex+1] = bag.BAttack.MeleeSound;
		StateParameters[paramindex+2] = bag.BAttack.MissileName;
		StateParameters[paramindex+3] = bag.BAttack.MissileHeight;
		state.Action = BasicAttacks[batk];
		return true;
	}
	return false;
}

//==========================================================================
//
// RetargetState(Pointer)s
//
// These functions are used when a goto follows one or more labels.
// Because multiple labels are permitted to occur consecutively with no
// intervening states, it is not enough to remember the last label defined
// and adjust it. So these functions search for all labels that point to
// the current position in the state array and give them a copy of the
// target string instead.
//
//==========================================================================

static void RetargetStatePointers (intptr_t count, const char *target, TArray<FStateDefine> & statelist)
{
	for(unsigned i = 0;i<statelist.Size(); i++)
	{
		if (statelist[i].State == (FState*)count)
		{
			statelist[i].State = target == NULL ? NULL : (FState *)copystring (target);
		}
		if (statelist[i].Children.Size() > 0)
		{
			RetargetStatePointers(count, target, statelist[i].Children);
		}
	}
}

static void RetargetStates (intptr_t count, const char *target)
{
	RetargetStatePointers(count, target, StateLabels);
}

//==========================================================================
//
// Reads a state label that may contain '.'s.
// processes a state block
//
//==========================================================================
static FString ParseStateString()
{
	FString statestring;

	SC_MustGetString();
	statestring = sc_String;
	if (SC_CheckString("::"))
	{
		SC_MustGetString ();
		statestring += "::";
		statestring += sc_String;
	}
	while (SC_CheckString ("."))
	{
		SC_MustGetString ();
		statestring += ".";
		statestring += sc_String;
	}
	return statestring;
}

//==========================================================================
//
// ProcessStates
// processes a state block
//
//==========================================================================
static int ProcessStates(FActorInfo * actor, AActor * defaults, Baggage &bag)
{
	FString statestring;
	intptr_t count = 0;
	FState state;
	FState * laststate = NULL;
	intptr_t lastlabel = -1;
	int minrequiredstate = -1;

	SC_MustGetStringName ("{");
	SC_SetEscape(false);	// disable escape sequences in the state parser
	while (!SC_CheckString ("}") && !sc_End)
	{
		memset(&state,0,sizeof(state));
		statestring = ParseStateString();
		if (!statestring.CompareNoCase("GOTO"))
		{
do_goto:	
			statestring = ParseStateString();
			if (SC_CheckString ("+"))
			{
				SC_MustGetNumber ();
				statestring += '+';
				statestring += sc_String;
			}
			// copy the text - this must be resolved later!
			if (laststate != NULL)
			{ // Following a state definition: Modify it.
				laststate->NextState = (FState*)copystring(statestring);	
			}
			else if (lastlabel >= 0)
			{ // Following a label: Retarget it.
				RetargetStates (count+1, statestring);
			}
			else
			{
				SC_ScriptError("GOTO before first state");
			}
		}
		else if (!statestring.CompareNoCase("STOP"))
		{
do_stop:
			if (laststate!=NULL)
			{
				laststate->NextState=(FState*)-1;
			}
			else if (lastlabel >=0)
			{
				RetargetStates (count+1, NULL);
			}
			else
			{
				SC_ScriptError("STOP before first state");
				continue;
			}
		}
		else if (!statestring.CompareNoCase("WAIT") || !statestring.CompareNoCase("FAIL"))
		{
			if (!laststate) 
			{
				SC_ScriptError("%s before first state", sc_String);
				continue;
			}
			laststate->NextState=(FState*)-2;
		}
		else if (!statestring.CompareNoCase("LOOP"))
		{
			if (!laststate) 
			{
				SC_ScriptError("LOOP before first state");
				continue;
			}
			laststate->NextState=(FState*)(lastlabel+1);
		}
		else
		{
			const char * statestrp;

			SC_MustGetString();
			if (SC_Compare (":"))
			{
				laststate = NULL;
				do
				{
					lastlabel = count;
					AddState(statestring, (FState *) (count+1));
					statestring = ParseStateString();
					if (!statestring.CompareNoCase("GOTO"))
					{
						goto do_goto;
					}
					else if (!statestring.CompareNoCase("STOP"))
					{
						goto do_stop;
					}
					SC_MustGetString ();
				} while (SC_Compare (":"));
//				continue;
			}

			SC_UnGet ();

			if (statestring.Len() != 4)
			{
				SC_ScriptError ("Sprite names must be exactly 4 characters\n");
			}

			memcpy(state.sprite.name, statestring, 4);
			state.Misc1=state.Misc2=0;
			state.ParameterIndex=0;
			SC_MustGetString();
			statestring = (sc_String+1);
			statestrp = statestring;
			state.Frame=(*sc_String&223)-'A';
			if ((*sc_String&223)<'A' || (*sc_String&223)>']')
			{
				SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
				state.Frame=0;
			}

			SC_MustGetNumber();
			sc_Number++;
			state.Tics=sc_Number&255;
			state.Misc1=(sc_Number>>8)&255;
			if (state.Misc1) state.Frame|=SF_BIGTIC;

			while (SC_GetString() && !sc_Crossed)
			{
				if (SC_Compare("BRIGHT")) 
				{
					state.Frame|=SF_FULLBRIGHT;
					continue;
				}
				if (SC_Compare("OFFSET"))
				{
					if (state.Frame&SF_BIGTIC)
					{
						SC_ScriptError("You cannot use OFFSET with a state duration larger than 254!");
					}
					// specify a weapon offset
					SC_MustGetStringName("(");
					SC_MustGetNumber();
					state.Misc1=sc_Number;
					SC_MustGetStringName (",");
					SC_MustGetNumber();
					state.Misc2=sc_Number;
					SC_MustGetStringName(")");
					continue;
				}

				// Make the action name lowercase to satisfy the gperf hashers
				strlwr (sc_String);

				int minreq=count;
				if (DoSpecialFunctions(state, !statestring.IsEmpty(), &minreq, bag))
				{
					if (minreq>minrequiredstate) minrequiredstate=minreq;
					goto endofstate;
				}

				PSymbol *sym = bag.Info->Class->Symbols.FindSymbol (FName(sc_String, true), true);
				if (sym != NULL && sym->SymbolType == SYM_ActionFunction)
				{
					PSymbolActionFunction *afd = static_cast<PSymbolActionFunction *>(sym);
					state.Action = afd->Function;
					if (!afd->Arguments.IsEmpty())
					{
						const char *params = afd->Arguments.GetChars();
						int numparams = (int)afd->Arguments.Len();
				
						int v;

						if (!islower(*params))
						{
							SC_MustGetStringName("(");
						}
						else
						{
							if (!SC_CheckString("(")) goto endofstate;
						}
						
						int paramindex = PrepareStateParameters(&state, numparams);
						int paramstart = paramindex;
						bool varargs = params[numparams - 1] == '+';

						if (varargs)
						{
							StateParameters[paramindex++] = 0;
						}

						while (*params)
						{
							switch(*params)
							{
							case 'I':
							case 'i':		// Integer
								SC_MustGetNumber();
								v=sc_Number;
								break;

							case 'F':
							case 'f':		// Fixed point
								SC_MustGetFloat();
								v=fixed_t(sc_Float*FRACUNIT);
								break;


							case 'S':
							case 's':		// Sound name
								SC_MustGetString();
								v=S_FindSound(sc_String);
								break;

							case 'M':
							case 'm':		// Actor name
							case 'T':
							case 't':		// String
								SC_SetEscape(true);
								SC_MustGetString();
								SC_SetEscape(false);
								v = (int)(sc_String[0] ? FName(sc_String) : NAME_None);
								break;

							case 'L':
							case 'l':		// Jump label

								if (SC_CheckNumber())
								{
									if (strlen(statestring)>0)
									{
										SC_ScriptError("You cannot use A_Jump commands with a jump index on multistate definitions\n");
									}

									v=sc_Number;
									if (v<1)
									{
										SC_ScriptError("Negative jump offsets are not allowed");
									}

									{
										int minreq=count+v;
										if (minreq>minrequiredstate) minrequiredstate=minreq;
									}
								}
								else
								{
									if (JumpParameters.Size()==0) JumpParameters.Push(NAME_None);

									v = -(int)JumpParameters.Size();
									FString statestring = ParseStateString();
									const PClass *stype=NULL;
									int scope = statestring.IndexOf("::");
									if (scope >= 0)
									{
										FName scopename = FName(statestring, scope, false);
										if (scopename == NAME_Super)
										{
											// Super refers to the direct superclass
											scopename = actor->Class->ParentClass->TypeName;
										}
										JumpParameters.Push(scopename);
										statestring = statestring.Right(statestring.Len()-scope-2);

										stype = PClass::FindClass (scopename);
										if (stype == NULL)
										{
											SC_ScriptError ("%s is an unknown class.", scopename.GetChars());
										}
										if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
										{
											SC_ScriptError ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
										}
										if (!stype->IsAncestorOf (actor->Class))
										{
											SC_ScriptError ("%s is not derived from %s so cannot access its states.",
												actor->Class->TypeName.GetChars(), stype->TypeName.GetChars());
										}
									}
									else
									{
										// No class name is stored. This allows 'virtual' jumps to
										// labels in subclasses.
										// It also means that the validity of the given state cannot
										// be checked here.
										JumpParameters.Push(NAME_None);
									}
									TArray<FName> names;
									MakeStateNameList(statestring, &names);

									if (stype != NULL)
									{
										if (!stype->ActorInfo->FindState(names.Size(), &names[0]))
										{
											SC_ScriptError("Jump to unknown state '%s' in class '%s'",
												statestring.GetChars(), stype->TypeName.GetChars());
										}
									}
									JumpParameters.Push((ENamedName)names.Size());
									for(unsigned i=0;i<names.Size();i++)
									{
										JumpParameters.Push(names[i]);
									}
									// No offsets here. The point of jumping to labels is to avoid such things!
								}

								break;

							case 'C':
							case 'c':		// Color
								SC_MustGetString ();
								if (SC_Compare("none"))
								{
									v = -1;
								}
								else
								{
									int c = V_GetColor (NULL, sc_String);
									// 0 needs to be the default so we have to mark the color.
									v = MAKEARGB(1, RPART(c), GPART(c), BPART(c));
								}
								break;

							case 'X':
							case 'x':
								v = ParseExpression (false, bag.Info->Class);
								break;

							case 'Y':
							case 'y':
								v = ParseExpression (true, bag.Info->Class);
								break;

							default:
								assert(false);
								v = -1;
								break;
							}
							StateParameters[paramindex++] = v;
							params++;
							if (varargs)
							{
								StateParameters[paramstart]++;
							}
							if (*params)
							{
								if (*params == '+')
								{
									if (SC_CheckString(")"))
									{
										goto endofstate;
									}
									params--;
									v = 0;
									StateParameters.Push(v);
								}
								else if ((islower(*params) || *params=='!') && SC_CheckString(")"))
								{
									goto endofstate;
								}
								SC_MustGetStringName (",");
							}
						}
						SC_MustGetStringName(")");
					}
					else 
					{
						SC_MustGetString();
						if (SC_Compare("("))
						{
							SC_ScriptError("You cannot pass parameters to '%s'\n",sc_String);
						}
						SC_UnGet();
					}
					goto endofstate;
				}
				SC_ScriptError("Invalid state parameter %s\n", sc_String);
			}
			SC_UnGet();
endofstate:
			StateArray.Push(state);
			while (*statestrp)
			{
				int frame=((*statestrp++)&223)-'A';

				if (frame<0 || frame>28)
				{
					SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
					frame=0;
				}

				state.Frame=(state.Frame&(SF_FULLBRIGHT|SF_BIGTIC))|frame;
				StateArray.Push(state);
				count++;
			}
			laststate=&StateArray[count];
			count++;
		}
	}
	if (count<=minrequiredstate)
	{
		SC_ScriptError("A_Jump offset out of range in %s", actor->Class->TypeName.GetChars());
	}
	SC_SetEscape(true);	// re-enable escape sequences
	return count;
}

//==========================================================================
//
// ResolveGotoLabel
//
//==========================================================================

static FState *ResolveGotoLabel (AActor *actor, const PClass *mytype, char *name)
{
	const PClass *type=mytype;
	FState *state;
	char *namestart = name;
	char *label, *offset, *pt;
	int v;

	// Check for classname
	if ((pt = strstr (name, "::")) != NULL)
	{
		const char *classname = name;
		*pt = '\0';
		name = pt + 2;

		// The classname may either be "Super" to identify this class's immediate
		// superclass, or it may be the name of any class that this one derives from.
		if (stricmp (classname, "Super") == 0)
		{
			type = type->ParentClass;
			actor = GetDefaultByType (type);
		}
		else
		{
			// first check whether a state of the desired name exists
			const PClass *stype = PClass::FindClass (classname);
			if (stype == NULL)
			{
				SC_ScriptError ("%s is an unknown class.", classname);
			}
			if (!stype->IsDescendantOf (RUNTIME_CLASS(AActor)))
			{
				SC_ScriptError ("%s is not an actor class, so it has no states.", stype->TypeName.GetChars());
			}
			if (!stype->IsAncestorOf (type))
			{
				SC_ScriptError ("%s is not derived from %s so cannot access its states.",
					type->TypeName.GetChars(), stype->TypeName.GetChars());
			}
			if (type != stype)
			{
				type = stype;
				actor = GetDefaultByType (type);
			}
		}
	}
	label = name;
	// Check for offset
	offset = NULL;
	if ((pt = strchr (name, '+')) != NULL)
	{
		*pt = '\0';
		offset = pt + 1;
	}
	v = offset ? strtol (offset, NULL, 0) : 0;

	// Get the state's address.
	if (type==mytype) state = FindState (actor, type, label);
	else state = FindStateInClass (actor, type, label);

	if (state != NULL)
	{
		state += v;
	}
	else if (v != 0)
	{
		SC_ScriptError ("Attempt to get invalid state %s from actor %s.", label, type->TypeName.GetChars());
	}
	delete[] namestart;		// free the allocated string buffer
	return state;
}

//==========================================================================
//
// FixStatePointers
//
// Fixes an actor's default state pointers.
//
//==========================================================================

static void FixStatePointers (FActorInfo *actor, TArray<FStateDefine> & list)
{
	for(unsigned i=0;i<list.Size(); i++)
	{
		size_t v=(size_t)list[i].State;
		if (v >= 1 && v < 0x10000)
		{
			list[i].State = actor->OwnedStates + v - 1;
		}
		if (list[i].Children.Size() > 0) FixStatePointers(actor, list[i].Children);
	}
}

//==========================================================================
//
// FixStatePointersAgain
//
// Resolves an actor's state pointers that were specified as jumps.
//
//==========================================================================

static void FixStatePointersAgain (FActorInfo *actor, AActor *defaults, TArray<FStateDefine> & list)
{
	for(unsigned i=0;i<list.Size(); i++)
	{
		if (list[i].State != NULL && FState::StaticFindStateOwner (list[i].State, actor) == NULL)
		{ // It's not a valid state, so it must be a label string. Resolve it.
			list[i].State = ResolveGotoLabel (defaults, actor->Class, (char *)list[i].State);
		}
		if (list[i].Children.Size() > 0) FixStatePointersAgain(actor, defaults, list[i].Children);
	}
}


//==========================================================================
//
// FinishStates
// copies a state block and fixes all state links
//
//==========================================================================

static int FinishStates (FActorInfo *actor, AActor *defaults, Baggage &bag)
{
	static int c=0;
	int count = StateArray.Size();

	if (count > 0)
	{
		FState *realstates = new FState[count];
		int i;
		int currange;

		memcpy(realstates, &StateArray[0], count*sizeof(FState));
		actor->OwnedStates = realstates;
		actor->NumOwnedStates = count;

		// adjust the state pointers
		// In the case new states are added these must be adjusted, too!
		FixStatePointers (actor, StateLabels);

		for(i = currange = 0; i < count; i++)
		{
			// resolve labels and jumps
			switch((ptrdiff_t)realstates[i].NextState)
			{
			case 0:		// next
				realstates[i].NextState = (i < count-1 ? &realstates[i+1] : &realstates[0]);
				break;

			case -1:	// stop
				realstates[i].NextState = NULL;
				break;

			case -2:	// wait
				realstates[i].NextState = &realstates[i];
				break;

			default:	// loop
				if ((size_t)realstates[i].NextState < 0x10000)
				{
					realstates[i].NextState = &realstates[(size_t)realstates[i].NextState-1];
				}
				else	// goto
				{
					realstates[i].NextState = ResolveGotoLabel (defaults, bag.Info->Class, (char *)realstates[i].NextState);
				}
			}
		}
	}
	StateArray.Clear ();

	// Fix state pointers that are gotos
	FixStatePointersAgain (actor, defaults, StateLabels);

	return count;
}

//==========================================================================
//
// For getting a state address from the parent
// No attempts have been made to add new functionality here
// This is strictly for keeping compatibility with old WADs!
//
//==========================================================================
static FState *CheckState(PClass *type)
{
	int v=0;

	if (SC_GetString() && !sc_Crossed)
	{
		if (SC_Compare("0")) return NULL;
		else if (SC_Compare("PARENT"))
		{
			FState * state = NULL;
			SC_MustGetString();

			FActorInfo * info = type->ParentClass->ActorInfo;

			if (info != NULL)
			{
				state = info->FindState(FName(sc_String));
			}

			if (SC_GetString ())
			{
				if (SC_Compare ("+"))
				{
					SC_MustGetNumber ();
					v = sc_Number;
				}
				else
				{
					SC_UnGet ();
				}
			}

			if (state == NULL && v==0) return NULL;

			if (v!=0 && state==NULL)
			{
				SC_ScriptError("Attempt to get invalid state from actor %s\n", type->ParentClass->TypeName.GetChars());
				return NULL;
			}
			state+=v;
			return state;
		}
		else SC_ScriptError("Invalid state assignment");
	}
	return NULL;
}


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
// Handle actor properties
//
//==========================================================================
void ParseActorProperties (Baggage &bag)
{
	const PClass *info;
	const ActorProps *prop;

	SC_MustGetStringName ("{");
	while (!SC_CheckString ("}"))
	{
		if (sc_End)
		{
			SC_ScriptError("Unexpected end of file encountered");
		}

		SC_GetString ();
		strlwr (sc_String);

		// Walk the ancestors of this type and see if any of them know
		// about the property.
		info = bag.Info->Class;

		FString propname = sc_String;

		if (sc_String[0]!='-' && sc_String[0]!='+')
		{
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
		}
		prop = is_actorprop (propname.GetChars());

		if (prop != NULL)
		{
			if (!info->IsDescendantOf(prop->type))
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
}

//==========================================================================
//
// Reads an actor definition
//
//==========================================================================
void ProcessActor(void (*process)(FState *, int))
{
	FActorInfo * info=NULL;
	AActor * defaults;
	Baggage bag;

	try
	{
		FActorInfo * parent;


		info=CreateNewActor(&parent, &bag);
		defaults=(AActor*)info->Class->Defaults;
		bag.StateSet = false;
		bag.DropItemSet = false;
		bag.CurrentState = 0;

		ParseActorProperties (bag);
		FinishStates (info, defaults, bag);
		InstallStates(info, defaults);
		process(info->OwnedStates, info->NumOwnedStates);
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

	catch(CRecoverableError & e)
	{
		throw e;
	}
	// I think this is better than a crash log.
#ifndef _DEBUG
	catch (...)
	{
		if (info)
			SC_ScriptError("Unexpected error during parsing of actor %s", info->Class->TypeName.GetChars());
		else
			SC_ScriptError("Unexpected error during parsing of actor definitions");
	}
#endif

	SC_SetCMode (false);
}

//==========================================================================
//
// StatePropertyIsDeprecated
//
//==========================================================================

static void StatePropertyIsDeprecated (const char *actorname, const char *prop)
{
	/*
	static bool warned = false;

	Printf (TEXTCOLOR_YELLOW "In actor %s, the %s property is deprecated.\n", actorname, prop);
	if (!warned)
	{
		warned = true;
		Printf (TEXTCOLOR_YELLOW "Instead of \"%s <state>\", add this to the actor's States block:\n"
				TEXTCOLOR_YELLOW "    %s:\n"
				TEXTCOLOR_YELLOW "        Goto <state>\n", prop, prop);
	}
	*/
}

//==========================================================================
//
// Property parsers
//
//==========================================================================

//==========================================================================
//
// ActorConstDef
//
// Parses a constant definition.
//
//==========================================================================

static void ActorConstDef (AActor *defaults, Baggage &bag)
{
	// Read the type and make sure it's int.
	// (Maybe there will be other types later.)
	SC_MustGetToken(TK_Int);
	SC_MustGetToken(TK_Identifier);
	FName symname = sc_Name;
	SC_MustGetToken('=');
	int expr = ParseExpression (false, bag.Info->Class);
	SC_MustGetToken(';');

	int val = EvalExpressionI (expr, NULL, bag.Info->Class);
	PSymbolConst *sym = new PSymbolConst;
	sym->SymbolName = symname;
	sym->SymbolType = SYM_Const;
	sym->Value = val;
	if (bag.Info->Class->Symbols.AddSymbol (sym) == NULL)
	{
		delete sym;
		SC_ScriptError ("'%s' is already defined in class '%s'.",
			symname.GetChars(), bag.Info->Class->TypeName.GetChars());
	}
}

//==========================================================================
//
// ParseGlobalConst
//
// Parses a constant outside an actor definition
// These will be inserted into AActor's symbol table
//
//==========================================================================

void ParseGlobalConst()
{
	Baggage bag;

	bag.Info = RUNTIME_CLASS(AActor)->ActorInfo;
	ActorConstDef(GetDefault<AActor>(), bag);
}

//==========================================================================
//
// ActorActionDef
//
// Parses an action function definition. A lot of this is essentially
// documentation in the declaration for when I have a proper language
// ready.
//
//==========================================================================

static void ActorActionDef (AActor *defaults, Baggage &bag)
{
#define OPTIONAL		1
#define EVAL			2
#define EVALNOT			4

	AFuncDesc *afd;
	FName funcname;
	FString args;

	SC_MustGetToken(TK_Native);
	SC_MustGetToken(TK_Identifier);
	funcname = sc_Name;
	afd = FindFunction(sc_String);
	if (afd == NULL)
	{
		SC_ScriptError ("The function '%s' has not been exported from the executable.", sc_String);
	}
	SC_MustGetToken('(');
	if (!SC_CheckToken(')'))
	{
		while (sc_TokenType != ')')
		{
			int flags = 0;
			char type = '@';

			// Retrieve flags before type name
			for (;;)
			{
				if (SC_CheckToken(TK_Optional))
				{
					flags |= OPTIONAL;
				}
				else if (SC_CheckToken(TK_Eval))
				{
					flags |= EVAL;
				}
				else if (SC_CheckToken(TK_EvalNot))
				{
					flags |= EVALNOT;
				}
				else if (SC_CheckToken(TK_Coerce) || SC_CheckToken(TK_Native))
				{
				}
				else
				{
					break;
				}
			}
			// Read the variable type
			SC_MustGetAnyToken();
			switch (sc_TokenType)
			{
			case TK_Bool:		type = 'i';		break;
			case TK_Int:		type = 'i';		break;
			case TK_Float:		type = 'f';		break;
			case TK_Sound:		type = 's';		break;
			case TK_String:		type = 't';		break;
			case TK_Name:		type = 't';		break;
			case TK_State:		type = 'l';		break;
			case TK_Color:		type = 'c';		break;
			case TK_Class:
				SC_MustGetToken('<');
				SC_MustGetToken(TK_Identifier);	// Skip class name, since the parser doesn't care
				SC_MustGetToken('>');
				type = 'm';
				break;
			case TK_Ellipsis:
				type = '+';
				SC_MustGetToken(')');
				SC_UnGet();
				break;
			default:
				SC_ScriptError ("Unknown variable type %s", SC_TokenName(sc_TokenType, sc_String).GetChars());
				break;
			}
			// Read the optional variable name
			if (!SC_CheckToken(',') && !SC_CheckToken(')'))
			{
				SC_MustGetToken(TK_Identifier);
			}
			else
			{
				SC_UnGet();
			}
			// If eval or evalnot were a flag, hey the decorate parser doesn't actually care about the type.
			if (flags & EVALNOT)
			{
				type = 'y';
			}
			else if (flags & EVAL)
			{
				type = 'x';
			}
			if (!(flags & OPTIONAL) && type != '+')
			{
				type -= 'a' - 'A';
			}
	#undef OPTIONAL
	#undef EVAL
	#undef EVALNOT
			args += type;
			SC_MustGetAnyToken();
			if (sc_TokenType != ',' && sc_TokenType != ')')
			{
				SC_ScriptError ("Expected ',' or ')' but got %s instead", SC_TokenName(sc_TokenType, sc_String).GetChars());
			}
		}
	}
	SC_MustGetToken(';');
	PSymbolActionFunction *sym = new PSymbolActionFunction;
	sym->SymbolName = funcname;
	sym->SymbolType = SYM_ActionFunction;
	sym->Arguments = args;
	sym->Function = afd->Function;
	if (bag.Info->Class->Symbols.AddSymbol (sym) == NULL)
	{
		delete sym;
		SC_ScriptError ("'%s' is already defined in class '%s'.",
			funcname.GetChars(), bag.Info->Class->TypeName.GetChars());
	}
}

//==========================================================================
//
//==========================================================================
static void ActorSkipSuper (AActor *defaults, Baggage &bag)
{
	ResetActor(defaults, &bag);
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
	SC_MustGetNumber();
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
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Spawn");
	AddState("Spawn", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorSeeState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "See");
	AddState("See", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMeleeState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Melee");
	AddState("Melee", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorMissileState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Missile");
	AddState("Missile", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorPainState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Pain");
	AddState("Pain", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDeathState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Death");
	AddState("Death", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorXDeathState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "XDeath");
	AddState("XDeath", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorBurnState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Burn");
	AddState("Burn", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorIceState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Ice");
	AddState("Ice", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorRaiseState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Raise");
	AddState("Raise", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrashState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Crash");
	AddState("Crash", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorCrushState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Crush");
	AddState("Crush", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorWoundState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Wound");
	AddState("Wound", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorDisintegrateState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Disintegrate");
	AddState("Disintegrate", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorHealState (AActor *defaults, Baggage &bag)
{
	StatePropertyIsDeprecated (bag.Info->Class->TypeName.GetChars(), "Heal");
	AddState("Heal", CheckState ( bag.Info->Class));
}

//==========================================================================
//
//==========================================================================
static void ActorStates (AActor *defaults, Baggage &bag)
{
	if (!bag.StateSet) ProcessStates(bag.Info, defaults, bag);
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
static void ActorMeleeDamage (AActor *defaults, Baggage &bag)
{
	SC_MustGetNumber();
	bag.BAttack.MeleeDamage = sc_Number;
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
	bag.BAttack.MeleeSound = S_FindSound(sc_String);
}

//==========================================================================
//
//==========================================================================
static void ActorMissileType (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	bag.BAttack.MissileName = sc_String;
}

//==========================================================================
//
//==========================================================================
static void ActorMissileHeight (AActor *defaults, Baggage &bag)
{
	SC_MustGetFloat();
	bag.BAttack.MissileHeight = fixed_t(sc_Float*FRACUNIT);
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
		unsigned char translation[256];
		for(int i=0;i<256;i++) translation[i]=i;
		do
		{
			SC_GetString();
			AddToTranslation(translation,sc_String);
		}
		while (SC_CheckString(","));
		defaults->Translation = StoreTranslation (translation);
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
static void ActorDecal (AActor *defaults, Baggage &bag)
{
	SC_MustGetString();
	defaults->DecalGenerator = (FDecalBase *) ((size_t)DecalNames.Push(copystring(sc_String))+1);
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

	if (sc_Float < 0.f || sc_Float > 1.f)
		SC_ScriptError ("Gravity must be in the range [0,1]");

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
//==========================================================================
static void ActorFlagSetOrReset (AActor *defaults, Baggage &bag)
{
	char mod = sc_String[0];
	flagdef *fd;

	SC_MustGetString ();

	// Fire and ice damage were once flags but now are not.
	if (SC_Compare ("FIREDAMAGE"))
	{
		if (mod == '+') defaults->DamageType = NAME_Fire;
		else defaults->DamageType = NAME_None;
	}
	else if (SC_Compare ("ICEDAMAGE"))
	{
		if (mod == '+') defaults->DamageType = NAME_Ice;
		else defaults->DamageType = NAME_None;
	}
	else if (SC_Compare ("LOWGRAVITY"))
	{
		if (mod == '+') defaults->gravity = FRACUNIT/8;
		else defaults->gravity = FRACUNIT;
	}
	else
	{
		FString part1 = sc_String;
		const char *part2 = NULL;
		if (SC_CheckString ("."))
		{
			SC_MustGetString ();
			part2 = sc_String;
		}
		if ( (fd = FindFlag (bag.Info->Class, part1.GetChars(), part2)) )
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
	di->probability=255;
	di->amount=0;
	if (CheckNumParm())
	{
		di->amount=sc_Number;
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
	{ "+",							ActorFlagSetOrReset,		RUNTIME_CLASS(AActor) },
	{ "-",							ActorFlagSetOrReset,		RUNTIME_CLASS(AActor) },
	{ "action",						ActorActionDef,				RUNTIME_CLASS(AActor) },
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
	{ "const",						ActorConstDef,				RUNTIME_CLASS(AActor) },
	{ "conversationid",				ActorConversationID,		RUNTIME_CLASS(AActor) },
	{ "crash",						ActorCrashState,			RUNTIME_CLASS(AActor) },
	{ "crush",						ActorCrushState,			RUNTIME_CLASS(AActor) },
	{ "damage",						ActorDamage,				RUNTIME_CLASS(AActor) },
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
	{ "melee",						ActorMeleeState,			RUNTIME_CLASS(AActor) },
	{ "meleedamage",				ActorMeleeDamage,			RUNTIME_CLASS(AActor) },
	{ "meleerange",					ActorMeleeRange,			RUNTIME_CLASS(AActor) },
	{ "meleesound",					ActorMeleeSound,			RUNTIME_CLASS(AActor) },
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
// Do some postprocessing after everything has been defined
// This also processes all the internal actors to adjust the type
// fields in the weapons
//
//==========================================================================
void FinishThingdef()
{
	unsigned int i;
	bool isRuntimeActor=false;

	for (i = 0;i < PClass::m_Types.Size(); i++)
	{
		PClass * ti = PClass::m_Types[i];

		// Skip non-actors
		if (!ti->IsDescendantOf(RUNTIME_CLASS(AActor))) continue;

		// Everything coming after the first runtime actor is also a runtime actor
		// so this check is sufficient
		if (ti == PClass::m_RuntimeActors[0])
		{
			isRuntimeActor=true;
		}

		// Friendlies never count as kills!
		if (GetDefaultByType(ti)->flags & MF_FRIENDLY)
		{
			GetDefaultByType(ti)->flags &=~MF_COUNTKILL;
		}

		// the typeinfo properties of weapons have to be fixed here after all actors have been declared
		if (ti->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon * defaults=(AWeapon *)ti->Defaults;
			fuglyname v;

			v = defaults->AmmoType1;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType1 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType1)
					{
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType1->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			v = defaults->AmmoType2;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->AmmoType2 = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->AmmoType2)
					{
						I_Error("Unknown ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (defaults->AmmoType2->ParentClass != RUNTIME_CLASS(AAmmo))
					{
						I_Error("Invalid ammo type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			v = defaults->SisterWeaponType;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->SisterWeaponType = PClass::FindClass(v);
				if (isRuntimeActor)
				{
					if (!defaults->SisterWeaponType)
					{
						I_Error("Unknown sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
					else if (!defaults->SisterWeaponType->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
					{
						I_Error("Invalid sister weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
					}
				}
			}

			if (isRuntimeActor)
			{
				// Do some consistency checks. If these states are undefined the weapon cannot work!
				if (!ti->ActorInfo->FindState(NAME_Ready)) I_Error("Weapon %s doesn't define a ready state.\n", ti->TypeName.GetChars());
				if (!ti->ActorInfo->FindState(NAME_Select)) I_Error("Weapon %s doesn't define a select state.\n", ti->TypeName.GetChars());
				if (!ti->ActorInfo->FindState(NAME_Deselect)) I_Error("Weapon %s doesn't define a deselect state.\n", ti->TypeName.GetChars());
				if (!ti->ActorInfo->FindState(NAME_Fire)) I_Error("Weapon %s doesn't define a fire state.\n", ti->TypeName.GetChars());
			}

		}
		// same for the weapon type of weapon pieces.
		else if (ti->IsDescendantOf(RUNTIME_CLASS(AWeaponPiece)))
		{
			AWeaponPiece * defaults=(AWeaponPiece *)ti->Defaults;
			fuglyname v;

			v = defaults->WeaponClass;
			if (v != NAME_None && v.IsValidName())
			{
				defaults->WeaponClass = PClass::FindClass(v);
				if (!defaults->WeaponClass)
				{
					I_Error("Unknown weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
				else if (!defaults->WeaponClass->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
				{
					I_Error("Invalid weapon type '%s' in '%s'\n", v.GetChars(), ti->TypeName.GetChars());
				}
			}
		}
	}

	// Since these are defined in DECORATE now the table has to be initialized here.
	for(int i=0;i<31;i++)
	{
		char fmt[20];
		sprintf(fmt, "QuestItem%d", i+1);
		QuestItemClasses[i]=PClass::FindClass(fmt);
	}
}

//==========================================================================
//
// ParseClass
//
// A minimal placeholder so that I can assign properties to some native
// classes. Please, no end users use this until it's finalized.
//
//==========================================================================

void ParseClass()
{
	Baggage bag;
	const PClass *cls;
	FName classname;
	FName supername;

	SC_MustGetToken(TK_Identifier);	// class name
	classname = sc_Name;
	SC_MustGetToken(TK_Extends);	// because I'm not supporting Object
	SC_MustGetToken(TK_Identifier);	// superclass name
	supername = sc_Name;
	SC_MustGetToken(TK_Native);		// use actor definitions for your own stuff
	SC_MustGetToken('{');

	cls = PClass::FindClass (classname);
	if (cls == NULL)
	{
		SC_ScriptError ("'%s' is not a native class", classname.GetChars());
	}
	if (cls->ParentClass == NULL || cls->ParentClass->TypeName != supername)
	{
		SC_ScriptError ("'%s' does not extend '%s'", classname.GetChars(), supername.GetChars());
	}
	bag.Info = cls->ActorInfo;

	SC_MustGetAnyToken();
	while (sc_TokenType != '}')
	{
		if (sc_TokenType == TK_Action)
		{
			ActorActionDef (0, bag);
		}
		else if (sc_TokenType == TK_Const)
		{
			ActorConstDef (0, bag);
		}
		else
		{
			FString tokname = SC_TokenName(sc_TokenType, sc_String);
			SC_ScriptError ("Expected 'action' or 'const' but got %s", tokname.GetChars());
		}
		SC_MustGetAnyToken();
	}
}
