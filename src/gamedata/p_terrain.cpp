/*
** p_terrain.cpp
** Terrain maintenance
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "doomtype.h"
#include "cmdlib.h"
#include "p_terrain.h"
#include "gi.h"
#include "r_state.h"
#include "w_wad.h"
#include "sc_man.h"
#include "p_local.h"
#include "actor.h"
#include "vm.h"

// MACROS ------------------------------------------------------------------

#define SET_FIELD(type,val) *((type*)((uint8_t *)fields + \
							parser[keyword].u.Offset)) = val;

// TYPES -------------------------------------------------------------------

enum EOuterKeywords
{
	OUT_SPLASH,
	OUT_TERRAIN,
	OUT_FLOOR,
	OUT_IFDOOM,
	OUT_IFHERETIC,
	OUT_IFHEXEN,
	OUT_IFSTRIFE,
	OUT_ENDIF,
	OUT_DEFAULTTERRAIN
};

enum ETerrainKeywords
{
	TR_CLOSE,
	TR_SPLASH,
	TR_DAMAGEAMOUNT,
	TR_DAMAGETYPE,
	TR_DAMAGETIMEMASK,
	TR_FOOTCLIP,
	TR_STEPVOLUME,
	TR_WALKINGSTEPTIME,
	TR_RUNNINGSTEPTIME,
	TR_LEFTSTEPSOUNDS,
	TR_RIGHTSTEPSOUNDS,
	TR_LIQUID,
	TR_FRICTION,
	TR_ALLOWPROTECTION
};

enum EGenericType
{
	GEN_End,
	GEN_Sound,
	GEN_Byte,
	GEN_Class,
	GEN_Splash,
	GEN_Float,
	GEN_Double,
	GEN_Time,
	GEN_Bool,
	GEN_Int,
	GEN_Custom,
};

struct FGenericParse
{
	EGenericType Type;
	union {
		size_t Offset;
		void (*Handler) (FScanner &sc, int type, void *fields);
	} u;
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void MakeDefaultTerrain ();
static void ParseOuter (FScanner &sc);
static void ParseSplash (FScanner &sc);
static void ParseTerrain (FScanner &sc);
static void ParseFloor (FScanner &sc);
static int FindSplash (FName name);
static void GenericParse (FScanner &sc, FGenericParse *parser, const char **keywords,
	void *fields, const char *type, FName name);
static void ParseDamage (FScanner &sc, int keyword, void *fields);
static void ParseFriction (FScanner &sc, int keyword, void *fields);
static void ParseDefault (FScanner &sc);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

FTerrainTypeArray TerrainTypes;
TArray<FSplashDef> Splashes;
TArray<FTerrainDef> Terrains;
uint16_t DefaultTerrainType;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *OuterKeywords[] =
{
	"splash",
	"terrain",
	"floor",
	"ifdoom",
	"ifheretic",
	"ifhexen",
	"ifstrife",
	"endif",
	"defaultterrain",
	NULL
};

static const char *SplashKeywords[] =
{
	"}",
	"smallsound",
	"smallclip",
	"sound",
	"smallclass",
	"baseclass",
	"chunkclass",
	"chunkxvelshift",
	"chunkyvelshift",
	"chunkzvelshift",
	"chunkbasezvel",
	"noalert",
	NULL
};

static const char *TerrainKeywords[] =
{
	"}",
	"splash",
	"damageamount",
	"damagetype",
	"damagetimemask",
	"footclip",
	"stepvolume",
	"walkingsteptime",
	"runningsteptime",
	"leftstepsounds",
	"rightstepsounds",
	"liquid",
	"friction",
	"allowprotection",
	NULL
};

static FGenericParse SplashParser[] =
{
	{ GEN_End,	  {0} },
	{ GEN_Sound,  {myoffsetof(FSplashDef, SmallSplashSound)} },
	{ GEN_Double, {myoffsetof(FSplashDef, SmallSplashClip)} },
	{ GEN_Sound,  {myoffsetof(FSplashDef, NormalSplashSound)} },
	{ GEN_Class,  {myoffsetof(FSplashDef, SmallSplash)} },
	{ GEN_Class,  {myoffsetof(FSplashDef, SplashBase)} },
	{ GEN_Class,  {myoffsetof(FSplashDef, SplashChunk)} },
	{ GEN_Byte,   {myoffsetof(FSplashDef, ChunkXVelShift)} },
	{ GEN_Byte,   {myoffsetof(FSplashDef, ChunkYVelShift)} },
	{ GEN_Byte,   {myoffsetof(FSplashDef, ChunkZVelShift)} },
	{ GEN_Double, {myoffsetof(FSplashDef, ChunkBaseZVel)} },
	{ GEN_Bool,	  {myoffsetof(FSplashDef, NoAlert)} }
};

static FGenericParse TerrainParser[] =
{
	{ GEN_End,	  {0} },
	{ GEN_Splash, {myoffsetof(FTerrainDef, Splash)} },
	{ GEN_Int,    {myoffsetof(FTerrainDef, DamageAmount)} },
	{ GEN_Custom, {(size_t)ParseDamage} },
	{ GEN_Int,    {myoffsetof(FTerrainDef, DamageTimeMask)} },
	{ GEN_Double, {myoffsetof(FTerrainDef, FootClip)} },
	{ GEN_Float,  {myoffsetof(FTerrainDef, StepVolume)} },
	{ GEN_Time,   {myoffsetof(FTerrainDef, WalkStepTics)} },
	{ GEN_Time,   {myoffsetof(FTerrainDef, RunStepTics)} },
	{ GEN_Sound,  {myoffsetof(FTerrainDef, LeftStepSound)} },
	{ GEN_Sound,  {myoffsetof(FTerrainDef, RightStepSound)} },
	{ GEN_Bool,   {myoffsetof(FTerrainDef, IsLiquid)} },
	{ GEN_Custom, {(size_t)ParseFriction} },
	{ GEN_Bool,   {myoffsetof(FTerrainDef, AllowProtection)} },
};



// CODE --------------------------------------------------------------------

//==========================================================================
//
// P_InitTerrainTypes
//
//==========================================================================

void P_InitTerrainTypes ()
{
	int lastlump;
	int lump;
	int size;

	Splashes.Clear();
	Terrains.Clear();
	size = (TexMan.NumTextures()+1);
	TerrainTypes.Resize(size);
	TerrainTypes.Clear();

	MakeDefaultTerrain ();

	lastlump = 0;
	while (-1 != (lump = Wads.FindLump ("TERRAIN", &lastlump)) )
	{
		FScanner sc(lump);
		ParseOuter (sc);
	}
	Splashes.ShrinkToFit ();
	Terrains.ShrinkToFit ();
}

//==========================================================================
//
// MakeDefaultTerrain
//
//==========================================================================

static void MakeDefaultTerrain ()
{
	FTerrainDef def = {};

	def.Name = "Solid";
	def.Splash = -1;
	Terrains.Push (def);
}

//==========================================================================
//
// ParseOuter
//
//==========================================================================

static void ParseOuter (FScanner &sc)
{
	int bracedepth = 0;
	bool ifskip = false;

	while (sc.GetString ())
	{
		if (ifskip)
		{
			if (bracedepth > 0)
			{
				if (sc.Compare ("}"))
				{
					bracedepth--;
					continue;
				}
			}
			else if (sc.Compare ("endif"))
			{
				ifskip = false;
				continue;
			}
			if (sc.Compare ("{"))
			{
				bracedepth++;
			}
			else if (sc.Compare ("}"))
			{
				sc.ScriptError ("Too many left braces ('}')");
			}
		}
		else
		{
			switch (sc.MustMatchString (OuterKeywords))
			{
			case OUT_SPLASH:
				ParseSplash (sc);
				break;

			case OUT_TERRAIN:
				ParseTerrain (sc);
				break;

			case OUT_FLOOR:
				ParseFloor (sc);
				break;

			case OUT_DEFAULTTERRAIN:
				ParseDefault (sc);
				break;

			case OUT_IFDOOM:
			case OUT_IFHERETIC:
			case OUT_IFHEXEN:
			case OUT_IFSTRIFE:
				ifskip = !CheckGame(sc.String+2, true);
				break;

			case OUT_ENDIF:
				break;
			}
		}
	}
}

//==========================================================================
//
// SetSplashDefaults
//
//==========================================================================

static void SetSplashDefaults (FSplashDef *splashdef)
{
	splashdef->SmallSplashSound =
		splashdef->NormalSplashSound = 0;
	splashdef->SmallSplash =
		splashdef->SplashBase =
		splashdef->SplashChunk = NULL;
	splashdef->ChunkXVelShift =
		splashdef->ChunkYVelShift =
		splashdef->ChunkZVelShift = 8;
	splashdef->ChunkBaseZVel = 1;
	splashdef->SmallSplashClip = 12.;
	splashdef->NoAlert = false;
}

//==========================================================================
//
// ParseSplash
//
//==========================================================================

void ParseSplash (FScanner &sc)
{
	int splashnum;
	FSplashDef *splashdef;
	bool isnew = false;

	sc.MustGetString ();
	FName name = sc.String;
	splashnum = (int)FindSplash (name);
	if (splashnum < 0)
	{
		FSplashDef def;
		SetSplashDefaults (&def);
		def.Name = name;
		splashnum = (int)Splashes.Push (def);
		isnew = true;
	}
	splashdef = &Splashes[splashnum];

	sc.MustGetString ();
	if (!sc.Compare ("modify"))
	{ // Set defaults
		if (!isnew)
		{ // New ones already have their defaults set before they're pushed.
			SetSplashDefaults (splashdef);
		}
	}
	else
	{
		sc.MustGetString();
	}
	if (!sc.Compare ("{"))
	{
		sc.ScriptError ("Expected {");
	}
	else
	{
		GenericParse (sc, SplashParser, SplashKeywords, splashdef, "splash",
			splashdef->Name);
	}
}

//==========================================================================
//
// ParseTerrain
//
//==========================================================================

void ParseTerrain (FScanner &sc)
{
	int terrainnum;

	sc.MustGetString ();
	FName name = sc.String;
	terrainnum = (int)P_FindTerrain (name);
	if (terrainnum < 0)
	{
		FTerrainDef def;
		memset (&def, 0, sizeof(def));
		def.Splash = -1;
		def.Name = name;
		terrainnum = (int)Terrains.Push (def);
	}

	// Set defaults
	sc.MustGetString ();
	if (!sc.Compare ("modify"))
	{
		name = Terrains[terrainnum].Name;
		memset (&Terrains[terrainnum], 0, sizeof(FTerrainDef));
		Terrains[terrainnum].Splash = -1;
		Terrains[terrainnum].Name = name;
	}
	else
	{
		sc.MustGetString ();
	}

	if (sc.Compare ("{"))
	{
		GenericParse (sc, TerrainParser, TerrainKeywords, &Terrains[terrainnum],
			"terrain", Terrains[terrainnum].Name);
	}
	else
	{
		sc.ScriptError ("Expected {");
	}
}

//==========================================================================
//
// ParseDamage
//
//==========================================================================

static void ParseDamage (FScanner &sc, int keyword, void *fields)
{
	FTerrainDef *def = (FTerrainDef *)fields;

	sc.MustGetString ();
	// Lava is synonymous with Fire here!
	if (sc.Compare("Lava")) def->DamageMOD=NAME_Fire;
	else def->DamageMOD=sc.String;
}

//==========================================================================
//
// ParseFriction
//
//==========================================================================

static void ParseFriction (FScanner &sc, int keyword, void *fields)
{
	FTerrainDef *def = (FTerrainDef *)fields;
	double friction, movefactor;

	sc.MustGetFloat ();

	// These calculations should match those in P_SetSectorFriction().
	// A friction of 1.0 is equivalent to ORIG_FRICTION.

	friction = (0x1EB8*(sc.Float*100))/0x80 + 0xD001;
	friction = clamp<double> (friction, 0, 65536.);

	if (friction > ORIG_FRICTION * 65536.)	// ice
		movefactor = ((0x10092 - friction) * 1024) / 4352 + 568;
	else
		movefactor = ((friction - 0xDB34)*(0xA))/0x80;

	if (movefactor < 32)
		movefactor = 32;

	def->Friction = friction / 65536.;
	def->MoveFactor = movefactor / 65536.;
}

//==========================================================================
//
// GenericParse
//
//==========================================================================

static void GenericParse (FScanner &sc, FGenericParse *parser, const char **keywords,
	void *fields, const char *type, FName name)
{
	bool notdone = true;
	int keyword;
	int val = 0;
	const PClass *info;

	do
	{
		sc.MustGetString ();
		keyword = sc.MustMatchString (keywords);
		switch (parser[keyword].Type)
		{
		case GEN_End:
			notdone = false;
			break;

		case GEN_Sound:
			sc.MustGetString ();
			SET_FIELD (FSoundID, FSoundID(sc.String));
			/* unknown sounds never produce errors anywhere else so they shouldn't here either.
			if (val == 0)
			{
				Printf ("Unknown sound %s in %s %s\n",
					sc.String, type, name.GetChars());
			}
			*/
			break;

		case GEN_Byte:
			sc.MustGetNumber ();
			SET_FIELD (uint8_t, sc.Number);
			break;

		case GEN_Class:
			sc.MustGetString ();
			if (sc.Compare ("None"))
			{
				info = NULL;
			}
			else
			{
				info = PClass::FindClass (sc.String);
				if (info == NULL)
				{
					Printf ("Unknown actor %s in %s %s\n",
						sc.String, type, name.GetChars());
				}
				else if (!info->IsDescendantOf (RUNTIME_CLASS(AActor)))
				{
					Printf ("%s is not an Actor (in %s %s)\n",
						sc.String, type, name.GetChars());
					info = NULL;
				}
			}
			SET_FIELD (const PClass *, info);
			break;

		case GEN_Splash:
			sc.MustGetString ();
			val = FindSplash (sc.String);
			SET_FIELD (int, val);
			if (val == -1)
			{
				Printf ("Splash %s is not defined yet (in %s %s)\n",
					sc.String, type, name.GetChars());
			}
			break;

		case GEN_Float:
			sc.MustGetFloat ();
			SET_FIELD (float, float(sc.Float));
			break;

		case GEN_Double:
			sc.MustGetFloat();
			SET_FIELD(double, sc.Float);
			break;

		case GEN_Time:
			sc.MustGetFloat ();
			SET_FIELD (int, (int)(sc.Float * TICRATE));
			break;

		case GEN_Bool:
			SET_FIELD (bool, true);
			break;

		case GEN_Int:
			sc.MustGetNumber ();
			SET_FIELD (int, sc.Number);
			break;

		case GEN_Custom:
			parser[keyword].u.Handler (sc, keyword, fields);
			break;
		}
	} while (notdone);
}

