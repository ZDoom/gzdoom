/*******************************
* B_Bot.h                      *
* Description:                 *
* Used with all b_*            *
*******************************/

#ifndef __B_BOT_H__
#define __B_BOT_H__

#include "c_cvars.h"
#include "m_argv.h"
#include "tables.h"
#include "info.h"
#include "doomdef.h"
#include "d_ticcmd.h"
#include "r_defs.h"

#define FORWARDWALK		0x1900
#define FORWARDRUN		0x3200
#define SIDEWALK		0x1800
#define SIDERUN			0x2800

#define BOT_VERSION 0.97
//Switches-
#define BOT_RELEASE_COMPILE //Define this when compiling a version that should be released.

#define NOCOLOR     11
#define MAXTHINGNODES 100 //Path stuff (paths created between items).
#define SPAWN_DELAY 80  //Used to determine how many tics there are between each bot spawn when bot's are being spawned in a row (like when entering a new map).

#define BOTFILENAME "bots.cfg"

#define MAX_TRAVERSE_DIST 100000000 //10 meters, used within b_func.c
#define AVOID_DIST   45000000 //Try avoid incoming missiles once they reached this close
#define SAFE_SELF_MISDIST 11000000    //Distance from self to target where it's safe to pull a rocket.
#define FRIEND_DIST  15000000 //To friend.
#define DARK_DIST  5000000 //Distance that bot can see enemies in the dark from.
#define WHATS_DARK  50 //light value thats classed as dark.
#define MAX_MONSTER_TARGET_DIST  50000000 //Too high can slow down the performance, see P_mobj.c
#define ENEMY_SCAN_FOV (120*ANGLE_1)
#define THINGTRYTICK 1000
#define MAXMOVEHEIGHT (32*FRACUNIT) //MAXSTEPMOVE but with jumping counted in.
#define GETINCOMBAT 35000000 //Max distance to item. if it's due to be icked up in a combat situation.
#define SHOOTFOV	(60*ANGLE_1)
#define AFTERTICS   (2*TICRATE) //Seconds that bot will be alert on an recent enemy. Ie not looking the other way
#define MAXROAM		(4*TICRATE) //When this time is elapsed the bot will roam after something else.
//monster mod
#define MSPAWN_DELAY 20//Tics between each spawn.
#define MMAXSELECT   100 //Maximum number of monsters that can be selected at a time.
#define MAXTHINGS	 400

struct botskill_t
{
	int aiming;
	int perfection;
	int reaction;   //How fast the bot will fire after seeing the player.
	int isp;        //Instincts of Self Preservation. Personality
};

FArchive &operator<< (FArchive &arc, botskill_t &skill);
FArchive &operator>> (FArchive &arc, botskill_t &skill);

struct pos_t
{
	fixed_t x;
	fixed_t y;
	fixed_t z;
};

enum prior_t
{
	Armour1		= MT_MISC0,
	Armour2		= MT_MISC1,
	Potion		= MT_MISC2,
	Helmet		= MT_MISC3,
	Stimpack	= MT_MISC10,
	Medikit		= MT_MISC11,
	Soul		= MT_MISC12,
	AmmoBox		= MT_MISC17,
	Rocket		= MT_MISC18,
	RoxBox		= MT_MISC19,
	Cell		= MT_MISC20,
	CellPack	= MT_MISC21,
	Shell		= MT_MISC22,
	ShellBox	= MT_MISC23,
	Backpack	= MT_MISC24,
	Bfg			= MT_MISC25,
	Saw			= MT_MISC26,
	Rl			= MT_MISC27,
	Plasma		= MT_MISC28,
	Clip		= MT_CLIP,
	Chain		= MT_CHAINGUN,
	Shotgun		= MT_SHOTGUN,
	Ssg			= MT_SUPERSHOTGUN,
	Invul		= MT_INV,
	Invis		= MT_INS,
	Mega		= MT_MEGA,
	Berserk		= MT_MISC13
};

inline FArchive &operator<< (FArchive &arc, prior_t type)
{
	return arc << (BYTE)type;
}
inline FArchive &operator>> (FArchive &arc, prior_t &type)
{
	BYTE in; arc >> in; type = (prior_t)type; return arc;
}

