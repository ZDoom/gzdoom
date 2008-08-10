/*
** decorations.cpp
** Loads custom actors out of DECORATE lumps.
**
**---------------------------------------------------------------------------
** Copyright 2002-2006 Randy Heit
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
#include "i_system.h"
#include "thingdef.h"
#include "r_translate.h"

// TYPES -------------------------------------------------------------------

struct FExtraInfo
{
	char DeathSprite[5];
	unsigned int SpawnStart, SpawnEnd;
	unsigned int DeathStart, DeathEnd;
	unsigned int IceDeathStart, IceDeathEnd;
	unsigned int FireDeathStart, FireDeathEnd;
	bool bSolidOnDeath, bSolidOnBurn;
	bool bBurnAway, bDiesAway, bGenericIceDeath;
	bool bExplosive;
	fixed_t DeathHeight, BurnHeight;
};

class AFakeInventory : public AInventory
{
	DECLARE_CLASS (AFakeInventory, AInventory);
public:
	bool Respawnable;

	bool ShouldRespawn ()
	{
		return Respawnable && Super::ShouldRespawn();
	}

	bool TryPickup (AActor *toucher)
	{
		INTBOOL success = LineSpecials[special] (NULL, toucher, false,
			args[0], args[1], args[2], args[3], args[4]);

		if (success)
		{
			GoAwayAndDie ();
			return true;
		}
		return false;
	}

	void DoPickupSpecial (AActor *toucher)
	{
		// The special was already executed by TryPickup, so do nothing here
	}
};
IMPLEMENT_CLASS (AFakeInventory)

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void A_ScreamAndUnblock (AActor *);
void A_ActiveAndUnblock (AActor *);
void A_ActiveSound (AActor *);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ParseInsideDecoration (FActorInfo *info, AActor *defaults,
	TArray<FState> &states, FExtraInfo &extra, EDefinitionType def, FScanner &sc);
static void ParseSpriteFrames (FActorInfo *info, TArray<FState> &states, FScanner &sc);

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *RenderStyles[] =
{
	"STYLE_None",
	"STYLE_Normal",
	"STYLE_Fuzzy",
	"STYLE_SoulTrans",
	"STYLE_OptFuzzy",
	"STYLE_Stencil",
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
	"Shadow",
	"NoBlood",

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

	"Ripper",
	"Pushable",
	"SlidesOnWalls",
	"*",

	"CanPass",
	"CannotPush",
	"ThruGhost",
	"*",

	"FireDamage",
	"NoDamageThrust",
	"Telestomp",
	"FloatBob",

	"*",
	"ActivateImpact",
	"CanPushWalls",
	"ActivateMCross",

	"ActivatePCross",
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
	"FloorHugger",
	"CeilingHugger",
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
// ParseOldDecoration
//
// Reads an old style decoration object
//
//==========================================================================

void ParseOldDecoration(FScanner &sc, EDefinitionType def)
{
	TArray<FState> states;
	FExtraInfo extra;
	PClass *type;
	PClass *parent;
	FActorInfo *info;
	FName typeName;

	if (def == DEF_Pickup) parent = RUNTIME_CLASS(AFakeInventory);
	else parent = RUNTIME_CLASS(AActor);

	sc.MustGetString();
	typeName = FName(sc.String);
	type = parent->CreateDerivedClass (typeName, parent->Size);
	info = type->ActorInfo;
	info->GameFilter = 0x80;
	MakeStateDefines(parent->ActorInfo->StateList);

	sc.MustGetString ();
	while (!sc.Compare ("{"))
	{
		if (sc.Compare ("Doom"))
		{
			info->GameFilter |= GAME_Doom;
		}
		else if (sc.Compare ("Heretic"))
		{
			info->GameFilter |= GAME_Heretic;
		}
		else if (sc.Compare ("Hexen"))
		{
			info->GameFilter |= GAME_Hexen;
		}
		else if (sc.Compare ("Raven"))
		{
			info->GameFilter |= GAME_Raven;
		}
		else if (sc.Compare ("Strife"))
		{
			info->GameFilter |= GAME_Strife;
		}
		else if (sc.Compare ("Any"))
		{
			info->GameFilter = GAME_Any;
		}
		else
		{
			if (def != DEF_Decoration || info->GameFilter != 0x80) 
			{
				sc.ScriptError ("Unknown game type %s in %s", sc.String, typeName.GetChars());
			}
			else 
			{
				// If this is a regular decoration (without preceding keyword) and no game 
				// filters defined this is more likely a general syntax error so output a 
				// more meaningful message.
				sc.ScriptError ("Syntax error: Unknown identifier '%s'", typeName.GetChars());
			}
		}
		sc.MustGetString ();
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
	memset (&extra, 0, sizeof(extra));
	ParseInsideDecoration (info, (AActor *)(type->Defaults), states, extra, def, sc);

	info->NumOwnedStates = states.Size();
	if (info->NumOwnedStates == 0)
	{
		sc.ScriptError ("%s does not define any animation frames", typeName.GetChars() );
	}
	else if (extra.SpawnEnd == 0)
	{
		sc.ScriptError ("%s does not have a Frames definition", typeName.GetChars() );
	}
	else if (def == DEF_BreakableDecoration && extra.DeathEnd == 0)
	{
		sc.ScriptError ("%s does not have a DeathFrames definition", typeName.GetChars() );
	}
	else if (extra.IceDeathEnd != 0 && extra.bGenericIceDeath)
	{
		sc.ScriptError ("You cannot use IceDeathFrames and GenericIceDeath together");
	}

	if (extra.IceDeathEnd != 0)
	{
		// Make a copy of the final frozen frame for A_FreezeDeathChunks
		FState icecopy = states[extra.IceDeathEnd-1];
		states.Push (icecopy);
		info->NumOwnedStates += 1;
	}

	info->OwnedStates = new FState[info->NumOwnedStates];
	memcpy (info->OwnedStates, &states[0], info->NumOwnedStates * sizeof(info->OwnedStates[0]));
	if (info->NumOwnedStates == 1)
	{
		info->OwnedStates->Tics = -1;
		info->OwnedStates->Misc1 = 0;
	}
	else
	{
		size_t i;

		// Spawn states loop endlessly
		for (i = extra.SpawnStart; i < extra.SpawnEnd-1; ++i)
		{
			info->OwnedStates[i].NextState = &info->OwnedStates[i+1];
		}
		info->OwnedStates[i].NextState = &info->OwnedStates[extra.SpawnStart];

		// Death states are one-shot and freeze on the final state
		if (extra.DeathEnd != 0)
		{
			for (i = extra.DeathStart; i < extra.DeathEnd-1; ++i)
			{
				info->OwnedStates[i].NextState = &info->OwnedStates[i+1];
			}
			if (extra.bDiesAway || def == DEF_Projectile)
			{
				info->OwnedStates[i].NextState = NULL;
			}
			else
			{
				info->OwnedStates[i].Tics = -1;
				info->OwnedStates[i].Misc1 = 0;
			}

			if (def == DEF_Projectile)
			{
				if (extra.bExplosive)
				{
					info->OwnedStates[extra.DeathStart].Action = A_ExplodeParms;
				}
			}
			else
			{
				// The first frame plays the death sound and
				// the second frame makes it nonsolid.
				info->OwnedStates[extra.DeathStart].Action= A_Scream;
				if (extra.bSolidOnDeath)
				{
				}
				else if (extra.DeathStart + 1 < extra.DeathEnd)
				{
					info->OwnedStates[extra.DeathStart+1].Action = A_NoBlocking;
				}
				else
				{
					info->OwnedStates[extra.DeathStart].Action = A_ScreamAndUnblock;
				}

				if (extra.DeathHeight == 0) extra.DeathHeight = ((AActor*)(type->Defaults))->height;
				info->Class->Meta.SetMetaFixed (AMETA_DeathHeight, extra.DeathHeight);
			}
			AddState("Death", &info->OwnedStates[extra.DeathStart]);
		}

		// Burn states are the same as death states, except they can optionally terminate
		if (extra.FireDeathEnd != 0)
		{
			for (i = extra.FireDeathStart; i < extra.FireDeathEnd-1; ++i)
			{
				info->OwnedStates[i].NextState = &info->OwnedStates[i+1];
			}
			if (extra.bBurnAway)
			{
				info->OwnedStates[i].NextState = NULL;
			}
			else
			{
				info->OwnedStates[i].Tics = -1;
				info->OwnedStates[i].Misc1 = 0;
			}

			// The first frame plays the burn sound and
			// the second frame makes it nonsolid.
			info->OwnedStates[extra.FireDeathStart].Action = A_ActiveSound;
			if (extra.bSolidOnBurn)
			{
			}
			else if (extra.FireDeathStart + 1 < extra.FireDeathEnd)
			{
				info->OwnedStates[extra.FireDeathStart+1].Action = A_NoBlocking;
			}
			else
			{
				info->OwnedStates[extra.FireDeathStart].Action = A_ActiveAndUnblock;
			}

			if (extra.BurnHeight == 0) extra.BurnHeight = ((AActor*)(type->Defaults))->height;
			type->Meta.SetMetaFixed (AMETA_BurnHeight, extra.BurnHeight);

			AddState("Burn", &info->OwnedStates[extra.FireDeathStart]);
		}

		// Ice states are similar to burn and death, except their final frame enters
		// a loop that eventually causes them to bust to pieces.
		if (extra.IceDeathEnd != 0)
		{
			for (i = extra.IceDeathStart; i < extra.IceDeathEnd-1; ++i)
			{
				info->OwnedStates[i].NextState = &info->OwnedStates[i+1];
			}
			info->OwnedStates[i].NextState = &info->OwnedStates[info->NumOwnedStates-1];
			info->OwnedStates[i].Tics = 5;
			info->OwnedStates[i].Misc1 = 0;
			info->OwnedStates[i].Action = A_FreezeDeath;

			i = info->NumOwnedStates - 1;
			info->OwnedStates[i].NextState = &info->OwnedStates[i];
			info->OwnedStates[i].Tics = 1;
			info->OwnedStates[i].Misc1 = 0;
			info->OwnedStates[i].Action = A_FreezeDeathChunks;
			AddState("Ice", &info->OwnedStates[extra.IceDeathStart]);
		}
		else if (extra.bGenericIceDeath)
		{
			AddState("Ice", RUNTIME_CLASS(AActor)->ActorInfo->FindState(NAME_GenericFreezeDeath));
		}
	}
	if (def == DEF_BreakableDecoration)
	{
		((AActor *)(type->Defaults))->flags |= MF_SHOOTABLE;
	}
	if (def == DEF_Projectile)
	{
		((AActor *)(type->Defaults))->flags |= MF_DROPOFF|MF_MISSILE;
	}
	AddState("Spawn", &info->OwnedStates[extra.SpawnStart]);
	InstallStates (info, ((AActor *)(type->Defaults)));
}

//==========================================================================
//
// ParseInsideDecoration
//
// Parses attributes out of a definition terminated with a }
//
//==========================================================================

static void ParseInsideDecoration (FActorInfo *info, AActor *defaults,
	TArray<FState> &states, FExtraInfo &extra, EDefinitionType def, FScanner &sc)
{
	AFakeInventory *const inv = static_cast<AFakeInventory *>(defaults);
	char sprite[5] = "TNT1";

	sc.MustGetString ();
	while (!sc.Compare ("}"))
	{
		if (sc.Compare ("DoomEdNum"))
		{
			sc.MustGetNumber ();
			if (sc.Number < -1 || sc.Number > 32767)
			{
				sc.ScriptError ("DoomEdNum must be in the range [-1,32767]");
			}
			info->DoomEdNum = (SWORD)sc.Number;
		}
		else if (sc.Compare ("SpawnNum"))
		{
			sc.MustGetNumber ();
			if (sc.Number < 0 || sc.Number > 255)
			{
				sc.ScriptError ("SpawnNum must be in the range [0,255]");
			}
			info->SpawnID = (BYTE)sc.Number;
		}
		else if (sc.Compare ("Sprite") || (
			(def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			sc.Compare ("DeathSprite") &&
			(extra.DeathSprite[0] = 1))	// This is intentionally = and not ==
			)
		{
			sc.MustGetString ();
			if (sc.StringLen != 4)
			{
				sc.ScriptError ("Sprite name must be exactly four characters long");
			}
			if (extra.DeathSprite[0] == 1)
			{
				memcpy (extra.DeathSprite, sc.String, 4);
			}
			else
			{
				memcpy (sprite, sc.String, 4);
			}
		}
		else if (sc.Compare ("Frames"))
		{
			sc.MustGetString ();
			extra.SpawnStart = states.Size();
			ParseSpriteFrames (info, states, sc);
			extra.SpawnEnd = states.Size();
		}
		else if ((def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			sc.Compare ("DeathFrames"))
		{
			sc.MustGetString ();
			extra.DeathStart = states.Size();
			ParseSpriteFrames (info, states, sc);
			extra.DeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("IceDeathFrames"))
		{
			sc.MustGetString ();
			extra.IceDeathStart = states.Size();
			ParseSpriteFrames (info, states, sc);
			extra.IceDeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnDeathFrames"))
		{
			sc.MustGetString ();
			extra.FireDeathStart = states.Size();
			ParseSpriteFrames (info, states, sc);
			extra.FireDeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("GenericIceDeath"))
		{
			extra.bGenericIceDeath = true;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnsAway"))
		{
			extra.bBurnAway = true;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("DiesAway"))
		{
			extra.bDiesAway = true;
		}
		else if (sc.Compare ("Alpha"))
		{
			sc.MustGetFloat ();
			defaults->alpha = int(clamp (sc.Float, 0.0, 1.0) * OPAQUE);
		}
		else if (sc.Compare ("Scale"))
		{
			sc.MustGetFloat ();
			defaults->scaleX = defaults->scaleY = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare ("RenderStyle"))
		{
			sc.MustGetString ();
			defaults->RenderStyle = LegacyRenderStyles[sc.MustMatchString (RenderStyles)];
		}
		else if (sc.Compare ("Radius"))
		{
			sc.MustGetFloat ();
			defaults->radius = int(sc.Float * FRACUNIT);
		}
		else if (sc.Compare ("Height"))
		{
			sc.MustGetFloat ();
			defaults->height = int(sc.Float * FRACUNIT);
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("DeathHeight"))
		{
			sc.MustGetFloat ();
			extra.DeathHeight = int(sc.Float * FRACUNIT);
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnHeight"))
		{
			sc.MustGetFloat ();
			extra.BurnHeight = int(sc.Float * FRACUNIT);
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("Health"))
		{
			sc.MustGetNumber ();
			defaults->health = sc.Number;
		}
		else if (def == DEF_Projectile && sc.Compare ("ExplosionRadius"))
		{
			sc.MustGetNumber ();
			info->Class->Meta.SetMetaInt(ACMETA_ExplosionRadius, sc.Number);
			extra.bExplosive = true;
		}
		else if (def == DEF_Projectile && sc.Compare ("ExplosionDamage"))
		{
			sc.MustGetNumber ();
			info->Class->Meta.SetMetaInt(ACMETA_ExplosionDamage, sc.Number);
			extra.bExplosive = true;
		}
		else if (def == DEF_Projectile && sc.Compare ("DoNotHurtShooter"))
		{
			info->Class->Meta.SetMetaInt(ACMETA_DontHurtShooter, true);
		}
		else if (def == DEF_Projectile && sc.Compare ("Damage"))
		{
			sc.MustGetNumber ();
			defaults->Damage = sc.Number;
		}
		else if (def == DEF_Projectile && sc.Compare ("DamageType"))
		{
			sc.MustGetString ();
			if (sc.Compare ("Normal"))
			{
				defaults->DamageType = NAME_None;
			}
			else
			{
				defaults->DamageType = sc.String;
			}
		}
		else if (def == DEF_Projectile && sc.Compare ("Speed"))
		{
			sc.MustGetFloat ();
			defaults->Speed = fixed_t(sc.Float * 65536.f);
		}
		else if (sc.Compare ("Mass"))
		{
			sc.MustGetFloat ();
			defaults->Mass = SDWORD(sc.Float);
		}
		else if (sc.Compare ("Translation1"))
		{
			sc.MustGetNumber ();
			if (sc.Number < 0 || sc.Number > 2)
			{
				sc.ScriptError ("Translation1 must be in the range [0,2]");
			}
			defaults->Translation = TRANSLATION(TRANSLATION_Standard, sc.Number);
		}
		else if (sc.Compare ("Translation2"))
		{
			sc.MustGetNumber ();
			if (sc.Number < 0 || sc.Number >= MAX_ACS_TRANSLATIONS)
			{
#define ERROR(foo) "Translation2 must be in the range [0," #foo "]"
				sc.ScriptError (ERROR(MAX_ACS_TRANSLATIONS));
#undef ERROR
			}
			defaults->Translation = TRANSLATION(TRANSLATION_LevelScripted, sc.Number);
		}
		else if ((def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			sc.Compare ("DeathSound"))
		{
			sc.MustGetString ();
			defaults->DeathSound = sc.String;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnDeathSound"))
		{
			sc.MustGetString ();
			defaults->ActiveSound = sc.String;
		}
		else if (def == DEF_Projectile && sc.Compare ("SpawnSound"))
		{
			sc.MustGetString ();
			defaults->SeeSound = sc.String;
		}
		else if (def == DEF_Projectile && sc.Compare ("DoomBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_DOOMBOUNCE;
		}
		else if (def == DEF_Projectile && sc.Compare ("HereticBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_HERETICBOUNCE;
		}
		else if (def == DEF_Projectile && sc.Compare ("HexenBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_HEXENBOUNCE;
		}
		else if (def == DEF_Pickup && sc.Compare ("PickupSound"))
		{
			sc.MustGetString ();
			inv->PickupSound = sc.String;
		}
		else if (def == DEF_Pickup && sc.Compare ("PickupMessage"))
		{
			sc.MustGetString ();
			info->Class->Meta.SetMetaString(AIMETA_PickupMessage, sc.String);
		}
		else if (def == DEF_Pickup && sc.Compare ("Respawns"))
		{
			inv->Respawnable = true;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("SolidOnDeath"))
		{
			extra.bSolidOnDeath = true;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("SolidOnBurn"))
		{
			extra.bSolidOnBurn = true;
		}
		else if (sc.String[0] != '*')
		{
			int bit = sc.MatchString (FlagNames1);
			if (bit != -1)
			{
				defaults->flags |= 1 << bit;
			}
			else if ((bit = sc.MatchString (FlagNames2)) != -1)
			{
				defaults->flags2 |= 1 << bit;
			}
			else if ((bit = sc.MatchString (FlagNames3)) != -1)
			{
				defaults->flags3 |= 1 << bit;
			}
			else
			{
				sc.ScriptError (NULL);
			}
		}
		else
		{
			sc.ScriptError (NULL);
		}
		sc.MustGetString ();
	}

	unsigned int i;
	int spr = GetSpriteIndex(sprite);

	for (i = 0; i < states.Size(); ++i)
	{
		states[i].sprite = spr;
	}
	if (extra.DeathSprite[0] && extra.DeathEnd != 0)
	{
		int spr = GetSpriteIndex(extra.DeathSprite);
		for (i = extra.DeathStart; i < extra.DeathEnd; ++i)
		{
			states[i].sprite = spr;
		}
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

static void ParseSpriteFrames (FActorInfo *info, TArray<FState> &states, FScanner &sc)
{
	FState state;
	char *token = strtok (sc.String, ",\t\n\r");

	memset (&state, 0, sizeof(state));

	while (token != NULL)
	{
		// Skip leading white space
		while (*token == ' ')
			token++;

		int rate = 4;
		bool firstState = true;
		char *colon = strchr (token, ':');

		if (colon != NULL)
		{
			char *stop;

			*colon = 0;
			rate = strtol (token, &stop, 10);
			if (stop == token || rate < 1 || rate > 65534)
			{
				sc.ScriptError ("Rates must be in the range [0,65534]");
			}
			token = colon + 1;
		}

		state.Tics = rate;

		while (*token)
		{
			if (*token == ' ')
			{
			}
			else if (*token == '*')
			{
				if (firstState)
				{
					sc.ScriptError ("* must come after a frame");
				}
				state.Frame |= SF_FULLBRIGHT;
			}
			else if (*token < 'A' || *token > ']')
			{
				sc.ScriptError ("Frames must be A-Z, [, \\, or ]");
			}
			else
			{
				if (!firstState)
				{
					states.Push (state);
				}
				firstState = false;
				state.Frame = *token-'A';	
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

//===========================================================================
//
// A_ScreamAndUnblock
//
//===========================================================================

void A_ScreamAndUnblock (AActor *actor)
{
	A_Scream (actor);
	A_NoBlocking (actor);
}

//===========================================================================
//
// A_ActiveAndUnblock
//
//===========================================================================

void A_ActiveAndUnblock (AActor *actor)
{
	A_ActiveSound (actor);
	A_NoBlocking (actor);
}

//===========================================================================
//
// A_ActiveSound
//
//===========================================================================

void A_ActiveSound (AActor *actor)
{
	if (actor->ActiveSound)
	{
		S_Sound (actor, CHAN_VOICE, actor->ActiveSound, 1, ATTN_NORM);
	}
}
