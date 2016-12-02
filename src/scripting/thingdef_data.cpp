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
#include "p_maputl.h"
#include "gi.h"
#include "p_terrain.h"
#include "gstrings.h"
#include "zstring.h"
#include "d_event.h"

static TArray<FPropertyInfo*> properties;
static TArray<AFuncDesc> AFTable;
static TArray<FieldDesc> FieldTable;

//==========================================================================
//
// List of all flags
//
//==========================================================================

// [RH] Keep GCC quiet by not using offsetof on Actor types.
#define DEFINE_FLAG(prefix, name, type, variable) { (unsigned int)prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable), VARF_Native }
#define DEFINE_PROTECTED_FLAG(prefix, name, type, variable) { (unsigned int)prefix##_##name, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable), VARF_Native|VARF_ReadOnly|VARF_InternalAccess }
#define DEFINE_FLAG2(symbol, name, type, variable) { (unsigned int)symbol, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable), VARF_Native }
#define DEFINE_FLAG2_DEPRECATED(symbol, name, type, variable) { (unsigned int)symbol, #name, (int)(size_t)&((type*)1)->variable - 1, sizeof(((type *)0)->variable), VARF_Native|VARF_Deprecated }
#define DEFINE_DEPRECATED_FLAG(name) { DEPF_##name, #name, -1, 0, true }
#define DEFINE_DUMMY_FLAG(name, deprec) { DEPF_UNUSED, #name, -1, 0, deprec? VARF_Deprecated:0 }

// internal flags. These do not get exposed to actor definitions but scripts need to be able to access them as variables.
static FFlagDef InternalActorFlagDefs[]=
{
	DEFINE_FLAG(MF, INCHASE, AActor, flags),
	DEFINE_FLAG(MF, UNMORPHED, AActor, flags),
	DEFINE_FLAG(MF2, FLY, AActor, flags2),
	DEFINE_FLAG(MF2, ONMOBJ, AActor, flags2),
	DEFINE_FLAG(MF2, ARGSDEFINED, AActor, flags2),
	DEFINE_FLAG(MF3, NOSIGHTCHECK, AActor, flags3),
	DEFINE_FLAG(MF3, CRASHED, AActor, flags3),
	DEFINE_FLAG(MF3, WARNBOT, AActor, flags3),
	DEFINE_FLAG(MF3, HUNTPLAYERS, AActor, flags3),
	DEFINE_FLAG(MF4, NOHATEPLAYERS, AActor, flags4),
	DEFINE_FLAG(MF4, SCROLLMOVE, AActor, flags4),
	DEFINE_FLAG(MF4, VFRICTION, AActor, flags4),
	DEFINE_FLAG(MF4, BOSSSPAWNED, AActor, flags4),
	DEFINE_FLAG(MF5, AVOIDINGDROPOFF, AActor, flags5),
	DEFINE_FLAG(MF5, CHASEGOAL, AActor, flags5),
	DEFINE_FLAG(MF5, INCONVERSATION, AActor, flags5),
	DEFINE_FLAG(MF6, ARMED, AActor, flags6),
	DEFINE_FLAG(MF6, FALLING, AActor, flags6),
	DEFINE_FLAG(MF6, LINEDONE, AActor, flags6),
	DEFINE_FLAG(MF6, SHATTERING, AActor, flags6),
	DEFINE_FLAG(MF6, KILLED, AActor, flags6),
	DEFINE_FLAG(MF6, BOSSCUBE, AActor, flags6),
	DEFINE_FLAG(MF6, INTRYMOVE, AActor, flags6),
	DEFINE_FLAG(MF7, HANDLENODELAY, AActor, flags7),
	DEFINE_FLAG(MF7, FLYCHEAT, AActor, flags7),
};


