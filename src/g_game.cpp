// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:  none
//
//-----------------------------------------------------------------------------



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <time.h>

#include "templates.h"
#include "version.h"
#include "m_alloc.h"
#include "doomdef.h" 
#include "doomstat.h"
#include "d_protocol.h"
#include "d_netinf.h"
#include "z_zone.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_effect.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "w_wad.h"
#include "p_local.h" 
#include "s_sound.h"
#include "gstrings.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_draw.h"
#include "g_game.h"
#include "g_level.h"
#include "b_bot.h"			//Added by MC:
#include "sbar.h"
#include "m_swap.h"

#include <zlib.h>

const int SAVEPICWIDTH = 216;
const int SAVEPICHEIGHT = 135;

BOOL	G_CheckDemoStatus (void);
void	G_ReadDemoTiccmd (ticcmd_t *cmd, int player);
void	G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf);
void	G_PlayerReborn (int player);

void	G_DoReborn (int playernum, bool freshbot);

void	G_DoNewGame (void);
void	G_DoLoadGame (void);
void	G_DoPlayDemo (void);
void	G_DoCompleted (void);
void	G_DoVictory (void);
void	G_DoWorldDone (void);
void	G_DoSaveGame (void);
void	G_DoAutoSave (void);

FIntCVar gameskill ("skill", 2, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Int, deathmatch, 0, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Bool, teamplay, false, CVAR_SERVERINFO);
CVAR (Bool, chasedemo, false, 0);
CVAR (Bool, storesavepic, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

gameaction_t	gameaction;
gamestate_t 	gamestate = GS_STARTUP;
BOOL 			respawnmonsters;

int 			paused;
bool 			sendpause;				// send a pause event next tic 
bool			sendsave;				// send a save event next tic 
bool			sendturn180;			// [RH] send a 180 degree turn next tic
bool 			usergame;				// ok to save / end game
bool			sendcenterview;			// send a center view event next tic

BOOL			timingdemo; 			// if true, exit with report on completion 
BOOL 			nodrawers;				// for comparative timing purposes 
BOOL 			noblit; 				// for comparative timing purposes 

bool	 		viewactive;

BOOL 			netgame;				// only true if packets are broadcast 
BOOL			multiplayer;
player_t		players[MAXPLAYERS];
bool			playeringame[MAXPLAYERS];

int 			consoleplayer;			// player taking events and displaying 
int 			displayplayer;			// view being displayed 
int 			gametic;

char			demoname[256];
BOOL 			demorecording;
BOOL 			demoplayback;
BOOL 			netdemo;
BOOL			demonew;				// [RH] Only used around G_InitNew for demos
int				demover;
byte*			demobuffer;
byte*			demo_p;
size_t			maxdemosize;
byte*			zdemformend;			// end of FORM ZDEM chunk
byte*			zdembodyend;			// end of ZDEM BODY chunk
BOOL 			singledemo; 			// quit after playing a demo from cmdline 
 
BOOL 			precache = true;		// if true, load all graphics at start 
 
wbstartstruct_t wminfo; 				// parms for world map / intermission 
 
short			consistancy[MAXPLAYERS][BACKUPTICS];
 
byte*			savebuffer;
 
 
#define MAXPLMOVE				(forwardmove[1]) 
 
#define TURBOTHRESHOLD	12800

float	 		normforwardmove[2] = {0x19, 0x32};		// [RH] For setting turbo from console
float	 		normsidemove[2] = {0x18, 0x28};			// [RH] Ditto

fixed_t			forwardmove[2], sidemove[2];
fixed_t 		angleturn[4] = {640, 1280, 320, 320};		// + slow turn
fixed_t			flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

CVAR (Bool,		cl_run,			false,	CVAR_ARCHIVE)		// Always run?
CVAR (Bool,		invertmouse,	false,	CVAR_ARCHIVE)		// Invert mouse look down/up?
CVAR (Bool,		freelook,		false,	CVAR_ARCHIVE)		// Always mlook?
CVAR (Bool,		lookstrafe,		false,	CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (Float,	m_pitch,		1.f,	CVAR_ARCHIVE)		// Mouse speeds
CVAR (Float,	m_yaw,			1.f,	CVAR_ARCHIVE)
CVAR (Float,	m_forward,		1.f,	CVAR_ARCHIVE)
CVAR (Float,	m_side,			2.f,	CVAR_ARCHIVE)
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
int 			mousex;
int 			mousey; 		

// joystick values are repeated
// [RH] now, if the joystick is enabled, it will generate an event every tick
//		so the values here are reset to zero after each tic build (in case
//		use_joystick gets set to 0 when the joystick is off center)
int 			joyxmove;
int 			joyymove;
 
char			*savegamefile;
char			savedescription[SAVESTRINGSIZE];

// [RH] Name of screenshot file to generate (usually NULL)
char			*shotfile;

AActor* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 

void R_ExecuteSetViewSize (void);

char savename[PATH_MAX];
char BackupSaveName[PATH_MAX];

bool SendLand;
BYTE SendWeaponSlot;
BYTE SendWeaponChoice;
int SendItemSelect;
artitype_t SendItemUse;
artitype_t LocalSelectedItem;

// [RH] Allow turbo setting anytime during game
CUSTOM_CVAR (Float, turbo, 100.f, 0)
{
	if (self < 10.f)
	{
		self = 10.f;
	}
	else if (self > 256.f)
	{
		self = 256.f;
	}
	else
	{
		float scale = self * 0.01f;

		forwardmove[0] = (int)(normforwardmove[0]*scale);
		forwardmove[1] = (int)(normforwardmove[1]*scale);
		sidemove[0] = (int)(normsidemove[0]*scale);
		sidemove[1] = (int)(normsidemove[1]*scale);
	}
}

CCMD (turnspeeds)
{
	if (argv.argc() == 1)
	{
		Printf ("Current turn speeds: %ld %ld %ld %ld\n", angleturn[0],
			angleturn[1], angleturn[2], angleturn[3]);
	}
	else
	{
		int i;

		for (i = 1; i <= 4 && i < argv.argc(); ++i)
		{
			angleturn[i-1] = atoi (argv[i]);
		}
		if (i <= 2)
		{
			angleturn[1] = angleturn[0] * 2;
		}
		if (i <= 3)
		{
			angleturn[2] = angleturn[0] / 2;
		}
		if (i <= 4)
		{
			angleturn[3] = angleturn[2];
		}
	}
}

CCMD (slot)
{
	if (argv.argc() > 1)
	{
		SendWeaponSlot = atoi (argv[1]);
	}
}

CCMD (weapon)
{
	if (argv.argc() > 1)
	{
		SendWeaponChoice = atoi (argv[1]);
	}
}

CCMD (centerview)
{
	sendcenterview = true;
}

CCMD (land)
{
	SendLand = true;
}

CCMD (pause)
{
	sendpause = true;
}

CCMD (turn180)
{
	sendturn180 = true;
}

CCMD (weapnext)
{
	Net_WriteByte (DEM_WEAPNEXT);
}

CCMD (weapprev)
{
	Net_WriteByte (DEM_WEAPPREV);
}

CCMD (invnext)
{
	LocalSelectedItem = P_NextInventory (m_Instigator->player, LocalSelectedItem);
	SendItemSelect = (argv.argc() == 1) ? 2 : 1;
}

CCMD (invprev)
{
	LocalSelectedItem = P_PrevInventory (m_Instigator->player, LocalSelectedItem);
	SendItemSelect = (argv.argc() == 1) ? 2 : 1;
}

CCMD (invuse)
{
	SendItemUse = LocalSelectedItem;
}

CCMD (invuseall)
{
	SendItemUse = (artitype_t)-1;
}

CCMD (use)
{
	if (argv.argc() > 1)
	{
		SendItemUse = P_FindNamedInventory (argv[1]);
	}
}

CCMD (select)
{
	if (argv.argc() > 1)
	{
		LocalSelectedItem = P_FindNamedInventory (argv[1]);
	}
	SendItemSelect = 1;
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
void G_BuildTiccmd (ticcmd_t *cmd)
{
	int 		strafe;
	int 		speed;
	int 		forward;
	int 		side;
	int			look;
	int			fly;

	ticcmd_t	*base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	*cmd = *base;

	cmd->consistancy = consistancy[consoleplayer][maketic%BACKUPTICS];

	strafe = Button_Strafe.bDown;
	speed = Button_Speed.bDown ^ (int)cl_run;

	forward = side = look = fly = 0;

	// [RH] only use two stage accelerative turning on the keyboard
	//		and not the joystick, since we treat the joystick as
	//		the analog device it is.
	if (Button_Left.bDown || Button_Right.bDown)
		turnheld += ticdup;
	else
		turnheld = 0;

	// let movement keys cancel each other out
	if (strafe)
	{
		if (Button_Right.bDown)
			side += sidemove[speed];
		if (Button_Left.bDown)
			side -= sidemove[speed];
	}
	else
	{
		int tspeed = speed;

		if (turnheld < SLOWTURNTICS)
			tspeed *= 2;		// slow turn
		
		if (Button_Right.bDown)
			cmd->ucmd.yaw -= angleturn[tspeed];
		if (Button_Left.bDown)
			cmd->ucmd.yaw += angleturn[tspeed];
	}

	if (Button_LookUp.bDown)
		look += lookspeed[speed];
	if (Button_LookDown.bDown)
		look -= lookspeed[speed];

	if (Button_MoveUp.bDown)
		fly += flyspeed[speed];
	if (Button_MoveDown.bDown)
		fly -= flyspeed[speed];

	if (Button_Klook.bDown)
	{
		if (Button_Forward.bDown)
			look += lookspeed[speed];
		if (Button_Back.bDown)
			look -= lookspeed[speed];
	}
	else
	{
		if (Button_Forward.bDown)
			forward += forwardmove[speed];
		if (Button_Back.bDown)
			forward -= forwardmove[speed];
	}

	if (Button_MoveRight.bDown)
		side += sidemove[speed];
	if (Button_MoveLeft.bDown)
		side -= sidemove[speed];

	// buttons
	if (Button_Attack.bDown)
		cmd->ucmd.buttons |= BT_ATTACK;

	if (Button_Use.bDown)
		cmd->ucmd.buttons |= BT_USE;

	if (Button_Jump.bDown)
		cmd->ucmd.buttons |= BT_JUMP;

	// [RH] Scale joystick moves to full range of allowed speeds
	if (strafe || lookstrafe)
		side += (MAXPLMOVE * joyxmove) / 256;
	else 
		cmd->ucmd.yaw -= (1280 * joyxmove) / 256; 

	// [RH] Scale joystick moves over full range
	if (Button_Mlook.bDown)
	{
		if (invertmouse)
			look -= (joyymove * 32767) / 256;
		else
			look += (joyymove * 32767) / 256;
	}
	else
	{
		forward += (MAXPLMOVE * joyymove) / 256;
	}

	if (Button_Mlook.bDown || freelook)
	{
		int val;

		val = (int)((float)(mousey * 16) * m_pitch) / ticdup;
		if (invertmouse)
			look -= val;
		else
			look += val;
	}
	else
	{
		forward += (int)((float)mousey * m_forward);
	}

	if (sendcenterview)
	{
		sendcenterview = false;
		look = -32768;
	}
	else
	{
		if (look > 32767)
			look = 32767;
		else if (look < -32767)
			look = -32767;
	}

	if (SendLand)
	{
		SendLand = false;
		fly = -32768;
	}

	if (strafe || lookstrafe)
		side += (int)((float)mousex * m_side);
	else
		cmd->ucmd.yaw -= (int)((float)(mousex*0x8) * m_yaw) / ticdup;

	mousex = mousey = 0;

	if (forward > MAXPLMOVE)
		forward = MAXPLMOVE;
	else if (forward < -MAXPLMOVE)
		forward = -MAXPLMOVE;
	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	cmd->ucmd.forwardmove += forward;
	cmd->ucmd.sidemove += side;
	cmd->ucmd.pitch = look;
	cmd->ucmd.upmove = fly;

	// special buttons
	if (sendturn180)
	{
		sendturn180 = false;
		cmd->ucmd.buttons |= BT_TURN180;
	}
	if (sendpause)
	{
		sendpause = false;
		Net_WriteByte (DEM_PAUSE);
	}
	if (sendsave)
	{
		sendsave = false;
		Net_WriteByte (DEM_SAVEGAME);
		Net_WriteString (savegamefile);
		Net_WriteString (savedescription);
		delete[] savegamefile;
		savegamefile = NULL;
	}
	if (SendWeaponSlot != 255)
	{
		Net_WriteByte (DEM_WEAPSLOT);
		Net_WriteByte (SendWeaponSlot);
		SendWeaponSlot = 255;
	}
	if (SendWeaponChoice != 255)
	{
		Net_WriteByte (DEM_WEAPSEL);
		Net_WriteByte (SendWeaponChoice);
		SendWeaponChoice = 255;
	}
	if (SendItemSelect)
	{
		Net_WriteByte (DEM_INVSEL);
		if (SendItemSelect == 2)
			Net_WriteByte (LocalSelectedItem | 0x80);
		else
			Net_WriteByte (LocalSelectedItem);
		SendItemSelect = 0;
	}
	if (SendItemUse != arti_none)
	{
		Net_WriteByte (DEM_INVUSE);
		Net_WriteByte (SendItemUse != -1 ? (byte)SendItemUse : 0);
		SendItemUse = arti_none;
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;

	joyxmove = 0;
	joyymove = 0;
}


// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
static void ChangeSpy (void)
{
	players[consoleplayer].camera = players[displayplayer].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	StatusBar->AttachToPlayer (&players[displayplayer]);
	if (demoplayback || multiplayer)
	{
		StatusBar->ShowPlayerName ();
	}
}

CVAR (Bool, bot_allowspy, false, 0)

CCMD (spynext)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (demoplayback || !netgame || !deathmatch))
	{
		if (deathmatch && bglobal.botnum && !bot_allowspy)
			return;
		do
		{
			displayplayer++;
			if (displayplayer == MAXPLAYERS)
				displayplayer = 0;
		} while (!playeringame[displayplayer] && displayplayer != consoleplayer);

		ChangeSpy ();
	}
}

CCMD (spyprev)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (demoplayback || !netgame || !deathmatch))
	{
		if (deathmatch && bglobal.botnum && !bot_allowspy)
			return;
		do 
		{ 
			displayplayer--; 
			if (displayplayer < 0) 
				displayplayer = MAXPLAYERS - 1; 
		} while (!playeringame[displayplayer] && displayplayer != consoleplayer);

		ChangeSpy ();
	}
}


