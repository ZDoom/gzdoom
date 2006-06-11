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
#include "i_system.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

enum EDefinitionType
{
	DEF_Decoration,
	DEF_BreakableDecoration,
	DEF_Pickup,
	DEF_Projectile,
};

struct FExtraInfo
{
	char DeathSprite[5];
	unsigned int SpawnStart, SpawnEnd;
	unsigned int DeathStart, DeathEnd;
	unsigned int IceDeathStart, IceDeathEnd;
	unsigned int FireDeathStart, FireDeathEnd;
	bool bSolidOnDeath, bSolidOnBurn;
	bool bBurnAway, bDiesAway, bGenericIceDeath;
	fixed_t DeathHeight, BurnHeight;
	int ExplosionRadius, ExplosionDamage;
	bool ExplosionShooterImmune;
};

class ASimpleProjectile : public AActor
{
	DECLARE_STATELESS_ACTOR (ASimpleProjectile, AActor);
public:
	void Serialize (FArchive &arc)
	{
		Super::Serialize (arc);
		arc << HurtShooter << ExplosionRadius << ExplosionDamage;
	}

	void GetExplodeParms (int &damage, int &dist, bool &hurtSource)
	{
		damage = ExplosionDamage;
		dist = ExplosionRadius;
		hurtSource = HurtShooter;
	}

	bool HurtShooter;
	int ExplosionRadius, ExplosionDamage;
};
IMPLEMENT_ABSTRACT_ACTOR (ASimpleProjectile)

class AFakeInventory : public AInventory
{
	DECLARE_STATELESS_ACTOR (AFakeInventory, AInventory);
public:
	bool Respawnable;

	bool ShouldRespawn ()
	{
		return Respawnable && Super::ShouldRespawn();
	}

