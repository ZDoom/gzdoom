/*
** decorations.cpp
** Loads custom actors out of DECORATE lumps.
**
**---------------------------------------------------------------------------
** Copyright 2002 Randy Heit
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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum EDefinitionType
{
	DEF_Decoration,
	DEF_Pickup
};

class ADecoration : public AActor
{
	DECLARE_STATELESS_ACTOR (ADecoration, AActor);
};
IMPLEMENT_ABSTRACT_ACTOR (ADecoration)

class AFakeInventory : public AInventory
{
	DECLARE_STATELESS_ACTOR (AFakeInventory, AInventory);
public:
	char *PickupText;
	bool Respawnable;

	const char *PickupMessage ()
	{
		if (PickupText == 0)
		{
			return Super::PickupMessage();
		}
		return PickupText;
	}

	bool ShouldRespawn ()
	{
		return Respawnable && Super::ShouldRespawn();
	}

	bool TryPickup (AActor *toucher)
	{
		if (LineSpecials[special] (NULL, toucher, args[0], args[1], args[2], args[3], args[4]))
		{
			special = 0;
			return true;
		}
		return false;
	}

	void PlayPickupSound (AActor *toucher)
	{
		if (AttackSound != 0)
		{
			S_SoundID (toucher, CHAN_PICKUP, AttackSound, 1, ATTN_NORM);
		}
		else
		{
			Super::PlayPickupSound (toucher);
		}
	}
};
IMPLEMENT_STATELESS_ACTOR (AFakeInventory, Any, -1, 0)
	PROP_Flags (MF_SPECIAL)
END_DEFAULTS

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ParseDecorate (void (*process)(FState *, int));
static void ParseInsideDecoration (FActorInfo *info, AActor *defaults,
	TArray<FState> &states, EDefinitionType def);
static void ParseSpriteFrames (FActorInfo *info, TArray<FState> &states);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TArray<FActorInfo *> Decorations;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *RenderStyles[] =
{
	"STYLE_None",
	"STYLE_Normal",
	"STYLE_Fuzzy",
	"STYLE_SoulTrans",
	"STYLE_OptFuzzy",
	"STYLE_Translucent",
	"STYLE_Add",
	//"STYLE_Shaded",
	NULL
};

static const char *FlagNames1[] =
{
	"*",
	"Solid",
	"*",
	"NoSector",

	"NoBlockmap",
	"*",
	"*",
	"*",

	"SpawnCeiling",
	"NoGravity",
	"*",
	"*",
	"*",

	"*",
	"*",
	"*",
	"*",

	"*",
	"*",
	"*",
	"*",

	"*",
	"*",
	"*",
	"CountItem",
	NULL
};

static const char *FlagNames2[] =
{
	"LowGravity",
	"WindThrust",
	"*",
	"*",

	"*",
	"FloorClip",
	"SpawnFloat",
	"NoTeleport",

	"*",
	"Pushable",
	"SlidesOnWalls",
	"*",

	"CanPass",
	"CannotPush",
	"*",
	"*",

	"*",
	"*",
	"Telestomp",
	"FloatBob",

	"*",
	"*",
	"*",
	"*",

	"*",
	"*",
	"*",
	"*",

	"*",
	"*",
	"*",
	"Reflective",
	NULL
};

static const char *FlagNames3[] =
{
	"*",
	"*",
	"*",
	"*",

	"*",
	"*",
	"DontSplash",
	NULL
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// LoadDecorations
//
// Called from FActor::StaticInit()
//
//==========================================================================

void LoadDecorations (void (*process)(FState *, int))
{
	int lastlump, lump;

	lastlump = 0;
	while ((lump = W_FindLump ("DECORATE", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "DECORATE");
		ParseDecorate (process);
		SC_Close ();
	}
}

//==========================================================================
//
// ParseDecorate
//
// Parses a single DECORATE lump
//
//==========================================================================

static void ParseDecorate (void (*process)(FState *, int))
{
	TArray<FState> states;
	TypeInfo *type;
	TypeInfo *parent;
	EDefinitionType def;
	FActorInfo *info;
	char *typeName;

	// Get actor class name. The A prefix is added automatically.
	while (SC_GetString ())
	{
		if (SC_Compare ("Pickup"))
		{
			parent = RUNTIME_CLASS(AFakeInventory);
			def = DEF_Pickup;
			SC_MustGetString ();
		}
		else
		{
			parent = RUNTIME_CLASS(ADecoration);
			def = DEF_Decoration;
		}

		type = new TypeInfo;
		typeName = new char[strlen(sc_String)+2];
		typeName[0] = 'A';
		strcpy (&typeName[1], sc_String);

		type = parent->CreateDerivedClass (typeName, parent->SizeOf);
		info = type->ActorInfo;
		info->GameFilter = 0x80;
		Decorations.Push (info);

		SC_MustGetString ();
		while (!SC_Compare ("{"))
		{
			if (SC_Compare ("Doom"))
			{
				info->GameFilter |= GAME_Doom;
			}
			else if (SC_Compare ("Heretic"))
			{
				info->GameFilter |= GAME_Heretic;
			}
			else if (SC_Compare ("Hexen"))
			{
				info->GameFilter |= GAME_Hexen;
			}
			else if (SC_Compare ("Raven"))
			{
				info->GameFilter |= GAME_Raven;
			}
			else if (SC_Compare ("Any"))
			{
				info->GameFilter = GAME_Any;
			}
			else
			{
				const char *name = sc_String;
				SC_ScriptError ("Unknown game type %s", &name);
			}
			SC_MustGetString ();
		}
		if (info->GameFilter == 0x80)
		{
			info->GameFilter = GAME_Any;
		}
		else
		{
			info->GameFilter &= ~0x80;
		}

		states.Clear ();
		ParseInsideDecoration (info, (AActor *)(info->Defaults), states, def);

		info->NumOwnedStates = states.Size();
		if (info->NumOwnedStates == 0)
		{
			const char *name = typeName + 1;
			SC_ScriptError ("%s did not define any animation frames", &name);
		}

		info->OwnedStates = new FState[info->NumOwnedStates];
		memcpy (info->OwnedStates, &states[0], info->NumOwnedStates * sizeof(info->OwnedStates[0]));
		if (info->NumOwnedStates == 1)
		{
			info->OwnedStates->Tics = 0;
			info->OwnedStates->Misc1 = 0;
			info->OwnedStates->Frame &= ~SF_BIGTIC;
		}
		else
		{
			int i;

			for (i = 0; i < info->NumOwnedStates-1; ++i)
			{
				info->OwnedStates[i].NextState = &info->OwnedStates[i+1];
			}
			info->OwnedStates[i].NextState = info->OwnedStates;
		}
		((AActor *)(info->Defaults))->SpawnState = info->OwnedStates;
		process (info->OwnedStates, info->NumOwnedStates);
	}
}

//==========================================================================
//
// ParseInsideDecoration
//
// Parses attributes out of a definition terminated with a }
//
//==========================================================================

static void ParseInsideDecoration (FActorInfo *info, AActor *defaults,
	TArray<FState> &states, EDefinitionType def)
{
	AFakeInventory *const inv = static_cast<AFakeInventory *>(defaults);
	char sprite[5] = "TNT1";

	SC_MustGetString ();
	while (!SC_Compare ("}"))
	{
		if (SC_Compare ("DoomEdNum"))
		{
			SC_MustGetNumber ();
			if (sc_Number < -1 || sc_Number > 32767)
			{
				SC_ScriptError ("DoomEdNum must be in the range [-1,32767]");
			}
			info->DoomEdNum = (SWORD)sc_Number;
		}
		else if (SC_Compare ("SpawnNum"))
		{
			SC_MustGetNumber ();
			if (sc_Number < 0 || sc_Number > 255)
			{
				SC_ScriptError ("SpawnNum must be in the range [0,255]");
			}
			info->SpawnID = (BYTE)sc_Number;
		}
		else if (SC_Compare ("Sprite"))
		{
			SC_MustGetString ();
			if (strlen (sc_String) != 4)
			{
				SC_ScriptError ("Sprite name must be exactly four characters long");
			}
			memcpy (sprite, sc_String, 4);
		}
		else if (SC_Compare ("Frames"))
		{
			SC_MustGetString ();
			ParseSpriteFrames (info, states);
		}
		else if (SC_Compare ("Alpha"))
		{
			SC_MustGetFloat ();
			defaults->alpha = int(clamp (sc_Float, 0.f, 1.f) * OPAQUE);
		}
		else if (SC_Compare ("Scale"))
		{
			SC_MustGetFloat ();
			defaults->xscale = defaults->yscale = clamp (int(sc_Float * 64.f), 1, 256) - 1;
		}
		else if (SC_Compare ("RenderStyle"))
		{
			SC_MustGetString ();
			defaults->RenderStyle = SC_MustMatchString (RenderStyles);
			if (defaults->RenderStyle > STYLE_OptFuzzy)
			{
				defaults->RenderStyle += STYLE_Translucent - STYLE_OptFuzzy - 1;
			}
		}
		else if (SC_Compare ("Radius"))
		{
			SC_MustGetFloat ();
			defaults->radius = int(sc_Float * FRACUNIT);
		}
		else if (SC_Compare ("Height"))
		{
			SC_MustGetFloat ();
			defaults->height = int(sc_Float * FRACUNIT);
		}
		else if (SC_Compare ("Mass"))
		{
			SC_MustGetFloat ();
			defaults->Mass = SDWORD(sc_Float);
		}
		else if (SC_Compare ("Translation1"))
		{
			SC_MustGetNumber ();
			if (sc_Number < 0 || sc_Number > 2)
			{
				SC_ScriptError ("Translation1 must be in the range [0,2]");
			}
			defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc_Number);
		}
		else if (SC_Compare ("Translation2"))
		{
			SC_MustGetNumber ();
			if (sc_Number < 0 || sc_Number >= MAX_ACS_TRANSLATIONS)
			{
#define ERROR(foo) "Translation2 must be in the range [0," #foo "]"
				SC_ScriptError (ERROR(MAX_ACS_TRANSLATIONS));
#undef foo
			}
			defaults->Translation = TRANSLATION(TRANSLATION_LevelScripted, sc_Number);
		}
		else if (def == DEF_Pickup && SC_Compare ("PickupSound"))
		{
			SC_MustGetString ();
			inv->AttackSound = S_FindSound (sc_String);
		}
		else if (def == DEF_Pickup && SC_Compare ("PickupMessage"))
		{
			SC_MustGetString ();
			inv->PickupText = copystring (sc_String);
		}
		else if (def == DEF_Pickup && SC_Compare ("Respawns"))
		{
			inv->Respawnable = true;
		}
		else if (sc_String[0] != '*')
		{
			int bit = SC_MatchString (FlagNames1);
			if (bit != -1)
			{
				defaults->flags |= 1 << bit;
			}
			else if ((bit = SC_MatchString (FlagNames2)) != -1)
			{
				defaults->flags2 |= 1 << bit;
			}
			else if ((bit = SC_MatchString (FlagNames3)) != -1)
			{
				defaults->flags3 |= 1 << bit;
			}
			else
			{
				SC_ScriptError (NULL);
			}
		}
		else
		{
			SC_ScriptError (NULL);
		}
		SC_MustGetString ();
	}

	for (size_t i = 0; i < states.Size(); ++i)
	{
		memcpy (states[i].sprite.name, sprite, 4);
	}
}

//==========================================================================
//
// ParseSpriteFrames
//
// Parses a sprite-based animation sequence out of a decoration definition.
// You can have multiple sequences, and they will be appended together.
// A sequence definition looks like this:
//
// "<rate>:<frames>,<rate>:<frames>,<rate>:<frames>,..."
// 
// Rate is a number describing the number of tics between frames in this
// sequence. If you don't specify it, then a rate of 4 is used. Frames is
// a list of consecutive frame characters. Each frame can be postfixed with
// the * character to indicate that it is fullbright.
//
// Examples:
//		ShortRedTorch looks like this:
//			"ABCD"
//
//		HeadCandles looks like this:
//			"6:AB"
//
//		TechLamp looks like this:
//			"A*B*C*D*"
//
//		BloodyTwich looks like this:
//			"10:A, 15:B, 8:C, 6:B"
//==========================================================================

static void ParseSpriteFrames (FActorInfo *info, TArray<FState> &states)
{
	FState state;
	char *token = strtok (sc_String, ",\t\n\r");

	memset (&state, 0, sizeof(state));

	while (token != NULL)
	{
		// Skip leading white space
		while (*token == ' ')
			token++;

		int rate = 5;
		bool firstState = true;
		char *colon = strchr (token, ':');

		if (colon != NULL)
		{
			char *stop;

			*colon = 0;
			rate = strtol (token, &stop, 10);
			if (stop == token || rate < 1 || rate > 65534)
			{
				SC_ScriptError ("Rates must be in the range [0,65534]");
			}
			token = colon + 1;
			rate += 1;
		}

		state.Tics = rate & 0xff;
		state.Misc1 = (rate >> 8);

		while (*token)
		{
			if (*token == ' ')
			{
			}
			else if (*token == '*')
			{
				if (firstState)
				{
					SC_ScriptError ("* must come after a frame");
				}
				state.Frame |= SF_FULLBRIGHT;
			}
			else if (*token < 'A' || *token > ']')
			{
				SC_ScriptError ("Frames must be A-Z, [, \\, or ]");
			}
			else
			{
				if (!firstState)
				{
					states.Push (state);
				}
				firstState = false;
				state.Frame = (rate > 256) ? (SF_BIGTIC | (*token-'A')) : (*token-'A');	
			}
			++token;
		}
		if (!firstState)
		{
			states.Push (state);
		}

		token = strtok (NULL, ",\t\n\r");
	}
}
