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

#include "version.h"
#include "minilzo.h"
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
#include "dstrings.h"
#include "r_data.h"
#include "r_sky.h"
#include "r_draw.h"
#include "g_game.h"
#include "g_level.h"
#include "b_bot.h"			//Added by MC:


#define SAVESTRINGSIZE	24

#define TURN180_TICKS	9				// [RH] # of ticks to complete a turn180

BOOL	G_CheckDemoStatus (void); 
void	G_ReadDemoTiccmd (ticcmd_t* cmd, int player); 
void	G_WriteDemoTiccmd (ticcmd_t* cmd, int player, int buf); 
void	G_PlayerReborn (int player); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
cvar_t gameskill ("skill", "2", CVAR_SERVERINFO | CVAR_LATCH);
CVAR (deathmatch, "0", CVAR_SERVERINFO | CVAR_LATCH);
CVAR (teamplay, "0", CVAR_SERVERINFO);
CVAR (chasedemo, "0", 0);
 
gameaction_t	gameaction; 
gamestate_t 	gamestate = GS_STARTUP; 
BOOL 			respawnmonsters;
 
BOOL 			paused; 
BOOL 			sendpause;				// send a pause event next tic 
BOOL			sendsave;				// send a save event next tic 
BOOL 			usergame;				// ok to save / end game
BOOL			sendcenterview;			// send a center view event next tic
 
BOOL			timingdemo; 			// if true, exit with report on completion 
BOOL 			nodrawers;				// for comparative timing purposes 
BOOL 			noblit; 				// for comparative timing purposes 
 
BOOL	 		viewactive; 
 
BOOL 			netgame;				// only true if packets are broadcast 
BOOL			multiplayer;
BOOL 			playeringame[MAXPLAYERS]; 
player_t		players[MAXPLAYERS]; 
 
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
fixed_t 		angleturn[3] = {640, 1280, 320};		// + slow turn
fixed_t			flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