//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
BOOL G_Responder (event_t *ev)
{
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	if (gameaction == ga_nothing && 
		(demoplayback || gamestate == GS_DEMOSCREEN))
	{
		char *cmd = C_GetBinding (ev->data1);

		if (ev->type == EV_KeyDown)
		{

			if (!cmd || (
				strnicmp (cmd, "menu_", 5) &&
				stricmp (cmd, "toggleconsole") &&
				stricmp (cmd, "sizeup") &&
				stricmp (cmd, "sizedown") &&
				stricmp (cmd, "togglemap") &&
				stricmp (cmd, "spynext") &&
				stricmp (cmd, "spyprev") &&
				stricmp (cmd, "chase") &&
				stricmp (cmd, "+showscores") &&
				stricmp (cmd, "bumpgamma") &&
				stricmp (cmd, "screenshot")))
			{
				M_StartControlPanel (true);
				return true;
			}
			else
			{
				return C_DoKey (ev);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev);

		return false;
	}

	if (gamestate == GS_LEVEL)
	{
		if (CT_Responder (ev))
			return true;		// chat ate the event
		if (ST_Responder (ev))
			return true;		// status window ate it
		if (!viewactive && AM_Responder (ev))
			return true;		// automap ate it
	}
	else if (gamestate == GS_FINALE)
	{
		if (F_Responder (ev))
			return true;		// finale ate the event
	}

	switch (ev->type)
	{
	case EV_KeyDown:
		if (C_DoKey (ev))
			return true;
		break;

	case EV_KeyUp:
		C_DoKey (ev);
		break;

	// [RH] mouse buttons are sent as key up/down events
	case EV_Mouse: 
		mousex = (int)(ev->x * mouse_sensitivity);
		mousey = (int)(ev->y * mouse_sensitivity);
		break;

	case EV_Joystick:
		joyxmove = ev->x;
		joyymove = ev->y;
		break;
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive)
		return AM_Responder (ev);

	return (ev->type == EV_KeyDown ||
			ev->type == EV_Mouse ||
			ev->type == EV_Joystick);
}



//
// G_Ticker
// Make ticcmd_ts for the players.
//
extern DCanvas *page;


void G_Ticker ()
{
	int i;
	gamestate_t	oldgamestate;

	// do player reborns if needed
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] &&
			(players[i].playerstate == PST_REBORN || players[i].playerstate == PST_ENTER))
		{
			G_DoReborn (i, false);
		}
	}

	// do things to change the game state
	oldgamestate = gamestate;
	while (gameaction != ga_nothing)
	{
		if (gameaction == ga_newgame2)
		{
			gameaction = ga_newgame;
			break;
		}
		switch (gameaction)
		{
		case ga_loadlevel:
			G_DoLoadLevel (-1);
			break;
		case ga_newgame2:	// Silence GCC (see above)
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame ();
			break;
		case ga_autosave:
			G_DoAutoSave ();
			break;
		case ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_victory:
//			F_StartFinale ();
			gameaction = ga_nothing;
			break;
		case ga_worlddone:
			G_DoWorldDone ();
			break;
		case ga_screenshot:
			M_ScreenShot (shotfile);
			if (shotfile)
			{
				free (shotfile);
				shotfile = NULL;
			}
			gameaction = ga_nothing;
			break;
		case ga_fullconsole:
			C_FullConsole ();
			gameaction = ga_nothing;
			break;
		case ga_nothing:
			break;
		}
		C_AdjustBottom ();
	}

	if (oldgamestate == GS_DEMOSCREEN && oldgamestate != gamestate && page)
	{
		delete page;
		page = NULL;
	}

	// get commands, check consistancy, and build new consistancy check
	int buf = (gametic/ticdup)%BACKUPTICS;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			ticcmd_t *cmd = &players[i].cmd;
			ticcmd_t *newcmd = &netcmds[i][buf];

			RunNetSpecs (i, buf);
			if (demorecording)
			{
				G_WriteDemoTiccmd (newcmd, i, buf);
			}
			if (demoplayback)
			{
				G_ReadDemoTiccmd (cmd, i);
			}
			else
			{
				memcpy (cmd, newcmd, sizeof(ticcmd_t));
			}

			// check for turbo cheats
			if (cmd->ucmd.forwardmove > TURBOTHRESHOLD
				&& !(gametic&31) && ((gametic>>5)&3) == i )
			{
				Printf ("%s is turbo!\n", players[i].userinfo.netname);
			}

			if (netgame && !players[i].isbot && !netdemo && !(gametic%ticdup))
			{
				if (gametic > BACKUPTICS && consistancy[i][buf] != cmd->consistancy)
				{
					players[i].inconsistant = 1;
				}
				if (players[i].mo)
					consistancy[i][buf] = players[i].mo->x;
				else
					consistancy[i][buf] = 0; // killough 2/14/98
			}
		}
	}

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:
		P_Ticker ();
		AM_Ticker ();
		break;

	case GS_INTERMISSION:
		WI_Ticker ();
		break;

	case GS_FINALE:
		F_Ticker ();
		break;

	case GS_DEMOSCREEN:
		D_PageTicker ();
		break;

	default:
		break;
	}
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
void G_PlayerFinishLevel (int player, EFinishLevelType mode)
{
	player_t *p;
	int i;
	int flightPower;

	p = &players[player];

	// Strip all current powers
	flightPower = p->powers[pw_flight];
	memset (p->powers, 0, sizeof (p->powers));
	if (!deathmatch && mode == FINISH_SameHub)
	{ // Keep flight if moving to another level in same hub
		p->powers[pw_flight] = flightPower;
	}
	p->mo->flags &= ~MF_SHADOW; 		// cancel invisibility
	p->mo->RenderStyle = STYLE_Normal;
	p->mo->alpha = FRACUNIT;
	p->extralight = 0;					// cancel gun flashes
	p->fixedcolormap = 0;				// cancel ir goggles
	p->damagecount = 0; 				// no palette changes
	p->bonuscount = 0;
	p->poisoncount = 0;
	p->rain1 = NULL;
	p->rain2 = NULL;
	p->inventorytics = 0;

	if (mode != FINISH_SameHub)
	{
		memset (p->keys, 0, sizeof (p->keys));	// Take away keys
		p->inventory[arti_fly] = 0;				// Take away flight
	}

	if (mode == FINISH_NoHub)
	{ // Reduce all owned inventory to 1 item each
		for (i = 0; i < NUMINVENTORYSLOTS; i++)
		{
			if (p->inventory[i])
				p->inventory[i] = 1;
		}
	}

	if (p->morphTics)
	{ // Undo morph
		P_UndoPlayerMorph (p, true);
	}
}


