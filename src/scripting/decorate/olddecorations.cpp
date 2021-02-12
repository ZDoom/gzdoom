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
#include "r_defs.h"
#include "a_pickups.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "decallib.h"
#include "thingdef.h"
#include "codegen.h"

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
	double DeathHeight, BurnHeight;
};

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ParseInsideDecoration (Baggage &bag, AActor *defaults,
	FExtraInfo &extra, EDefinitionType def, FScanner &sc, TArray<FState> &StateArray, TArray<FScriptPosition> &SourceLines);
static void ParseSpriteFrames (PClassActor *info, TArray<FState> &states, TArray<FScriptPosition> &SourceLines, FScanner &sc);

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
	nullptr
};

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ParseOldDecoration
//
// Reads an old style decoration object
//
//==========================================================================
PClassActor *DecoDerivedClass(const FScriptPosition &sc, PClassActor *parent, FName typeName);

void ParseOldDecoration(FScanner &sc, EDefinitionType def, PNamespace *ns)
{
	Baggage bag;
	TArray<FState> StateArray;
	TArray<FScriptPosition> SourceLines;
	FExtraInfo extra;
	PClassActor *type;
	PClassActor *parent;

	parent = (def == DEF_Pickup) ? PClass::FindActor("FakeInventory") : RUNTIME_CLASS(AActor);

	sc.MustGetString();
	FName typeName = FName(sc.String);
	type = DecoDerivedClass(FScriptPosition(sc), parent, typeName);
	ResetBaggage(&bag, parent);
	bag.Namespace = ns;
	bag.Info = type;
	bag.fromDecorate = true;
	bag.Version = { 2, 0, 0 };
#ifdef _DEBUG
	bag.ClassName = type->TypeName.GetChars();
#endif

	sc.MustGetStringName("{");

	memset (&extra, 0, sizeof(extra));
	ParseInsideDecoration (bag, (AActor *)(type->Defaults), extra, def, sc, StateArray, SourceLines);

	if (StateArray.Size() == 0)
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
		FState icecopy = StateArray[extra.IceDeathEnd-1];
		FScriptPosition icepos = SourceLines[extra.IceDeathEnd - 1];
		StateArray.Push (icecopy);
		SourceLines.Push(icepos);
	}

	FState *states;
	states = (FState*)ClassDataAllocator.Alloc(StateArray.Size() * sizeof(FState));
	SaveStateSourceLines(states, SourceLines);
	memcpy (states, &StateArray[0], StateArray.Size() * sizeof(states[0]));
	if (StateArray.Size() == 1)
	{
		states->Tics = -1;
		states->TicRange = 0;
		states->Misc1 = 0;
	}
	else
	{
		size_t i;

		// Spawn states loop endlessly
		for (i = extra.SpawnStart; i < extra.SpawnEnd-1; ++i)
		{
			states[i].NextState = &states[i+1];
		}
		states[i].NextState = &states[extra.SpawnStart];

		// Death states are one-shot and freeze on the final state
		if (extra.DeathEnd != 0)
		{
			for (i = extra.DeathStart; i < extra.DeathEnd-1; ++i)
			{
				states[i].NextState = &states[i+1];
			}
			FState *state = &states[i];
			if (extra.bDiesAway || def == DEF_Projectile)
			{
				state->NextState = nullptr;
			}
			else
			{
				state->Tics = -1;
				state->TicRange = 0;
				state->Misc1 = 0;
			}

			if (def == DEF_Projectile)
			{
				if (extra.bExplosive)
				{
					states[extra.DeathStart].SetAction("A_Explode");
				}
			}
			else
			{
				// The first frame plays the death sound and
				// the second frame makes it nonsolid.
				states[extra.DeathStart].SetAction("A_Scream");
				if (extra.bSolidOnDeath)
				{
				}
				else if (extra.DeathStart + 1 < extra.DeathEnd)
				{
					states[extra.DeathStart+1].SetAction("A_NoBlocking");
				}
				else
				{
					states[extra.DeathStart].SetAction("A_ScreamAndUnblock");
				}

				if (extra.DeathHeight == 0)
				{
					extra.DeathHeight = ((AActor*)(type->Defaults))->Height;
				}
				((AActor*)(type->Defaults))->FloatVar("DeathHeight") = extra.DeathHeight;
			}
			bag.statedef.SetStateLabel("Death", &states[extra.DeathStart]);
		}

		// Burn states are the same as death states, except they can optionally terminate
		if (extra.FireDeathEnd != 0)
		{
			for (i = extra.FireDeathStart; i < extra.FireDeathEnd-1; ++i)
			{
				states[i].NextState = &states[i+1];
			}
			FState *state = &states[i];
			if (extra.bBurnAway)
			{
				state->NextState = nullptr;
			}
			else
			{
				state->Tics = -1;
				state->TicRange = 0;
				state->Misc1 = 0;
			}

			// The first frame plays the burn sound and
			// the second frame makes it nonsolid.
			states[extra.FireDeathStart].SetAction("A_ActiveSound");
			if (extra.bSolidOnBurn)
			{
			}
			else if (extra.FireDeathStart + 1 < extra.FireDeathEnd)
			{
				states[extra.FireDeathStart+1].SetAction("A_NoBlocking");
			}
			else
			{
				states[extra.FireDeathStart].SetAction("A_ActiveAndUnblock");
			}

			if (extra.BurnHeight == 0) extra.BurnHeight = ((AActor*)(type->Defaults))->Height;
			((AActor*)(type->Defaults))->FloatVar("BurnHeight") = extra.BurnHeight;

			bag.statedef.SetStateLabel("Burn", &states[extra.FireDeathStart]);
		}

		// Ice states are similar to burn and death, except their final frame enters
		// a loop that eventually causes them to bust to pieces.
		if (extra.IceDeathEnd != 0)
		{
			for (i = extra.IceDeathStart; i < extra.IceDeathEnd-1; ++i)
			{
				states[i].NextState = &states[i+1];
			}
			FState *state = &states[i];
			state->NextState = &states[StateArray.Size() - 1];
			state->Tics = 5;
			state->TicRange = 0;
			state->Misc1 = 0;
			state->SetAction("A_FreezeDeath");

			i = StateArray.Size() - 1;
			state->NextState = &states[i];
			state->Tics = 1;
			state->TicRange = 0;
			state->Misc1 = 0;
			state->SetAction("A_FreezeDeathChunks");
			bag.statedef.SetStateLabel("Ice", &states[extra.IceDeathStart]);
		}
		else if (extra.bGenericIceDeath)
		{
			bag.statedef.SetStateLabel("Ice", RUNTIME_CLASS(AActor)->FindState(NAME_GenericFreezeDeath));
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
	bag.statedef.SetStateLabel("Spawn", &states[extra.SpawnStart]);
	bag.statedef.InstallStates (type, ((AActor *)(type->Defaults)));
	bag.Info->ActorInfo()->OwnedStates = states;
	bag.Info->ActorInfo()->NumOwnedStates = StateArray.Size();
}

//==========================================================================
//
// ParseInsideDecoration
//
// Parses attributes out of a definition terminated with a }
//
//==========================================================================

static void ParseInsideDecoration (Baggage &bag, AActor *defaults,
	FExtraInfo &extra, EDefinitionType def, FScanner &sc, TArray<FState> &StateArray, TArray<FScriptPosition> &SourceLines)
{
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
			bag.Info->ActorInfo()->DoomEdNum = (int16_t)sc.Number;
		}
		else if (sc.Compare ("SpawnNum"))
		{
			sc.MustGetNumber ();
			if (sc.Number < 0 || sc.Number > 255)
			{
				sc.ScriptError ("SpawnNum must be in the range [0,255]");
			}
			bag.Info->ActorInfo()->SpawnID = (uint8_t)sc.Number;
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
			extra.SpawnStart = StateArray.Size();
			ParseSpriteFrames (bag.Info, StateArray, SourceLines, sc);
			extra.SpawnEnd = StateArray.Size();
		}
		else if ((def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			sc.Compare ("DeathFrames"))
		{
			sc.MustGetString ();
			extra.DeathStart = StateArray.Size();
			ParseSpriteFrames (bag.Info, StateArray, SourceLines, sc);
			extra.DeathEnd = StateArray.Size();
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("IceDeathFrames"))
		{
			sc.MustGetString ();
			extra.IceDeathStart = StateArray.Size();
			ParseSpriteFrames (bag.Info, StateArray, SourceLines, sc);
			extra.IceDeathEnd = StateArray.Size();
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnDeathFrames"))
		{
			sc.MustGetString ();
			extra.FireDeathStart = StateArray.Size();
			ParseSpriteFrames (bag.Info, StateArray, SourceLines, sc);
			extra.FireDeathEnd = StateArray.Size();
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
			defaults->Alpha = clamp (sc.Float, 0.0, 1.0);
		}
		else if (sc.Compare ("Scale"))
		{
			sc.MustGetFloat ();
			defaults->Scale.X = defaults->Scale.Y = sc.Float;
		}
		else if (sc.Compare ("RenderStyle"))
		{
			sc.MustGetString ();
			defaults->RenderStyle = LegacyRenderStyles[sc.MustMatchString (RenderStyles)];
		}
		else if (sc.Compare ("Radius"))
		{
			sc.MustGetFloat ();
			defaults->radius = sc.Float;
		}
		else if (sc.Compare ("Height"))
		{
			sc.MustGetFloat ();
			defaults->Height = sc.Float;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("DeathHeight"))
		{
			sc.MustGetFloat ();
			extra.DeathHeight = sc.Float;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("BurnHeight"))
		{
			sc.MustGetFloat ();
			extra.BurnHeight = sc.Float;
		}
		else if (def == DEF_BreakableDecoration && sc.Compare ("Health"))
		{
			sc.MustGetNumber ();
			defaults->health = sc.Number;
		}
		else if (def == DEF_Projectile && sc.Compare ("ExplosionRadius"))
		{
			sc.MustGetNumber ();
			defaults->IntVar(NAME_ExplosionRadius) = sc.Number;
			extra.bExplosive = true;
		}
		else if (def == DEF_Projectile && sc.Compare ("ExplosionDamage"))
		{
			sc.MustGetNumber ();
			defaults->IntVar(NAME_ExplosionDamage) = sc.Number;
			extra.bExplosive = true;
		}
		else if (def == DEF_Projectile && sc.Compare ("DoNotHurtShooter"))
		{
			defaults->BoolVar(NAME_DontHurtShooter) = true;
		}
		else if (def == DEF_Projectile && sc.Compare ("Damage"))
		{
			sc.MustGetNumber ();
			defaults->SetDamage(sc.Number);
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
			defaults->Speed = sc.Float;
		}
		else if (sc.Compare ("Mass"))
		{
			sc.MustGetFloat ();
			defaults->Mass = int32_t(sc.Float);
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
			defaults->BounceFlags = BOUNCE_DoomCompat;
		}
		else if (def == DEF_Projectile && sc.Compare ("HereticBounce"))
		{
			defaults->BounceFlags = BOUNCE_HereticCompat;
		}
		else if (def == DEF_Projectile && sc.Compare ("HexenBounce"))
		{
			defaults->BounceFlags = BOUNCE_HexenCompat;
		}
		else if (def == DEF_Pickup && sc.Compare ("PickupSound"))
		{
			sc.MustGetString ();
			defaults->IntVar(NAME_PickupSound) = FSoundID(sc.String);
		}
		else if (def == DEF_Pickup && sc.Compare ("PickupMessage"))
		{
			sc.MustGetString ();
			defaults->StringVar(NAME_PickupMsg) = sc.String;
		}
		else if (def == DEF_Pickup && sc.Compare ("Respawns"))
		{
			defaults->BoolVar(NAME_Respawnable) = true;
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
			HandleActorFlag(sc, bag, sc.String, nullptr, '+');
		}
		else
		{
			sc.ScriptError (nullptr);
		}
		sc.MustGetString ();
	}

	unsigned int i;
	int spr = GetSpriteIndex(sprite);

	for (i = 0; i < StateArray.Size(); ++i)
	{
		StateArray[i].sprite = spr;
	}
	if (extra.DeathSprite[0] && extra.DeathEnd != 0)
	{
		int spr = GetSpriteIndex(extra.DeathSprite);
		for (i = extra.DeathStart; i < extra.DeathEnd; ++i)
		{
			StateArray[i].sprite = spr;
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

static void ParseSpriteFrames (PClassActor *info, TArray<FState> &states, TArray<FScriptPosition> &SourceLines, FScanner &sc)
{
	FState state;
	char *token = strtok (sc.String, ",\t\n\r");

	memset (&state, 0, sizeof(state));
	state.UseFlags = info->ActorInfo()->DefaultStateUsage;

	while (token != nullptr)
	{
		// Skip leading white space
		while (*token == ' ')
			token++;

		int rate = 4;
		bool firstState = true;
		char *colon = strchr (token, ':');

		if (colon != nullptr)
		{
			char *stop;

			*colon = 0;
			rate = (int)strtoll (token, &stop, 10);
			if (stop == token || rate < 1 || rate > 65534)
			{
				sc.ScriptError ("Rates must be in the range [0,65534]");
			}
			token = colon + 1;
		}

		state.Tics = rate;
		state.TicRange = 0;

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
				state.StateFlags |= STF_FULLBRIGHT;
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
					SourceLines.Push(sc);
				}
				firstState = false;
				state.Frame = *token-'A';	
			}
			++token;
		}
		if (!firstState)
		{
			states.Push (state);
			SourceLines.Push(sc);
		}

		token = strtok (nullptr, ",\t\n\r");
	}
}

