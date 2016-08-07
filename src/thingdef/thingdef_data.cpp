/*
** thingdef_data.cpp
**
** DECORATE data tables
**
**---------------------------------------------------------------------------
** Copyright 2002-2008 Christoph Oelckers
** Copyright 2004-2008 Randy Heit
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

#include "thingdef.h"
#include "actor.h"
#include "d_player.h"
#include "p_effect.h"
#include "autosegs.h"

static TArray<FPropertyInfo*> properties;
static TArray<AFuncDesc> AFTable;

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { (unsigned int)prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable) }
#define DEFINE_FLAG2(symbol, name, type, variable) { (unsigned int)symbol, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable) }
#define DEFINE_DEPRECATED_FLAG(name) { DEPF_##name, #name, -1, 0 }
#define DEFINE_DUMMY_FLAG(name) { DEPF_UNUSED, #name, -1, 0 }

static FFlagDef ActorFlagDefs[]=
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
	DEFINE_FLAG(MF2, DONTSEEKINVISIBLE, AActor, flags2),
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
	DEFINE_FLAG(MF2, THRUACTORS, AActor, flags2),
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
	DEFINE_FLAG(MF3, AVOIDMELEE, AActor, flags3),
	DEFINE_FLAG(MF3, SCREENSEEKER, AActor, flags3),
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
	DEFINE_FLAG(MF4, NOSPLASHALERT, AActor, flags4),
	DEFINE_FLAG(MF4, SYNCHRONIZED, AActor, flags4),
	DEFINE_FLAG(MF4, NOTARGETSWITCH, AActor, flags4),
	DEFINE_FLAG(MF4, DONTHARMCLASS, AActor, flags4),
	DEFINE_FLAG2(MF4_DONTHARMCLASS, DONTHURTSPECIES, AActor, flags4), // Deprecated name as an alias
	DEFINE_FLAG(MF4, SHIELDREFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, DEFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, ALLOWPARTICLES, AActor, flags4),
	DEFINE_FLAG(MF4, EXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, NOEXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, FRIGHTENED, AActor, flags4),
	DEFINE_FLAG(MF4, NOSKIN, AActor, flags4),
	DEFINE_FLAG(MF4, BOSSDEATH, AActor, flags4),

	DEFINE_FLAG(MF5, DONTDRAIN, AActor, flags5),
	DEFINE_FLAG(MF5, NODROPOFF, AActor, flags5),
	DEFINE_FLAG(MF5, NOFORWARDFALL, AActor, flags5),
	DEFINE_FLAG(MF5, COUNTSECRET, AActor, flags5),
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
	DEFINE_FLAG(MF5, BRIGHT, AActor, flags5),
	DEFINE_FLAG(MF5, CANTSEEK, AActor, flags5),
	DEFINE_FLAG(MF5, PAINLESS, AActor, flags5),
	DEFINE_FLAG(MF5, MOVEWITHSECTOR, AActor, flags5),

	DEFINE_FLAG(MF6, NOBOSSRIP, AActor, flags6),
	DEFINE_FLAG(MF6, THRUSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, MTHRUSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, FORCEPAIN, AActor, flags6),
	DEFINE_FLAG(MF6, NOFEAR, AActor, flags6),
	DEFINE_FLAG(MF6, BUMPSPECIAL, AActor, flags6),
	DEFINE_FLAG(MF6, DONTHARMSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, STEPMISSILE, AActor, flags6),
	DEFINE_FLAG(MF6, NOTELEFRAG, AActor, flags6),
	DEFINE_FLAG(MF6, TOUCHY, AActor, flags6),
	DEFINE_FLAG(MF6, CANJUMP, AActor, flags6),
	DEFINE_FLAG(MF6, JUMPDOWN, AActor, flags6),
	DEFINE_FLAG(MF6, VULNERABLE, AActor, flags6),
	DEFINE_FLAG(MF6, NOTRIGGER, AActor, flags6),
	DEFINE_FLAG(MF6, ADDITIVEPOISONDAMAGE, AActor, flags6),
	DEFINE_FLAG(MF6, ADDITIVEPOISONDURATION, AActor, flags6),
	DEFINE_FLAG(MF6, BLOCKEDBYSOLIDACTORS, AActor, flags6),
	DEFINE_FLAG(MF6, NOMENU, AActor, flags6),
	DEFINE_FLAG(MF6, SEEINVISIBLE, AActor, flags6),
	DEFINE_FLAG(MF6, DONTCORPSE, AActor, flags6),
	DEFINE_FLAG(MF6, DOHARMSPECIES, AActor, flags6),
	DEFINE_FLAG(MF6, POISONALWAYS, AActor, flags6),
	DEFINE_FLAG(MF6, NOTAUTOAIMED, AActor, flags6),
	DEFINE_FLAG(MF6, NOTONAUTOMAP, AActor, flags6),
	DEFINE_FLAG(MF6, RELATIVETOFLOOR, AActor, flags6),

	DEFINE_FLAG(MF7, NEVERTARGET, AActor, flags7),
	DEFINE_FLAG(MF7, NOTELESTOMP, AActor, flags7),
	DEFINE_FLAG(MF7, ALWAYSTELEFRAG, AActor, flags7),
	DEFINE_FLAG(MF7, WEAPONSPAWN, AActor, flags7),
	DEFINE_FLAG(MF7, HARMFRIENDS, AActor, flags7),
	DEFINE_FLAG(MF7, BUDDHA, AActor, flags7),
	DEFINE_FLAG(MF7, FOILBUDDHA, AActor, flags7),
	DEFINE_FLAG(MF7, DONTTHRUST, AActor, flags7),
	DEFINE_FLAG(MF7, ALLOWPAIN, AActor, flags7),
	DEFINE_FLAG(MF7, CAUSEPAIN, AActor, flags7),
	DEFINE_FLAG(MF7, THRUREFLECT, AActor, flags7),
	DEFINE_FLAG(MF7, MIRRORREFLECT, AActor, flags7),
	DEFINE_FLAG(MF7, AIMREFLECT, AActor, flags7),
	DEFINE_FLAG(MF7, HITTARGET, AActor, flags7),
	DEFINE_FLAG(MF7, HITMASTER, AActor, flags7),
	DEFINE_FLAG(MF7, HITTRACER, AActor, flags7),
	DEFINE_FLAG(MF7, NODECAL, AActor, flags7),		// [ZK] Decal flags
	DEFINE_FLAG(MF7, FORCEDECAL, AActor, flags7),
	DEFINE_FLAG(MF7, LAXTELEFRAGDMG, AActor, flags7),
	DEFINE_FLAG(MF7, ICESHATTER, AActor, flags7),
	DEFINE_FLAG(MF7, ALLOWTHRUFLAGS, AActor, flags7),
	DEFINE_FLAG(MF7, USEKILLSCRIPTS, AActor, flags7),
	DEFINE_FLAG(MF7, NOKILLSCRIPTS, AActor, flags7),
	DEFINE_FLAG(MF7, SPRITEANGLE, AActor, flags7),

	// Effect flags
	DEFINE_FLAG(FX, VISIBILITYPULSE, AActor, effects),
	DEFINE_FLAG2(FX_ROCKET, ROCKETTRAIL, AActor, effects),
	DEFINE_FLAG2(FX_GRENADE, GRENADETRAIL, AActor, effects),
	DEFINE_FLAG(RF, INVISIBLE, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, FORCEXYBILLBOARD, AActor, renderflags),
	DEFINE_FLAG(RF, ROLLSPRITE, AActor, renderflags), // [marrub] roll the sprite billboard 
			// [fgsfds] Flat sprites
	DEFINE_FLAG(RF, FLATSPRITE, AActor, renderflags),
	DEFINE_FLAG(RF, WALLSPRITE, AActor, renderflags),
	DEFINE_FLAG(RF, DONTFLIP, AActor, renderflags),
	DEFINE_FLAG(RF, ROLLCENTER, AActor, renderflags),

	// Bounce flags
	DEFINE_FLAG2(BOUNCE_Walls, BOUNCEONWALLS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Floors, BOUNCEONFLOORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Ceilings, BOUNCEONCEILINGS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Actors, ALLOWBOUNCEONACTORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AutoOff, BOUNCEAUTOOFF, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_HereticType, BOUNCELIKEHERETIC, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_CanBounceWater, CANBOUNCEWATER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_NoWallSound, NOWALLBOUNCESND, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_Quiet, NOBOUNCESOUND, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AllActors, BOUNCEONACTORS, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_ExplodeOnWater, EXPLODEONWATER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_MBF, MBFBOUNCER, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_AutoOffFloorOnly, BOUNCEAUTOOFFFLOORONLY, AActor, BounceFlags),
	DEFINE_FLAG2(BOUNCE_UseBounceState, USEBOUNCESTATE, AActor, BounceFlags),

	// Deprecated flags. Handling must be performed in HandleDeprecatedFlags
	DEFINE_DEPRECATED_FLAG(FIREDAMAGE),
	DEFINE_DEPRECATED_FLAG(ICEDAMAGE),
	DEFINE_DEPRECATED_FLAG(LOWGRAVITY),
	DEFINE_DEPRECATED_FLAG(SHORTMISSILERANGE),
	DEFINE_DEPRECATED_FLAG(LONGMELEERANGE),
	DEFINE_DEPRECATED_FLAG(QUARTERGRAVITY),
	DEFINE_DEPRECATED_FLAG(FIRERESIST),
	DEFINE_DEPRECATED_FLAG(HERETICBOUNCE),
	DEFINE_DEPRECATED_FLAG(HEXENBOUNCE),
	DEFINE_DEPRECATED_FLAG(DOOMBOUNCE),

	// Deprecated flags with no more existing functionality.
	DEFINE_DUMMY_FLAG(FASTER),				// obsolete, replaced by 'Fast' state flag
	DEFINE_DUMMY_FLAG(FASTMELEE),			// obsolete, replaced by 'Fast' state flag

	// Various Skulltag flags that are quite irrelevant to ZDoom
	DEFINE_DUMMY_FLAG(NONETID),				// netcode-based
	DEFINE_DUMMY_FLAG(ALLOWCLIENTSPAWN),	// netcode-based
	DEFINE_DUMMY_FLAG(CLIENTSIDEONLY),	    // netcode-based
	DEFINE_DUMMY_FLAG(SERVERSIDEONLY),		// netcode-based
	DEFINE_DUMMY_FLAG(EXPLODEONDEATH),	    // seems useless
};

static FFlagDef InventoryFlagDefs[] =
{
	// Inventory flags
	DEFINE_FLAG(IF, QUIET, AInventory, ItemFlags),
	DEFINE_FLAG(IF, AUTOACTIVATE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNDROPPABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, INVBAR, AInventory, ItemFlags),
	DEFINE_FLAG(IF, HUBPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, UNTOSSABLE, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ADDITIVETIME, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ALWAYSPICKUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, FANCYPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, BIGPOWERUP, AInventory, ItemFlags),
	DEFINE_FLAG(IF, KEEPDEPLETED, AInventory, ItemFlags),
	DEFINE_FLAG(IF, IGNORESKILL, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NOATTENPICKUPSOUND, AInventory, ItemFlags),
	DEFINE_FLAG(IF, PERSISTENTPOWER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, RESTRICTABSOLUTELY, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NEVERRESPAWN, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NOSCREENFLASH, AInventory, ItemFlags),
	DEFINE_FLAG(IF, TOSSED, AInventory, ItemFlags),
	DEFINE_FLAG(IF, ALWAYSRESPAWN, AInventory, ItemFlags),
	DEFINE_FLAG(IF, TRANSFER, AInventory, ItemFlags),
	DEFINE_FLAG(IF, NOTELEPORTFREEZE, AInventory, ItemFlags),

	DEFINE_DEPRECATED_FLAG(PICKUPFLASH),
	DEFINE_DEPRECATED_FLAG(INTERHUBSTRIP),
};

static FFlagDef WeaponFlagDefs[] =
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
	DEFINE_FLAG(WIF, MELEEWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF_BOT, BFG, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, CHEATNOTWEAPON, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NO_AUTO_SWITCH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, AMMO_CHECKBOTH, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NOAUTOAIM, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NODEATHDESELECT, AWeapon, WeaponFlags),
	DEFINE_FLAG(WIF, NODEATHINPUT, AWeapon, WeaponFlags),
	
	DEFINE_DUMMY_FLAG(NOLMS),
	DEFINE_FLAG(WIF, ALT_USES_BOTH, AWeapon, WeaponFlags),
	DEFINE_DUMMY_FLAG(ALLOW_WITH_RESPAWN_INVUL),
};

static FFlagDef PlayerPawnFlagDefs[] =
{
	// PlayerPawn flags
	DEFINE_FLAG(PPF, NOTHRUSTWHENINVUL, APlayerPawn, PlayerFlags),
	DEFINE_FLAG(PPF, CANSUPERMORPH, APlayerPawn, PlayerFlags),
	DEFINE_FLAG(PPF, CROUCHABLEMORPH, APlayerPawn, PlayerFlags),
};

static FFlagDef PowerSpeedFlagDefs[] =
{
	// PowerSpeed flags
	DEFINE_FLAG(PSF, NOTRAIL, APowerSpeed, SpeedFlags),
};

static const struct FFlagList { const PClass * const *Type; FFlagDef *Defs; int NumDefs; } FlagLists[] =
{
	{ &RUNTIME_CLASS_CASTLESS(AActor), 		ActorFlagDefs,		countof(ActorFlagDefs) },
	{ &RUNTIME_CLASS_CASTLESS(AInventory), 	InventoryFlagDefs,	countof(InventoryFlagDefs) },
	{ &RUNTIME_CLASS_CASTLESS(AWeapon), 	WeaponFlagDefs,		countof(WeaponFlagDefs) },
	{ &RUNTIME_CLASS_CASTLESS(APlayerPawn),	PlayerPawnFlagDefs,	countof(PlayerPawnFlagDefs) },
	{ &RUNTIME_CLASS_CASTLESS(APowerSpeed),	PowerSpeedFlagDefs,	countof(PowerSpeedFlagDefs) },
};
#define NUM_FLAG_LISTS (countof(FlagLists))

//==========================================================================
//
// Find a flag by name using a binary search
//
//==========================================================================
static FFlagDef *FindFlag (FFlagDef *flags, int numflags, const char *flag)
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

//==========================================================================
//
// Finds a flag that may have a qualified name
//
//==========================================================================

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2)
{
	FFlagDef *def;
	size_t i;

	if (part2 == NULL)
	{ // Search all lists
		for (i = 0; i < NUM_FLAG_LISTS; ++i)
		{
			if (type->IsDescendantOf (*FlagLists[i].Type))
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
			if (stricmp ((*FlagLists[i].Type)->TypeName.GetChars(), part1) == 0)
			{
				if (type->IsDescendantOf (*FlagLists[i].Type))
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


//==========================================================================
//
// Gets the name of an actor flag 
//
//==========================================================================

const char *GetFlagName(unsigned int flagnum, int flagoffset)
{
	for(size_t i = 0; i < countof(ActorFlagDefs); i++)
	{
		if (ActorFlagDefs[i].flagbit == flagnum && ActorFlagDefs[i].structoffset == flagoffset)
		{
			return ActorFlagDefs[i].name;
		}
	}
	return "(unknown)";	// return something printable
}

//==========================================================================
//
// Find a property by name using a binary search
//
//==========================================================================

FPropertyInfo *FindProperty(const char * string)
{
	int min = 0, max = properties.Size()-1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp (string, properties[mid]->name);
		if (lexval == 0)
		{
			return properties[mid];
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
// Find a function by name using a binary search
//
//==========================================================================

AFuncDesc *FindFunction(const char * string)
{
	int min = 0, max = AFTable.Size()-1;

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


//==========================================================================
//
// Find an action function in AActor's table
//
//==========================================================================

PFunction *FindGlobalActionFunction(const char *name)
{
	return dyn_cast<PFunction>(RUNTIME_CLASS(AActor)->Symbols.FindSymbol(name, false));
}


//==========================================================================
//
// Sorting helpers
//
//==========================================================================

static int flagcmp (const void * a, const void * b)
{
	return stricmp( ((FFlagDef*)a)->name, ((FFlagDef*)b)->name);
}

static int propcmp(const void * a, const void * b)
{
	return stricmp( (*(FPropertyInfo**)a)->name, (*(FPropertyInfo**)b)->name);
}

static int funccmp(const void * a, const void * b)
{
	return stricmp( ((AFuncDesc*)a)->Name, ((AFuncDesc*)b)->Name);
}

//==========================================================================
//
// Initialization
//
//==========================================================================

void InitThingdef()
{
	// Sort the flag lists
	for (size_t i = 0; i < NUM_FLAG_LISTS; ++i)
	{
		qsort (FlagLists[i].Defs, FlagLists[i].NumDefs, sizeof(FFlagDef), flagcmp);
	}

	// Create a sorted list of properties
	if (properties.Size() == 0)
	{
		FAutoSegIterator probe(GRegHead, GRegTail);

		while (*++probe != NULL)
		{
			properties.Push((FPropertyInfo *)*probe);
		}
		properties.ShrinkToFit();
		qsort(&properties[0], properties.Size(), sizeof(properties[0]), propcmp);
	}

	// Create a sorted list of native action functions
	AFTable.Clear();
	if (AFTable.Size() == 0)
	{
		FAutoSegIterator probe(ARegHead, ARegTail);

		while (*++probe != NULL)
		{
			AFuncDesc *afunc = (AFuncDesc *)*probe;
			assert(afunc->VMPointer != NULL);
			*(afunc->VMPointer) = new VMNativeFunction(afunc->Function, afunc->Name);
			AFTable.Push(*afunc);
		}
		AFTable.ShrinkToFit();
		qsort(&AFTable[0], AFTable.Size(), sizeof(AFTable[0]), funccmp);
	}

	// Define some member variables we feel like exposing to the user
	PSymbolTable &symt = RUNTIME_CLASS(AActor)->Symbols;
	PType *array5 = NewArray(TypeSInt32, 5);
	symt.AddSymbol(new PField(NAME_Alpha,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Alpha)));
	symt.AddSymbol(new PField(NAME_Angle,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Angles.Yaw)));
	symt.AddSymbol(new PField(NAME_Args,			array5,			VARF_Native,				myoffsetof(AActor, args)));
	symt.AddSymbol(new PField(NAME_CeilingZ,		TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, ceilingz)));
	symt.AddSymbol(new PField(NAME_FloorZ,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, floorz)));
	symt.AddSymbol(new PField(NAME_Health,			TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, health)));
	symt.AddSymbol(new PField(NAME_Mass,			TypeSInt32,		VARF_Native,				myoffsetof(AActor, Mass)));
	symt.AddSymbol(new PField(NAME_Pitch,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Angles.Pitch)));
	symt.AddSymbol(new PField(NAME_Roll,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Angles.Roll)));
	symt.AddSymbol(new PField(NAME_Special,			TypeSInt32,		VARF_Native,				myoffsetof(AActor, special)));
	symt.AddSymbol(new PField(NAME_TID,				TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, tid)));
	symt.AddSymbol(new PField(NAME_TIDtoHate,		TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, TIDtoHate)));
	symt.AddSymbol(new PField(NAME_WaterLevel,		TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, waterlevel)));
	symt.AddSymbol(new PField(NAME_X,				TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, __Pos.X)));	// must remain read-only!
	symt.AddSymbol(new PField(NAME_Y,				TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, __Pos.Y)));	// must remain read-only!
	symt.AddSymbol(new PField(NAME_Z,				TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, __Pos.Z)));	// must remain read-only!
	symt.AddSymbol(new PField(NAME_VelX,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.X)));
	symt.AddSymbol(new PField(NAME_VelY,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.Y)));
	symt.AddSymbol(new PField(NAME_VelZ,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.Z)));
	symt.AddSymbol(new PField(NAME_MomX,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.X)));
	symt.AddSymbol(new PField(NAME_MomY,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.Y)));
	symt.AddSymbol(new PField(NAME_MomZ,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Vel.Z)));
	symt.AddSymbol(new PField(NAME_ScaleX,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Scale.X)));
	symt.AddSymbol(new PField(NAME_ScaleY,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Scale.Y)));
	symt.AddSymbol(new PField(NAME_Score,			TypeSInt32,		VARF_Native,				myoffsetof(AActor, Score)));
	symt.AddSymbol(new PField(NAME_Accuracy,		TypeSInt32,		VARF_Native,				myoffsetof(AActor, accuracy)));
	symt.AddSymbol(new PField(NAME_Stamina,			TypeSInt32,		VARF_Native,				myoffsetof(AActor, stamina)));
	symt.AddSymbol(new PField(NAME_Height,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, Height)));
	symt.AddSymbol(new PField(NAME_Radius,			TypeFloat64,	VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, radius)));
	symt.AddSymbol(new PField(NAME_ReactionTime,	TypeSInt32,		VARF_Native,				myoffsetof(AActor, reactiontime)));
	symt.AddSymbol(new PField(NAME_MeleeRange,		TypeFloat64,	VARF_Native,				myoffsetof(AActor, meleerange)));
	symt.AddSymbol(new PField(NAME_Speed,			TypeFloat64,	VARF_Native,				myoffsetof(AActor, Speed)));
	symt.AddSymbol(new PField(NAME_Threshold,		TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, threshold)));
	symt.AddSymbol(new PField(NAME_DefThreshold,	TypeSInt32,		VARF_Native|VARF_ReadOnly,	myoffsetof(AActor, DefThreshold)));
}