//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn (int player)
{
	player_t*	p;
	int 		i;
	int 		frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags
	int 		killcount;
	int 		itemcount;
	int 		secretcount;
	userinfo_t  userinfo;	// [RH] Save userinfo
	botskill_t  b_skill;//Added by MC:
	APlayerPawn *actor;
	const TypeInfo *cls;

	p = &players[player];

	memcpy (frags, p->frags, sizeof(frags));
	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
    b_skill = p->skill;    //Added by MC:
	memcpy (&userinfo, &p->userinfo, sizeof(userinfo));
	actor = p->mo;
	cls = p->cls;

	memset (p, 0, sizeof(*p));

	memcpy (p->frags, frags, sizeof(p->frags));
	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	memcpy (&p->userinfo, &userinfo, sizeof(userinfo));
	p->mo = actor;
	p->cls = cls;

    p->skill = b_skill;	//Added by MC:

	p->oldbuttons = 255;	// don't do anything immediately
	p->playerstate = PST_LIVE;

	for (i = 0; i < NUMAMMO; i++)
		p->maxammo[i] = maxammo[i];

	actor->GiveDefaultInventory ();

    //Added by MC: Init bot structure.
    if (bglobal.botingame[player])
        bglobal.CleanBotstuff (p);
    else
		p->isbot = false;

	// [BC] Handle temporary invulnerability when respawned
	if ((dmflags2 & DF2_YES_INVUL) &&
		(deathmatch || alwaysapplydmflags))
	{
		p->powers[pw_invulnerability] = 2*TICRATE;
		actor->effects |= FX_RESPAWNINVUL;	// [RH] special effect
	}
}