static FFlagDef ActorFlagDefs[]=
{
	DEFINE_FLAG(MF, PICKUP, APlayerPawn, flags),
	DEFINE_FLAG(MF, SPECIAL, APlayerPawn, flags),
	DEFINE_FLAG(MF, SOLID, AActor, flags),
	DEFINE_FLAG(MF, SHOOTABLE, AActor, flags),
	DEFINE_PROTECTED_FLAG(MF, NOSECTOR, AActor, flags),
	DEFINE_PROTECTED_FLAG(MF, NOBLOCKMAP, AActor, flags),
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
	DEFINE_FLAG(MF4, SHIELDREFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, DEFLECT, AActor, flags4),
	DEFINE_FLAG(MF4, ALLOWPARTICLES, AActor, flags4),
	DEFINE_FLAG(MF4, EXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, NOEXTREMEDEATH, AActor, flags4),
	DEFINE_FLAG(MF4, FRIGHTENED, AActor, flags4),
	DEFINE_FLAG(MF4, NOSKIN, AActor, flags4),
	DEFINE_FLAG(MF4, BOSSDEATH, AActor, flags4),

	DEFINE_FLAG(MF5, DONTDRAIN, AActor, flags5),
	DEFINE_FLAG(MF5, GETOWNER, AActor, flags5),
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
	DEFINE_FLAG(MF7, SMASHABLE, AActor, flags7),
	DEFINE_FLAG(MF7, NOSHIELDREFLECT, AActor, flags7),

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
	DEFINE_FLAG(RF, MASKROTATION, AActor, renderflags),
	DEFINE_FLAG(RF, ABSMASKANGLE, AActor, renderflags),
	DEFINE_FLAG(RF, ABSMASKPITCH, AActor, renderflags),

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
};