//==========================================================================
//
// ParseFloor
//
//==========================================================================

static void ParseFloor (FScanner &sc)
{
	FTextureID picnum;
	int terrain;

	bool opt = sc.CheckString("optional");
	sc.MustGetString ();

	picnum = TexMan.CheckForTexture (sc.String, ETextureType::Flat,
		FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_TryAny);

	if (!picnum.Exists())
	{
		if (!opt)
		{
			Printf("Unknown flat %s\n", sc.String);
		}
		sc.MustGetString();
		return;
	}
	sc.MustGetString ();
	terrain = P_FindTerrain (sc.String);
	if (terrain == -1)
	{
		Printf ("Unknown terrain %s\n", sc.String);
		terrain = 0;
	}
	TerrainTypes.Set(picnum.GetIndex(), terrain);
}

//==========================================================================
//
// ParseDefault
//
//==========================================================================

static void ParseDefault (FScanner &sc)
{
	int terrain;

	sc.MustGetString ();
	terrain = P_FindTerrain (sc.String);
	if (terrain == -1)
	{
		Printf ("Unknown terrain %s\n", sc.String);
		terrain = 0;
	}
	DefaultTerrainType = terrain;
}

//==========================================================================
//
// FindSplash
//
//==========================================================================

int FindSplash (FName name)
{
	unsigned int i;

	for (i = 0; i < Splashes.Size (); i++)
	{
		if (Splashes[i].Name == name)
		{
			return (int)i;
		}
	}
	return -1;
}