//
// G_CheckSpot	
// Returns false if the player cannot be respawned
// at the given mapthing2_t spot  
// because something is occupying it 
//
void P_SpawnPlayer (mapthing2_t* mthing);

BOOL G_CheckSpot (int playernum, mapthing2_t *mthing)
{
	fixed_t x;
	fixed_t y;
	fixed_t z, oldz;
	int i;

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = mthing->z << FRACBITS;

	z += R_PointInSubsector (x, y)->sector->floorplane.ZatPoint (x, y);

	if (!players[playernum].mo)
	{ // first spawn of level, before corpses
		for (i = 0; i < playernum; i++)
			if (players[i].mo && players[i].mo->x == x && players[i].mo->y == y)
				return false;
		return true;
	}

	oldz = players[playernum].mo->z;	// [RH] Need to save corpse's z-height
	players[playernum].mo->z = z;		// [RH] Checks are now full 3-D

	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	//
	// Old code:
	// if (!P_CheckPosition (players[playernum].mo, x, y))
	//    return false;

	players[playernum].mo->flags |=  MF_SOLID;
	i = P_CheckPosition(players[playernum].mo, x, y);
	players[playernum].mo->flags &= ~MF_SOLID;
	players[playernum].mo->z = oldz;	// [RH] Restore corpse's height
	if (!i)
		return false;

	return true;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//

// [RH] Returns the distance of the closest player to the given mapthing2_t.
static fixed_t PlayersRangeFromSpot (mapthing2_t *spot)
{
	fixed_t closest = INT_MAX;
	fixed_t distance;
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		distance = P_AproxDistance (players[i].mo->x - spot->x * FRACUNIT,
									players[i].mo->y - spot->y * FRACUNIT);

		if (distance < closest)
			closest = distance;
	}

	return closest;
}