CVAR (cl_run,		"0",	CVAR_ARCHIVE)		// Always run?
CVAR (invertmouse,	"0",	CVAR_ARCHIVE)		// Invert mouse look down/up?
CVAR (freelook,		"0",	CVAR_ARCHIVE)		// Always mlook?
CVAR (lookstrafe,	"0",	CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (m_pitch,		"1.0",	CVAR_ARCHIVE)		// Mouse speeds
CVAR (m_yaw,		"1.0",	CVAR_ARCHIVE)
CVAR (m_forward,	"1.0",	CVAR_ARCHIVE)
CVAR (m_side,		"2.0",	CVAR_ARCHIVE)
 
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
 
int 			savegameslot; 
char			savedescription[32]; 

// [RH] Name of screenshot file to generate (usually NULL)
char			*shotfile;

AActor* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 


// [RH] Allow turbo setting anytime during game
BEGIN_CUSTOM_CVAR (turbo, "100", 0)
{
	float scale = var.value * 0.01;

	if (scale < 0.10)
	{
		var.Set (10);
	}
	else if (scale > 2.56)
	{
		var.Set (256);
	}
	else
	{
		forwardmove[0] = (int)(normforwardmove[0]*scale);
		forwardmove[1] = (int)(normforwardmove[1]*scale);
		sidemove[0] = (int)(normsidemove[0]*scale);
		sidemove[1] = (int)(normsidemove[1]*scale);
	}
}
END_CUSTOM_CVAR (turbo)
 
 
/* [RH] Impulses: Temporary hack to get weapon changing
 * working with keybindings until I can get the
 * inventory system working.
 *
 * So this turned out to not be so temporary. It *will*
 * change, though.
 */
int Impulse;

BEGIN_COMMAND (impulse)
{
	if (argc > 1)
		Impulse = atoi (argv[1]);
}
END_COMMAND (impulse)

BEGIN_COMMAND (centerview)
{
	sendcenterview = true;
}
END_COMMAND (centerview)

BEGIN_COMMAND (pause)
{
	sendpause = true;
}
END_COMMAND (pause)

static int turntick;

BEGIN_COMMAND (turn180)
{
	turntick = TURN180_TICKS;
}
END_COMMAND (turn180)

//
// [RH] WeapNext and WeapPrev commands
//

static const char *weaponnames[] =
{
	"Fist",
	"Pistol",
	"Shotgun",
	"Chaingun",
	"Rocket Launcher",
	"Plasma Gun",
	"BFG9000",
	"Chainsaw",
	"Super Shotgun",
	"Chainsaw"
};

BEGIN_COMMAND (weapnext)
{
	player_t *plyr = m_Instigator->player;
	gitem_t *item = FindItem (weaponnames[plyr->readyweapon]);
	int selected_weapon;
	int i, index;

	if (!item)
		return;

	selected_weapon = ITEM_INDEX(item);

	for (i = 1; i <= num_items; i++)
	{
		index = (selected_weapon + i) % num_items;
		if (!(itemlist[index].flags & IT_WEAPON))
			continue;
		if (!plyr->weaponowned[itemlist[index].offset])
			continue;
		if (!plyr->ammo[weaponinfo[itemlist[index].offset].ammo])
			continue;
		Impulse = itemlist[index].offset + 50;
		return;
	}
}
END_COMMAND (weapnext)

BEGIN_COMMAND (weapprev)
{
	player_t *plyr = m_Instigator->player;
	gitem_t *item = FindItem (weaponnames[plyr->readyweapon]);
	int selected_weapon;
	int i, index;

	if (!item)
		return;

	selected_weapon = ITEM_INDEX(item);

	for (i = 1; i <= num_items; i++) {
		index = (selected_weapon + num_items - i) % num_items;
		if (!(itemlist[index].flags & IT_WEAPON))
			continue;
		if (!plyr->weaponowned[itemlist[index].offset])
			continue;
		if (!plyr->ammo[weaponinfo[itemlist[index].offset].ammo])
			continue;
		Impulse = itemlist[index].offset + 50;
		return;
	}
}
END_COMMAND (weapprev)

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
	int 		tspeed; 
	int 		forward;
	int 		side;
	int			look;
	int			fly;

	ticcmd_t	*base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	memcpy (cmd,base,sizeof(*cmd));

	cmd->consistancy = consistancy[consoleplayer][maketic%BACKUPTICS];

	strafe = Actions[ACTION_STRAFE];
	speed = Actions[ACTION_SPEED];
	if (cl_run.value)
		speed ^= 1;

	forward = side = look = fly = 0;

	// [RH] only use two stage accelerative turning on the keyboard
	//		and not the joystick, since we treat the joystick as
	//		the analog device it is.
	if ((Actions[ACTION_LEFT]) || (Actions[ACTION_RIGHT]))
		turnheld += ticdup;
	else
		turnheld = 0;

	if (turnheld < SLOWTURNTICS)
		tspeed = 2; 			// slow turn
	else
		tspeed = speed;
	
	// let movement keys cancel each other out
	if (strafe)
	{
		if (Actions[ACTION_RIGHT])
			side += sidemove[speed];
		if (Actions[ACTION_LEFT])
			side -= sidemove[speed];
	}
	else
	{
		if (Actions[ACTION_RIGHT])
			cmd->ucmd.yaw -= angleturn[tspeed];
		if (Actions[ACTION_LEFT])
			cmd->ucmd.yaw += angleturn[tspeed];
	}

	if (Actions[ACTION_LOOKUP])
		look += lookspeed[speed];
	if (Actions[ACTION_LOOKDOWN])
		look -= lookspeed[speed];

	if (Actions[ACTION_MOVEUP])
		fly += flyspeed[speed];
	if (Actions[ACTION_MOVEDOWN])
		fly -= flyspeed[speed];

	if (Actions[ACTION_KLOOK])
	{
		if (Actions[ACTION_FORWARD])
			look += lookspeed[speed];
		if (Actions[ACTION_BACK])
			look -= lookspeed[speed];
	}
	else
	{
		if (Actions[ACTION_FORWARD])
			forward += forwardmove[speed];
		if (Actions[ACTION_BACK])
			forward -= forwardmove[speed];
	}

	if (Actions[ACTION_MOVERIGHT])
		side += sidemove[speed];
	if (Actions[ACTION_MOVELEFT])
		side -= sidemove[speed];

	// buttons
	if (Actions[ACTION_ATTACK])
		cmd->ucmd.buttons |= BT_ATTACK;

	if (Actions[ACTION_USE])
		cmd->ucmd.buttons |= BT_USE;

	if (Actions[ACTION_JUMP])
		cmd->ucmd.buttons |= BT_JUMP;

	// [RH] Handle impulses. If they are between 1 and 7,
	//		they get sent as weapon change events.
	if (Impulse >= 1 && Impulse <= 7)
	{
		cmd->ucmd.buttons |= BT_CHANGE;
		cmd->ucmd.buttons |= (Impulse - 1) << BT_WEAPONSHIFT;
	}
	else
	{
		cmd->ucmd.impulse = Impulse;
	}
	Impulse = 0;

	// [RH] Scale joystick moves to full range of allowed speeds
	if (strafe || lookstrafe.value)
		side += (MAXPLMOVE * joyxmove) / 256;
	else 
		cmd->ucmd.yaw -= (angleturn[1] * joyxmove) / 256; 

	// [RH] Scale joystick moves over full range
	if (Actions[ACTION_MLOOK])
	{
		if (invertmouse.value)
			look -= (joyymove * 32767) / 256;
		else
			look += (joyymove * 32767) / 256;
	}
	else
	{
		forward += (MAXPLMOVE * joyymove) / 256;
	}

	if ((Actions[ACTION_MLOOK]) || freelook.value)
	{
		int val;

		val = (int)((float)(mousey * 16) * m_pitch.value);
		if (invertmouse.value)
			look -= val;
		else
			look += val;
	}
	else
	{
		forward += (int)((float)mousey * m_forward.value);
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

	if (strafe || lookstrafe.value)
		side += (int)((float)mousex * m_side.value);
	else 
		cmd->ucmd.yaw -= (int)((float)(mousex*0x8) * m_yaw.value); 

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
	if (sendpause) 
	{ 
		sendpause = false; 
		cmd->ucmd.buttons = BT_SPECIAL | BTS_PAUSE; 
	} 
 
	if (sendsave) 
	{ 
		sendsave = false; 
		cmd->ucmd.buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT); 
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;

	// [RH] 180-degree turn overrides all other yaws
	if (turntick) {
		turntick--;
		cmd->ucmd.yaw = (ANG180 / TURN180_TICKS) >> 16;
	}

	joyxmove = 0;
	joyymove = 0;
} 
 

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
static void ChangeSpy (void)
{
	players[consoleplayer].camera = players[displayplayer].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	ST_Start();		// killough 3/7/98: switch status bar views too
}

CVAR (bot_allowspy, "0", 0)

BEGIN_COMMAND (spynext)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (demoplayback || !netgame || !deathmatch.value))
	{
		if (deathmatch.value && bglobal.botnum && !bot_allowspy.value)
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
END_COMMAND (spynext)

BEGIN_COMMAND (spyprev)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (demoplayback || !netgame || !deathmatch.value))
	{
		if (deathmatch.value && bglobal.botnum && !bot_allowspy.value)
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
END_COMMAND (spyprev)


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

		if (ev->type == ev_keydown) 
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
				S_Sound (CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
				M_StartControlPanel (); 
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
		if (!viewactive)
			if (AM_Responder (ev))
				return true;	// automap ate it 
	}
	else if (gamestate == GS_FINALE) 
	{ 
		if (F_Responder (ev)) 
			return true;		// finale ate the event 
	} 
		 
	switch (ev->type) 
	{ 
	  case ev_keydown:
		if (C_DoKey (ev))
			return true;
		break;
 
	  case ev_keyup: 
		C_DoKey (ev);
		break;

	  // [RH] mouse buttons are now sent with key up/down events
	  case ev_mouse: 
		mousex = ev->data2 * mouse_sensitivity.value; 
		mousey = ev->data3 * mouse_sensitivity.value; 
		break;
 
	  case ev_joystick: 
		joyxmove = ev->data2; 
		joyymove = ev->data3; 
		break;
 
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive)
		return AM_Responder (ev);
 
	if (ev->type == ev_keydown ||
		ev->type == ev_mouse ||
		ev->type == ev_joystick)
		return true;
	else
		return false; 
} 
 
 
 
