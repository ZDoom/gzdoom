// Emacs style mode select -*- C++ -*-
//---------------------------------------------------------------------------
//
// Copyright(C) 2000 Simon Howard
// Copyright(C) 2005-2008 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Functions
//
// functions are stored as variables(see variable.c), the
// value being a pointer to a 'handler' function for the
// function. Arguments are stored in an argc/argv-style list
//
// this module contains all the handler functions for the
// basic FraggleScript Functions.
//
// By Simon Howard
//
//---------------------------------------------------------------------------
//
// FraggleScript is from SMMU which is under the GPL. Technically, 
// therefore, combining the FraggleScript code with the non-free 
// ZDoom code is a violation of the GPL.
//
// As this may be a problem for you, I hereby grant an exception to my 
// copyright on the SMMU source (including FraggleScript). You may use 
// any code from SMMU in (G)ZDoom, provided that:
//
//    * For any binary release of the port, the source code is also made 
//      available.
//    * The copyright notice is kept on any file containing my code.
//
//

#include "templates.h"
#include "p_local.h"
#include "t_script.h"
#include "s_sound.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "d_player.h"
#include "a_doomglobal.h"
#include "w_wad.h"
#include "gi.h"
#include "zstring.h"
#include "i_system.h"
#include "doomstat.h"
#include "g_level.h"
#include "v_palette.h"
#include "v_font.h"
#include "r_data/colormaps.h"
#include "farchive.h"
#include "p_setup.h"

static FRandom pr_script("FScript");


#define AngleToFixed(x)  ((((double) x) / ((double) ANG45/45)) * FRACUNIT)
#define FixedToAngle(x)  ((((double) x) / FRACUNIT) * ANG45/45)

// functions. FParser::SF_ means Script Function not, well.. heh, me

/////////// actually running a function /////////////

//==========================================================================
//
// The Doom actors in their original order
//
//==========================================================================

static const char * const ActorNames_init[]=
{
	"DoomPlayer",
	"ZombieMan",
	"ShotgunGuy",
	"Archvile",
	"ArchvileFire",
	"Revenant",
	"RevenantTracer",
	"RevenantTracerSmoke",
	"Fatso",
	"FatShot",
	"ChaingunGuy",
	"DoomImp",
	"Demon",
	"Spectre",
	"Cacodemon",
	"BaronOfHell",
	"BaronBall",
	"HellKnight",
	"LostSoul",
	"SpiderMastermind",
	"Arachnotron",
	"Cyberdemon",
	"PainElemental",
	"WolfensteinSS",
	"CommanderKeen",
	"BossBrain",
	"BossEye",
	"BossTarget",
	"SpawnShot",
	"SpawnFire",
	"ExplosiveBarrel",
	"DoomImpBall",
	"CacodemonBall",
	"Rocket",
	"PlasmaBall",
	"BFGBall",
	"ArachnotronPlasma",
	"BulletPuff",
	"Blood",
	"TeleportFog",
	"ItemFog",
	"TeleportDest",
	"BFGExtra",
	"GreenArmor",
	"BlueArmor",
	"HealthBonus",
	"ArmorBonus",
	"BlueCard",
	"RedCard",
	"YellowCard",
	"YellowSkull",
	"RedSkull",
	"BlueSkull",
	"Stimpack",
	"Medikit",
	"Soulsphere",
	"InvulnerabilitySphere",
	"Berserk",
	"BlurSphere",
	"RadSuit",
	"Allmap",
	"Infrared",
	"Megasphere",
	"Clip",
	"ClipBox",
	"RocketAmmo",
	"RocketBox",
	"Cell",
	"CellPack",
	"Shell",
	"ShellBox",
	"Backpack",
	"BFG9000",
	"Chaingun",
	"Chainsaw",
	"RocketLauncher",
	"PlasmaRifle",
	"Shotgun",
	"SuperShotgun",
	"TechLamp",
	"TechLamp2",
	"Column",
	"TallGreenColumn",
	"ShortGreenColumn",
	"TallRedColumn",
	"ShortRedColumn",
	"SkullColumn",
	"HeartColumn",
	"EvilEye",
	"FloatingSkull",
	"TorchTree",
	"BlueTorch",
	"GreenTorch",
	"RedTorch",
	"ShortBlueTorch",
	"ShortGreenTorch",
	"ShortRedTorch",
	"Slalagtite",
	"TechPillar",
	"CandleStick",
	"Candelabra",
	"BloodyTwitch",
	"Meat2",
	"Meat3",
	"Meat4",
	"Meat5",
	"NonsolidMeat2",
	"NonsolidMeat4",
	"NonsolidMeat3",
	"NonsolidMeat5",
	"NonsolidTwitch",
	"DeadCacodemon",
	"DeadMarine",
	"DeadZombieMan",
	"DeadDemon",
	"DeadLostSoul",
	"DeadDoomImp",
	"DeadShotgunGuy",
	"GibbedMarine",
	"GibbedMarineExtra",
	"HeadsOnAStick",
	"Gibs",
	"HeadOnAStick",
	"HeadCandles",
	"DeadStick",
	"LiveStick",
	"BigTree",
	"BurningBarrel",
	"HangNoGuts",
	"HangBNoBrain",
	"HangTLookingDown",
	"HangTSkull",
	"HangTLookingUp",
	"HangTNoBrain",
	"ColonGibs",
	"SmallBloodPool",
	"BrainStem",
	"PointPusher",
	"PointPuller",
};

static const PClass * ActorTypes[countof(ActorNames_init)];

//==========================================================================
//
// Some functions that take care of the major differences between the class
// based actor system from ZDoom and Doom's index based one
//
//==========================================================================

//==========================================================================
//
// Gets an actor class
// Input can be either a class name, an actor variable or a Doom index
// Doom index is only supported for the original things up to MBF
//
//==========================================================================
const PClass * T_GetMobjType(svalue_t arg)
{
	const PClass * PClass=NULL;
	
	if (arg.type==svt_string)
	{
		PClass=PClass::FindClass(arg.string);

		// invalid object to spawn
		if(!PClass) script_error("unknown object type: %s\n", arg.string.GetChars()); 
	}
	else if (arg.type==svt_mobj)
	{
		AActor * mo = actorvalue(arg);
		if (mo) PClass = mo->GetClass();
	}
	else
	{
		int objtype = intvalue(arg);
		if (objtype>=0 && objtype<int(countof(ActorTypes))) PClass=ActorTypes[objtype];
		else PClass=NULL;

		// invalid object to spawn
		if(!PClass) script_error("unknown object type: %i\n", objtype); 
	}
	return PClass;
}

//==========================================================================
//
// Gets a player index
// Input can be either an actor variable or an index value
//
//==========================================================================
static int T_GetPlayerNum(const svalue_t &arg)
{
	int playernum;
	if(arg.type == svt_mobj)
	{
		if(!actorvalue(arg) || !arg.value.mobj->player)
		{
			// I prefer this not to make an error.
			// This way a player function used for a non-player
			// object will just do nothing
			//script_error("mobj not a player!\n");
			return -1;
		}
		playernum = int(arg.value.mobj->player - players);
	}
	else
		playernum = intvalue(arg);
	
	if(playernum < 0 || playernum > MAXPLAYERS)
	{
		return -1;
	}
	if(!playeringame[playernum]) // no error, just return -1
	{
		return -1;
	}
	return playernum;
}

//==========================================================================
//
// Finds a sector from a tag. This has been extended to allow looking for
// sectors directly by passing a negative value
//
//==========================================================================
class FSSectorTagIterator : public FSectorTagIterator
{
public:
	FSSectorTagIterator(int tag)
		: FSectorTagIterator(tag)
	{
		if (tag < 0)
		{
			searchtag = INT_MIN;
			start = tag == -32768? 0 : -tag < numsectors? -tag : -1;
		}
	}
};

inline int T_FindFirstSectorFromTag(int tagnum)
{
	FSSectorTagIterator it(tagnum);
	return it.Next();
}


//==========================================================================
//
// Get an ammo type
// Input can be either a class name or a Doom index
// Doom index is only supported for the 4 original ammo types
//
//==========================================================================
static const PClass * T_GetAmmo(const svalue_t &t)
{
	const char * p;

	if (t.type==svt_string)
	{
		p=stringvalue(t);
	}
	else	
	{
		// backwards compatibility with Legacy.
		// allow only Doom's standard types here!
		static const char * DefAmmo[]={"Clip","Shell","Cell","RocketAmmo"};
		int ammonum = intvalue(t);
		if(ammonum < 0 || ammonum >= 4)	
		{
			script_error("ammo number out of range: %i", ammonum);
			return NULL;
		}
		p=DefAmmo[ammonum];
	}
	const PClass * am=PClass::FindClass(p);
	if (!am->IsDescendantOf(RUNTIME_CLASS(AAmmo)))
	{
		script_error("unknown ammo type : %s", p);
		return NULL;
	}
	return am;

}

//==========================================================================
//
// Finds a sound in the sound table and adds a new entry if it isn't defined
// It's too bad that this is necessary but FS doesn't know about this kind
// of sound management.
//
//==========================================================================
static FSoundID T_FindSound(const char * name)
{
	char buffer[40];
	FSoundID so=S_FindSound(name);

	if (so>0) return so;

	// Now it gets dirty!

	if (gameinfo.gametype & GAME_DoomStrifeChex)
	{
		mysnprintf(buffer, countof(buffer), "DS%.35s", name);
		if (Wads.CheckNumForName(buffer, ns_sounds)<0) strcpy(buffer, name);
	}
	else
	{
		strcpy(buffer, name);
		if (Wads.CheckNumForName(buffer, ns_sounds)<0) mysnprintf(buffer, countof(buffer), "DS%.35s", name);
	}

	int id = S_AddSound(name, buffer);
	S_HashSounds();
	return FSoundID(id);
}


//==========================================================================
//
// Creates a string out of a print argument list. This version does not
// have any length restrictions like the original FS versions had.
//
//==========================================================================
FString FParser::GetFormatString(int startarg)
{
	FString fmt="";
	for(int i=startarg; i<t_argc; i++) fmt += stringvalue(t_argv[i]);
	return fmt;
}

bool FParser::CheckArgs(int cnt)
{
	if (t_argc<cnt)
	{
		script_error("Insufficient parameters for '%s'\n", t_func.GetChars());
		return false;
	}
	return true;
}
//==========================================================================

//FUNCTIONS

// the actual handler functions for the
// functions themselves

// arguments are evaluated and passed to the
// handler functions using 't_argc' and 't_argv'
// in a similar way to the way C does with command
// line options.

// values can be returned from the functions using
// the variable 't_return'
//
//==========================================================================

//==========================================================================
//
// prints some text to the console and the notify buffer
//
//==========================================================================
void FParser::SF_Print(void)
{
	Printf(PRINT_HIGH, "%s\n", GetFormatString(0).GetChars());
}


//==========================================================================
//
// return a random number from 0 to 255
//
//==========================================================================
void FParser::SF_Rnd(void)
{
	t_return.type = svt_int;
	t_return.value.i = pr_script();
}

//==========================================================================
//
// looping section. using the rover, find the highest level
// loop we are currently in and return the DFsSection for it.
//
//==========================================================================

DFsSection *FParser::looping_section()
{
	DFsSection *best = NULL;         // highest level loop we're in
	// that has been found so far
	int n;
	
	// check thru all the hashchains
	SDWORD rover_index = Script->MakeIndex(Rover);
	
	for(n=0; n<SECTIONSLOTS; n++)
	{
		DFsSection *current = Script->sections[n];
		
		// check all the sections in this hashchain
		while(current)
		{
			// a loop?
			
			if(current->type == st_loop)
				// check to see if it's a loop that we're inside
				if(rover_index >= current->start_index && rover_index <= current->end_index)
				{
					// a higher nesting level than the best one so far?
					if(!best || (current->start_index > best->start_index))
						best = current;     // save it
				}
				current = current->next;
		}
	}
	
	return best;    // return the best one found
}

//==========================================================================
//
// "continue;" in FraggleScript is a function
//
//==========================================================================