// [RH] Select the deathmatch spawn spot farthest from everyone.
static mapthing2_t *SelectFarthestDeathmatchSpot (int selections)
{
	fixed_t bestdistance = 0;
	mapthing2_t *bestspot = NULL;
	int i;

	for (i = 0; i < selections; i++)
	{
		fixed_t distance = PlayersRangeFromSpot (&deathmatchstarts[i]);

		if (distance > bestdistance)
		{
			bestdistance = distance;
			bestspot = &deathmatchstarts[i];
		}
	}

	return bestspot;
}

// [RH] Select a deathmatch spawn spot at random (original mechanism)
static mapthing2_t *SelectRandomDeathmatchSpot (int playernum, int selections)
{
	int i, j;

	for (j=0; j < 20; j++)
	{
		i = P_Random (pr_dmspawn) % selections;
		if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
		{
			return &deathmatchstarts[i];
		}
	}

	// [RH] return a spot anyway, since we allow telefragging when a player spawns
	return &deathmatchstarts[i];
}

void G_DeathMatchSpawnPlayer (int playernum)
{
	int selections;
	mapthing2_t *spot;

	selections = deathmatchstarts.Size ();
	// [RH] We can get by with just 1 deathmatch start
	if (selections < 1)
		I_Error ("No deathmatch starts");

	// At level start, none of the players have mobjs attached to them,
	// so we always use the random deathmatch spawn. During the game,
	// though, we use whatever dmflags specifies.
	if (dmflags & DF_SPAWN_FARTHEST && players[playernum].mo)
		spot = SelectFarthestDeathmatchSpot (selections);
	else
		spot = SelectRandomDeathmatchSpot (playernum, selections);

	if (!spot)
	{ // no good spot, so the player will probably get stuck
		spot = &playerstarts[playernum];
	}
	else
	{
		if (playernum < 4)
			spot->type = playernum+1;
		else
			spot->type = playernum+4001-4;	// [RH] > 4 players
	}

	P_SpawnPlayer (spot);
}

//
// G_QueueBody
//
static void G_QueueBody (AActor *body)
{
	// flush an old corpse if needed
	if (bodyqueslot >= BODYQUESIZE)
		bodyque[bodyqueslot%BODYQUESIZE]->Destroy ();
	bodyque[bodyqueslot%BODYQUESIZE] = body;
	bodyqueslot++;
}

//
// G_DoReborn
//
void G_DoReborn (int playernum, bool freshbot)
{
	int i;

	if (!multiplayer)
	{
		if (BackupSaveName[0])
		{ // Load game from the last point it was saved
			strcpy (savename, BackupSaveName);
			gameaction = ga_loadgame;
		}
		else
		{ // Reload the level from scratch
			gameaction = ga_loadlevel;
		}
	}
	else
	{
		// respawn at the start

		// first disassociate the corpse
		if (players[playernum].mo)
		{
			G_QueueBody (players[playernum].mo);
			players[playernum].mo->player = NULL;
		}

		// spawn at random spot if in death match
		if (deathmatch)
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		// Cooperative net-play, retain keys and weapons
		bool oldweapons[NUMWEAPONS];
		bool oldkeys[NUMKEYS];
		int oldpieces = 0;

		if (!freshbot)
		{
			memcpy (oldweapons, players[playernum].weaponowned, sizeof(oldweapons));
			memcpy (oldkeys, players[playernum].keys, sizeof(oldkeys));
			oldpieces = players[playernum].pieces;
		}

		if (G_CheckSpot (playernum, &playerstarts[playernum]) )
		{
			P_SpawnPlayer (&playerstarts[playernum]);
		}
		else
		{
			// try to spawn at one of the other players' spots
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (G_CheckSpot (playernum, &playerstarts[i]) )
				{
					int oldtype = playerstarts[i].type;

					// fake as other player
					playerstarts[i].type = (playernum < 4) ? playernum + 1 : playernum + 4001 - 4;
					P_SpawnPlayer (&playerstarts[i]);
					playerstarts[i].type = oldtype; 			// restore 
					return;
				}
				// he's going to be inside something.  Too bad.
			}
			P_SpawnPlayer (&playerstarts[playernum]);
		}

		if (!freshbot)
		{ // Restore keys and weapons
			memcpy (players[playernum].weaponowned, oldweapons, sizeof(oldweapons));
			memcpy (players[playernum].keys, oldkeys, sizeof(oldkeys));
			players[playernum].pieces = oldpieces;

			// Give the player some ammo, based on the weapons owned
			for (i = 0; i < NUMWEAPONS; i++)
			{
				if (players[playernum].weaponowned[i])
				{
					int ammo = wpnlev1info[i]->ammo;
					players[playernum].ammo[ammo] =
						MAX (25, players[playernum].ammo[ammo]);
				}
			}
		}
	}
}

void G_ScreenShot (char *filename)
{
	shotfile = filename;
	gameaction = ga_screenshot;
}





//
// G_InitFromSavegame
// Can be called by the startup code or the menu task.
//
void G_LoadGame (char* name)
{
	strcpy (savename, name);
	gameaction = ga_loadgame;
}

// Return false if not all the needed wads have been loaded.
static bool G_CheckSaveGameWads (FILE *file)
{
	char wadbuff[PATH_MAX];
	int i, c;
	bool printRequires = false;

	for (;;)
	{
		i = 0;
		do
		{
			c = fgetc (file);
			wadbuff[i++] = c;
		} while (c != 0 && c != EOF);

		if (c == EOF)
		{
			Printf ("Savegame is too short\n");
			return false;
		}

		if (i == 1)
		{ // The end of the wad list is an empty string.
			if (printRequires)
			{
				Printf ("\n");
				return false;
			}
			return true;
		}

		if (!W_CheckIfWadLoaded (wadbuff))
		{
			if (!printRequires)
			{
				printRequires = true;
				Printf ("This savegame needs these wads:\n");
			}
			Printf ("%s, ", wadbuff);
		}
	}
}