//
// G_Ticker
// Make ticcmd_ts for the players.
//
extern DCanvas *page;

void G_Ticker (void) 
{ 
	int 		i;
	int 		buf; 
	ticcmd_t*	cmd;
	gamestate_t	oldgamestate;

	// do player reborns if needed
	for (i=0 ; i<MAXPLAYERS ; i++) 
		if (playeringame[i] && players[i].playerstate == PST_REBORN) 
			G_DoReborn (i);

	// do things to change the game state
	oldgamestate = gamestate;
	while (gameaction != ga_nothing) 
	{ 
		switch (gameaction) 
		{ 
		case ga_loadlevel: 
			G_DoLoadLevel (-1); 
			break; 
		case ga_newgame: 
			G_DoNewGame (); 
			break; 
		case ga_loadgame: 
			G_DoLoadGame (); 
			break; 
		case ga_savegame: 
			G_DoSaveGame (); 
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
			if (shotfile) {
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

	// get commands, check consistancy,
	// and build new consistancy check
	buf = (gametic/ticdup)%BACKUPTICS;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			cmd = &players[i].cmd;

			memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t));

			RunNetSpecs (i, buf);

			if (demorecording)
				G_WriteDemoTiccmd (cmd, i, buf);
			if (demoplayback)
				G_ReadDemoTiccmd (cmd, i);

			// check for turbo cheats
			if (cmd->ucmd.forwardmove > TURBOTHRESHOLD
				&& !(gametic&31) && ((gametic>>5)&3) == i )
			{
				Printf (PRINT_HIGH, "%s is turbo!\n", players[i].userinfo.netname);
			}

			if (netgame && !players[i].isbot && !netdemo && !(gametic%ticdup))
			{
				if (gametic > BACKUPTICS
					&& consistancy[i][buf] != cmd->consistancy)
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

	// check for special buttons
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			if (players[i].cmd.ucmd.buttons & BT_SPECIAL)
			{
				switch (players[i].cmd.ucmd.buttons & BT_SPECIALMASK)
				{
				case BTS_PAUSE:
					paused ^= 1;
					if (paused) {
						S_PauseSound ();
						I_PauseMouse ();
					} else {
						S_ResumeSound ();
						I_ResumeMouse ();
					}
					break;

				case BTS_SAVEGAME:
					if (!savedescription[0])
						strcpy (savedescription, "NET GAME");
					savegameslot =
						(players[i].cmd.ucmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
					gameaction = ga_savegame;
					break;
				}
			}
		}
	}

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:
		P_Ticker ();
		ST_Ticker ();
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
	}
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Call when a player completes a level.
//
void G_PlayerFinishLevel (int player)
{
	player_t *p;

	p = &players[player];

	memset (p->powers, 0, sizeof (p->powers));
	memset (p->cards, 0, sizeof (p->cards));
	p->mo->flags &= ~MF_SHADOW; 		// cancel invisibility
	p->extralight = 0;					// cancel gun flashes
	p->fixedcolormap = 0;				// cancel ir goggles
	p->damagecount = 0; 				// no palette changes
	p->bonuscount = 0;
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

	p = &players[player];

	memcpy (frags,p->frags,sizeof(frags));
	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
    b_skill = players[player].skill;    //Added by MC:
	memcpy (&userinfo, &p->userinfo, sizeof(userinfo));

	memset (p, 0, sizeof(*p));

	memcpy (p->frags, frags, sizeof(p->frags));
	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	memcpy (&p->userinfo, &userinfo, sizeof(userinfo));

    p->skill = b_skill;	//Added by MC:

	p->usedown = p->attackdown = true;	// don't do anything immediately
	p->playerstate = PST_LIVE;
	p->health = deh.StartHealth;		// [RH] Used to be MAXHEALTH
	p->readyweapon = p->pendingweapon = wp_pistol;
	p->weaponowned[wp_fist] = true;
	p->weaponowned[wp_pistol] = true;
	p->ammo[am_clip] = deh.StartBullets; // [RH] Used to be 50

	for (i = 0; i < NUMAMMO; i++)
		p->maxammo[i] = maxammo[i];

    //Added by MC: Init bot structure.
    if (bglobal.botingame[player])
        bglobal.CleanBotstuff (players + player);
    else
		p->isbot = false;
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
	fixed_t 			x;
	fixed_t 			y;
	fixed_t				z, oldz;
	subsector_t*		ss;
	unsigned			an;
	AActor* 			mo;
	int 				i;

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = mthing->z << FRACBITS;

	ss = R_PointInSubsector (x,y);
	z += ss->sector->floorheight;

	if (!players[playernum].mo)
	{
		// first spawn of level, before corpses
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

	// flush an old corpse if needed
	if (bodyqueslot >= BODYQUESIZE)
		delete bodyque[bodyqueslot%BODYQUESIZE];
	bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo;
	bodyqueslot++;

	// spawn a teleport fog
	an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT;

	mo = new AActor (x+20*finecosine[an], y+20*finesine[an], z, MT_TFOG);

	if (level.time)
		S_Sound (mo, CHAN_VOICE, "misc/teleport", 1, ATTN_NORM);	// don't start sound on first frame 

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
	fixed_t closest = MAXINT;
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

	selections = deathmatch_p - deathmatchstarts;
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

	if (!spot) {
		// no good spot, so the player will probably get stuck
		spot = &playerstarts[playernum];
	} else {
		if (playernum < 4)
			spot->type = playernum+1;
		else
			spot->type = playernum+4001-4;	// [RH] > 4 players
	}

	P_SpawnPlayer (spot);
}

//
// G_DoReborn
//
void G_DoReborn (int playernum)
{
	int i;

	if (!multiplayer)
	{
		// reload the level from scratch
		gameaction = ga_loadlevel;
	}
	else
	{
		// respawn at the start

		// first disassociate the corpse
		if (players[playernum].mo)
		{
			players[playernum].mo->player = NULL;
			players[playernum].mo = NULL;
		}

		// spawn at random spot if in death match
		if (deathmatch.value)
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (G_CheckSpot (playernum, &playerstarts[playernum]) )
		{
			P_SpawnPlayer (&playerstarts[playernum]);
			return;
		}

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
extern BOOL setsizeneeded;
void R_ExecuteSetViewSize (void);

char savename[256];

void G_LoadGame (char* name)
{
	strcpy (savename, name);
	gameaction = ga_loadgame;
}


void G_DoLoadGame (void)
{
	int i;
	char text[16];

	gameaction = ga_nothing;

	FILE *stdfile = fopen (savename, "rb");
	if (stdfile == NULL)
	{
		Printf (PRINT_HIGH, "Could not read savegame '%s'\n", savename);
		return;
	}

	fseek (stdfile, SAVESTRINGSIZE, SEEK_SET);	// skip the description field
	fread (text, 16, 1, stdfile);
	if (strncmp (text, SAVESIG, 16))
	{
		Printf (PRINT_HIGH, "Savegame is from a different version\n");
		return;
	}
	fread (text, 8, 1, stdfile);
	text[8] = 0;

	FLZOFile savefile (stdfile, FFile::EReading);

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
	G_SerializeSnapshots (arc);
	P_SerializeRNGState (arc);
	P_SerializeACSDefereds (arc);

	// load a base level
	savegamerestore = true;		// Use the player actors in the savegame
	G_InitNew (text);
	savegamerestore = false;

	arc >> level.time;

	for (i = 0; i < NUM_WORLDVARS; i++)
		arc >> WorldVars[i];

	arc >> text[9];

	arc.Close ();

	if (text[9] != 0x1d)
		I_Error ("Bad savegame");
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (int slot, char *description)
{
	savegameslot = slot;
	strcpy (savedescription, description);
	sendsave = true;
}

void G_BuildSaveName (char *name, int slot)
{
	if (Args.CheckParm ("-cdrom"))
		sprintf(name, "c:\\zdoomdat\\%s%d.zds", SAVEGAMENAME, slot);
	else
		sprintf (name, "%s%d.zds", SAVEGAMENAME, slot);
}

void G_DoSaveGame (void)
{
	char name[100];
	char *description;
	int i;

	G_SnapshotLevel ();

	G_BuildSaveName (name, savegameslot);
	description = savedescription;

	FILE *stdfile = fopen (name, "wb");

	if (stdfile == NULL)
	{
		Printf (PRINT_HIGH, "Could not create savegame '%s'\n", name);
		return;
	}

	fwrite (description, SAVESTRINGSIZE, 1, stdfile);
	fwrite (SAVESIG, 16, 1, stdfile);
	fwrite (level.mapname, 8, 1, stdfile);

	FLZOFile savefile (stdfile, FFile::EWriting);
	FArchive arc (savefile);

	{
		byte vars[4096], *vars_p;
		vars_p = vars;
		C_WriteCVars (&vars_p, CVAR_SERVERINFO);
		arc.WriteCount (vars_p - vars);
		arc.Write (vars, vars_p - vars);
	}

	G_SerializeSnapshots (arc);
	P_SerializeRNGState (arc);
	P_SerializeACSDefereds (arc);

	arc << level.time;
	for (i = 0; i < NUM_WORLDVARS; i++)
		arc << WorldVars[i];

	arc << (BYTE)0x1d;			// consistancy marker

	gameaction = ga_nothing;
	savedescription[0] = 0;

	Printf (PRINT_HIGH, "%s\n", GGSAVED);
	arc.Close ();
}




//
// DEMO RECORDING
//
#define DEMOMARKER				0x80

static usercmd_t LastUserCmd;

static void MakeEmptyUserCmd (void)
{
	memset (&LastUserCmd, 0, sizeof(usercmd_t));
}

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	static int clonecount = 0;
	int i;
	int id = DEM_BAD;

	while (!clonecount && id != DEM_USERCMD && id != DEM_USERCMDCLONE)
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
			UnpackUserCmd (&cmd->ucmd, &demo_p);
			memcpy (&LastUserCmd, &cmd->ucmd, sizeof(usercmd_t));
			break;
		case DEM_USERCMDCLONE:
			clonecount = ReadByte (&demo_p) + 1;
			break;

		case DEM_DROPPLAYER:
			i = ReadByte (&demo_p);
			if (i < MAXPLAYERS)
				playeringame[i] = false;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
	if (clonecount)
	{
		clonecount--;
		memcpy (&cmd->ucmd, &LastUserCmd, sizeof(usercmd_t));
	}
} 

BOOL stoprecording;

BEGIN_COMMAND (stop)
{
	stoprecording = true;
}
END_COMMAND (stop)

extern byte *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	static int clonecount = 0;
	static byte *clonepos = NULL;
	byte *specdata;
	int speclen;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		gameaction = ga_fullconsole;
		return;
	}

	// [RH] Write any special "ticcmds" for this player to the demo
	if ((specdata = NetSpecs[player][buf].GetData (&speclen)))
	{
		memcpy (demo_p, specdata, speclen);
		demo_p += speclen;
		NetSpecs[player][buf].SetData (NULL, 0);
	}

	// [RH] Now write out a "normal" ticcmd. Use the clone command if this
	//		one is identical to the previous one.
	if (!memcmp (&LastUserCmd, &cmd->ucmd, sizeof(usercmd_t)))
	{
		if (!clonecount || clonecount == 256 || clonepos != demo_p)
		{
			WriteByte (DEM_USERCMDCLONE, &demo_p);
			WriteByte (0, &demo_p);
			clonecount = 1;
			clonepos = demo_p;
		}
		else
		{
			*(demo_p - 1) = clonecount++;
		}
	}
	else
	{
		WriteUserCmdMessage (&cmd->ucmd, &demo_p);
		clonecount = 0;
		memcpy (&LastUserCmd, &cmd->ucmd, sizeof(usercmd_t));
	}

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

//	if (!clonecount)
//		G_ReadDemoTiccmd (cmd); 	// make SURE it is exactly the same
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

	MakeEmptyUserCmd();

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

BEGIN_COMMAND (playdemo)
{
	if (argc > 1) {
		G_DeferedPlayDemo (argv[1]);
		singledemo = true;
	}
}
END_COMMAND (playdemo)

BEGIN_COMMAND (timedemo)
{
	if (argc > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}
END_COMMAND (timedemo)

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
		Printf (PRINT_HIGH, "Not a ZDoom demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit) {
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend) {
			Printf (PRINT_HIGH, "Demo is mangled!\n");
			return true;
		}

		switch (id) {
			case ZDHD_ID:
				headerHit = true;

				demover = ReadWord (&demo_p);	// ZDoom version demo was created with
				if (ReadWord (&demo_p) > GAMEVER) {		// Minimum ZDoom version
					Printf (PRINT_HIGH, "Demo requires a newer version of ZDoom!\n");
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
		Printf (PRINT_HIGH, "Demo has no players!\n");
		return true;
	}

	if (!bodyHit) {
		zdembodyend = NULL;
		Printf (PRINT_HIGH, "Demo has no BODY chunk!\n");
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
	if (demolump >= 0) {
		demobuffer = demo_p = (byte *)W_CacheLumpNum (demolump, PU_STATIC);
	} else {
		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".lmp");
		M_ReadFile (defdemoname, &demobuffer);
		demo_p = demobuffer;
	}

	Printf (PRINT_HIGH, "Playing demo %s\n", defdemoname);

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo
	MakeEmptyUserCmd ();

	if (ReadLong (&demo_p) != FORM_ID) {
		if (singledemo)
			I_Error ("Doom v1.9 demos are no longer supported.");
		else {
			Printf (PRINT_HIGH, "Doom v1.9 demos are no longer supported.\n");
			gameaction = ga_nothing;
		}
	} else if (G_ProcessIFFDemo (mapname)) {
		gameaction = ga_nothing;
		demoplayback = false;
	} else {
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

EXTERN_CVAR (name)
EXTERN_CVAR (autoaim)
EXTERN_CVAR (color)

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
		int endtime;

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

		if (singledemo || timingdemo) {
			if (timingdemo)
				// Trying to get back to a stable state after timing a demo
				// seems to cause problems. I don't feel like fixing that
				// right now.
				I_FatalError ("timed %i gametics in %i realtics (%.1f fps)", gametic,
							  endtime, (float)gametic/(float)endtime*(float)TICRATE);
			else
				Printf (PRINT_HIGH, "Demo ended.\n");
			gameaction = ga_fullconsole;
			timingdemo = false;
			return false;
		} else {
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
		Printf (PRINT_HIGH, "Demo %s recorded\n", demoname); 
	}

	return false; 
}
