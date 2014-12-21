// Cajun bot
//
// [RH] Moved console commands out of d_netcmd.c (in Cajun source), because
// they don't really belong there.

#include "c_cvars.h"
#include "c_dispatch.h"
#include "b_bot.h"
#include "m_argv.h"
#include "doomstat.h"
#include "p_local.h"
#include "cmdlib.h"
#include "teaminfo.h"
#include "d_net.h"
#include "farchive.h"

IMPLEMENT_POINTY_CLASS(DBot)
 DECLARE_POINTER(dest)
 DECLARE_POINTER(prev)
 DECLARE_POINTER(enemy)
 DECLARE_POINTER(missile)
 DECLARE_POINTER(mate)
 DECLARE_POINTER(last_mate)
END_POINTERS

DBot::DBot ()
: DThinker(STAT_BOT)
{
	Clear ();
}

void DBot::Clear ()
{
	player = NULL;
	angle = 0;
	dest = NULL;
	prev = NULL;
	enemy = NULL;
	missile = NULL;
	mate = NULL;
	last_mate = NULL;
	memset(&skill, 0, sizeof(skill));
	t_active = 0;
	t_respawn = 0;
	t_strafe = 0;
	t_react = 0;
	t_fight = 0;
	t_roam = 0;
	t_rocket = 0;
	first_shot = true;
	sleft = false;
	allround = false;
	increase = false;
	oldx = 0;
	oldy = 0;
}

void DBot::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	if (SaveVersion < 4515)
	{
		angle_t savedyaw;
		int savedpitch;
		arc << savedyaw
			<< savedpitch;
	}
	else
	{
		arc << player;
	}

	arc << angle
		<< dest
		<< prev
		<< enemy
		<< missile
		<< mate
		<< last_mate
		<< skill
		<< t_active
		<< t_respawn
		<< t_strafe
		<< t_react
		<< t_fight
		<< t_roam
		<< t_rocket
		<< first_shot
		<< sleft
		<< allround
		<< increase
		<< oldx
		<< oldy;
}

void DBot::Tick ()
{
	Super::Tick ();

	if (player->mo == NULL || bglobal.freeze)
	{
		return;
	}

	BotThinkCycles.Clock();
	bglobal.m_Thinking = true;
	Think ();
	bglobal.m_Thinking = false;
	BotThinkCycles.Unclock();
}

CVAR (Int, bot_next_color, 11, 0)
CVAR (Bool, bot_observer, false, 0)

CCMD (addbot)
{
	if (gamestate != GS_LEVEL && gamestate != GS_INTERMISSION)
	{
		Printf ("Bots cannot be added when not in a game!\n");
		return;
	}

	if (!players[consoleplayer].settings_controller)
	{
		Printf ("Only setting controllers can add bots\n");
		return;
	}

	if (argv.argc() > 2)
	{
		Printf ("addbot [botname] : add a bot to the game\n");
		return;
	}

	if (argv.argc() > 1)
		bglobal.SpawnBot (argv[1]);
	else
		bglobal.SpawnBot (NULL);
}

void FCajunMaster::ClearPlayer (int i, bool keepTeam)
{
	if (players[i].mo)
	{
		players[i].mo->Destroy ();
		players[i].mo = NULL;
	}
	botinfo_t *bot = botinfo;
	while (bot && stricmp (players[i].userinfo.GetName(), bot->name))
		bot = bot->next;
	if (bot)
	{
		bot->inuse = BOTINUSE_No;
		bot->lastteam = keepTeam ? players[i].userinfo.GetTeam() : TEAM_NONE;
	}
	if (players[i].Bot != NULL)
	{
		players[i].Bot->Destroy ();
		players[i].Bot = NULL;
	}
	players[i].~player_t();
	::new(&players[i]) player_t;
	players[i].userinfo.Reset();
	playeringame[i] = false;
}

CCMD (removebots)
{
	if (!players[consoleplayer].settings_controller)
	{
		Printf ("Only setting controllers can remove bots\n");
		return;
	}

	Net_WriteByte (DEM_KILLBOTS);
}

CCMD (freeze)
{
	if (CheckCheatmode ())
		return;

	if (netgame && !players[consoleplayer].settings_controller)
	{
		Printf ("Only setting controllers can use freeze mode\n");
		return;
	}

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_FREEZE);
}