void G_DoLoadGame (void)
{
	int i;
	char text[16];

	gameaction = ga_nothing;

	FILE *stdfile = fopen (savename, "rb");
	if (stdfile == NULL)
	{
		Printf ("Could not read savegame '%s'\n", savename);
		return;
	}

	fread (text, 16, 1, stdfile);
	if (strncmp (text, SAVESIG, 16))
	{
		Printf ("Savegame is from a different version\n");
		fclose (stdfile);
		return;
	}
	fseek (stdfile, SAVESTRINGSIZE, SEEK_CUR);	// skip the title field
	fread (text, 8, 1, stdfile);
	text[8] = 0;

	if (!G_CheckSaveGameWads (stdfile))
	{
		fclose (stdfile);
		return;
	}

	// Skip the comment and pic
	WORD t1, t2;

	fread (&t1, 2, 1, stdfile);
	fseek (stdfile, SHORT(t1), SEEK_CUR);
	fread (&t1, 2, 1, stdfile);
	fread (&t2, 2, 1, stdfile);
	if (t1 != 0 && t2 != 0)
	{
		WORD t3;
		fread (&t3, 4, 1, stdfile);
		t3 = LONG(t3);
		if (t3 == 0)
		{
			t3 = SHORT(t1) * SHORT(t2);
		}
		fseek (stdfile, t3, SEEK_CUR);		// skip (compressed) pic
	}

	bglobal.RemoveAllBots (true);

	FCompressedFile savefile (stdfile, FFile::EReading);

	if (!savefile.IsOpen ())
		I_Error ("Savegame '%s' is corrupt\n", savename);

	FArchive arc (savefile);

	{
		byte vars[4096], *vars_p;
		unsigned int len;
		vars_p = vars;
		len = arc.ReadCount ();
		arc.Read (vars, len);
		C_ReadCVars (&vars_p);
	}

	// dearchive all the modifications
	arc << level.time;

	G_SerializeSnapshots (arc);
	P_SerializeRNGState (arc);
	P_SerializeACSDefereds (arc);

	// load a base level
	savegamerestore = true;		// Use the player actors in the savegame
	G_InitNew (text);
	savegamerestore = false;

	for (i = 0; i < NUM_WORLDVARS; i++)
		arc << ACS_WorldVars[i];

	for (i = 0; i < NUM_GLOBALVARS; i++)
		arc << ACS_GlobalVars[i];

	arc << text[9];

	arc.Close ();

	if (text[9] != 0x1d)
		I_Error ("Bad savegame");

	LocalSelectedItem = players[consoleplayer].readyArtifact;

	delete level.info->snapshot;
	level.info->snapshot = NULL;

	strcpy (BackupSaveName, savename);
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (const char *filename, const char *description)
{
	savegamefile = copystring (filename);
	strcpy (savedescription, description);
	sendsave = true;
}

void G_BuildSaveName (char *name, int slot)
{
#ifndef unix
	if (Args.CheckParm ("-cdrom"))
		sprintf(name, "c:\\zdoomdat\\%s%d.zds", GStrings(SAVEGAMENAME), slot);
	else
		sprintf (name, "%s%d.zds", GStrings(SAVEGAMENAME), slot);
#else
	sprintf (name, "%s%d.zds", GStrings(SAVEGAMENAME), slot);
	char *path = GetUserFile (name);
	strcpy (name, path);
	delete[] path;
#endif
}

CVAR (Int, autosavenum, 0, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
extern void P_CalcHeight (player_t *);

void G_DoAutoSave ()
{
	// Do not autosave in multiplayer games or demos
	if (multiplayer || demoplayback)
	{
		gameaction = ga_nothing;
		return;
	}

	// Keep up to four autosaves at a time
	UCVarValue num;
	char name[PATH_MAX];
	time_t utcTime;
	struct tm *now;
	char *readableTime;
	
	num.Int = (autosavenum + 1) & 3;
	autosavenum.ForceSet (num, CVAR_Int);

	sprintf (name, "auto%d.zds", num.Int);
	savegamefile = copystring (name);

	time (&utcTime);
	now = localtime (&utcTime);
	readableTime = asctime (now);
	strcpy (savedescription, "Autosave ");
	strncpy (savedescription+9, readableTime+4, 12);
	savedescription[9+12] = 0;

	G_DoSaveGame ();
}


static void PutSaveWads (FILE *file)
{
	const char *name;

	// Name of IWAD
	name = W_GetWadName (1);
	fwrite (name, 1, strlen (name) + 1, file);

	// Name of wad the map resides in
	if (lumpinfo[level.lumpnum].wadnum != 1)
	{
		name = W_GetWadName (lumpinfo[level.lumpnum].wadnum);
		fwrite (name, 1, strlen (name) + 1, file);
	}

	fputc (0, file);
}

static void PutSaveComment (FILE *file)
{
	char comment[256];
	time_t utcTime;
	struct tm *now;
	char *readableTime;
	WORD len;
	int levelTime;

	// Get the current date and time
	time (&utcTime);
	now = localtime (&utcTime);
	readableTime = asctime (now);

	strncpy (comment, readableTime, 10);
	strncpy (comment+10, readableTime+19, 5);
	strncpy (comment+15, readableTime+10, 9);
	comment[24] = '\n';
	comment[25] = '\n';

	// Append level name
	strcpy (comment+26, level.level_name);
	len = strlen (comment);
	comment[len] = '\n';

	// Append elapsed time
	levelTime = level.time / TICRATE;
	sprintf (comment+len+1, "time: %02d:%02d:%02d",
		levelTime/3600, (levelTime%3600)/60, levelTime%60);
	len += 16;
	comment[len-1] = 0;

	// Write out the comment
	len = SHORT(len);
	fwrite (&len, 2, 1, file);
	len = SHORT(len);
	fwrite (comment, 1, len, file);
}

static void PutSavePic (FILE *file, int width, int height)
{
	const uLong picArea = width * height;

	if (picArea == 0 || !storesavepic)
	{
		DWORD zero = 0;
		fwrite (&zero, 4, 1, file);
	}
	else
	{
		DCanvas *pic = new DSimpleCanvas (width, height);

		// Take a snapshot of the player's view
		pic->Lock ();
		R_RenderViewToCanvas (&players[consoleplayer], pic, 0, 0, width, height);

		// Compress the pic and save it to disk
		uLong outlen;
		Byte *compressed;
		int r;

		outlen = picArea + picArea/1000 + 12;
		do
		{
			compressed = new Bytef[outlen];
			r = compress (compressed, &outlen, pic->GetBuffer(), picArea);
			if (r == Z_BUF_ERROR)
			{
				delete[] compressed;
				outlen += 1024;
			}
		} while (r == Z_BUF_ERROR);

		WORD temp;
		DWORD dtemp;

		temp = SHORT(width);
		fwrite (&temp, 2, 1, file);
		temp = SHORT(height);
		fwrite (&temp, 2, 1, file);

		if (r != Z_OK || outlen >= picArea)
		{ // Save uncompressable data as-is
			dtemp = 0;
			fwrite (&dtemp, 4, 1, file);
			fwrite (pic->GetBuffer(), 1, picArea, file);
		}
		else
		{ // Save compressable data in compressed form
			dtemp = LONG(outlen);
			fwrite (&dtemp, 4, 1, file);
			fwrite (compressed, 1, outlen, file);
		}

		pic->Unlock ();
		delete[] compressed;
		delete pic;
	}
}

void G_DoSaveGame ()
{
	int i;

	if (demoplayback)
		return;

	G_SnapshotLevel ();

	FILE *stdfile = fopen (savegamefile, "wb");

	if (stdfile == NULL)
	{
		Printf ("Could not create savegame '%s'\n", savegamefile);
		delete[] savegamefile;
		savegamefile = NULL;
		return;
	}

	fwrite (SAVESIG, 16, 1, stdfile);
	fwrite (savedescription, SAVESTRINGSIZE, 1, stdfile);
	fwrite (level.mapname, 8, 1, stdfile);
	PutSaveWads (stdfile);
	PutSaveComment (stdfile);
	PutSavePic (stdfile, SAVEPICWIDTH, SAVEPICHEIGHT);

	FCompressedFile savefile (stdfile, FFile::EWriting, true);
	FArchive arc (savefile);

	{
		byte vars[4096], *vars_p;
		vars_p = vars;
		C_WriteCVars (&vars_p, CVAR_SERVERINFO);
		arc.WriteCount (vars_p - vars);
		arc.Write (vars, vars_p - vars);
	}

	arc << level.time;

	G_SerializeSnapshots (arc);
	P_SerializeRNGState (arc);
	P_SerializeACSDefereds (arc);

	for (i = 0; i < NUM_WORLDVARS; i++)
		arc << ACS_WorldVars[i];

	for (i = 0; i < NUM_GLOBALVARS; i++)
		arc << ACS_GlobalVars[i];

	BYTE consist = 0x1d;			// consistancy marker
	arc << consist;

	M_NotifyNewSave (savegamefile, savedescription);
	gameaction = ga_nothing;
	savedescription[0] = 0;

	Printf ("%s\n", GStrings(GGSAVED));
	arc.Close ();

	strcpy (BackupSaveName, savegamefile);

	delete[] savegamefile;
	savegamefile = NULL;
}




//
// DEMO RECORDING
//

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	int id = DEM_BAD;

	while (id != DEM_USERCMD)
	{
		if (!demorecording && demo_p >= zdembodyend)
		{
			// nothing left in the BODY chunk, so end playback.
			G_CheckDemoStatus ();
			break;
		}

		id = ReadByte (&demo_p);

		switch (id)
		{
		case DEM_STOP:
			// end of demo stream
			G_CheckDemoStatus ();
			break;

		case DEM_USERCMD:
			UnpackUserCmd (&cmd->ucmd, &cmd->ucmd, &demo_p);
			break;

		case DEM_DROPPLAYER:
			{
				byte i = ReadByte (&demo_p);
				if (i < MAXPLAYERS)
					playeringame[i] = false;
			}
			break;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
} 

BOOL stoprecording;

CCMD (stop)
{
	stoprecording = true;
}

extern byte *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	byte *specdata;
	int speclen;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		gameaction = ga_fullconsole;
		return;
	}

	// [RH] Write any special "ticcmds" for this player to the demo
	if ((specdata = NetSpecs[player][buf].GetData (&speclen)) && gametic % ticdup == 0)
	{
		memcpy (demo_p, specdata, speclen);
		demo_p += speclen;
		NetSpecs[player][buf].SetData (NULL, 0);
	}

	// [RH] Now write out a "normal" ticcmd.
	WriteUserCmdMessage (&cmd->ucmd, &players[player].cmd.ucmd, &demo_p);

	// [RH] Bigger safety margin
	if (demo_p > demobuffer + maxdemosize - 64)
	{
		ptrdiff_t pos = demo_p - demobuffer;
		ptrdiff_t spot = lenspot - demobuffer;
		// [RH] Allocate more space for the demo
		maxdemosize += 0x20000;
		demobuffer = (byte *)Realloc (demobuffer, maxdemosize);
		demo_p = demobuffer + pos;
		lenspot = demobuffer + spot;
	}
}



//
// G_RecordDemo
//
void G_RecordDemo (char* name)
{
	char *v;

	usergame = false;
	strcpy (demoname, name);
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	v = Args.CheckValue ("-maxdemo");
	if (v)
		maxdemosize = atoi(v)*1024;
	if (maxdemosize < 0x20000)
		maxdemosize = 0x20000;
	demobuffer = (byte *)Malloc (maxdemosize);

	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (void)
{
	int i;

	demo_p = demobuffer;

	WriteLong (FORM_ID, &demo_p);			// Write FORM ID
	demo_p += 4;							// Leave space for len
	WriteLong (ZDEM_ID, &demo_p);			// Write ZDEM ID

	// Write header chunk
	StartChunk (ZDHD_ID, &demo_p);
	WriteWord (GAMEVER, &demo_p);			// Write ZDoom version
	*demo_p++ = 1;							// Write minimum version needed to use this demo.
	*demo_p++ = 13;							// (Useful?)
	for (i = 0; i < 8; i++) {				// Write name of map demo was recorded on.
		*demo_p++ = level.mapname[i];
	}
	WriteLong (rngseed, &demo_p);			// Write RNG seed
	*demo_p++ = consoleplayer;
	FinishChunk (&demo_p);

	// Write player info chunks
	for (i = 0; i < MAXPLAYERS; i++) {
		if (playeringame[i]) {
			StartChunk (UINF_ID, &demo_p);
			WriteByte ((byte)i, &demo_p);
			D_WriteUserInfoStrings (i, &demo_p);
			FinishChunk (&demo_p);
		}
	}

	// Write cvars chunk
	StartChunk (VARS_ID, &demo_p);
	C_WriteCVars (&demo_p, CVAR_SERVERINFO|CVAR_DEMOSAVE);
	FinishChunk (&demo_p);

	// Begin BODY chunk
	StartChunk (BODY_ID, &demo_p);
}


//
// G_PlayDemo
//

char defdemoname[128];

void G_DeferedPlayDemo (char *name)
{
	strncpy (defdemoname, name, 127);
	gameaction = ga_playdemo;
}

CCMD (playdemo)
{
	if (argv.argc() > 1)
	{
		G_DeferedPlayDemo (argv[1]);
		singledemo = true;
	}
}

CCMD (timedemo)
{
	if (argv.argc() > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}

// [RH] Process all the information in a FORM ZDEM
//		until a BODY chunk is entered.
BOOL G_ProcessIFFDemo (char *mapname)
{
	BOOL headerHit = false;
	BOOL bodyHit = false;
	int numPlayers = 0;
	int id, len, i;
	byte *nextchunk;

	demoplayback = true;

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT. Might be useful.

	id = ReadLong (&demo_p);
	if (id != ZDEM_ID) {
		Printf ("Not a ZDoom demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit) {
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend) {
			Printf ("Demo is mangled!\n");
			return true;
		}

		switch (id) {
			case ZDHD_ID:
				headerHit = true;

				demover = ReadWord (&demo_p);	// ZDoom version demo was created with
				if (ReadWord (&demo_p) > GAMEVER) {		// Minimum ZDoom version
					Printf ("Demo requires a newer version of ZDoom!\n");
					return true;
				}
				memcpy (mapname, demo_p, 8);	// Read map name
				mapname[8] = 0;
				demo_p += 8;
				rngseed = ReadLong (&demo_p);
				consoleplayer = displayplayer = *demo_p++;
				break;

			case VARS_ID:
				C_ReadCVars (&demo_p);
				break;

			case UINF_ID:
				i = ReadByte (&demo_p);
				if (!playeringame[i]) {
					playeringame[i] = 1;
					numPlayers++;
				}
				D_ReadUserInfoStrings (i, &demo_p, false);
				break;

			case BODY_ID:
				bodyHit = true;
				zdembodyend = demo_p + len;
				break;
		}

		if (!bodyHit)
			demo_p = nextchunk;
	}

	if (!numPlayers) {
		Printf ("Demo has no players!\n");
		return true;
	}

	if (!bodyHit) {
		zdembodyend = NULL;
		Printf ("Demo has no BODY chunk!\n");
		return true;
	}

	if (numPlayers > 1)
		multiplayer = netgame = netdemo = true;

	return false;
}

void G_DoPlayDemo (void)
{
	char mapname[9];
	int demolump;

	gameaction = ga_nothing;

	// [RH] Allow for demos not loaded as lumps
	demolump = W_CheckNumForName (defdemoname);
	if (demolump >= 0)
	{
		demobuffer = demo_p = (byte *)W_CacheLumpNum (demolump, PU_STATIC);
	}
	else
	{
		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".lmp");
		M_ReadFile (defdemoname, &demobuffer);
		demo_p = demobuffer;
	}

	Printf ("Playing demo %s\n", defdemoname);

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo

	if (ReadLong (&demo_p) != FORM_ID)
	{
		const char *eek = "Cannot play non-ZDoom demos.\n(They would go out of sync badly.)\n";

		if (singledemo)
		{
			I_Error (eek);
		}
		else
		{
			Printf_Bold (eek);
			gameaction = ga_nothing;
		}
	}
	else if (G_ProcessIFFDemo (mapname))
	{
		gameaction = ga_nothing;
		demoplayback = false;
	}
	else
	{
		// don't spend a lot of time in loadlevel 
		precache = false;
		demonew = true;
		G_InitNew (mapname);
		C_HideConsole ();
		demonew = false;
		precache = true;

		usergame = false;
		demoplayback = true;
	}
}

//
// G_TimeDemo
//
void G_TimeDemo (char* name)
{
	nodrawers = Args.CheckParm ("-nodraw");
	noblit = Args.CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	strncpy (defdemoname, name, 128);
	gameaction = ga_playdemo;
}


/*
===================
=
= G_CheckDemoStatus
=
= Called after a death or level completion to allow demos to be cleaned up
= Returns true if a new demo loop action will take place
===================
*/

EXTERN_CVAR (String, name)
EXTERN_CVAR (Float, autoaim)
EXTERN_CVAR (Color, color)

BOOL G_CheckDemoStatus (void)
{
	if (!demorecording)
	{ // [RH] Restore the player's userinfo settings.
		D_UserInfoChanged (&name);
		D_UserInfoChanged (&autoaim);
		D_UserInfoChanged (&color);
	}

	if (demoplayback)
	{
		extern int starttime;
		int endtime = 0;

		if (timingdemo)
			endtime = I_GetTimePolled () - starttime;
			
		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

		Z_Free (demobuffer);
		demoplayback = false;
		netdemo = false;
		netgame = false;
		multiplayer = false;
		singletics = false;
		{
			int i;

			for (i = 1; i < MAXPLAYERS; i++)
				playeringame[i] = 0;
		}
		consoleplayer = 0;

		if (singledemo || timingdemo)
		{
			if (timingdemo)
			{
				// Trying to get back to a stable state after timing a demo
				// seems to cause problems. I don't feel like fixing that
				// right now.
				I_FatalError ("timed %i gametics in %i realtics (%.1f fps)\n"
							  "(This is not really an error.)", gametic,
							  endtime, (float)gametic/(float)endtime*(float)TICRATE);
			}
			else
			{
				Printf ("Demo ended.\n");
			}
			gameaction = ga_fullconsole;
			timingdemo = false;
			return false;
		}
		else
		{
			D_AdvanceDemo (); 
		}

		return true; 
	}

	if (demorecording)
	{
		byte *formlen;

		WriteByte (DEM_STOP, &demo_p);
		FinishChunk (&demo_p);
		formlen = demobuffer + 4;
		WriteLong (demo_p - demobuffer - 8, &formlen);

		M_WriteFile (demoname, demobuffer, demo_p - demobuffer); 
		free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		Printf ("Demo %s recorded\n", demoname); 
	}

	return false; 
}