//==========================================================================
//
// FindTerrain
//
//==========================================================================

int P_FindTerrain (FName name)
{
	unsigned int i;

	if (name == NAME_Null) return -1;
	for (i = 0; i < Terrains.Size (); i++)
	{
		if (Terrains[i].Name == name)
		{
			return (int)i;
		}
	}
	return -1;
}

FName P_GetTerrainName(int terrainnum)
{
	if (terrainnum < 0 || terrainnum >= (int)Terrains.Size())
	{
		return NAME_Null;
	}
	else
	{
		return Terrains[terrainnum].Name;
	}
}

DEFINE_FIELD_NAMED(FTerrainDef, Name, TerrainName)
DEFINE_FIELD(FTerrainDef, Splash)
DEFINE_FIELD(FTerrainDef, DamageAmount)
DEFINE_FIELD(FTerrainDef, DamageMOD)
DEFINE_FIELD(FTerrainDef, DamageTimeMask)
DEFINE_FIELD(FTerrainDef, FootClip)
DEFINE_FIELD(FTerrainDef, StepVolume)
DEFINE_FIELD(FTerrainDef, WalkStepTics)
DEFINE_FIELD(FTerrainDef, RunStepTics)
DEFINE_FIELD(FTerrainDef, LeftStepSound)
DEFINE_FIELD(FTerrainDef, RightStepSound)
DEFINE_FIELD(FTerrainDef, IsLiquid)
DEFINE_FIELD(FTerrainDef, AllowProtection)
DEFINE_FIELD(FTerrainDef, Friction)
DEFINE_FIELD(FTerrainDef, MoveFactor)