void FParser::SF_Continue(void)
{
	DFsSection *section;
	
	if(!(section = looping_section()) )       // no loop found
	{
		script_error("continue() not in loop\n");
		return;
	}
	
	Rover = Script->SectionEnd(section);      // jump to the closing brace
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Break(void)
{
	DFsSection *section;
	
	if(!(section = looping_section()) )
	{
		script_error("break() not in loop\n");
		return;
	}
	
	Rover = Script->SectionEnd(section) + 1;   // jump out of the loop
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Goto(void)
{
	if (CheckArgs(1))
	{
		// check argument is a labelptr
		
		if(t_argv[0].type != svt_label)
		{
			script_error("goto argument not a label\n");
			return;
		}
		
		// go there then if everythings fine
		Rover = Script->LabelValue(t_argv[0]);
	}	
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Return(void)
{
	throw CFsTerminator();
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Include(void)
{
	char tempstr[12];
	
	if (CheckArgs(1))
	{
		if(t_argv[0].type == svt_string)
		{
			strncpy(tempstr, t_argv[0].string, 8);
			tempstr[8]=0;
		}
		else
			mysnprintf(tempstr, countof(tempstr), "%i", (int)t_argv[0].value.i);
		
		Script->ParseInclude(tempstr);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Input(void)
{
	Printf(PRINT_BOLD,"input() function not available in doom\n");
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Beep(void)
{
	S_Sound(CHAN_AUTO, "misc/chat", 1.0f, ATTN_IDLE);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Clock(void)
{
	t_return.type = svt_int;
	t_return.value.i = (gametic*100)/TICRATE;
}

/**************** doom stuff ****************/

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ExitLevel(void)
{
	G_ExitLevel(0, false);
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Tip(void)
{
	if (t_argc>0 && Script->trigger &&
		Script->trigger->CheckLocalView(consoleplayer)) 
	{
		C_MidPrint(SmallFont, GetFormatString(0).GetChars());
	}
}

//==========================================================================
//
// FParser::SF_TimedTip
//
// Implements: void timedtip(int clocks, ...)
//
//==========================================================================

EXTERN_CVAR(Float, con_midtime)

void FParser::SF_TimedTip(void)
{
	if (CheckArgs(2))
	{
		float saved = con_midtime;
		con_midtime = intvalue(t_argv[0])/100.0f;
		C_MidPrint(SmallFont, GetFormatString(1).GetChars());
		con_midtime=saved;
	}
}


//==========================================================================
//
// tip to a particular player
//
//==========================================================================

void FParser::SF_PlayerTip(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);
		if (plnum!=-1 && players[plnum].mo->CheckLocalView(consoleplayer)) 
		{
			C_MidPrint(SmallFont, GetFormatString(1).GetChars());
		}
	}
}

//==========================================================================
//
// message player
//
//==========================================================================

void FParser::SF_Message(void)
{
	if (t_argc>0 && Script->trigger &&
		Script->trigger->CheckLocalView(consoleplayer))
	{
		Printf(PRINT_HIGH, "%s\n", GetFormatString(0).GetChars());
	}
}

//==========================================================================
//
// message to a particular player
//
//==========================================================================

void FParser::SF_PlayerMsg(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);
		if (plnum!=-1 && players[plnum].mo->CheckLocalView(consoleplayer)) 
		{
			Printf(PRINT_HIGH, "%s\n", GetFormatString(1).GetChars());
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerInGame(void)
{
	if (CheckArgs(1))
	{
		int plnum = T_GetPlayerNum(t_argv[0]);

		if (plnum!=-1)
		{
			t_return.type = svt_int;
			t_return.value.i = playeringame[plnum];
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerName(void)
{
	int plnum;
	
	if(!t_argc)
	{
		player_t *pl=NULL;
		if (Script->trigger) pl = Script->trigger->player;
		if(pl) plnum = int(pl - players);
		else plnum=-1;
	}
	else
		plnum = T_GetPlayerNum(t_argv[0]);
	
	if(plnum !=-1)
	{
		t_return.type = svt_string;
		t_return.string = players[plnum].userinfo.GetName();
	}
	else
	{
		script_error("script not started by player\n");
	}
}

//==========================================================================
//
// object being controlled by player
//
//==========================================================================

void FParser::SF_PlayerObj(void)
{
	int plnum;

	if(!t_argc)
	{
		player_t *pl=NULL;
		if (Script->trigger) pl = Script->trigger->player;
		if(pl) plnum = int(pl - players);
		else plnum=-1;
	}
	else
		plnum = T_GetPlayerNum(t_argv[0]);

	if(plnum !=-1)
	{
		t_return.type = svt_mobj;
		t_return.value.mobj = players[plnum].mo;
	}
	else
	{
		script_error("script not started by player\n");
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Player(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	
	if(mo && mo->player) // haleyjd: added mo->player
	{
		t_return.value.i = (int)(mo->player - players);
	}
	else
	{
		t_return.value.i = -1;
	}
}

//==========================================================================
//
// FParser::SF_Spawn
// 
// Implements: mobj spawn(int type, int x, int y, [int angle], [int z], [bool zrel])
//
//==========================================================================

void FParser::SF_Spawn(void)
{
	int x, y, z;
	const PClass *PClass;
	angle_t angle = 0;
	
	if (CheckArgs(3))
	{
		if (!(PClass=T_GetMobjType(t_argv[0]))) return;
		
		x = fixedvalue(t_argv[1]);
		y = fixedvalue(t_argv[2]);

		if(t_argc >= 5)
		{
			z = fixedvalue(t_argv[4]);
			// [Graf Zahl] added option of spawning with a relative z coordinate
			if(t_argc > 5)
			{
				if (intvalue(t_argv[5])) z+=P_PointInSector(x, y)->floorplane.ZatPoint(x,y);
			}
		}
		else
		{
			// Legacy compatibility is more important than correctness.
			z = ONFLOORZ;// (GetDefaultByType(PClass)->flags & MF_SPAWNCEILING) ? ONCEILINGZ : ONFLOORZ;
		}
		
		if(t_argc >= 4)
		{
			angle = intvalue(t_argv[3]) * (SQWORD)ANG45 / 45;
		}
		
		t_return.type = svt_mobj;
		t_return.value.mobj = Spawn(PClass, x, y, z, ALLOW_REPLACE);

		if (t_return.value.mobj)		
		{
			t_return.value.mobj->angle = angle;

			if (!DFraggleThinker::ActiveThinker->nocheckposition)
			{
				if (!P_TestMobjLocation(t_return.value.mobj))
				{
					if (t_return.value.mobj->flags&MF_COUNTKILL) level.total_monsters--;
					if (t_return.value.mobj->flags&MF_COUNTITEM) level.total_items--;
					t_return.value.mobj->Destroy();
					t_return.value.mobj = NULL;
				}
			}
		}
	}	
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_RemoveObj(void)
{
	if (CheckArgs(1))
	{
		AActor * mo = actorvalue(t_argv[0]);
		if(mo)  // nullptr check
		{
			mo->ClearCounters();
			mo->Destroy();
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_KillObj(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;  // default to trigger object
	}

	if(mo) 
	{
		// ensure the thing can be killed
		mo->flags|=MF_SHOOTABLE;
		mo->flags2&=~(MF2_INVULNERABLE|MF2_DORMANT);
		// [GrafZahl] This called P_KillMobj directly 
		// which is a very bad thing to do!
		P_DamageMobj(mo, NULL, NULL, mo->health, NAME_Massacre);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjX(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_fixed;           // haleyjd: SoM's fixed-point fix
	t_return.value.f = mo ? mo->X() : 0;   // null ptr check
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjY(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_fixed;         // haleyjd
	t_return.value.f = mo ? mo->Y() : 0; // null ptr check
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjZ(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;	
	}

	t_return.type = svt_fixed;         // haleyjd
	t_return.value.f = mo ? mo->Z() : 0; // null ptr check
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjAngle(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_fixed; // haleyjd: fixed-point -- SoM again :)
	t_return.value.f = mo ? (fixed_t)AngleToFixed(mo->angle) : 0;   // null ptr check
}


//==========================================================================
//
//
//
//==========================================================================

// teleport: object, sector_tag
void FParser::SF_Teleport(void)
{
	int tag;
	AActor *mo;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: sector tag
		{
			mo = Script->trigger;   // default to trigger
			tag = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // teleport a given object
			mo = actorvalue(t_argv[0]);
			tag = intvalue(t_argv[1]);
		}
		
		if(mo)
			EV_Teleport(0, tag, NULL, 0, mo, TELF_DESTFOG | TELF_SOURCEFOG);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SilentTeleport(void)
{
	int tag;
	AActor *mo;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: sector tag
		{
			mo = Script->trigger;   // default to trigger
			tag = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // teleport a given object
			mo = actorvalue(t_argv[0]);
			tag = intvalue(t_argv[1]);
		}
		
		if(mo)
			EV_Teleport(0, tag, NULL, 0, mo, TELF_KEEPORIENTATION);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_DamageObj(void)
{
	AActor *mo;
	int damageamount;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)    // 1 argument: damage trigger by amount
		{
			mo = Script->trigger;   // default to trigger
			damageamount = intvalue(t_argv[0]);
		}
		else    // 2 or more
		{                       // damage a given object
			mo = actorvalue(t_argv[0]);
			damageamount = intvalue(t_argv[1]);
		}
		
		if(mo)
			P_DamageMobj(mo, NULL, Script->trigger, damageamount, NAME_None);
	}
}

//==========================================================================
//
//
//
//==========================================================================

// the tag number of the sector the thing is in
void FParser::SF_ObjSector(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = mo ? tagManager.GetFirstSectorTag(mo->Sector) : 0; // nullptr check
}

//==========================================================================
//
//
//
//==========================================================================

// the health number of an object
void FParser::SF_ObjHealth(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = mo ? mo->health : 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjFlag(void)
{
	AActor *mo;
	int flagnum;
	
	if (CheckArgs(1))
	{
		if(t_argc == 1)         // use trigger, 1st is flag
		{
			// use trigger:
			mo = Script->trigger;
			flagnum = intvalue(t_argv[0]);
		}
		else if(t_argc == 2)	// specified object
		{
			mo = actorvalue(t_argv[0]);
			flagnum = intvalue(t_argv[1]);
		}
		else                     // >= 3 : SET flags
		{
			mo = actorvalue(t_argv[0]);
			flagnum = intvalue(t_argv[1]);
			
			if(mo && flagnum<26)          // nullptr check
			{
				DWORD tempflags = mo->flags;

				// remove old bit
				tempflags &= ~(1 << flagnum);
				
				// make the new flag
				tempflags |= (!!intvalue(t_argv[2])) << flagnum;
				mo->flags = ActorFlags::FromInt (tempflags);
			}  
		}
		t_return.type = svt_int;  
		if (mo && flagnum<26)
		{
			t_return.value.i = !!(mo->flags & ActorFlags::FromInt(1 << flagnum));
		}
		else t_return.value.i = 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================

// apply momentum to a thing
void FParser::SF_PushThing(void)
{
	if (CheckArgs(3))
	{
		AActor * mo = actorvalue(t_argv[0]);
		if(!mo) return;
	
		angle_t angle = (angle_t)FixedToAngle(fixedvalue(t_argv[1]));
		fixed_t force = fixedvalue(t_argv[2]);
	
		P_ThrustMobj(mo, angle, force);
	}
}

//==========================================================================
//
//  FParser::SF_ReactionTime -- useful for freezing things
//
//==========================================================================


void FParser::SF_ReactionTime(void)
{
	if (CheckArgs(1))
	{
		AActor *mo = actorvalue(t_argv[0]);
	
		if(t_argc > 1)
		{
			if(mo) mo->reactiontime = (intvalue(t_argv[1]) * TICRATE) / 100;
		}
	
		t_return.type = svt_int;
		t_return.value.i = mo ? mo->reactiontime : 0;
	}
}

//==========================================================================
//
//  FParser::SF_MobjTarget   -- sets a thing's target field
//
//==========================================================================

// Sets a mobj's Target! >:)
void FParser::SF_MobjTarget(void)
{
	AActor*  mo;
	AActor*  target;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			target = actorvalue(t_argv[1]);
			if(mo && target && mo->SeeState) // haleyjd: added target check -- no NULL allowed
			{
				mo->target=target;
				mo->SetState(mo->SeeState);
				mo->flags|=MF_JUSTHIT;
			}
		}
		
		t_return.type = svt_mobj;
		t_return.value.mobj = mo ? mo->target : NULL;
	}
}

//==========================================================================
//
//  FParser::SF_MobjMomx, MobjMomy, MobjMomz -- momentum functions
//
//==========================================================================

void FParser::SF_MobjMomx(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->velx = fixedvalue(t_argv[1]);
		}
		
		t_return.type = svt_fixed;
		t_return.value.f = mo ? mo->velx : 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MobjMomy(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo)
				mo->vely = fixedvalue(t_argv[1]);
		}
		
		t_return.type = svt_fixed;
		t_return.value.f = mo ? mo->vely : 0;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MobjMomz(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo)
				mo->velz = fixedvalue(t_argv[1]);
		}
		
		t_return.type = svt_fixed;
		t_return.value.f = mo ? mo->velz : 0;
	}
}


//==========================================================================
//
//
//
//==========================================================================

/****************** Trig *********************/

void FParser::SF_PointToAngle(void)
{
	if (CheckArgs(4))
	{
		fixed_t x1 = fixedvalue(t_argv[0]);
		fixed_t y1 = fixedvalue(t_argv[1]);
		fixed_t x2 = fixedvalue(t_argv[2]);
		fixed_t y2 = fixedvalue(t_argv[3]);
		
		angle_t angle = R_PointToAngle2(x1, y1, x2, y2);
		
		t_return.type = svt_fixed;
		t_return.value.f = (fixed_t)AngleToFixed(angle);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PointToDist(void)
{
	if (CheckArgs(4))
	{
		// Doing this in floating point is actually faster with modern computers!
		double x = floatvalue(t_argv[2]) - floatvalue(t_argv[0]);
		double y = floatvalue(t_argv[3]) - floatvalue(t_argv[1]);
   
		t_return.type = svt_fixed;
		t_return.value.f = FLOAT2FIXED(sqrt(x*x+y*y));
	}
}


//==========================================================================
//
// setcamera(obj, [angle], [height], [pitch])
//
// [GrafZahl] This is a complete rewrite.
// Although both Eternity and Legacy implement this function
// they are mutually incompatible with each other and with ZDoom...
//
//==========================================================================

void FParser::SF_SetCamera(void)
{
	angle_t angle;
	player_t * player;
	AActor * newcamera;
	
	if (CheckArgs(1))
	{
		player=Script->trigger->player;
		if (!player) player=&players[0];
		
		newcamera = actorvalue(t_argv[0]);
		if(!newcamera)
		{
			script_error("invalid location object for camera\n");
			return;         // nullptr check
		}
		
		angle = t_argc < 2 ? newcamera->angle : (fixed_t)FixedToAngle(fixedvalue(t_argv[1]));

		newcamera->special1=newcamera->angle;
		newcamera->special2=newcamera->Z();
		newcamera->SetZ(t_argc < 3 ? (newcamera->Z() + (41 << FRACBITS)) : (intvalue(t_argv[2]) << FRACBITS));
		newcamera->angle = angle;
		if(t_argc < 4) newcamera->pitch = 0;
		else
		{
			fixed_t pitch = fixedvalue(t_argv[3]);
			if(pitch < -50*FRACUNIT) pitch = -50*FRACUNIT;
			if(pitch > 50*FRACUNIT)  pitch =  50*FRACUNIT;
			newcamera->pitch=(angle_t)((pitch/65536.0f)*(ANGLE_45/45.0f)*(20.0f/32.0f));
		}
		player->camera=newcamera;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ClearCamera(void)
{
	player_t * player;
	player=Script->trigger->player;
	if (!player) player=&players[0];

	AActor * cam=player->camera;
	if (cam)
	{
		player->camera=player->mo;
		cam->angle=cam->special1;
		cam->SetZ(cam->special2);
	}

}



/*********** sounds ******************/

//==========================================================================
//
//
//
//==========================================================================

// start sound from thing
void FParser::SF_StartSound(void)
{
	AActor *mo;
	
	if (CheckArgs(2))
	{
		mo = actorvalue(t_argv[0]);
		
		if (mo)
		{
			S_Sound(mo, CHAN_BODY, T_FindSound(stringvalue(t_argv[1])), 1, ATTN_NORM);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

// start sound from sector
void FParser::SF_StartSectorSound(void)
{
	sector_t *sector;
	int tagnum;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		
		int i=-1;
		FSSectorTagIterator itr(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			sector = &sectors[i];
			S_Sound(sector, CHAN_BODY, T_FindSound(stringvalue(t_argv[1])), 1.0f, ATTN_NORM);
		}
	}
}

/************* Sector functions ***************/

//DMover::EResult P_MoveFloor (sector_t * m_Sector, fixed_t speed, fixed_t dest, int crush, int direction, int flags=0);
//DMover::EResult P_MoveCeiling (sector_t * m_Sector, fixed_t speed, fixed_t dest, int crush, int direction, int flags=0);

class DFloorChanger : public DFloor
{
public:
	DFloorChanger(sector_t * sec)
		: DFloor(sec) {}

	bool Move(fixed_t speed, fixed_t dest, int crush, int direction)
	{
		bool res = DMover::crushed != MoveFloor(speed, dest, crush, direction, false);
		Destroy();
		m_Sector->floordata=NULL;
		StopInterpolation(true);
		m_Sector=NULL;
		return res;
	}
};


//==========================================================================
//
//
//
//==========================================================================

// floor height of sector
void FParser::SF_FloorHeight(void)
{
	int tagnum;
	int secnum;
	fixed_t dest;
	int returnval = 1; // haleyjd: SoM's fixes
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		if(t_argc > 1)          // > 1: set floor height
		{
			int i;
			int crush = (t_argc >= 3) ? intvalue(t_argv[2]) : false;
			
			i = -1;
			dest = fixedvalue(t_argv[1]);
			
			// set all sectors with tag
			
			FSSectorTagIterator itr(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				if (sectors[i].floordata) continue;	// don't move floors that are active!

				DFloorChanger * f = new DFloorChanger(&sectors[i]);
				if (!f->Move(
					abs(dest - sectors[i].CenterFloor()), 
					sectors[i].floorplane.PointToDist (CenterSpot(&sectors[i]), dest), 
					crush? 10:-1, 
					(dest > sectors[i].CenterFloor()) ? 1 : -1))
				{
					returnval = 0;
				}
			}
		}
		else
		{
			secnum = T_FindFirstSectorFromTag(tagnum);
			if(secnum < 0)
			{ 
				script_error("sector not found with tagnum %i\n", tagnum); 
				return;
			}
			returnval = sectors[secnum].CenterFloor() >> FRACBITS;
		}
		
		// return floor height
		
		t_return.type = svt_int;
		t_return.value.i = returnval;
	}
}


//=============================================================================
//
//
//=============================================================================
class DMoveFloor : public DFloor
{
public:
	DMoveFloor(sector_t * sec,int moveheight,int _m_Direction,int crush,int movespeed)
	: DFloor(sec)
	{
		m_Type = floorRaiseByValue;
		m_Crush = crush;
		m_Speed=movespeed;
		m_Direction = _m_Direction;
		m_FloorDestDist = moveheight;

		// Do not interpolate instant movement floors.
		fixed_t movedist = abs(-sec->floorplane.d - moveheight);
		if (m_Speed >= movedist)
		{
			StopInterpolation(true);
		}

		StartFloorSound();
	}
};



//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MoveFloor(void)
{
	int secnum = -1;
	sector_t *sec;
	int tagnum, platspeed = 1, destheight, crush;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		destheight = intvalue(t_argv[1]) * FRACUNIT;
		platspeed = t_argc > 2 ? fixedvalue(t_argv[2]) : FRACUNIT;
		crush = (t_argc > 3 ? intvalue(t_argv[3]) : -1);
		
		// move all sectors with tag
		
		FSSectorTagIterator itr(tagnum);
		while ((secnum = itr.Next()) >= 0)
		{
			sec = &sectors[secnum];
			// Don't start a second thinker on the same floor
			if (sec->floordata) continue;
			
			new DMoveFloor(sec,sec->floorplane.PointToDist(CenterSpot(sec),destheight),
				destheight < sec->CenterFloor() ? -1:1,crush,platspeed);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

class DCeilingChanger : public DCeiling
{
public:
	DCeilingChanger(sector_t * sec)
		: DCeiling(sec) {}

	bool Move(fixed_t speed, fixed_t dest, int crush, int direction)
	{
		bool res = DMover::crushed != MoveCeiling(speed, dest, crush, direction, false);
		Destroy();
		m_Sector->ceilingdata=NULL;
		StopInterpolation (true);
		m_Sector=NULL;
		return res;
	}
};

//==========================================================================
//
//
//
//==========================================================================

// ceiling height of sector
void FParser::SF_CeilingHeight(void)
{
	fixed_t dest;
	int secnum;
	int tagnum;
	int returnval = 1;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		if(t_argc > 1)          // > 1: set ceilheight
		{
			int i;
			int crush = (t_argc >= 3) ? intvalue(t_argv[2]) : false;
			
			i = -1;
			dest = fixedvalue(t_argv[1]);
			
			// set all sectors with tag
			FSSectorTagIterator itr(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				if (sectors[i].ceilingdata) continue;	// don't move ceilings that are active!

				DCeilingChanger * c = new DCeilingChanger(&sectors[i]);
				if (!c->Move(
					abs(dest - sectors[i].CenterCeiling()), 
					sectors[i].ceilingplane.PointToDist (CenterSpot(&sectors[i]), dest), 
					crush? 10:-1,
					(dest > sectors[i].CenterCeiling()) ? 1 : -1))
				{
					returnval = 0;
				}
			}
		}
		else
		{
			secnum = T_FindFirstSectorFromTag(tagnum);
			if(secnum < 0)
			{ 
				script_error("sector not found with tagnum %i\n", tagnum); 
				return;
			}
			returnval = sectors[secnum].CenterCeiling() >> FRACBITS;
		}
		
		// return ceiling height
		t_return.type = svt_int;
		t_return.value.i = returnval;
	}
}


//==========================================================================
//
//
//
//==========================================================================

class DMoveCeiling : public DCeiling
{
public:

	DMoveCeiling(sector_t * sec,int tag,fixed_t destheight,fixed_t speed,int silent,int crush)
		: DCeiling(sec)
	{
		m_Crush = crush;
		m_Speed2 = m_Speed = m_Speed1 = speed;
		m_Silent = silent;
		m_Type = DCeiling::ceilLowerByValue;	// doesn't really matter as long as it's no special value
		m_Tag=tag;			
		vertex_t * spot=CenterSpot(sec);
		m_TopHeight=m_BottomHeight=sec->ceilingplane.PointToDist(spot,destheight);
		m_Direction=destheight>sec->GetPlaneTexZ(sector_t::ceiling)? 1:-1;

		// Do not interpolate instant movement ceilings.
		fixed_t movedist = abs(sec->ceilingplane.d - m_BottomHeight);
		if (m_Speed >= movedist)
		{
			StopInterpolation (true);
			m_Silent=2;
		}
		PlayCeilingSound();
	}
};


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MoveCeiling(void)
{
	int secnum = -1;
	sector_t *sec;
	int tagnum, platspeed = 1, destheight;
	int crush;
	int silent;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		destheight = intvalue(t_argv[1]) * FRACUNIT;
		platspeed = /*FLOORSPEED **/ (t_argc > 2 ? fixedvalue(t_argv[2]) : FRACUNIT);
		crush=t_argc>3 ? intvalue(t_argv[3]):-1;
		silent=t_argc>4 ? intvalue(t_argv[4]):1;
		
		// move all sectors with tag
		FSSectorTagIterator itr(tagnum);
		while ((secnum = itr.Next()) >= 0)
		{
			sec = &sectors[secnum];
			
			// Don't start a second thinker on the same floor
			if (sec->ceilingdata) continue;
			new DMoveCeiling(sec, tagnum, destheight, platspeed, silent, crush);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LightLevel(void)
{
	sector_t *sector;
	int secnum;
	int tagnum;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = T_FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ 
			return;
		}
		
		sector = &sectors[secnum];
		
		if(t_argc > 1)          // > 1: set light level
		{
			int i = -1;
			
			// set all sectors with tag
			FSSectorTagIterator itr(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				sectors[i].SetLightLevel(intvalue(t_argv[1]));
			}
		}
		
		// return lightlevel
		t_return.type = svt_int;
		t_return.value.i = sector->lightlevel;
	}
}



//==========================================================================
//
// Simple light fade - locks lightingdata. For FParser::SF_FadeLight
//
//==========================================================================
class DLightLevel : public DLighting
{
	DECLARE_CLASS (DLightLevel, DLighting)

	unsigned char destlevel;
	unsigned char speed;

	DLightLevel() {}

public:

	DLightLevel(sector_t * s,int destlevel,int speed);
	void	Serialize (FArchive &arc);
	void		Tick ();
	void		Destroy() { Super::Destroy(); m_Sector->lightingdata=NULL; }
};



IMPLEMENT_CLASS (DLightLevel)

void DLightLevel::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << destlevel << speed;
	if (arc.IsLoading()) m_Sector->lightingdata=this;
}


//==========================================================================
// sf 13/10/99:
//
// T_LightFade()
//
// Just fade the light level in a sector to a new level
//
//==========================================================================

void DLightLevel::Tick()
{
	Super::Tick();
	if(m_Sector->lightlevel < destlevel)
	{
		// increase the lightlevel
		if(m_Sector->lightlevel + speed >= destlevel)
		{
			// stop changing light level
			m_Sector->SetLightLevel(destlevel);    // set to dest lightlevel
			Destroy();
		}
		else
		{
			m_Sector->ChangeLightLevel(speed);
		}
	}
	else
	{
		// decrease lightlevel
		if(m_Sector->lightlevel - speed <= destlevel)
		{
			// stop changing light level
			m_Sector->SetLightLevel(destlevel);    // set to dest lightlevel
			Destroy();
		}
		else
		{
			m_Sector->ChangeLightLevel(-speed);
		}
	}
}

//==========================================================================
//
//==========================================================================
DLightLevel::DLightLevel(sector_t * s,int _destlevel,int _speed) : DLighting(s)
{
	destlevel=_destlevel;
	speed=_speed;
	s->lightingdata=this;
}

//==========================================================================
// sf 13/10/99:
//
// P_FadeLight()
//
// Fade all the lights in sectors with a particular tag to a new value
//
//==========================================================================
void FParser::SF_FadeLight(void)
{
	int sectag, destlevel, speed = 1;
	int i;
	
	if (CheckArgs(2))
	{
		sectag = intvalue(t_argv[0]);
		destlevel = intvalue(t_argv[1]);
		speed = t_argc>2 ? intvalue(t_argv[2]) : 1;
		
		FSectorTagIterator it(sectag);
		while ((i = it.Next()) >= 0)
		{
			if (!sectors[i].lightingdata) new DLightLevel(&sectors[i],destlevel,speed);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================
void FParser::SF_FloorTexture(void)
{
	int tagnum, secnum;
	sector_t *sector;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = T_FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ script_error("sector not found with tagnum %i\n", tagnum); return;}
		
		sector = &sectors[secnum];
		
		if(t_argc > 1)
		{
			int i = -1;
			FTextureID picnum = TexMan.GetTexture(t_argv[1].string, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
			
			// set all sectors with tag
			FSSectorTagIterator itr(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				sectors[i].SetTexture(sector_t::floor, picnum);
			}
		}
		
		t_return.type = svt_string;
		FTexture * tex = TexMan[sector->GetTexture(sector_t::floor)];
		t_return.string = tex? tex->Name : "";
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SectorColormap(void)
{
	// This doesn't work properly and it never will.
	// Whatever was done here originally, it is incompatible 
	// with Boom and ZDoom and doesn't work properly in Legacy either.
	
	// Making it no-op is probably the best thing one can do in this case.
	
	/*
	int tagnum, secnum;
	sector_t *sector;
	int c=2;
	int i = -1;

	if(t_argc<2)
	{ script_error("insufficient arguments to function\n"); return; }
	
	tagnum = intvalue(t_argv[0]);
	
	// argv is sector tag
	secnum = T_FindFirstSectorFromTag(tagnum);
	
	if(secnum < 0)
	{ script_error("sector not found with tagnum %i\n", tagnum); return;}
	
	sector = &sectors[secnum];

	if (t_argv[1].type==svt_string)
	{
		DWORD cm = R_ColormapNumForName(t_argv[1].value.s);

		FSSectorTagIterator itr(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			sectors[i].midmap=cm;
			sectors[i].heightsec=&sectors[i];
		}
	}
	*/	
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_CeilingTexture(void)
{
	int tagnum, secnum;
	sector_t *sector;
	
	if (CheckArgs(1))
	{
		tagnum = intvalue(t_argv[0]);
		
		// argv is sector tag
		secnum = T_FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ script_error("sector not found with tagnum %i\n", tagnum); return;}
		
		sector = &sectors[secnum];
		
		if(t_argc > 1)
		{
			int i = -1;
			FTextureID picnum = TexMan.GetTexture(t_argv[1].string, FTexture::TEX_Flat, FTextureManager::TEXMAN_Overridable);
			
			// set all sectors with tag
			FSSectorTagIterator itr(tagnum);
			while ((i = itr.Next()) >= 0)
			{
				sectors[i].SetTexture(sector_t::ceiling, picnum);
			}
		}
		
		t_return.type = svt_string;
		FTexture * tex = TexMan[sector->GetTexture(sector_t::ceiling)];
		t_return.string = tex? tex->Name : "";
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ChangeHubLevel(void)
{
	I_Error("FS hub system permanently disabled\n");
}

// for start map: start new game on a particular skill
void FParser::SF_StartSkill(void)
{
	I_Error("startskill is not supported by this implementation!\n");
}

//==========================================================================
//
// Doors
//
// opendoor(sectag, [delay], [speed])
//
//==========================================================================

void FParser::SF_OpenDoor(void)
{
	int speed, wait_time;
	int sectag;
	
	if (CheckArgs(1))
	{
		// got sector tag
		sectag = intvalue(t_argv[0]);
		if (sectag==0) return;	// tag 0 not allowed
		
		// door wait time
		if(t_argc > 1) wait_time = (intvalue(t_argv[1]) * TICRATE) / 100;
		else wait_time = 0;  // 0= stay open
		
		// door speed
		if(t_argc > 2) speed = intvalue(t_argv[2]);
		else speed = 1;    // 1= normal speed

		EV_DoDoor(wait_time? DDoor::doorRaise:DDoor::doorOpen,NULL,NULL,sectag,2*FRACUNIT*clamp(speed,1,127),wait_time,0,0);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_CloseDoor(void)
{
	int speed;
	int sectag;
	
	if (CheckArgs(1))
	{
		// got sector tag
		sectag = intvalue(t_argv[0]);
		if (sectag==0) return;	// tag 0 not allowed
		
		// door speed
		if(t_argc > 1) speed = intvalue(t_argv[1]);
		else speed = 1;    // 1= normal speed
		
		EV_DoDoor(DDoor::doorClose,NULL,NULL,sectag,2*FRACUNIT*clamp(speed,1,127),0,0,0);
	}
}

//==========================================================================
//
//
//
//==========================================================================

// run console cmd
void FParser::SF_RunCommand(void)
{
	FS_EmulateCmd(GetFormatString(0).LockBuffer());
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LineTrigger()
{
	if (CheckArgs(1))
	{
		line_t line;
		maplinedef_t mld;
		mld.special=intvalue(t_argv[0]);
		mld.tag=t_argc > 1 ? intvalue(t_argv[1]) : 0;
		P_TranslateLineDef(&line, &mld);
		P_ExecuteSpecial(line.special, NULL, Script->trigger, false, 
			line.args[0],line.args[1],line.args[2],line.args[3],line.args[4]); 
	}
}

//==========================================================================
//
//
//
//==========================================================================
bool FS_ChangeMusic(const char * string)
{
	char buffer[40];

	if (Wads.CheckNumForName(string, ns_music)<0 || !S_ChangeMusic(string,true))
	{
		// Retry with O_ prepended to the music name, then with D_
		mysnprintf(buffer, countof(buffer), "O_%s", string);
		if (Wads.CheckNumForName(buffer, ns_music)<0 || !S_ChangeMusic(buffer,true))
		{
			mysnprintf(buffer, countof(buffer), "D_%s", string);
			if (Wads.CheckNumForName(buffer, ns_music)<0) 
			{
				S_ChangeMusic(NULL, 0);
				return false;
			}
			else S_ChangeMusic(buffer,true);
		}
	}
	return true;
}

void FParser::SF_ChangeMusic(void)
{
	if (CheckArgs(1))
	{
		FS_ChangeMusic(stringvalue(t_argv[0]));
	}
}


//==========================================================================
//
// FParser::SF_SetLineBlocking()
//
//  Sets a line blocking or unblocking
//
//==========================================================================
void FParser::SF_SetLineBlocking(void)
{
	static unsigned short blocks[]={0,ML_BLOCKING,ML_BLOCKEVERYTHING};
	
	if (CheckArgs(2))
	{
		int blocking=intvalue(t_argv[1]);
		if (blocking>=0 && blocking<=2) 
		{
			blocking=blocks[blocking];
			int tag=intvalue(t_argv[0]);
			FLineIdIterator itr(tag);
			int i;
			while ((i = itr.Next()) >= 0)
			{
				lines[i].flags = (lines[i].flags & ~(ML_BLOCKING | ML_BLOCKEVERYTHING)) | blocking;
			}
		}
	}
}

//==========================================================================
//
// similar, but monster blocking
//
//==========================================================================

void FParser::SF_SetLineMonsterBlocking(void)
{
	if (CheckArgs(2))
	{
		int blocking = intvalue(t_argv[1]) ? ML_BLOCKMONSTERS : 0;
		int tag=intvalue(t_argv[0]);

		FLineIdIterator itr(tag);
		int i;
		while ((i = itr.Next()) >= 0)
		{
			lines[i].flags = (lines[i].flags & ~ML_BLOCKMONSTERS) | blocking;
		}
	}
}



//==========================================================================
//
//FParser::SF_SetLineTexture
//
//  #2 in a not-so-long line of ACS-inspired functions
//  This one is *much* needed, IMO
//  
//	Eternity: setlinetexture(tag, side, position, texture)
//	Legacy:	  setlinetexture(tag, texture, side, sections)
//
//==========================================================================

void FParser::SF_SetLineTexture(void)
{
	int tag;
	int side;
	int position;
	const char *texture;
	FTextureID texturenum;
	int i;
	
	if (CheckArgs(4))
	{
		tag = intvalue(t_argv[0]);

		// the eternity version
		if (t_argv[3].type==svt_string)
		{
			side = intvalue(t_argv[1]);   
			if(side < 0 || side > 1)
			{
				script_error("invalid side number for texture change\n");
				return;
			}
			
			position = intvalue(t_argv[2]);
			if(position < 1 || position > 3)
			{
				script_error("invalid position for texture change\n");
				return;
			}
			position=3-position;
			
			texture = stringvalue(t_argv[3]);
			texturenum = TexMan.GetTexture(texture, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable);
			
			FLineIdIterator itr(tag);
			while ((i = itr.Next()) >= 0)
			{
				// bad sidedef, Hexen just SEGV'd here!
				if (lines[i].sidedef[side] != NULL)
				{
					if (position >= 0 && position <= 2)
					{
						lines[i].sidedef[side]->SetTexture(position, texturenum);
					}
				}
			}
		}
		else // and an improved legacy version
		{ 
			FTextureID picnum = TexMan.GetTexture(t_argv[1].string, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable); 
			side = !!intvalue(t_argv[2]); 
			int sections = intvalue(t_argv[3]); 
			
			// set all sectors with tag 
			FLineIdIterator itr(tag);
			while ((i = itr.Next()) >= 0)
			{ 
				side_t *sided = lines[i].sidedef[side];
				if(sided != NULL)
				{ 
					if(sections & 1) sided->SetTexture(side_t::top, picnum);
					if(sections & 2) sided->SetTexture(side_t::mid, picnum);
					if(sections & 4) sided->SetTexture(side_t::bottom, picnum);
				} 
			} 
		} 
	}
}


//==========================================================================
//
// SoM: Max, Min, Abs math functions.
//
//==========================================================================

void FParser::SF_Max(void)
{
	fixed_t n1, n2;
	
	if (CheckArgs(2))
	{
		n1 = fixedvalue(t_argv[0]);
		n2 = fixedvalue(t_argv[1]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 > n2) ? n1 : n2;
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Min(void)
{
	fixed_t   n1, n2;
	
	if (CheckArgs(1))
	{
		n1 = fixedvalue(t_argv[0]);
		n2 = fixedvalue(t_argv[1]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 < n2) ? n1 : n2;
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Abs(void)
{
	fixed_t   n1;
	
	if (CheckArgs(1))
	{
		n1 = fixedvalue(t_argv[0]);
		
		t_return.type = svt_fixed;
		t_return.value.f = (n1 < 0) ? -n1 : n1;
	}
}

//==========================================================================
// 
// FParser::SF_Gameskill, FParser::SF_Gamemode
//
//  Access functions are more elegant for these than variables, 
//  especially for the game mode, which doesn't exist as a numeric 
//  variable already.
//
//==========================================================================

void FParser::SF_Gameskill(void)
{
	t_return.type = svt_int;
	t_return.value.i = G_SkillProperty(SKILLP_ACSReturn) + 1;  // +1 for the user skill value
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Gamemode(void)
{
	t_return.type = svt_int;   
	if(!multiplayer)
	{
		t_return.value.i = 0; // single-player
	}
	else if(!deathmatch)
	{
		t_return.value.i = 1; // cooperative
	}
	else
		t_return.value.i = 2; // deathmatch
}

//==========================================================================
//
// FParser::SF_IsPlayerObj()
//
//  A function suggested by SoM to help the script coder prevent
//  exceptions related to calling player functions on non-player
//  objects.
//
//==========================================================================

void FParser::SF_IsPlayerObj(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	t_return.value.i = (mo && mo->player) ? 1 : 0;
}

//============================================================================
//
// Inventory stuff - mostly new to GZDoom
//
// all the original functions are still supported but they have not
// been expanded from their original and are limited as a result
//
// Since FraggleScript is rather hard coded to the original inventory
// handling of Doom this is quite messy.
//
//============================================================================


//============================================================================
//
// DoGiveInv
//
// Gives an item to a single actor.
//
//============================================================================

static void FS_GiveInventory (AActor *actor, const char * type, int amount)
{
	if (amount <= 0)
	{
		return;
	}
	if (strcmp (type, "Armor") == 0)
	{
		type = "BasicArmorPickup";
	}
	const PClass * info = PClass::FindClass (type);
	if (info == NULL || !info->IsDescendantOf (RUNTIME_CLASS(AInventory)))
	{
		Printf ("Unknown inventory item: %s\n", type);
		return;
	}

	AWeapon *savedPendingWeap = actor->player != NULL? actor->player->PendingWeapon : NULL;
	bool hadweap = actor->player != NULL ? actor->player->ReadyWeapon != NULL : true;

	AInventory *item = static_cast<AInventory *>(Spawn (info, 0,0,0, NO_REPLACE));

	// This shouldn't count for the item statistics!
	item->ClearCounters();
	if (info->IsDescendantOf (RUNTIME_CLASS(ABasicArmorPickup)) ||
		info->IsDescendantOf (RUNTIME_CLASS(ABasicArmorBonus)))
	{
		static_cast<ABasicArmorPickup*>(item)->SaveAmount *= amount;
	}
	else
	{
		item->Amount = amount;
	}
	if (!item->CallTryPickup (actor))
	{
		item->Destroy ();
	}
	// If the item was a weapon, don't bring it up automatically
	// unless the player was not already using a weapon.
	if (savedPendingWeap != NULL && hadweap)
	{
		actor->player->PendingWeapon = savedPendingWeap;
	}
}

//============================================================================
//
// DoTakeInv
//
// Takes an item from a single actor.
//
//============================================================================

static void FS_TakeInventory (AActor *actor, const char * type, int amount)
{
	if (strcmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	if (amount <= 0)
	{
		return;
	}
	const PClass * info = PClass::FindClass (type);
	if (info == NULL)
	{
		return;
	}

	AInventory *item = actor->FindInventory (info);
	if (item != NULL)
	{
		item->Amount -= amount;
		if (item->Amount <= 0)
		{
			// If it's not ammo, destroy it. Ammo needs to stick around, even
			// when it's zero for the benefit of the weapons that use it and 
			// to maintain the maximum ammo amounts a backpack might have given.
			if (item->GetClass()->ParentClass != RUNTIME_CLASS(AAmmo))
			{
				item->Destroy ();
			}
			else
			{
				item->Amount = 0;
			}
		}
	}
}

//============================================================================
//
// CheckInventory
//
// Returns how much of a particular item an actor has.
//
//============================================================================

static int FS_CheckInventory (AActor *activator, const char *type)
{
	if (activator == NULL)
		return 0;

	if (strcmp (type, "Armor") == 0)
	{
		type = "BasicArmor";
	}
	else if (strcmp (type, "Health") == 0)
	{
		return activator->health;
	}

	const PClass *info = PClass::FindClass (type);
	AInventory *item = activator->FindInventory (info);
	return item ? item->Amount : 0;
}


//==========================================================================
//
// This function is just kept for backwards compatibility 
// and intentionally limited to thr standard keys!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerKeys(void)
{
	static const char * const DoomKeys[]={"BlueCard", "YellowCard", "RedCard", "BlueSkull", "YellowSkull", "RedSkull"};
	int  playernum, keynum, givetake;
	const char * keyname;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;
		
		keynum = intvalue(t_argv[1]);
		if(keynum < 0 || keynum >= 6)
		{
			script_error("key number out of range: %i\n", keynum);
			return;
		}
		keyname=DoomKeys[keynum];
		
		if(t_argc == 2)
		{
			t_return.type = svt_int;
			t_return.value.i = FS_CheckInventory(players[playernum].mo, keyname);
			return;
		}
		else
		{
			givetake = intvalue(t_argv[2]);
			if(givetake) FS_GiveInventory(players[playernum].mo, keyname, 1);
			else FS_TakeInventory(players[playernum].mo, keyname, 1);
			t_return.type = svt_int;
			t_return.value.i = 0;
		}
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and intentionally limited!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerAmmo(void)
{
	int playernum, amount;
	const PClass * ammotype;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		ammotype=T_GetAmmo(t_argv[1]);
		if (!ammotype) return;

		if(t_argc >= 3)
		{
			AInventory * iammo = players[playernum].mo->FindInventory(ammotype);
			amount = intvalue(t_argv[2]);
			if(amount < 0) amount = 0;
			if (iammo) iammo->Amount = amount;
			else players[playernum].mo->GiveAmmo(ammotype, amount);
		}

		t_return.type = svt_int;
		AInventory * iammo = players[playernum].mo->FindInventory(ammotype);
		if (iammo) t_return.value.i = iammo->Amount;
		else t_return.value.i = 0;
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MaxPlayerAmmo()
{
	int playernum, amount;
	const PClass * ammotype;

	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		ammotype=T_GetAmmo(t_argv[1]);
		if (!ammotype) return;

		if(t_argc == 2)
		{
		}
		else if(t_argc >= 3)
		{
			AAmmo * iammo = (AAmmo*)players[playernum].mo->FindInventory(ammotype);
			amount = intvalue(t_argv[2]);
			if(amount < 0) amount = 0;
			if (!iammo) 
			{
				iammo = static_cast<AAmmo *>(Spawn (ammotype, 0, 0, 0, NO_REPLACE));
				iammo->Amount = 0;
				iammo->AttachToOwner (players[playernum].mo);
			}
			iammo->MaxAmount = amount;


			for (AInventory *item = players[playernum].mo->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->IsKindOf(RUNTIME_CLASS(ABackpackItem)))
				{
					if (t_argc>=4) amount = intvalue(t_argv[3]);
					else amount*=2;
					break;
				}
			}
			iammo->BackpackMaxAmount=amount;
		}

		t_return.type = svt_int;
		AInventory * iammo = players[playernum].mo->FindInventory(ammotype);
		if (iammo) t_return.value.i = iammo->MaxAmount;
		else t_return.value.i = ((AAmmo*)GetDefaultByType(ammotype))->MaxAmount;
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and 
// intentionally limited to the standard weapons!
// Use Give/Take/CheckInventory instead!
//
//==========================================================================

void FParser::SF_PlayerWeapon()
{
	static const char * const WeaponNames[]={
		"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher", 
		"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun" };


	int playernum;
	int weaponnum;
	int newweapon;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		weaponnum = intvalue(t_argv[1]);
		if (playernum==-1) return;
		if (weaponnum<0 || weaponnum>9)
		{
			script_error("weaponnum out of range! %s\n", weaponnum);
			return;
		}
		const PClass * ti = PClass::FindClass(WeaponNames[weaponnum]);
		if (!ti)
		{
			script_error("incompatibility in playerweapon\n", weaponnum);
			return;
		}
		
		if (t_argc == 2)
		{
			AActor * wp = players[playernum].mo->FindInventory(ti);
			t_return.type = svt_int;
			t_return.value.i = wp!=NULL;;
			return;
		}
		else
		{
			AActor * wp = players[playernum].mo->FindInventory(ti);

			newweapon = !!intvalue(t_argv[2]);
			if (!newweapon)
			{
				if (wp) 
				{
					wp->Destroy();
					// If the weapon is active pick a replacement. Legacy didn't do this!
					if (players[playernum].PendingWeapon==wp) players[playernum].PendingWeapon=WP_NOCHANGE;
					if (players[playernum].ReadyWeapon==wp) 
					{
						players[playernum].ReadyWeapon=NULL;
						players[playernum].mo->PickNewWeapon(NULL);
					}
				}
			}
			else 
			{
				if (!wp) 
				{
					AWeapon * pw=players[playernum].PendingWeapon;
					players[playernum].mo->GiveInventoryType(ti);
					players[playernum].PendingWeapon=pw;
				}
			}
			
			t_return.type = svt_int;
			t_return.value.i = !!newweapon;
			return;
		}
	}
}

//==========================================================================
//
// This function is just kept for backwards compatibility and 
// intentionally limited to the standard weapons!
//
//==========================================================================

void FParser::SF_PlayerSelectedWeapon()
{
	int playernum;
	int weaponnum;


	static const char * const WeaponNames[]={
		"Fist", "Pistol", "Shotgun", "Chaingun", "RocketLauncher", 
		"PlasmaRifle", "BFG9000", "Chainsaw", "SuperShotgun" };


	if (CheckArgs(1))
	{
		playernum=T_GetPlayerNum(t_argv[0]);

		if(t_argc == 2)
		{
			weaponnum = intvalue(t_argv[1]);

			if (weaponnum<0 || weaponnum>=9)
			{
				script_error("weaponnum out of range! %s\n", weaponnum);
				return;
			}
			const PClass * ti = PClass::FindClass(WeaponNames[weaponnum]);
			if (!ti)
			{
				script_error("incompatibility in playerweapon\n", weaponnum);
				return;
			}

			players[playernum].PendingWeapon = (AWeapon*)players[playernum].mo->FindInventory(ti);

		} 
		t_return.type = svt_int;
		for(int i=0;i<9;i++)
		{
			if (players[playernum].ReadyWeapon->GetClass ()->TypeName == FName(WeaponNames[i]))
			{
				t_return.value.i=i;
				break;
			}
		}
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_GiveInventory(void)
{
	int  playernum, count;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		if(t_argc == 2) count=1;
		else count=intvalue(t_argv[2]);
		FS_GiveInventory(players[playernum].mo, stringvalue(t_argv[1]), count);
		t_return.type = svt_int;
		t_return.value.i = 0;
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_TakeInventory(void)
{
	int  playernum, count;

	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) return;

		if(t_argc == 2) count=32767;
		else count=intvalue(t_argv[2]);
		FS_TakeInventory(players[playernum].mo, stringvalue(t_argv[1]), count);
		t_return.type = svt_int;
		t_return.value.i = 0;
	}
}

//==========================================================================
//
// new for GZDoom: named inventory handling
//
//==========================================================================

void FParser::SF_CheckInventory(void)
{
	int  playernum;
	
	if (CheckArgs(2))
	{
		playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum==-1) 
		{
			t_return.value.i = 0;
			return;
		}
		t_return.type = svt_int;
		t_return.value.i = FS_CheckInventory(players[playernum].mo, stringvalue(t_argv[1]));
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SetWeapon()
{
	if (CheckArgs(2))
	{
		int playernum=T_GetPlayerNum(t_argv[0]);
		if (playernum!=-1) 
		{
			AInventory *item = players[playernum].mo->FindInventory (PClass::FindClass (stringvalue(t_argv[1])));

			if (item == NULL || !item->IsKindOf (RUNTIME_CLASS(AWeapon)))
			{
			}
			else if (players[playernum].ReadyWeapon == item)
			{
				// The weapon is already selected, so setweapon succeeds by default,
				// but make sure the player isn't switching away from it.
				players[playernum].PendingWeapon = WP_NOCHANGE;
				t_return.value.i = 1;
			}
			else
			{
				AWeapon *weap = static_cast<AWeapon *> (item);

				if (weap->CheckAmmo (AWeapon::EitherFire, false))
				{
					// There's enough ammo, so switch to it.
					t_return.value.i = 1;
					players[playernum].PendingWeapon = weap;
				}
			}
		}
		t_return.value.i = 0;
	}
}

//==========================================================================
//
// movecamera(camera, targetobj, targetheight, movespeed, targetangle, anglespeed)
//
//==========================================================================

void FParser::SF_MoveCamera(void)
{
	fixed_t    x, y, z;  
	fixed_t    zdist, xydist, movespeed;
	fixed_t    xstep, ystep, zstep, targetheight;
	angle_t    anglespeed, anglestep, angledist, targetangle, 
		mobjangle, bigangle, smallangle;
	
	// I have to use floats for the math where angles are divided 
	// by fixed values.  
	double     fangledist, fanglestep, fmovestep;
	int        angledir;  
	AActor*    target;
	int        moved;
	int        quad1, quad2;
	AActor		* cam;
	
	angledir = moved = 0;

	if (CheckArgs(6))
	{
		cam = actorvalue(t_argv[0]);

		target = actorvalue(t_argv[1]);
		if(!cam || !target) 
		{ 
			script_error("invalid target for camera\n"); return; 
		}
		
		targetheight = fixedvalue(t_argv[2]);
		movespeed    = fixedvalue(t_argv[3]);
		targetangle  = (angle_t)FixedToAngle(fixedvalue(t_argv[4]));
		anglespeed   = (angle_t)FixedToAngle(fixedvalue(t_argv[5]));
		
		// figure out how big one step will be
		fixedvec2 dist = cam->Vec2To(target);
		zdist = targetheight - cam->Z();
		
		// Angle checking...  
		//    90  
		//   Q1|Q0  
		//180--+--0  
		//   Q2|Q3  
		//    270
		quad1 = targetangle / ANG90;
		quad2 = cam->angle / ANG90;
		bigangle = targetangle > cam->angle ? targetangle : cam->angle;
		smallangle = targetangle < cam->angle ? targetangle : cam->angle;
		if((quad1 > quad2 && quad1 - 1 == quad2) || (quad2 > quad1 && quad2 - 1 == quad1) ||
			quad1 == quad2)
		{
			angledist = bigangle - smallangle;
			angledir = targetangle > cam->angle ? 1 : -1;
		}
		else
		{
			angle_t diff180 = (bigangle + ANG180) - (smallangle + ANG180);
			
			if(quad2 == 3 && quad1 == 0)
			{
				angledist = diff180;
				angledir = 1;
			}
			else if(quad1 == 3 && quad2 == 0)
			{
				angledist = diff180;
				angledir = -1;
			}
			else
			{
				angledist = bigangle - smallangle;
				if(angledist > ANG180)
				{
					angledist = diff180;
					angledir = targetangle > cam->angle ? -1 : 1;
				}
				else
					angledir = targetangle > cam->angle ? 1 : -1;
			}
		}
		
		// set step variables based on distance and speed
		mobjangle = cam->AngleTo(target);
		xydist = cam->Distance2D(target);
		
		xstep = FixedMul(finecosine[mobjangle >> ANGLETOFINESHIFT], movespeed);
		ystep = FixedMul(finesine[mobjangle >> ANGLETOFINESHIFT], movespeed);
		
		if(xydist && movespeed)
			zstep = FixedDiv(zdist, FixedDiv(xydist, movespeed));
		else
			zstep = zdist > 0 ? movespeed : -movespeed;
		
		if(xydist && movespeed && !anglespeed)
		{
			fangledist = ((double)angledist / (ANG45/45));
			fmovestep = ((double)FixedDiv(xydist, movespeed) / FRACUNIT);
			if(fmovestep)
				fanglestep = fangledist / fmovestep;
			else
				fanglestep = 360;
			
			anglestep =(angle_t) (fanglestep * (ANG45/45));
		}
		else
			anglestep = anglespeed;
		
		if(abs(xstep) >= (abs(dist.x) - 1))
			x = cam->X() + dist.x;
		else
		{
			x = cam->X() + xstep;
			moved = 1;
		}
		
		if(abs(ystep) >= (abs(dist.y) - 1))
			y = cam->Y() + dist.y;
		else
		{
			y = cam->Y() + ystep;
			moved = 1;
		}
		
		if(abs(zstep) >= (abs(zdist) - 1))
			z = targetheight;
		else
		{
			z = cam->Z() + zstep;
			moved = 1;
		}
		
		if(anglestep >= angledist)
			cam->angle = targetangle;
		else
		{
			if(angledir == 1)
			{
				cam->angle += anglestep;
				moved = 1;
			}
			else if(angledir == -1)
			{
				cam->angle -= anglestep;
				moved = 1;
			}
		}

		cam->radius=8;
		cam->height=8;
		if ((x != cam->X() || y != cam->Y()) && !P_TryMove(cam, x, y, true))
		{
			Printf("Illegal camera move to (%f, %f)\n", x/65536.f, y/65536.f);
			return;
		}
		cam->SetZ(z);

		t_return.type = svt_int;
		t_return.value.i = moved;
	}
}



//==========================================================================
//
// FParser::SF_ObjAwaken
//
// Implements: void objawaken([mobj mo])
//
//==========================================================================

void FParser::SF_ObjAwaken(void)
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
   	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if(mo)
	{
		mo->Activate(Script->trigger);
	}
}

//==========================================================================
//
// FParser::SF_AmbientSound
//
// Implements: void ambientsound(string name)
//
//==========================================================================

void FParser::SF_AmbientSound(void)
{
	if (CheckArgs(1))
	{
		S_Sound(CHAN_AUTO, T_FindSound(stringvalue(t_argv[0])), 1, ATTN_NORM);
	}
}


//==========================================================================
// 
// FParser::SF_ExitSecret
//
// Implements: void exitsecret()
//
//==========================================================================

void FParser::SF_ExitSecret(void)
{
	G_SecretExitLevel(0);
}


//==========================================================================
//
// Type forcing functions -- useful with arrays et al
//
//==========================================================================

void FParser::SF_MobjValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_mobj;
		t_return.value.mobj = actorvalue(t_argv[0]);
	}
}

void FParser::SF_StringValue(void)
{  
	if (CheckArgs(1))
	{
		t_return.type = svt_string;
		if (t_argv[0].type == svt_string)
		{
			t_return.string = t_argv[0].string;
		}
		else
		{
			t_return.string = stringvalue(t_argv[0]);
		}
	}
}

void FParser::SF_IntValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_int;
		t_return.value.i = intvalue(t_argv[0]);
	}
}

void FParser::SF_FixedValue(void)
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = fixedvalue(t_argv[0]);
	}
}


//==========================================================================
//
// Starting here are functions present in Legacy but not Eternity.
//
//==========================================================================

void FParser::SF_SpawnExplosion()
{
	fixed_t   x, y, z;
	AActor*   spawn;
	const PClass * PClass;
	
	if (CheckArgs(3))
	{
		if (!(PClass=T_GetMobjType(t_argv[0]))) return;
		
		x = fixedvalue(t_argv[1]);
		y = fixedvalue(t_argv[2]);
		if(t_argc > 3)
			z = fixedvalue(t_argv[3]);
		else
			z = P_PointInSector(x, y)->floorplane.ZatPoint(x,y);
		
		spawn = Spawn (PClass, x, y, z, ALLOW_REPLACE);
		t_return.type = svt_int;
		t_return.value.i=0;
		if (spawn)
		{
			spawn->ClearCounters();
			t_return.value.i = spawn->SetState(spawn->FindState(NAME_Death));
			if(spawn->DeathSound) S_Sound (spawn, CHAN_BODY, spawn->DeathSound, 1, ATTN_NORM);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_RadiusAttack()
{
	AActor *spot;
	AActor *source;
	int damage;

	if (CheckArgs(3))
	{
		spot = actorvalue(t_argv[0]);
		source = actorvalue(t_argv[1]);
		damage = intvalue(t_argv[2]);

		if (spot && source)
		{
			P_RadiusAttack(spot, source, damage, damage, NAME_None, RADF_HURTSOURCE);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SetObjPosition()
{
	AActor* mobj;

	if (CheckArgs(2))
	{
		mobj = actorvalue(t_argv[0]);

		if (!mobj) return;

		mobj->SetOrigin(
			fixedvalue(t_argv[1]),
			(t_argc >= 3)? fixedvalue(t_argv[2]) : mobj->Y(),
			(t_argc >= 4)? fixedvalue(t_argv[3]) : mobj->Z(), false);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_TestLocation()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if (mo)
	{
		t_return.type = svt_int;
		t_return.value.f = !!P_TestMobjLocation(mo);
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_HealObj()  //no pain sound
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if(t_argc < 2)
	{
		mo->health = mo->GetDefault()->health;
		if(mo->player) mo->player->health = mo->health;
	}

	else if (t_argc == 2)
	{
		mo->health += intvalue(t_argv[1]);
		if(mo->player) mo->player->health = mo->health;
	}

	else
		script_error("invalid number of arguments for objheal");
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjDead()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	t_return.type = svt_int;
	if(mo && (mo->health <= 0 || mo->flags&MF_CORPSE))
		t_return.value.i = 1;
	else
		t_return.value.i = 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SpawnMissile()
{
	AActor *mobj;
	AActor *target;
	const PClass * PClass;

	if (CheckArgs(3))
	{
		if (!(PClass=T_GetMobjType(t_argv[2]))) return;

		mobj = actorvalue(t_argv[0]);
		target = actorvalue(t_argv[1]);
		if (mobj && target) P_SpawnMissile(mobj, target, PClass);
	}
}

//==========================================================================
//
//checks to see if a Map Thing Number exists; used to avoid script errors
//
//==========================================================================

void FParser::SF_MapThingNumExist()
{
	TArray<TObjPtr<AActor> > &SpawnedThings = DFraggleThinker::ActiveThinker->SpawnedThings;

	int intval;

	if (CheckArgs(1))
	{
		intval = intvalue(t_argv[0]);

		if (intval < 0 || intval >= int(SpawnedThings.Size()) || !SpawnedThings[intval])
		{
			t_return.type = svt_int;
			t_return.value.i = 0;
		}
		else
		{
			// Inventory items in the player's inventory have to be considered non-present.
			if (SpawnedThings[intval]->IsKindOf(RUNTIME_CLASS(AInventory)) && 
				barrier_cast<AInventory*>(SpawnedThings[intval])->Owner != NULL)
			{
				t_return.value.i = 0;
			}
			else
			{
				t_return.value.i = 1;
			}
			t_return.type = svt_int;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_MapThings()
{
	TArray<TObjPtr<AActor> > &SpawnedThings = DFraggleThinker::ActiveThinker->SpawnedThings;

	t_return.type = svt_int;
	t_return.value.i = SpawnedThings.Size();
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ObjState()
{
	int state;
	AActor *mo = NULL;

	if (CheckArgs(1))
	{
		if(t_argc == 1)
		{
			mo = Script->trigger;
			state = intvalue(t_argv[0]);
		}

		else if(t_argc == 2)
		{
			mo = actorvalue(t_argv[0]);
			state = intvalue(t_argv[1]);
		}

		if (mo) 
		{
			static ENamedName statenames[]= {
				NAME_Spawn, NAME_See, NAME_Missile,	NAME_Melee,
				NAME_Pain, NAME_Death, NAME_Raise, NAME_XDeath, NAME_Crash };

			if (state <1 || state > 9)
			{
				script_error("objstate: invalid state");
				return;
			}

			t_return.type = svt_int;
			t_return.value.i = mo->SetState(mo->FindState(statenames[state]));
		}
	}
}


//==========================================================================
//
// only here for Legacy maps. The implementation of this function
// is completely useless.
//
//==========================================================================

void FParser::SF_LineFlag()
{
	line_t*  line;
	int      linenum;
	int      flagnum;
	
	if (CheckArgs(2))
	{
		linenum = intvalue(t_argv[0]);
		if(linenum < 0 || linenum > numlines)
		{
			script_error("LineFlag: Invalid line number.\n");
			return;
		}
		
		line = lines + linenum;
		
		flagnum = intvalue(t_argv[1]);
		if(flagnum < 0 || (flagnum > 8 && flagnum!=15))
		{
			script_error("LineFlag: Invalid flag number.\n");
			return;
		}
		
		if(t_argc > 2)
		{
			line->flags &= ~(1 << flagnum);
			if(intvalue(t_argv[2]))
				line->flags |= (1 << flagnum);
		}
		
		t_return.type = svt_int;
		t_return.value.i = line->flags & (1 << flagnum);
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_PlayerAddFrag()
{
	int playernum1;
	int playernum2;

	if (CheckArgs(1))
	{
		if (t_argc == 1)
		{
			playernum1 = T_GetPlayerNum(t_argv[0]);

			players[playernum1].fragcount++;

			t_return.type = svt_int;
			t_return.value.f = players[playernum1].fragcount;
		}

		else
		{
			playernum1 = T_GetPlayerNum(t_argv[0]);
			playernum2 = T_GetPlayerNum(t_argv[1]);

			players[playernum1].frags[playernum2]++;

			t_return.type = svt_int;
			t_return.value.f = players[playernum1].frags[playernum2];
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_SkinColor()
{
	// Ignoring it for now.
}

void FParser::SF_PlayDemo()
{ 
	// Ignoring it for now.
}

void FParser::SF_CheckCVar()
{
	// can't be done so return 0.
}
//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_Resurrect()
{

	AActor *mo;

	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);

		FState * state = mo->FindState(NAME_Raise);
		if (!state)  //Don't resurrect things that can't be resurrected
			return;

		mo->SetState(state);
		mo->height = mo->GetDefault()->height;
		mo->radius = mo->GetDefault()->radius;
		mo->flags =  mo->GetDefault()->flags;
		mo->flags2 = mo->GetDefault()->flags2;
		mo->flags3 = mo->GetDefault()->flags3;
		mo->flags4 = mo->GetDefault()->flags4;
		mo->flags5 = mo->GetDefault()->flags5;
		mo->health = mo->GetDefault()->health;
		mo->target = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_LineAttack()
{
	AActor	*mo;
	int		damage, angle, slope;

	if (CheckArgs(3))
	{
		mo = actorvalue(t_argv[0]);
		damage = intvalue(t_argv[2]);

		angle = (intvalue(t_argv[1]) * (ANG45 / 45));
		slope = P_AimLineAttack(mo, angle, MISSILERANGE);

		P_LineAttack(mo, angle, MISSILERANGE, slope, damage, NAME_Hitscan, NAME_BulletPuff);
	}
}

//==========================================================================
//
// This is a lousy hack. It only works for the standard actors
// and it is quite inefficient.
//
//==========================================================================

void FParser::SF_ObjType()
{
	// use trigger object if not specified
	AActor *mo;
	if(t_argc)
	{
		mo = actorvalue(t_argv[0]);
	}
	else
	{
		mo = Script->trigger;
	}

	if (mo != NULL)
	{
		for (unsigned int i = 0; i < countof(ActorTypes); i++) if (mo->GetClass() == ActorTypes[i])
		{
			t_return.type = svt_int;
			t_return.value.i = i;
			return;
		}
	}
	t_return.type = svt_int;
	t_return.value.i = -1;
}

//==========================================================================
//
// some new math functions
//
//==========================================================================

inline fixed_t double2fixed(double t)
{
	return (fixed_t)(t*65536.0);
}



void FParser::SF_Sin()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(sin(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ASin()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(asin(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Cos()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(cos(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ACos()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(acos(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Tan()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(tan(floatvalue(t_argv[0])));
	}
}


void FParser::SF_ATan()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(atan(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Exp()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(exp(floatvalue(t_argv[0])));
	}
}

void FParser::SF_Log()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(log(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Sqrt()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(sqrt(floatvalue(t_argv[0])));
	}
}


void FParser::SF_Floor()
{
	if (CheckArgs(1))
	{
		t_return.type = svt_fixed;
		t_return.value.f = fixedvalue(t_argv[0]) & 0xffFF0000;
	}
}


void FParser::SF_Pow()
{
	if (CheckArgs(2))
	{
		t_return.type = svt_fixed;
		t_return.value.f = double2fixed(pow(floatvalue(t_argv[0]), floatvalue(t_argv[1])));
	}
}

//==========================================================================
//
// HUD pics (not operational yet!)
//
//==========================================================================


int HU_GetFSPic(FTextureID lumpnum, int xpos, int ypos);
int HU_DeleteFSPic(unsigned int handle);
int HU_ModifyFSPic(unsigned int handle, FTextureID lumpnum, int xpos, int ypos);
int HU_FSDisplay(unsigned int handle, bool newval);

void FParser::SF_NewHUPic()
{
	if (CheckArgs(3))
	{
		t_return.type = svt_int;
		t_return.value.i = HU_GetFSPic(
			TexMan.GetTexture(stringvalue(t_argv[0]), FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny), 
			intvalue(t_argv[1]), intvalue(t_argv[2]));
	}
}

void FParser::SF_DeleteHUPic()
{
	if (CheckArgs(1))
	{
		if (HU_DeleteFSPic(intvalue(t_argv[0])) == -1)
			script_error("deletehupic: Invalid sfpic handle: %i\n", intvalue(t_argv[0]));
	}
}

void FParser::SF_ModifyHUPic()
{
	if (t_argc != 4)
	{
		script_error("modifyhupic: invalid number of arguments\n");
		return;
	}

	if (HU_ModifyFSPic(intvalue(t_argv[0]), 
			TexMan.GetTexture(stringvalue(t_argv[0]), FTexture::TEX_MiscPatch, FTextureManager::TEXMAN_TryAny),
			intvalue(t_argv[2]), intvalue(t_argv[3])) == -1)
	{
		script_error("modifyhypic: invalid sfpic handle %i\n", intvalue(t_argv[0]));
	}
	return;
}

void FParser::SF_SetHUPicDisplay()
{
	if (t_argc != 2)
	{
		script_error("sethupicdisplay: invalud number of arguments\n");
		return;
	}

	if (HU_FSDisplay(intvalue(t_argv[0]), intvalue(t_argv[1]) > 0 ? 1 : 0) == -1)
		script_error("sethupicdisplay: invalid pic handle %i\n", intvalue(t_argv[0]));
}


//==========================================================================
//
// Yet to be made operational.
//
//==========================================================================

void FParser::SF_SetCorona(void)
{
	if(t_argc != 3)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	int num = t_argv[0].value.i;    // which corona we want to modify
	int what = t_argv[1].value.i;   // what we want to modify (type, color, offset,...)
	int ival = t_argv[2].value.i;   // the value of what we modify
	double fval = ((double) t_argv[2].value.f / FRACUNIT);

  	/*
	switch (what)
	{
		case 0:
			lspr[num].type = ival;
			break;
		case 1:
			lspr[num].light_xoffset = fval;
			break;
		case 2:
			lspr[num].light_yoffset = fval;
			break;
		case 3:
			if (t_argv[2].type == svt_string)
				lspr[num].corona_color = String2Hex(t_argv[2].value.s);
			else
				memcpy(&lspr[num].corona_color, &ival, sizeof(int));
			break;
		case 4:
			lspr[num].corona_radius = fval;
			break;
		case 5:
			if (t_argv[2].type == svt_string)
				lspr[num].dynamic_color = String2Hex(t_argv[2].value.s);
			else
				memcpy(&lspr[num].dynamic_color, &ival, sizeof(int));
			break;
		case 6:
			lspr[num].dynamic_radius = fval;
			lspr[num].dynamic_sqrradius = sqrt(lspr[num].dynamic_radius);
			break;
		default:
			CONS_Printf("Error in setcorona\n");
			break;
	}
	*/

	// no use for this!
	t_return.type = svt_int;
	t_return.value.i = 0;
}

//==========================================================================
//
// new for GZDoom: Call a Hexen line special (deprecated, superseded by direct use)
//
//==========================================================================

void FParser::SF_Ls()
{
	int args[5]={0,0,0,0,0};
	int spc;

	if (CheckArgs(1))
	{
		spc=intvalue(t_argv[0]);
		for(int i=0;i<5;i++)
		{
			if (t_argc>=i+2) args[i]=intvalue(t_argv[i+1]);
		}
		if (spc>=0 && spc<256)
			P_ExecuteSpecial(spc, NULL,Script->trigger,false, args[0],args[1],args[2],args[3],args[4]);
	}
}


//==========================================================================
//
// new for GZDoom: Gets the levelnum
//
//==========================================================================

void FParser::SF_LevelNum()
{
	t_return.type = svt_int;
	t_return.value.f = level.levelnum;
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_MobjRadius(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->radius = fixedvalue(t_argv[1]);
		}
		
		t_return.type = svt_fixed;
		t_return.value.f = mo ? mo->radius : 0;
	}
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_MobjHeight(void)
{
	AActor*   mo;
	
	if (CheckArgs(1))
	{
		mo = actorvalue(t_argv[0]);
		if(t_argc > 1)
		{
			if(mo) 
				mo->height = fixedvalue(t_argv[1]);
		}
		
		t_return.type = svt_fixed;
		t_return.value.f = mo ? mo->height : 0;
	}
}


//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void FParser::SF_ThingCount(void)
{
	const PClass *pClass;
	AActor * mo;
	int count=0;
	bool replacemented = false;

	
	if (CheckArgs(1))
	{
		pClass=T_GetMobjType(t_argv[0]);
		if (!pClass) return;
		// If we want to count map items we must consider actor replacement
		pClass = pClass->ActorInfo->GetReplacement()->Class;
		
again:
		TThinkerIterator<AActor> it;

		if (t_argc<2 || intvalue(t_argv[1])==0 || pClass->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			while ((mo=it.Next()))
			{
				if (mo->IsA(pClass))
				{
					if (!mo->IsKindOf (RUNTIME_CLASS(AInventory)) ||
						static_cast<AInventory *>(mo)->Owner == NULL)
					{
						count++;
					}
				}
			}
		}
		else
		{
			while ((mo=it.Next()))
			{
				if (mo->IsA(pClass) && mo->health>0) count++;
			}
		}
		if (!replacemented)
		{
			// Again, with decorate replacements
			replacemented = true;
			PClass *newkind = pClass->ActorInfo->GetReplacement()->Class;
			if (newkind != pClass)
			{
				pClass = newkind;
				goto again;
			}
		}
		t_return.type = svt_int;
		t_return.value.i = count;
	}	
}

//==========================================================================
//
// new for GZDoom: Sets a sector color
//
//==========================================================================

void FParser::SF_SetColor(void)
{
	int tagnum, secnum;
	int c=2;
	int i = -1;
	PalEntry color=0;
	
	if (CheckArgs(2))
	{
		tagnum = intvalue(t_argv[0]);
		
		secnum = T_FindFirstSectorFromTag(tagnum);
		
		if(secnum < 0)
		{ 
			return;
		}
		
		if(t_argc >1 && t_argc<4)
		{
			color=intvalue(t_argv[1]);
		}
		else if (t_argc>=4)
		{
			color.r=intvalue(t_argv[1]);
			color.g=intvalue(t_argv[2]);
			color.b=intvalue(t_argv[3]);
			color.a=0;
		}
		else return;

		// set all sectors with tag
		FSSectorTagIterator itr(tagnum);
		while ((i = itr.Next()) >= 0)
		{
			sectors[i].ColorMap = GetSpecialLights (color, sectors[i].ColorMap->Fade, 0);
		}
	}
}


//==========================================================================
//
// Spawns a projectile at a map spot
//
//==========================================================================

void FParser::SF_SpawnShot2(void)
{
	AActor *source = NULL;
	const PClass * PClass;
	int z=0;
	
	// t_argv[0] = type to spawn
	// t_argv[1] = source mobj, optional, -1 to default
	// shoots at source's angle
	
	if (CheckArgs(2))
	{
		if(t_argv[1].type == svt_int && t_argv[1].value.i < 0)
			source = Script->trigger;
		else
			source = actorvalue(t_argv[1]);

		if (t_argc>2) z=fixedvalue(t_argv[2]);
		
		if(!source)	return;
		
		if (!(PClass=T_GetMobjType(t_argv[0]))) return;
		
		t_return.type = svt_mobj;

		AActor *mo = Spawn (PClass, source->PosPlusZ(z), ALLOW_REPLACE);
		if (mo) 
		{
			S_Sound (mo, CHAN_VOICE, mo->SeeSound, 1, ATTN_NORM);
			mo->target = source;
			P_ThrustMobj(mo, mo->angle = source->angle, mo->Speed);
			if (!P_CheckMissileSpawn(mo, source->radius)) mo = NULL;
		}
		t_return.value.mobj = mo;
	}
}



//==========================================================================
//
// new for GZDoom
//
//==========================================================================

void  FParser::SF_KillInSector()
{
	if (CheckArgs(1))
	{
		TThinkerIterator<AActor> it;
		AActor * mo;
		int tag=intvalue(t_argv[0]);

		while ((mo=it.Next()))
		{
			if (mo->flags3&MF3_ISMONSTER && tagManager.SectorHasTag(mo->Sector, tag)) P_DamageMobj(mo, NULL, NULL, 1000000, NAME_Massacre);
		}
	}
}

//==========================================================================
//
// new for GZDoom: Sets a sector's type
//
//==========================================================================

void FParser::SF_SectorType(void)
{
	// I don't think this was ever used publicly so I'm not going to bother fixing it.
}

//==========================================================================
//
// new for GZDoom: Sets a new line trigger type (Doom format!)
// (Sure, this is not particularly useful. But having it made it possible
//  to fix a few annoying bugs in some old maps ;) )
//
//==========================================================================

void FParser::SF_SetLineTrigger()
{
	int i,id,spec,tag(0);

	if (CheckArgs(2))
	{
		id=intvalue(t_argv[0]);
		spec=intvalue(t_argv[1]);
		if (t_argc>2) tag=intvalue(t_argv[2]);
		FLineIdIterator itr(id);
		while ((i = itr.Next()) >= 0)
		{
			maplinedef_t mld;
			mld.special = spec;
			mld.tag = tag;
			mld.flags = 0;
			int f = lines[i].flags;
			P_TranslateLineDef(&lines[i], &mld);
			lines[i].flags = (lines[i].flags & (ML_MONSTERSCANACTIVATE | ML_REPEAT_SPECIAL | ML_SPAC_MASK | ML_FIRSTSIDEONLY)) |
				(f & ~(ML_MONSTERSCANACTIVATE | ML_REPEAT_SPECIAL | ML_SPAC_MASK | ML_FIRSTSIDEONLY));

		}
	}
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_ChangeTag()
{
	// Development garbage!
}


//==========================================================================
//
//
//
//==========================================================================

void FParser::SF_WallGlow()
{
	// Development garbage!
}


//==========================================================================
//
// new for GZDoom: Call a Hexen line special
//
//==========================================================================

void FParser::RunLineSpecial(const FLineSpecial *spec)
{

	if (CheckArgs(spec->min_args))
	{
		int args[5];
		for(int i=0;i<5;i++)
		{
			if (t_argc>i) args[i]=intvalue(t_argv[i]);
			else args[i] = 0;
		}
		t_return.value.i = P_ExecuteSpecial(spec->number, NULL,Script->trigger,false, args[0],args[1],args[2],args[3],args[4]);
	}
}


//==========================================================================
//
//
//
//==========================================================================

DRunningScript *FParser::SaveCurrentScript()
{
	DFraggleThinker *th = DFraggleThinker::ActiveThinker;
	if (th)
	{
		DRunningScript *runscr = new DRunningScript(Script->trigger, Script, Script->MakeIndex(Rover));

		// hook into chain at start
		th->AddRunningScript(runscr);
		return runscr;
	}
	return NULL;
}

//==========================================================================
//
// script function
//
//==========================================================================

void FParser::SF_Wait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_delay;
	
	runscr->wait_data = (intvalue(t_argv[0]) * TICRATE) / 100;
	throw CFsTerminator();
}

//==========================================================================
//
// wait for sector with particular tag to stop moving
//
//==========================================================================

void FParser::SF_TagWait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_tagwait;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// wait for a script to finish
//
//==========================================================================

void FParser::SF_ScriptWait()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("insufficient arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	
	runscr->wait_type = wt_scriptwait;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// haleyjd 05/23/01: wait for a script to start (zdoom-inspired)
//
//==========================================================================

void FParser::SF_ScriptWaitPre()
{
	DRunningScript *runscr;
	
	if(t_argc != 1)
	{
		script_error("insufficient arguments to function\n");
		return;
	}
	
	runscr = SaveCurrentScript();
	runscr->wait_type = wt_scriptwaitpre;
	runscr->wait_data = intvalue(t_argv[0]);
	throw CFsTerminator();
}

//==========================================================================
//
// start a new script
//
//==========================================================================

void FParser::SF_StartScript()
{
	if(t_argc != 1)
	{
		script_error("incorrect arguments to function\n");
		return;
	}
	
	int snum = intvalue(t_argv[0]);

	if(snum < 0 || snum >= MAXSCRIPTS)
	{
		script_error("script number %d out of range\n",snum);
		return;
	}

	DFraggleThinker *th = DFraggleThinker::ActiveThinker;
	if (th)
	{

		DFsScript *script = th->LevelScript->children[snum];
	
		if(!script)
		{
			script_error("script %i not defined\n", snum);
		}
		
		DRunningScript *runscr = new DRunningScript(Script->trigger, script, 0);
		// hook into chain at start
		th->AddRunningScript(runscr);
	}
}

//==========================================================================
//
// checks if a script is running
//
//==========================================================================

void FParser::SF_ScriptRunning()
{
	DRunningScript *current;
	int snum = 0;
	
	if(t_argc < 1)
	{
		script_error("not enough arguments to function\n");
		return;
	}
	
	snum = intvalue(t_argv[0]);  
	
	for(current = DFraggleThinker::ActiveThinker->RunningScripts->next; current; current=current->next)
	{
		if(current->script->scriptnum == snum)
		{
			// script found so return
			t_return.type = svt_int;
			t_return.value.i = 1;
			return;
		}
	}
	
	// script not found
	t_return.type = svt_int;
	t_return.value.i = 0;
}


//==========================================================================
//
// Init Functions
//
//==========================================================================

static int zoom=1;	// Dummy - no longer needed!

void init_functions(void)
{
	for(unsigned i=0;i<countof(ActorNames_init);i++)
	{
		ActorTypes[i]=PClass::FindClass(ActorNames_init[i]);
	}

	DFsScript * gscr = global_script;

	// add all the functions
	gscr->NewVariable("consoleplayer", svt_pInt)->value.pI = &consoleplayer;
	gscr->NewVariable("displayplayer", svt_pInt)->value.pI = &consoleplayer;
	gscr->NewVariable("zoom", svt_pInt)->value.pI = &zoom;
	gscr->NewVariable("fov", svt_pInt)->value.pI = &zoom;
	gscr->NewVariable("trigger", svt_pMobj)->value.pMobj = &trigger_obj;

	// Create constants for all existing line specials
	for(int i=0; i<256; i++)
	{
		const FLineSpecial *ls = LineSpecialsInfo[i];

		if (ls != NULL && ls->max_args >= 0)	// specials with max args set to -1 can only be used in a map and are of no use hee.
		{
			gscr->NewVariable(ls->name, svt_linespec)->value.ls = ls;
		}
	}
	
	// important C-emulating stuff
	gscr->NewFunction("break", &FParser::SF_Break);
	gscr->NewFunction("continue", &FParser::SF_Continue);
	gscr->NewFunction("return", &FParser::SF_Return);
	gscr->NewFunction("goto", &FParser::SF_Goto);
	gscr->NewFunction("include", &FParser::SF_Include);
	
	// standard FraggleScript functions
	gscr->NewFunction("print", &FParser::SF_Print);
	gscr->NewFunction("rnd", &FParser::SF_Rnd);	// Legacy uses a normal rand() call for this which is extremely dangerous.
	gscr->NewFunction("prnd", &FParser::SF_Rnd);	// I am mapping rnd and prnd to the same named RNG which should eliminate any problem
	gscr->NewFunction("input", &FParser::SF_Input);
	gscr->NewFunction("beep", &FParser::SF_Beep);
	gscr->NewFunction("clock", &FParser::SF_Clock);
	gscr->NewFunction("wait", &FParser::SF_Wait);
	gscr->NewFunction("tagwait", &FParser::SF_TagWait);
	gscr->NewFunction("scriptwait", &FParser::SF_ScriptWait);
	gscr->NewFunction("startscript", &FParser::SF_StartScript);
	gscr->NewFunction("scriptrunning", &FParser::SF_ScriptRunning);
	
	// doom stuff
	gscr->NewFunction("startskill", &FParser::SF_StartSkill);
	gscr->NewFunction("exitlevel", &FParser::SF_ExitLevel);
	gscr->NewFunction("tip", &FParser::SF_Tip);
	gscr->NewFunction("timedtip", &FParser::SF_TimedTip);
	gscr->NewFunction("message", &FParser::SF_Message);
	gscr->NewFunction("gameskill", &FParser::SF_Gameskill);
	gscr->NewFunction("gamemode", &FParser::SF_Gamemode);
	
	// player stuff
	gscr->NewFunction("playermsg", &FParser::SF_PlayerMsg);
	gscr->NewFunction("playertip", &FParser::SF_PlayerTip);
	gscr->NewFunction("playeringame", &FParser::SF_PlayerInGame);
	gscr->NewFunction("playername", &FParser::SF_PlayerName);
	gscr->NewFunction("playeraddfrag", &FParser::SF_PlayerAddFrag);
	gscr->NewFunction("playerobj", &FParser::SF_PlayerObj);
	gscr->NewFunction("isplayerobj", &FParser::SF_IsPlayerObj);
	gscr->NewFunction("isobjplayer", &FParser::SF_IsPlayerObj);
	gscr->NewFunction("skincolor", &FParser::SF_SkinColor);
	gscr->NewFunction("playerkeys", &FParser::SF_PlayerKeys);
	gscr->NewFunction("playerammo", &FParser::SF_PlayerAmmo);
	gscr->NewFunction("maxplayerammo", &FParser::SF_MaxPlayerAmmo); 
	gscr->NewFunction("playerweapon", &FParser::SF_PlayerWeapon);
	gscr->NewFunction("playerselwep", &FParser::SF_PlayerSelectedWeapon);
	
	// mobj stuff
	gscr->NewFunction("spawn", &FParser::SF_Spawn);
	gscr->NewFunction("spawnexplosion", &FParser::SF_SpawnExplosion);
	gscr->NewFunction("radiusattack", &FParser::SF_RadiusAttack);
	gscr->NewFunction("kill", &FParser::SF_KillObj);
	gscr->NewFunction("removeobj", &FParser::SF_RemoveObj);
	gscr->NewFunction("objx", &FParser::SF_ObjX);
	gscr->NewFunction("objy", &FParser::SF_ObjY);
	gscr->NewFunction("objz", &FParser::SF_ObjZ);
	gscr->NewFunction("testlocation", &FParser::SF_TestLocation);
	gscr->NewFunction("teleport", &FParser::SF_Teleport);
	gscr->NewFunction("silentteleport", &FParser::SF_SilentTeleport);
	gscr->NewFunction("damageobj", &FParser::SF_DamageObj);
	gscr->NewFunction("healobj", &FParser::SF_HealObj);
	gscr->NewFunction("player", &FParser::SF_Player);
	gscr->NewFunction("objsector", &FParser::SF_ObjSector);
	gscr->NewFunction("objflag", &FParser::SF_ObjFlag);
	gscr->NewFunction("pushobj", &FParser::SF_PushThing);
	gscr->NewFunction("pushthing", &FParser::SF_PushThing);
	gscr->NewFunction("objangle", &FParser::SF_ObjAngle);
	gscr->NewFunction("objhealth", &FParser::SF_ObjHealth);
	gscr->NewFunction("objdead", &FParser::SF_ObjDead);
	gscr->NewFunction("reactiontime", &FParser::SF_ReactionTime);
	gscr->NewFunction("objreactiontime", &FParser::SF_ReactionTime);
	gscr->NewFunction("objtarget", &FParser::SF_MobjTarget);
	gscr->NewFunction("objmomx", &FParser::SF_MobjMomx);
	gscr->NewFunction("objmomy", &FParser::SF_MobjMomy);
	gscr->NewFunction("objmomz", &FParser::SF_MobjMomz);

	gscr->NewFunction("spawnmissile", &FParser::SF_SpawnMissile);
	gscr->NewFunction("mapthings", &FParser::SF_MapThings);
	gscr->NewFunction("objtype", &FParser::SF_ObjType);
	gscr->NewFunction("mapthingnumexist", &FParser::SF_MapThingNumExist);
	gscr->NewFunction("objstate", &FParser::SF_ObjState);
	gscr->NewFunction("resurrect", &FParser::SF_Resurrect);
	gscr->NewFunction("lineattack", &FParser::SF_LineAttack);
	gscr->NewFunction("setobjposition", &FParser::SF_SetObjPosition);

	// sector stuff
	gscr->NewFunction("floorheight", &FParser::SF_FloorHeight);
	gscr->NewFunction("floortext", &FParser::SF_FloorTexture);
	gscr->NewFunction("floortexture", &FParser::SF_FloorTexture);   // haleyjd: alias
	gscr->NewFunction("movefloor", &FParser::SF_MoveFloor);
	gscr->NewFunction("ceilheight", &FParser::SF_CeilingHeight);
	gscr->NewFunction("ceilingheight", &FParser::SF_CeilingHeight); // haleyjd: alias
	gscr->NewFunction("moveceil", &FParser::SF_MoveCeiling);
	gscr->NewFunction("moveceiling", &FParser::SF_MoveCeiling);     // haleyjd: aliases
	gscr->NewFunction("ceilingtexture", &FParser::SF_CeilingTexture);
	gscr->NewFunction("ceiltext", &FParser::SF_CeilingTexture);  // haleyjd: wrong
	gscr->NewFunction("lightlevel", &FParser::SF_LightLevel);    // handler - was
	gscr->NewFunction("fadelight", &FParser::SF_FadeLight);      // &FParser::SF_FloorTexture!
	gscr->NewFunction("colormap", &FParser::SF_SectorColormap);
	
	// cameras!
	gscr->NewFunction("setcamera", &FParser::SF_SetCamera);
	gscr->NewFunction("clearcamera", &FParser::SF_ClearCamera);
	gscr->NewFunction("movecamera", &FParser::SF_MoveCamera);
	
	// trig functions
	gscr->NewFunction("pointtoangle", &FParser::SF_PointToAngle);
	gscr->NewFunction("pointtodist", &FParser::SF_PointToDist);
	
	// sound functions
	gscr->NewFunction("startsound", &FParser::SF_StartSound);
	gscr->NewFunction("startsectorsound", &FParser::SF_StartSectorSound);
	gscr->NewFunction("ambientsound", &FParser::SF_AmbientSound);
	gscr->NewFunction("startambiantsound", &FParser::SF_AmbientSound);	// Legacy's incorrectly spelled name!
	gscr->NewFunction("changemusic", &FParser::SF_ChangeMusic);
	
	// hubs!
	gscr->NewFunction("changehublevel", &FParser::SF_ChangeHubLevel);
	
	// doors
	gscr->NewFunction("opendoor", &FParser::SF_OpenDoor);
	gscr->NewFunction("closedoor", &FParser::SF_CloseDoor);

	// HU Graphics
	gscr->NewFunction("newhupic", &FParser::SF_NewHUPic);
	gscr->NewFunction("createpic", &FParser::SF_NewHUPic);
	gscr->NewFunction("deletehupic", &FParser::SF_DeleteHUPic);
	gscr->NewFunction("modifyhupic", &FParser::SF_ModifyHUPic);
	gscr->NewFunction("modifypic", &FParser::SF_ModifyHUPic);
	gscr->NewFunction("sethupicdisplay", &FParser::SF_SetHUPicDisplay);
	gscr->NewFunction("setpicvisible", &FParser::SF_SetHUPicDisplay);

	//
	gscr->NewFunction("playdemo", &FParser::SF_PlayDemo);
	gscr->NewFunction("runcommand", &FParser::SF_RunCommand);
	gscr->NewFunction("checkcvar", &FParser::SF_CheckCVar);
	gscr->NewFunction("setlinetexture", &FParser::SF_SetLineTexture);
	gscr->NewFunction("linetrigger", &FParser::SF_LineTrigger);
	gscr->NewFunction("lineflag", &FParser::SF_LineFlag);

	//Hurdler: new math functions
	gscr->NewFunction("max", &FParser::SF_Max);
	gscr->NewFunction("min", &FParser::SF_Min);
	gscr->NewFunction("abs", &FParser::SF_Abs);

	gscr->NewFunction("sin", &FParser::SF_Sin);
	gscr->NewFunction("asin", &FParser::SF_ASin);
	gscr->NewFunction("cos", &FParser::SF_Cos);
	gscr->NewFunction("acos", &FParser::SF_ACos);
	gscr->NewFunction("tan", &FParser::SF_Tan);
	gscr->NewFunction("atan", &FParser::SF_ATan);
	gscr->NewFunction("exp", &FParser::SF_Exp);
	gscr->NewFunction("log", &FParser::SF_Log);
	gscr->NewFunction("sqrt", &FParser::SF_Sqrt);
	gscr->NewFunction("floor", &FParser::SF_Floor);
	gscr->NewFunction("pow", &FParser::SF_Pow);
	
	// Eternity extensions
	gscr->NewFunction("setlineblocking", &FParser::SF_SetLineBlocking);
	gscr->NewFunction("setlinetrigger", &FParser::SF_SetLineTrigger);
	gscr->NewFunction("setlinemnblock", &FParser::SF_SetLineMonsterBlocking);
	gscr->NewFunction("scriptwaitpre", &FParser::SF_ScriptWaitPre);
	gscr->NewFunction("exitsecret", &FParser::SF_ExitSecret);
	gscr->NewFunction("objawaken", &FParser::SF_ObjAwaken);
	
	// forced coercion functions
	gscr->NewFunction("mobjvalue", &FParser::SF_MobjValue);
	gscr->NewFunction("stringvalue", &FParser::SF_StringValue);
	gscr->NewFunction("intvalue", &FParser::SF_IntValue);
	gscr->NewFunction("fixedvalue", &FParser::SF_FixedValue);

	// new for GZDoom
	gscr->NewFunction("spawnshot2", &FParser::SF_SpawnShot2);
	gscr->NewFunction("setcolor", &FParser::SF_SetColor);
	gscr->NewFunction("sectortype", &FParser::SF_SectorType);
	gscr->NewFunction("wallglow", &FParser::SF_WallGlow);
	gscr->NewFunction("objradius", &FParser::SF_MobjRadius);
	gscr->NewFunction("objheight", &FParser::SF_MobjHeight);
	gscr->NewFunction("thingcount", &FParser::SF_ThingCount);
	gscr->NewFunction("killinsector", &FParser::SF_KillInSector);
	gscr->NewFunction("changetag", &FParser::SF_ChangeTag);
	gscr->NewFunction("levelnum", &FParser::SF_LevelNum);

	// new inventory
	gscr->NewFunction("giveinventory", &FParser::SF_GiveInventory);
	gscr->NewFunction("takeinventory", &FParser::SF_TakeInventory);
	gscr->NewFunction("checkinventory", &FParser::SF_CheckInventory);
	gscr->NewFunction("setweapon", &FParser::SF_SetWeapon);

	gscr->NewFunction("ls", &FParser::SF_Ls);	// execute Hexen type line special

	// Dummies - shut up warnings
	gscr->NewFunction("setcorona", &FParser::SF_SetCorona);
}