CCMD (listbots)
{
	botinfo_t *thebot = bglobal.botinfo;
	int count = 0;

	while (thebot)
	{
		Printf ("%s%s\n", thebot->name, thebot->inuse == BOTINUSE_Yes ? " (active)" : "");
		thebot = thebot->next;
		count++;
	}
	Printf ("> %d bots\n", count);
}

FArchive &operator<< (FArchive &arc, botskill_t &skill)
{
	return arc << skill.aiming << skill.perfection << skill.reaction << skill.isp;
}

// set the bot specific weapon information
// This is intentionally not in the weapon definition anymore.
void InitBotStuff()
{
	static struct BotInit
	{
		const char *type;
		int movecombatdist;
		int weaponflags;
		const char *projectile;
	} botinits[] = {

		{ "Pistol", 25000000, 0, NULL },
		{ "Shotgun", 24000000, 0, NULL },
		{ "SuperShotgun", 15000000, 0, NULL },
		{ "Chaingun", 27000000, 0, NULL },
		{ "RocketLauncher", 18350080, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "Rocket" },
		{ "PlasmaRifle",  27000000, 0, "PlasmaBall" },
		{ "BFG9000", 10000000, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_BFG, "BFGBall" },
		{ "GoldWand", 25000000, 0, NULL },
		{ "GoldWandPowered", 25000000, 0, NULL },
		{ "Crossbow", 24000000, 0, "CrossbowFX1" },
		{ "CrossbowPowered", 24000000, 0, "CrossbowFX2" },
		{ "Blaster", 27000000, 0, NULL },
		{ "BlasterPowered", 27000000, 0, "BlasterFX1" },
		{ "SkullRod", 27000000, 0, "HornRodFX1" },
		{ "SkullRodPowered", 27000000, 0, "HornRodFX2" },
		{ "PhoenixRod", 18350080, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "PhoenixFX1" },
		{ "Mace", 27000000, WIF_BOT_REACTION_SKILL_THING, "MaceFX2" },
		{ "MacePowered", 27000000, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "MaceFX4" },
		{ "FWeapHammer", 22000000, 0, "HammerMissile" },
		{ "FWeapQuietus", 20000000, 0, "FSwordMissile" },
		{ "CWeapStaff", 25000000, 0, "CStaffMissile" },
		{ "CWeapFlane", 27000000, 0, "CFlameMissile" },
		{ "MWeapWand", 25000000, 0, "MageWandMissile" },
		{ "CWeapWraithverge", 22000000, 0, "HolyMissile" },
		{ "MWeapFrost", 19000000, 0, "FrostMissile" },
		{ "MWeapLightning", 23000000, 0, "LightningFloor" },
		{ "MWeapBloodscourge", 20000000, 0, "MageStaffFX2" },
		{ "StrifeCrossbow", 24000000, 0, "ElectricBolt" },
		{ "StrifeCrossbow2", 24000000, 0, "PoisonBolt" },
		{ "AssaultGun", 27000000, 0, NULL },
		{ "MiniMissileLauncher", 18350080, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "MiniMissile" },
		{ "FlameThrower", 24000000, 0, "FlameMissile" },
		{ "Mauler", 15000000, 0, NULL },
		{ "Mauler2", 10000000, 0, "MaulerTorpedo" },
		{ "StrifeGrenadeLauncher", 18350080, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "HEGrenade" },
		{ "StrifeGrenadeLauncher2", 18350080, WIF_BOT_REACTION_SKILL_THING|WIF_BOT_EXPLOSIVE, "PhosphorousGrenade" },
	};

	for(unsigned i=0;i<sizeof(botinits)/sizeof(botinits[0]);i++)
	{
		const PClass *cls = PClass::FindClass(botinits[i].type);
		if (cls != NULL && cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			AWeapon *w = (AWeapon*)GetDefaultByType(cls);
			if (w != NULL)
			{
				w->MoveCombatDist = botinits[i].movecombatdist;
				w->WeaponFlags |= botinits[i].weaponflags;
				w->ProjectileType = PClass::FindClass(botinits[i].projectile);
			}
		}
	}

	static const char *warnbotmissiles[] = { "PlasmaBall", "Ripper", "HornRodFX1" };
	for(unsigned i=0;i<countof(warnbotmissiles);i++)
	{
		AActor *a = GetDefaultByName (warnbotmissiles[i]);
		if (a != NULL)
		{
			a->flags3|=MF3_WARNBOT;
		}
	}
}
