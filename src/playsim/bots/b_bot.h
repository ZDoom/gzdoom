/*******************************
* B_Bot.h                      *
* Description:                 *
* Used with all b_*            *
*******************************/

#ifndef __B_BOT_H__
#define __B_BOT_H__

#include "c_cvars.h"
#include "info.h"
#include "doomdef.h"
#include "d_protocol.h"
#include "r_defs.h"
#include "a_pickups.h"
#include "a_weapons.h"
#include "stats.h"

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

#define MAX_TRAVERSE_DIST (100000000/65536.) //10 meters, used within b_func.c
#define AVOID_DIST   (45000000/65536.) //Try avoid incoming missiles once they reached this close
#define SAFE_SELF_MISDIST (140.)    //Distance from self to target where it's safe to pull a rocket.
#define FRIEND_DIST  (15000000/65536.) //To friend.
#define DARK_DIST  (5000000/65536.) //Distance that bot can see enemies in the dark from.
#define WHATS_DARK  50 //light value thats classed as dark.
#define MAX_MONSTER_TARGET_DIST  (50000000/65536.) //Too high can slow down the performance, see P_mobj.c
#define ENEMY_SCAN_FOV (120.)
#define THINGTRYTICK 1000
#define MAXMOVEHEIGHT (32) //MAXSTEPMOVE but with jumping counted in.
#define GETINCOMBAT (35000000/65536.) //Max distance to item. if it's due to be icked up in a combat situation.
#define SHOOTFOV	(60.)
#define AFTERTICS   (2*TICRATE) //Seconds that bot will be alert on an recent enemy. Ie not looking the other way
#define MAXROAM		(4*TICRATE) //When this time is elapsed the bot will roam after something else.
//monster mod
#define MSPAWN_DELAY 20//Tics between each spawn.
#define MMAXSELECT   100 //Maximum number of monsters that can be selected at a time.

struct FCheckPosition;

struct botskill_t
{
	int aiming;
	int perfection;
	int reaction;   //How fast the bot will fire after seeing the player.
	int isp;        //Instincts of Self Preservation. Personality
};

enum
{
	BOTINUSE_No,
	BOTINUSE_Waiting,
	BOTINUSE_Yes,
};

//Info about all bots in the bots.cfg
//Updated during each level start.
//Info given to bots when they're spawned.
struct botinfo_t
{
	botinfo_t *next = nullptr;
	FString Name;
	FString Info;
	botskill_t skill = {};
	int inuse = 0;
	int lastteam = 0;
};

struct BotInfoData
{
	int MoveCombatDist = 0;
	int flags = 0;
	PClassActor *projectileType = nullptr;
};


enum
{
	BIF_BOT_REACTION_SKILL_THING = 1,
	BIF_BOT_EXPLOSIVE = 2,
	BIF_BOT_BFG = 4,
};


using BotInfoMap = TMap<FName, BotInfoData>;

extern BotInfoMap BotInfo;

inline BotInfoData GetBotInfo(AActor *weap)
{
	if (weap == nullptr) return BotInfoData();
	auto k = BotInfo.CheckKey(weap->GetClass()->TypeName);
	if (k) return *k;
	return BotInfoData();
}

//Used to keep all the globally needed variables in nice order.
class FCajunMaster
{
public:
	~FCajunMaster();

	void ClearPlayer (int playernum, bool keepTeam);

	//(b_game.cpp)
	void Main (FLevelLocals *Level);
	void Init ();
	void End();
	bool SpawnBot (const char *name, int color = NOCOLOR);
	void TryAddBot (FLevelLocals *Level, uint8_t **stream, int player);
	void RemoveAllBots (FLevelLocals *Level, bool fromlist);
	bool LoadBots ();
	void ForgetBots ();

	//(b_func.cpp)
	void StartTravel ();
	void FinishTravel ();
	bool IsLeader (player_t *player);
	void SetBodyAt (FLevelLocals *Level, const DVector3 &pos, int hostnum);
	double FakeFire (AActor *source, AActor *dest, ticcmd_t *cmd);
	bool SafeCheckPosition (AActor *actor, double x, double y, FCheckPosition &tm);
	void BotTick(AActor *mo);

	//(b_move.cpp)
	bool CleanAhead (AActor *thing, double x, double y, ticcmd_t *cmd);
	bool IsDangerous (sector_t *sec);

	TArray<FString> getspawned; //Array of bots (their names) which should be spawned when starting a game.
	
	//uint8_t freeze;			//Game in freeze mode.
	//uint8_t changefreeze;	//Game wants to change freeze mode.
	int botnum;
	botinfo_t *botinfo;
	int spawn_tries;
	int wanted_botnum;
	TObjPtr<AActor*> firstthing;
	TObjPtr<AActor*>	body1;
	TObjPtr<AActor*> body2;

	bool	 m_Thinking;

private:
	//(b_game.cpp)
	bool DoAddBot (FLevelLocals *Level, uint8_t *info, botskill_t skill);

protected:
	bool	 ctf;
	int		 t_join;
};

class DBot : public DThinker
{
	DECLARE_CLASS(DBot,DThinker)
	HAS_OBJECT_POINTERS
public:
	static const int DEFAULT_STAT = STAT_BOT;
	void Construct ();

	void Clear ();
	void Serialize(FSerializer &arc);
	void Tick ();

	//(b_think.cpp)
	void WhatToGet (AActor *item);

	//(b_func.cpp)
	bool Check_LOS (AActor *to, DAngle vangle);

	player_t	*player;
	DAngle		Angle;		// The wanted angle that the bot try to get every tic.
							//  (used to get a smooth view movement)
	TObjPtr<AActor*>		dest;		// Move Destination.
	TObjPtr<AActor*>		prev;		// Previous move destination.
	TObjPtr<AActor*>		enemy;		// The dead meat.
	TObjPtr<AActor*>		missile;	// A threatening missile that needs to be avoided.
	TObjPtr<AActor*>		mate;		// Friend (used for grouping in teamplay or coop).
	TObjPtr<AActor*>		last_mate;	// If bots mate disappeared (not if died) that mate is
							// pointed to by this. Allows bot to roam to it if
							// necessary.

	//Skills
	struct botskill_t	skill;

	//Tickers
	int			t_active;	// Open door, lower lift stuff, door must open and
							// lift must go down before bot does anything
							// radical like try a stuckmove
	int			t_respawn;
	int			t_strafe;
	int			t_react;
	int			t_fight;
	int			t_roam;
	int			t_rocket;

	//Misc booleans
	bool		first_shot;	// Used for reaction skill.
	bool		sleft;		// If false, strafe is right.
	bool		allround;
	bool		increase;

	DVector2	old;

private:
	//(b_think.cpp)
	void Think ();
	void ThinkForMove (ticcmd_t *cmd);
	void Set_enemy ();

	//(b_func.cpp)
	bool Reachable (AActor *target);
	void Dofire (ticcmd_t *cmd);
	AActor *Choose_Mate ();
	AActor *Find_enemy ();
	DAngle FireRox (AActor *enemy, ticcmd_t *cmd);

	//(b_move.cpp)
	void Roam (ticcmd_t *cmd);
	bool Move (ticcmd_t *cmd);
	bool TryWalk (ticcmd_t *cmd);
	void NewChaseDir (ticcmd_t *cmd);
	void TurnToAng ();
	void Pitch (AActor *target);
};


//Externs
extern cycle_t BotThinkCycles, BotSupportCycles;

EXTERN_CVAR (Float, bot_flag_return_time)
EXTERN_CVAR (Int, bot_next_color)

#endif	// __B_BOT_H__