// These won't be accessible through bitfield variables
static FFlagDef MoreFlagDefs[] =
{

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
	DEFINE_DUMMY_FLAG(FASTER, true),				// obsolete, replaced by 'Fast' state flag
	DEFINE_DUMMY_FLAG(FASTMELEE, true),			// obsolete, replaced by 'Fast' state flag

	// Various Skulltag flags that are quite irrelevant to ZDoom
	DEFINE_DUMMY_FLAG(NONETID, false),				// netcode-based
	DEFINE_DUMMY_FLAG(ALLOWCLIENTSPAWN, false),	// netcode-based
	DEFINE_DUMMY_FLAG(CLIENTSIDEONLY, false),	    // netcode-based
	DEFINE_DUMMY_FLAG(SERVERSIDEONLY, false),		// netcode-based
	DEFINE_DUMMY_FLAG(EXPLODEONDEATH, true),	    // seems useless
	DEFINE_FLAG2_DEPRECATED(MF4_DONTHARMCLASS, DONTHURTSPECIES, AActor, flags4), // Deprecated name as an alias
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
	DEFINE_FLAG(WIF, ALT_USES_BOTH, AWeapon, WeaponFlags),

	DEFINE_DUMMY_FLAG(NOLMS, false),
	DEFINE_DUMMY_FLAG(ALLOW_WITH_RESPAWN_INVUL, false),
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

static const struct FFlagList { const PClass * const *Type; FFlagDef *Defs; int NumDefs; int Use; } FlagLists[] =
{
	{ &RUNTIME_CLASS_CASTLESS(AActor), 		ActorFlagDefs,		countof(ActorFlagDefs), 3 },	// -1 to account for the terminator
	{ &RUNTIME_CLASS_CASTLESS(AActor), 		MoreFlagDefs,		countof(MoreFlagDefs), 1 },
	{ &RUNTIME_CLASS_CASTLESS(AActor), 	InternalActorFlagDefs,	countof(InternalActorFlagDefs), 2 },
	{ &RUNTIME_CLASS_CASTLESS(AInventory), 	InventoryFlagDefs,	countof(InventoryFlagDefs), 3 },
	{ &RUNTIME_CLASS_CASTLESS(AWeapon), 	WeaponFlagDefs,		countof(WeaponFlagDefs), 3 },
	{ &RUNTIME_CLASS_CASTLESS(APlayerPawn),	PlayerPawnFlagDefs,	countof(PlayerPawnFlagDefs), 3 },
	{ &RUNTIME_CLASS_CASTLESS(APowerSpeed),	PowerSpeedFlagDefs,	countof(PowerSpeedFlagDefs), 3 },
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

FFlagDef *FindFlag (const PClass *type, const char *part1, const char *part2, bool strict)
{
	FFlagDef *def;

	if (part2 == NULL)
	{ // Search all lists
		int max = strict ? 2 : NUM_FLAG_LISTS;
		for (int i = 0; i < max; ++i)
		{
			if ((FlagLists[i].Use & 1) && type->IsDescendantOf (*FlagLists[i].Type))
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
		for (size_t i = 0; i < NUM_FLAG_LISTS; ++i)
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

AFuncDesc *FindFunction(PStruct *cls, const char * string)
{
	int min = 0, max = AFTable.Size() - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp(cls->TypeName.GetChars(), AFTable[mid].ClassName + 1);
		if (lexval == 0) lexval = stricmp(string, AFTable[mid].FuncName);
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
	return nullptr;
}

//==========================================================================
//
// Find a function by name using a binary search
//
//==========================================================================

FieldDesc *FindField(PStruct *cls, const char * string)
{
	int min = 0, max = FieldTable.Size() - 1;

	while (min <= max)
	{
		int mid = (min + max) / 2;
		int lexval = stricmp(cls->TypeName.GetChars(), FieldTable[mid].ClassName + 1);
		if (lexval == 0) lexval = stricmp(string, FieldTable[mid].FieldName);
		if (lexval == 0)
		{
			return &FieldTable[mid];
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
	return nullptr;
}


//==========================================================================
//
// Find an action function in AActor's table
//
//==========================================================================

VMFunction *FindVMFunction(PClass *cls, const char *name)
{
	auto f = dyn_cast<PFunction>(cls->Symbols.FindSymbol(name, true));
	return f == nullptr ? nullptr : f->Variants[0].Implementation;
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
	// +1 to get past the prefix letter of the native class name, which gets omitted by the FName for the class.
	int res = stricmp(((AFuncDesc*)a)->ClassName + 1, ((AFuncDesc*)b)->ClassName + 1);
	if (res == 0) res = stricmp(((AFuncDesc*)a)->FuncName, ((AFuncDesc*)b)->FuncName);
	return res;
}

static int fieldcmp(const void * a, const void * b)
{
	// +1 to get past the prefix letter of the native class name, which gets omitted by the FName for the class.
	int res = stricmp(((FieldDesc*)a)->ClassName + 1, ((FieldDesc*)b)->ClassName + 1);
	if (res == 0) res = stricmp(((FieldDesc*)a)->FieldName, ((FieldDesc*)b)->FieldName);
	return res;
}

//==========================================================================
//
// Initialization
//
//==========================================================================

void InitThingdef()
{
	PType *TypeActor = NewPointer(RUNTIME_CLASS(AActor));

	PStruct *sstruct = NewNativeStruct("Sector", nullptr);
	auto sptr = NewPointer(sstruct);
	sstruct->AddNativeField("soundtarget", TypeActor, myoffsetof(sector_t, SoundTarget));
	
	// expose the global validcount variable.
	PField *vcf = new PField("validcount", TypeSInt32, VARF_Native | VARF_Static, (intptr_t)&validcount);
	GlobalSymbols.AddSymbol(vcf);

	// expose the global Multiplayer variable.
	PField *multif = new PField("multiplayer", TypeBool, VARF_Native | VARF_ReadOnly | VARF_Static, (intptr_t)&multiplayer);
	GlobalSymbols.AddSymbol(multif);

	// set up a variable for the global level data structure
	PStruct *lstruct = NewNativeStruct("LevelLocals", nullptr);
	PField *levelf = new PField("level", lstruct, VARF_Native | VARF_Static, (intptr_t)&level);
	GlobalSymbols.AddSymbol(levelf);

	// set up a variable for the DEH data
	PStruct *dstruct = NewNativeStruct("DehInfo", nullptr);
	PField *dehf = new PField("deh", dstruct, VARF_Native | VARF_Static, (intptr_t)&deh);

	GlobalSymbols.AddSymbol(dehf);

	// set up a variable for the global players array.
	PStruct *pstruct = NewNativeStruct("PlayerInfo", nullptr);
	pstruct->Size = sizeof(player_t);
	pstruct->Align = alignof(player_t);
	PArray *parray = NewArray(pstruct, MAXPLAYERS);
	PField *playerf = new PField("players", parray, VARF_Native | VARF_Static, (intptr_t)&players);
	GlobalSymbols.AddSymbol(playerf);

	// set up the lines array in the sector struct. This is a bit messy because the type system is not prepared to handle a pointer to an array of pointers to a native struct even remotely well...
	// As a result, the size has to be set to something large and arbritrary because it can change between maps. This will need some serious improvement when things get cleaned up.
	pstruct = NewNativeStruct("Sector", nullptr);
	pstruct->AddNativeField("lines", NewPointer(NewArray(NewPointer(NewNativeStruct("line", nullptr), false), 0x40000), false), myoffsetof(sector_t, lines), VARF_Native);

	parray = NewArray(TypeBool, MAXPLAYERS);
	playerf = new PField("playeringame", parray, VARF_Native | VARF_Static | VARF_ReadOnly, (intptr_t)&playeringame);
	GlobalSymbols.AddSymbol(playerf);

	playerf = new PField("gameaction", TypeUInt8, VARF_Native | VARF_Static, (intptr_t)&gameaction);
	GlobalSymbols.AddSymbol(playerf);

	playerf = new PField("consoleplayer", TypeSInt32, VARF_Native | VARF_Static | VARF_ReadOnly, (intptr_t)&consoleplayer);
	GlobalSymbols.AddSymbol(playerf);

	// Argh. It sucks when bad hacks need to be supported. WP_NOCHANGE is just a bogus pointer but it used everywhere as a special flag.
	// It cannot be defined as constant because constants can either be numbers or strings but nothing else, so the only 'solution'
	// is to create a static variable from it and reference that in the script. Yuck!!!
	static AWeapon *wpnochg = WP_NOCHANGE;
	playerf = new PField("WP_NOCHANGE", NewPointer(RUNTIME_CLASS(AWeapon), false), VARF_Native | VARF_Static | VARF_ReadOnly, (intptr_t)&wpnochg);
	GlobalSymbols.AddSymbol(playerf);

	// this needs to be done manually until it can be given a proper type.
	RUNTIME_CLASS(AActor)->AddNativeField("DecalGenerator", NewPointer(TypeVoid), myoffsetof(AActor, DecalGenerator));

	// synthesize a symbol for each flag from the flag name tables to avoid redundant declaration of them.
	for (auto &fl : FlagLists)
	{
		if (fl.Use & 2)
		{
			for(int i=0;i<fl.NumDefs;i++)
			{
				if (fl.Defs[i].structoffset > 0) // skip the deprecated entries in this list
				{
					const_cast<PClass*>(*fl.Type)->AddNativeField(FStringf("b%s", fl.Defs[i].name), (fl.Defs[i].fieldsize == 4 ? TypeSInt32 : TypeSInt16), fl.Defs[i].structoffset, fl.Defs[i].varflags, fl.Defs[i].flagbit);
				}
			}
		}
	}

	FAutoSegIterator probe(CRegHead, CRegTail);

	while (*++probe != NULL)
	{
		if (((ClassReg *)*probe)->InitNatives)
			((ClassReg *)*probe)->InitNatives();
	}

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
			*(afunc->VMPointer) = new VMNativeFunction(afunc->Function, afunc->FuncName);
			(*(afunc->VMPointer))->PrintableName.Format("%s.%s [Native]", afunc->ClassName+1, afunc->FuncName);
			AFTable.Push(*afunc);
		}
		AFTable.ShrinkToFit();
		qsort(&AFTable[0], AFTable.Size(), sizeof(AFTable[0]), funccmp);
	}

	FieldTable.Clear();
	if (FieldTable.Size() == 0)
	{
		FAutoSegIterator probe(FRegHead, FRegTail);

		while (*++probe != NULL)
		{
			FieldDesc *afield = (FieldDesc *)*probe;
			FieldTable.Push(*afield);
		}
		FieldTable.ShrinkToFit();
		qsort(&FieldTable[0], FieldTable.Size(), sizeof(FieldTable[0]), fieldcmp);
	}

}

DEFINE_ACTION_FUNCTION(DObject, GameType)
{
	PARAM_PROLOGUE;
	ACTION_RETURN_INT(gameinfo.gametype);
}

DEFINE_ACTION_FUNCTION(DObject, BAM)
{
	PARAM_PROLOGUE;
	PARAM_FLOAT(ang);
	ACTION_RETURN_INT(DAngle(ang).BAMs());
}

DEFINE_ACTION_FUNCTION(FStringTable, Localize)
{
	PARAM_PROLOGUE;
	PARAM_STRING(label);
	ACTION_RETURN_STRING(GStrings(label));
}

DEFINE_ACTION_FUNCTION(FString, Replace)
{
	PARAM_SELF_STRUCT_PROLOGUE(FString);
	PARAM_STRING(s1);
	PARAM_STRING(s2);
	self->Substitute(*s1, *s2);
	return 0;
}