	bool TryPickup (AActor *toucher)
	{
		BOOL success = LineSpecials[special] (NULL, toucher, false,
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
IMPLEMENT_STATELESS_ACTOR (AFakeInventory, Any, -1, 0)
	PROP_Flags (MF_SPECIAL)
END_DEFAULTS

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void ProcessActor(void (*process)(FState *, int));
void ProcessWeapon(void (*process)(FState *, int));
void FinishThingdef();
void InitDecorateTranslations();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void A_ScreamAndUnblock (AActor *);
void A_ActiveAndUnblock (AActor *);
void A_ActiveSound (AActor *);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void ParseDecorate (void (*process)(FState *, int));
static void ParseInsideDecoration (FActorInfo *info, AActor *defaults,
	TArray<FState> &states, FExtraInfo &extra, EDefinitionType def);
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
// LoadDecorations
//
// Called from FActor::StaticInit()
//
//==========================================================================

void LoadDecorations (void (*process)(FState *, int))
{
	int lastlump, lump;

	InitDecorateTranslations();
	lastlump = 0;
	while ((lump = Wads.FindLump ("DECORATE", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "DECORATE");
		ParseDecorate (process);
		SC_Close ();
	}
	FinishThingdef();
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
	FExtraInfo extra;
	PClass *type;
	PClass *parent;
	EDefinitionType def;
	FActorInfo *info;
	FName typeName;
	int recursion=0;

	// Get actor class name. The A prefix is added automatically.
	while (true)
	{
		if (!SC_GetString ())
		{
			if (recursion==0) return;
			SC_Close();
			SC_RestoreScriptState();
			recursion--;
			continue;
		}
		if (SC_Compare ("#include"))
		{
			int lump;

			SC_MustGetString();
			// This is not using SC_Open because it can print a more useful error message when done here
			lump = Wads.CheckNumForFullName(sc_String);
			if (lump==-1) lump = Wads.CheckNumForName(sc_String);
			if (lump==-1) SC_ScriptError("Lump '%s' not found", sc_String);
			SC_SaveScriptState();
			SC_OpenLumpNum(lump, sc_String);
			recursion++;
			continue;
		}
		if (SC_Compare ("Actor"))
		{
			ProcessActor (process);
			continue;
		}
		if (SC_Compare ("Pickup"))
		{
			parent = RUNTIME_CLASS(AFakeInventory);
			def = DEF_Pickup;
			SC_MustGetString ();
		}
		else if (SC_Compare ("Breakable"))
		{
			parent = RUNTIME_CLASS(AActor);
			def = DEF_BreakableDecoration;
			SC_MustGetString ();
		}
		else if (SC_Compare ("Projectile"))
		{
			parent = RUNTIME_CLASS(ASimpleProjectile);
			def = DEF_Projectile;
			SC_MustGetString ();
		}
		else
		{
			parent = RUNTIME_CLASS(AActor);
			def = DEF_Decoration;
		}

		typeName = FName(sc_String);
		type = parent->CreateDerivedClass (typeName, parent->Size);
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
			else if (SC_Compare ("Strife"))
			{
				info->GameFilter |= GAME_Strife;
			}
			else if (SC_Compare ("Any"))
			{
				info->GameFilter = GAME_Any;
			}
			else
			{
				SC_ScriptError ("Unknown game type %s", sc_String);
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
		memset (&extra, 0, sizeof(extra));
		ParseInsideDecoration (info, (AActor *)(type->Defaults), states, extra, def);

		info->NumOwnedStates = states.Size();
		if (info->NumOwnedStates == 0)
		{
			SC_ScriptError ("%s does not define any animation frames", typeName.GetChars() );
		}
		else if (extra.SpawnEnd == 0)
		{
			SC_ScriptError ("%s does not have a Frames definition", typeName.GetChars() );
		}
		else if (def == DEF_BreakableDecoration && extra.DeathEnd == 0)
		{
			SC_ScriptError ("%s does not have a DeathFrames definition", typeName.GetChars() );
		}
		else if (extra.IceDeathEnd != 0 && extra.bGenericIceDeath)
		{
			SC_ScriptError ("You cannot use IceDeathFrames and GenericIceDeath together");
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
			info->OwnedStates->Tics = 0;
			info->OwnedStates->Misc1 = 0;
			info->OwnedStates->Frame &= ~SF_BIGTIC;
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
					info->OwnedStates[i].Tics = 0;
					info->OwnedStates[i].Misc1 = 0;
					info->OwnedStates[i].Frame &= ~SF_BIGTIC;
				}

				if (def == DEF_Projectile)
				{
					if (extra.ExplosionRadius > 0)
					{
						info->OwnedStates[extra.DeathStart].Action = A_Explode;
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
				((AActor *)(type->Defaults))->DeathState = &info->OwnedStates[extra.DeathStart];
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
					info->OwnedStates[i].Tics = 0;
					info->OwnedStates[i].Misc1 = 0;
					info->OwnedStates[i].Frame &= ~SF_BIGTIC;
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

				((AActor *)(type->Defaults))->BDeathState = &info->OwnedStates[extra.FireDeathStart];
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
				info->OwnedStates[i].Tics = 6;
				info->OwnedStates[i].Misc1 = 0;
				info->OwnedStates[i].Action = A_FreezeDeath;

				i = info->NumOwnedStates - 1;
				info->OwnedStates[i].NextState = &info->OwnedStates[i];
				info->OwnedStates[i].Tics = 2;
				info->OwnedStates[i].Misc1 = 0;
				info->OwnedStates[i].Action = A_FreezeDeathChunks;
				((AActor *)(type->Defaults))->IDeathState = &info->OwnedStates[extra.IceDeathStart];
			}
			else if (extra.bGenericIceDeath)
			{
				((AActor *)(type->Defaults))->IDeathState = &AActor::States[AActor::S_GENERICFREEZEDEATH];
			}
		}
		if (def == DEF_BreakableDecoration)
		{
			((AActor *)(type->Defaults))->flags |= MF_SHOOTABLE;
		}
		if (def == DEF_Projectile)
		{
			if (extra.ExplosionRadius > 0)
			{
				((ASimpleProjectile *)(type->Defaults))->ExplosionRadius =
					extra.ExplosionRadius;
				((ASimpleProjectile *)(type->Defaults))->ExplosionDamage =
					extra.ExplosionDamage > 0 ? extra.ExplosionDamage : extra.ExplosionRadius;
				((ASimpleProjectile *)(type->Defaults))->HurtShooter =
					!extra.ExplosionShooterImmune;
			}
			((AActor *)(type->Defaults))->flags |= MF_DROPOFF|MF_MISSILE;
		}
		((AActor *)(type->Defaults))->SpawnState = &info->OwnedStates[extra.SpawnStart];
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
	TArray<FState> &states, FExtraInfo &extra, EDefinitionType def)
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
		else if (SC_Compare ("Sprite") || (
			(def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			SC_Compare ("DeathSprite") &&
			(extra.DeathSprite[0] = 1))	// This is intentionally = and not ==
			)
		{
			SC_MustGetString ();
			if (strlen (sc_String) != 4)
			{
				SC_ScriptError ("Sprite name must be exactly four characters long");
			}
			if (extra.DeathSprite[0] == 1)
			{
				memcpy (extra.DeathSprite, sc_String, 4);
			}
			else
			{
				memcpy (sprite, sc_String, 4);
			}
		}
		else if (SC_Compare ("Frames"))
		{
			SC_MustGetString ();
			extra.SpawnStart = states.Size();
			ParseSpriteFrames (info, states);
			extra.SpawnEnd = states.Size();
		}
		else if ((def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			SC_Compare ("DeathFrames"))
		{
			SC_MustGetString ();
			extra.DeathStart = states.Size();
			ParseSpriteFrames (info, states);
			extra.DeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("IceDeathFrames"))
		{
			SC_MustGetString ();
			extra.IceDeathStart = states.Size();
			ParseSpriteFrames (info, states);
			extra.IceDeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("BurnDeathFrames"))
		{
			SC_MustGetString ();
			extra.FireDeathStart = states.Size();
			ParseSpriteFrames (info, states);
			extra.FireDeathEnd = states.Size();
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("GenericIceDeath"))
		{
			extra.bGenericIceDeath = true;
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("BurnsAway"))
		{
			extra.bBurnAway = true;
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("DiesAway"))
		{
			extra.bDiesAway = true;
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
		else if (def == DEF_BreakableDecoration && SC_Compare ("DeathHeight"))
		{
			SC_MustGetFloat ();
			extra.DeathHeight = int(sc_Float * FRACUNIT);
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("BurnHeight"))
		{
			SC_MustGetFloat ();
			extra.BurnHeight = int(sc_Float * FRACUNIT);
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("Health"))
		{
			SC_MustGetNumber ();
			defaults->health = sc_Number;
		}
		else if (def == DEF_Projectile && SC_Compare ("ExplosionRadius"))
		{
			SC_MustGetNumber ();
			extra.ExplosionRadius = sc_Number;
		}
		else if (def == DEF_Projectile && SC_Compare ("ExplosionDamage"))
		{
			SC_MustGetNumber ();
			extra.ExplosionDamage = sc_Number;
		}
		else if (def == DEF_Projectile && SC_Compare ("DoNotHurtShooter"))
		{
			extra.ExplosionShooterImmune = true;
		}
		else if (def == DEF_Projectile && SC_Compare ("Damage"))
		{
			SC_MustGetNumber ();
			defaults->damage = sc_Number;
		}
		else if (def == DEF_Projectile && SC_Compare ("DamageType"))
		{
			SC_MustGetString ();
			if (SC_Compare ("Normal"))
			{
				defaults->DamageType = 0;
			}
			else if (SC_Compare ("Ice"))
			{
				defaults->DamageType = MOD_ICE;
			}
			else if (SC_Compare ("Fire"))
			{
				defaults->DamageType = MOD_FIRE;
			}
			else
			{
				SC_ScriptError ("Unknown damage type \"%s\"", sc_String);
			}
		}
		else if (def == DEF_Projectile && SC_Compare ("Speed"))
		{
			SC_MustGetFloat ();
			defaults->Speed = fixed_t(sc_Float * 65536.f);
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
#undef ERROR
			}
			defaults->Translation = TRANSLATION(TRANSLATION_LevelScripted, sc_Number);
		}
		else if ((def == DEF_BreakableDecoration || def == DEF_Projectile) &&
			SC_Compare ("DeathSound"))
		{
			SC_MustGetString ();
			defaults->DeathSound = S_FindSound (sc_String);
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("BurnDeathSound"))
		{
			SC_MustGetString ();
			defaults->ActiveSound = S_FindSound (sc_String);
		}
		else if (def == DEF_Projectile && SC_Compare ("SpawnSound"))
		{
			SC_MustGetString ();
			defaults->SeeSound = S_FindSound (sc_String);
		}
		else if (def == DEF_Projectile && SC_Compare ("DoomBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_DOOMBOUNCE;
		}
		else if (def == DEF_Projectile && SC_Compare ("HereticBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_HERETICBOUNCE;
		}
		else if (def == DEF_Projectile && SC_Compare ("HexenBounce"))
		{
			defaults->flags2 = (defaults->flags2 & ~MF2_BOUNCETYPE) | MF2_HEXENBOUNCE;
		}
		else if (def == DEF_Pickup && SC_Compare ("PickupSound"))
		{
			SC_MustGetString ();
			inv->PickupSound = S_FindSound (sc_String);
		}
		else if (def == DEF_Pickup && SC_Compare ("PickupMessage"))
		{
			SC_MustGetString ();
			info->Class->Meta.SetMetaString(AIMETA_PickupMessage, sc_String);
		}
		else if (def == DEF_Pickup && SC_Compare ("Respawns"))
		{
			inv->Respawnable = true;
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("SolidOnDeath"))
		{
			extra.bSolidOnDeath = true;
		}
		else if (def == DEF_BreakableDecoration && SC_Compare ("SolidOnBurn"))
		{
			extra.bSolidOnBurn = true;
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

	unsigned int i;

	for (i = 0; i < states.Size(); ++i)
	{
		memcpy (states[i].sprite.name, sprite, 4);
	}
	if (extra.DeathSprite[0] && extra.DeathEnd != 0)
	{
		for (i = extra.DeathStart; i < extra.DeathEnd; ++i)
		{
			memcpy (states[i].sprite.name, extra.DeathSprite, 4);
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
				state.Frame = (rate >= 256) ? (SF_BIGTIC | (*token-'A')) : (*token-'A');	
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
		S_SoundID (actor, CHAN_VOICE, actor->ActiveSound, 1, ATTN_NORM);
	}
}