//Type of chat to give away this tic (if any).
enum botchat_t
{
	c_insult,
	c_kill,
	c_teamup,
	c_none
};

inline FArchive &operator<< (FArchive &arc, botchat_t chat)
{
	return arc << (BYTE)chat;
}
inline FArchive &operator>> (FArchive &arc, botchat_t &chat)
{
	BYTE in; arc >> in; chat = (botchat_t)in; return arc;
}

//Info about all bots in the bots.cfg
//Updated during each level start.
//Info given to bots when they're spawned.
struct botinfo_t
{
	botinfo_t *next;
	char *name;
	char *info;
	botskill_t skill;
	bool inuse;
};

//Used to keep all the globally needed variables
//in nice order.
class DCajunMaster : public DObject
{
	DECLARE_CLASS (DCajunMaster, DObject)
public:
	void ClearPlayer (int playernum);

	//(B_Game.c)
	void Main (int buf);
	void Init ();
	void End();
	void CleanBotstuff (player_s *p);
	bool Spawn (const char *name, int color = NOCOLOR);
	bool LoadBots ();
	void ForgetBots ();
	void DoAddBot (int bnum, char *info);
	void RemoveAllBots (bool fromlist);

	//(B_Func.c)
	bool Check_LOS (AActor *mobj1, AActor *mobj2, angle_t vangle);
	mobjtype_t Warrior_weapon_drop (AActor *actor);

	//(B_Think.c)
	void WhatToGet (AActor *actor, AActor *item);

	//(B_move.c)
	void Roam (AActor *actor, ticcmd_t *cmd);
	BOOL Move (AActor *actor, ticcmd_t *cmd);
	bool TryWalk (AActor *actor, ticcmd_t *cmd);
	void NewChaseDir (AActor *actor, ticcmd_t *cmd);
	bool CleanAhead (AActor *thing, fixed_t x, fixed_t y, ticcmd_t *cmd);
	void TurnToAng (AActor *actor);
	void Pitch (AActor *actor, AActor *target);
	bool IsDangerous (sector_t *sec);

	DArgs *getspawned; //Array of bots (their names) which should be spawned when starting a game.
	bool botingame[MAXPLAYERS]; 
	bool freeze;    //Game in freeze mode.
	int botnum;
	botinfo_t *botinfo;
	int spawn_tries;
	int wanted_botnum;
	bool itemsdone; //When array filled. bots can start to think, prevents lock after level completion.
	AActor *firstthing;

	int		 thingnum;
	AActor	*things[MAXTHINGS];
	bool	 m_Thinking;

private:
	//(B_Func.c)
	bool Reachable (AActor *actor, AActor *target);
	void Dofire (AActor *actor, ticcmd_t *cmd);
	AActor *Choose_Mate (AActor *bot);
	AActor *Find_enemy (AActor *bot);
	void SetBodyAt (fixed_t x, fixed_t y, fixed_t z, int hostnum);
	pos_t FakeFire (AActor *source, AActor *dest, ticcmd_t *cmd);
	angle_t FireRox (AActor *bot, AActor *enemy, ticcmd_t *cmd);
	bool SafeCheckPosition (AActor *actor, fixed_t x, fixed_t y);

	//(B_Think.c)
	void Think (AActor *actor, ticcmd_t *cmd);
	void ThinkForMove (AActor *actor, ticcmd_t *cmd);
	void Set_enemy (AActor *actor);

protected:
	int		 playernumber;
	bool	 ctf;
	int		 loaded_bots;
	int		 t_join;
	fixed_t	 combatdst[NUMWEAPONS+1]; //different for each weapon.
	AActor	*body1;
	AActor	*body2;
	bool	 observer; //Consoleplayer is observer.
};


//Externs
extern DCajunMaster bglobal;

EXTERN_CVAR (bot_flag_return_time)
EXTERN_CVAR (bot_next_color)
EXTERN_CVAR (weapondrop)
EXTERN_CVAR (bot_allow_duds)
EXTERN_CVAR (bot_maxcorpses)
EXTERN_CVAR (bot_observer)
EXTERN_CVAR (bot_watersplash)
EXTERN_CVAR (bot_chat)

#endif	// __B_BOT_H__




