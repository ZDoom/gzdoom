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

#include "doomdef.h" 
#include "doomstat.h"

#include "d_proto.h"
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

#include "c_consol.h"
#include "c_cvars.h"
#include "c_bind.h"
#include "c_dispch.h"

// Needs access to LFB.
#include "v_video.h"

#include "w_wad.h"

#include "p_local.h" 

#include "s_sound.h"

// Data.
#include "dstrings.h"
#include "sounds.h"

// SKY handling - still the wrong place.
#include "r_data.h"
#include "r_sky.h"

#include "r_draw.h"

#include "g_game.h"
#include "g_level.h"


#define SAVEGAMESIZE	0x2c000
#define SAVESTRINGSIZE	24


BOOL	G_CheckDemoStatus (void); 
void	G_ReadDemoTiccmd (ticcmd_t* cmd, int player); 
void	G_WriteDemoTiccmd (ticcmd_t* cmd, int player); 
void	G_PlayerReborn (int player); 
 
void	G_DoReborn (int playernum); 
 
void	G_DoLoadLevel (void); 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
 
 
gameaction_t	gameaction; 
gamestate_t 	gamestate = -1; 
cvar_t		   *gameskill; 
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
 
cvar_t			*deathmatch; 			// only if started as net death 
BOOL 			netgame;				// only true if packets are broadcast 
BOOL 			playeringame[MAXPLAYERS]; 
player_t		players[MAXPLAYERS]; 
 
int 			consoleplayer;			// player taking events and displaying 
int 			displayplayer;			// view being displayed 
int 			gametic; 
 
char			demoname[256]; 
BOOL 			demorecording; 
BOOL 			demoplayback; 
BOOL 			netdemo; 
byte*			demobuffer;
byte*			demo_p;
byte*			demoend;
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
byte			flyspeed[2] = {1, 3};
int				lookspeed[2] = {3072, 5120};

#define SLOWTURNTICS	6 

cvar_t			*cl_run;								// Always run?
cvar_t			*invertmouse;							// Invert mouse look down/up?
cvar_t			*freelook;								// Always mlook?
cvar_t			*lookstrafe;							// Always strafe with mouse?
cvar_t			*m_pitch, *m_yaw, *m_forward, *m_side;	// Mouse speeds
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
int 			mousex;
int 			mousey; 		

// joystick values are repeated 
int 			joyxmove;
int 			joyymove;
 
int 			savegameslot; 
char			savedescription[32]; 

// [RH] Name of screenshot file to generate (usually NULL)
char			*shotfile;

#define BODYQUESIZE 	32

mobj_t* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 


// [RH] Allow turbo setting anytime during game
void TurboCallback (cvar_t *var)
{
	float scale = var->value * 0.01;

	if (scale < 0.10) {
		SetCVarFloat (var, 10);
	} else if (scale > 2.56) {
		SetCVarFloat (var, 256);
	} else {
		forwardmove[0] = (int)(normforwardmove[0]*scale);
		forwardmove[1] = (int)(normforwardmove[1]*scale);
		sidemove[0] = (int)(normsidemove[0]*scale);
		sidemove[1] = (int)(normsidemove[1]*scale);
	}
}
 
 
/* [RH] Impulses: Temporary hack to get weapon changing
 * working with keybindings until I can get the
 * inventory system working.
 */
int Impulse;

void Cmd_Impulse (player_t *plyr, int argc, char **argv)
{
	if (argc > 1)
		Impulse = atoi (argv[1]);
}

void Cmd_CenterView (player_t *plyr, int argc, char **argv)
{
	sendcenterview = true;
}

void Cmd_Pause (player_t *plyr, int argc, char **argv)
{
	sendpause = true;
}

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer. 
// If recording a demo, write it out 
// 
void G_BuildTiccmd (ticcmd_t* cmd) 
{ 
	BOOL 		strafe;
	int 		speed;
	int 		tspeed; 
	int 		forward;
	int 		side;
	int			look;
	int			fly;
	
	ticcmd_t*	base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	memcpy (cmd,base,sizeof(*cmd)); 
		
	cmd->consistancy = 
		consistancy[consoleplayer][maketic%BACKUPTICS]; 

 
	strafe = Actions & ACTION_STRAFE;
	speed = ((Actions & ACTION_SPEED) || (cl_run->value)) ? 1 : 0;
 
	forward = side = look = fly = 0;
	
	// use two stage accelerative turning
	// on the keyboard and joystick
	if (joyxmove < 0
		|| joyxmove > 0  
		|| (Actions & ACTION_LEFT)
		|| (Actions & ACTION_RIGHT))
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
		if (Actions & ACTION_RIGHT) 
		{
			// fprintf(stderr, "strafe right\n");
			side += sidemove[speed]; 
		}
		if (Actions & ACTION_LEFT) 
		{
			//	fprintf(stderr, "strafe left\n");
			side -= sidemove[speed]; 
		}
		if (joyxmove > 0) 
			side += sidemove[speed]; 
		if (joyxmove < 0) 
			side -= sidemove[speed]; 
 
	} 
	else 
	{ 
		if (Actions & ACTION_RIGHT) 
			cmd->ucmd.yaw -= angleturn[tspeed]; 
		if (Actions & ACTION_LEFT) 
			cmd->ucmd.yaw += angleturn[tspeed]; 
		if (joyxmove > 0) 
			cmd->ucmd.yaw -= angleturn[tspeed]; 
		if (joyxmove < 0) 
			cmd->ucmd.yaw += angleturn[tspeed]; 
	} 

	if (Actions & ACTION_LOOKUP)
		look += lookspeed[speed];
	if (Actions & ACTION_LOOKDOWN)
		look -= lookspeed[speed];
	if (Actions & ACTION_MOVEUP)
		fly += flyspeed[speed];
	if (Actions & ACTION_MOVEDOWN)
		fly -= flyspeed[speed];


	if (Actions & ACTION_KLOOK) {
		if (Actions & ACTION_FORWARD)
			look += lookspeed[speed];
		if (Actions & ACTION_BACK)
			look -= lookspeed[speed];
	} else {
		if (Actions & ACTION_FORWARD)
			forward += forwardmove[speed];
		if (Actions & ACTION_BACK)
			forward -= forwardmove[speed];
	}

	if (joyymove < 0) 
		forward += forwardmove[speed]; 
	if (joyymove > 0) 
		forward -= forwardmove[speed]; 
	if (Actions & ACTION_MOVERIGHT) 
		side += sidemove[speed]; 
	if (Actions & ACTION_MOVELEFT) 
		side -= sidemove[speed];

	// buttons
	if (Actions & ACTION_ATTACK) 
		cmd->ucmd.buttons |= BT_ATTACK; 
 
	if (Actions & ACTION_USE) 
		cmd->ucmd.buttons |= BT_USE;

	if (Actions & ACTION_JUMP)
		cmd->ucmd.buttons |= BT_JUMP;

	// chainsaw overrides 
	if (Impulse >= 1 && Impulse <= NUMWEAPONS-1) {
		cmd->ucmd.buttons |= BT_CHANGE;
		cmd->ucmd.buttons |= (Impulse - 1) << BT_WEAPONSHIFT;
		Impulse = 0;
	}

	if ((Actions & ACTION_MLOOK) || freelook->value) {
		int val;

		val = (int)((float)(mousey * 256) * m_pitch->value);
		if (invertmouse->value)
			look -= val;
		else
			look += val;
		if (look > 32767)
			look = 32767;
		else if (look < -32767)
			look = -32767;
	} else {
		forward += (int)((float)mousey * m_forward->value);
	}

	if (sendcenterview) {
		sendcenterview = false;
		look = -32768;
	}

	if (strafe || lookstrafe->value)
		side += (int)((float)mousex * m_side->value);
	else 
		cmd->ucmd.yaw -= (int)((float)(mousex*0x8) * m_yaw->value); 

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
} 
 

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
static void ChangeSpy (void)
{
	// [RH] Use some BOOM code here:
	ST_Start();		// killough 3/7/98: switch status bar views too
	HU_Start();
	S_UpdateSounds(players[displayplayer].mo);
}

void Cmd_SpyNext (player_t *plyr, int argc, char **argv)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (singledemo || !deathmatch->value)) {
		do 
		{ 
			displayplayer++; 
			if (displayplayer == MAXPLAYERS) 
				displayplayer = 0; 
		} while (!playeringame[displayplayer] && displayplayer != consoleplayer);

		ChangeSpy ();
	}
}

void Cmd_SpyPrev (player_t *plyr, int argc, char **argv)
{
	// allow spy mode changes even during the demo
	if (gamestate == GS_LEVEL && (singledemo || !deathmatch->value)) {
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
BOOL G_Responder (event_t* ev) 
{ 
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	if (gameaction == ga_nothing && !singledemo && 
		(demoplayback || gamestate == GS_DEMOSCREEN) 
		) 
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
				stricmp (cmd, "+showscores") &&
				stricmp (cmd, "bumpgamma") &&
				stricmp (cmd, "screenshot"))) {
				M_StartControlPanel (); 
				return true; 
			} else {
				return C_DoKey (ev->data1, false);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev->data1, true);

		return false; 
	} 
 
	if (gamestate == GS_LEVEL) 
	{ 
#if 0
		if (devparm && ev->type == ev_keydown && ev->data2 == ';') 
		{ 
			G_DeathMatchSpawnPlayer (0); 
			return true; 
		} 
#endif 

		if (HU_Responder (ev)) 
			return true;		// chat ate the event 
		if (ST_Responder (ev)) 
			return true;		// status window ate it 
		if (!viewactive)
			if (AM_Responder (ev))
				return true;	// automap ate it 
	} 
		 
	if (gamestate == GS_FINALE) 
	{ 
		if (F_Responder (ev)) 
			return true;		// finale ate the event 
	} 
		 
	switch (ev->type) 
	{ 
	  case ev_keydown:
		if (C_DoKey (ev->data1, false))
		  return true;
		break;
 
	  case ev_keyup: 
		C_DoKey (ev->data1, true);
		break;

	  // [RH] mouse buttons are now sent with key up/down events
	  case ev_mouse: 
		mousex = ev->data2*mouseSensitivity->value; 
		mousey = ev->data3*mouseSensitivity->value; 
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
void G_Ticker (void) 
{ 
	int 		i;
	int 		buf; 
	ticcmd_t*	cmd;

	// do player reborns if needed
	for (i=0 ; i<MAXPLAYERS ; i++) 
		if (playeringame[i] && players[i].playerstate == PST_REBORN) 
			G_DoReborn (i);

	// do things to change the game state
	while (gameaction != ga_nothing) 
	{ 
		switch (gameaction) 
		{ 
		  case ga_loadlevel: 
			G_DoLoadLevel (); 
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
		  case ga_nothing: 
			break; 
		} 
	}

	// get commands, check consistancy,
	// and build new consistancy check
	buf = (gametic/ticdup)%BACKUPTICS;

	for (i=0 ; i<MAXPLAYERS ; i++)
	{
		if (playeringame[i])
		{
			cmd = &players[i].cmd;

			memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t));

			if (demorecording)
				G_WriteDemoTiccmd (cmd, i);
			if (demoplayback)
				G_ReadDemoTiccmd (cmd, i);

			// check for turbo cheats
			if (cmd->ucmd.forwardmove > TURBOTHRESHOLD
				&& !(gametic&31) && ((gametic>>5)&3) == i )
			{
				Printf ("%s is turbo!\n",players[i].userinfo->netname);
			}

			if (netgame && !netdemo && !(gametic%ticdup) )
			{
				if (gametic > BACKUPTICS
					&& consistancy[i][buf] != cmd->consistancy)
				{
					Printf_Bold ("Consistency failure! %d: %i (!%i)\n",
							 i, cmd->consistancy, consistancy[i][buf]);
				}
				if (players[i].mo)
					consistancy[i][buf] = players[i].mo->x;
				else
					consistancy[i][buf] = 0; // killough 2/14/98
			}
		}
	}

	// check for special buttons
	for (i=0 ; i<MAXPLAYERS ; i++)
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
		HU_Ticker ();
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
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player)
{
	// clear everything else to defaults
	G_PlayerReborn (player);
}



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
	userinfo_t  *userinfo;	// [RH] Save userinfo

	p = &players[player];

	memcpy (frags,p->frags,sizeof(frags));
	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	userinfo = p->userinfo;

	memset (p, 0, sizeof(*p));

	memcpy (p->frags, frags, sizeof(p->frags));
	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->userinfo = userinfo;

	p->usedown = p->attackdown = p->jumpdown = true;	// don't do anything immediately
	p->playerstate = PST_LIVE;
	p->health = deh_StartHealth;		// [RH] Used to be MAXHEALTH
	p->readyweapon = p->pendingweapon = wp_pistol;
	p->weaponowned[wp_fist] = true;
	p->weaponowned[wp_pistol] = true;
	p->ammo[am_clip] = deh_StartBullets; // [RH] Used to be 50

	for (i=0 ; i<NUMAMMO ; i++)
		p->maxammo[i] = maxammo[i];
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
	fixed_t				z;
	subsector_t*		ss;
	unsigned			an;
	mobj_t* 			mo;
	int 				i;

	x = mthing->x << FRACBITS;
	y = mthing->y << FRACBITS;
	z = mthing->z << FRACBITS;

	ss = R_PointInSubsector (x,y);
	z += ss->sector->floorheight;

	if (!players[playernum].mo)
	{
		// first spawn of level, before corpses
		for (i=0 ; i<playernum ; i++)
			if (players[i].mo->x == x
				&& players[i].mo->y == y)
				return false;
		return true;
	}

	players[playernum].mo->z = z;	// [RH] Checks are now full 3-D
	if (!P_CheckPosition (players[playernum].mo, x, y))
		return false;

	// flush an old corpse if needed
	if (bodyqueslot >= BODYQUESIZE)
		P_RemoveMobj (bodyque[bodyqueslot%BODYQUESIZE]);
	bodyque[bodyqueslot%BODYQUESIZE] = players[playernum].mo;
	bodyqueslot++;

	// spawn a teleport fog
	an = ( ANG45 * (mthing->angle/45) ) >> ANGLETOFINESHIFT;

	mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
					  , z
					  , MT_TFOG, 0);

	// [RH] consoleplayer changed to displayplayer
	if (players[displayplayer].viewz != 1)
		S_StartSound (mo, sfx_telept);	// don't start sound on first frame 

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

	for (i = 0; i < MAXPLAYERS; i++) {
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

	for (i = 0; i < selections; i++) {
		fixed_t distance = PlayersRangeFromSpot (&deathmatchstarts[i]);

		if (distance > bestdistance) {
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

	return NULL;
}

void G_DeathMatchSpawnPlayer (int playernum)
{
	int selections;
	mapthing2_t *spot;

	selections = deathmatch_p - deathmatchstarts;
	// [RH] 4 changed to doomcom->numplayers
	if (selections < doomcom->numplayers)
		I_Error ("Only %i deathmatch spots, %d required",
				 selections, doomcom->numplayers); 

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

	if (!netgame)
	{
		// reload the level from scratch
		gameaction = ga_loadlevel;
	}
	else
	{
		// respawn at the start

		// first dissasociate the corpse
		players[playernum].mo->player = NULL;

		// spawn at random spot if in death match
		if (deathmatch->value)
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (G_CheckSpot (playernum, &playerstarts[playernum]) )
		{
			P_SpawnPlayer (&playerstarts[playernum]);
			return;
		}

		// try to spawn at one of the other players spots
		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (G_CheckSpot (playernum, &playerstarts[i]) )
			{
				int oldtype = playerstarts[i].type;
				if (playernum < 4)
					playerstarts[i].type = playernum+1; 	// fake as other player
				else
					playerstarts[i].type = playernum+4001-4;	// [RH] fake as high player
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

char	savename[256];

void G_LoadGame (char* name)
{
	strcpy (savename, name);
	gameaction = ga_loadgame;
}

#define VERSIONSIZE 			16


void G_DoLoadGame (void)
{
	int 		length;
	int 		i;
	char		vcheck[VERSIONSIZE];
	char		mapname[9];

	gameaction = ga_nothing;

	length = M_ReadFile (savename, &savebuffer);
	save_p = savebuffer + SAVESTRINGSIZE;

	// skip the description field
	memset (vcheck,0,sizeof(vcheck));
	sprintf (vcheck,"version %i",VERSION);
	if (strcmp (save_p, vcheck))
		return; 						// bad version
	save_p += VERSIONSIZE;

	memcpy (mapname, save_p, 8);
	mapname[8] = 0;
	save_p += 8;
	for (i=0 ; i<MAXPLAYERS ; i++)
		playeringame[i] = *save_p++;

	C_ReadCVars (&save_p);			// [RH] Read server info cvars

	// load a base level
	G_InitNew (mapname);

	// dearchive all the modifications
	P_UnArchiveLevelLocals ();		// [RH] dearchive level locals
	P_UnArchivePlayers ();
	P_UnArchiveWorld ();
	P_UnArchiveThinkers ();
	P_UnArchiveSpecials ();
	P_UnArchiveRNGState ();		// [RH] retrieve the state of the RNG

	if (*save_p != 0x1d)
		I_Error ("Bad savegame");

	// done
	Z_Free (savebuffer);

	if (setsizeneeded)
		R_ExecuteSetViewSize ();
	
	// draw the pattern into the back screen
	R_FillBackScreen ();
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

void G_DoSaveGame (void)
{
	char		name[100];
	char		name2[VERSIONSIZE];
	char*		description;
	int 		length;
	int 		i;

	if (M_CheckParm("-cdrom"))
		sprintf(name,"c:\\doomdata\\"SAVEGAMENAME"%d.dsg",savegameslot);
	else
		sprintf (name,SAVEGAMENAME"%d.dsg",savegameslot);
	description = savedescription;

	save_p = savebuffer = Z_Malloc (SAVEGAMESIZE + 0x4000, PU_STATIC, 0);

	memcpy (save_p, description, SAVESTRINGSIZE);
	save_p += SAVESTRINGSIZE;
	memset (name2,0,sizeof(name2));
	sprintf (name2,"version %i",VERSION);
	memcpy (save_p, name2, VERSIONSIZE);
	save_p += VERSIONSIZE;

	memcpy (save_p, level.mapname, 8);
	save_p += 8;
	for (i=0 ; i<MAXPLAYERS ; i++)
		*save_p++ = playeringame[i];
	C_WriteCVars (&save_p, CVAR_SERVERINFO);	// [RH] Save serverinfo cvars

	P_ArchiveLevelLocals ();	// [RH] Archive level locals
	P_ArchivePlayers ();
	P_ArchiveWorld ();
	P_ArchiveThinkers ();
	P_ArchiveSpecials ();
	P_ArchiveRNGState ();		// [RH] save the state of the RNG

	*save_p++ = 0x1d;			// consistancy marker

	length = save_p - savebuffer;
	if (length > SAVEGAMESIZE)
		I_Error ("Savegame buffer overrun");
	M_WriteFile (name, savebuffer, length);
	Z_Free (savebuffer);
	gameaction = ga_nothing;
	savedescription[0] = 0;

	players[consoleplayer].message = GGSAVED;

	// draw the pattern into the back screen
	R_FillBackScreen ();
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

	if (olddemo) {
		if (*demo_p == DEMOMARKER) {
			// end of demo data stream
			G_CheckDemoStatus ();
		} else {
			cmd->ucmd.forwardmove = ((signed char)demo_p[0]) * 256;
			cmd->ucmd.sidemove = ((signed char)demo_p[1]) * 256;
			cmd->ucmd.yaw = ((unsigned char)demo_p[2]) * 256;
			cmd->ucmd.buttons = (unsigned char)demo_p[3];
			demo_p += 4;
		}
	} else {
		int id = DEM_BAD;

		while (!clonecount && id != DEM_USERCMD && id != DEM_USERCMDCLONE) {
			if (!demorecording && demo_p >= zdembodyend) {
				// nothing left in the BODY chunk, so end playback.
				G_CheckDemoStatus ();
				break;
			}

			id = ReadByte (&demo_p);

			switch (id) {
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
		if (clonecount) {
			clonecount--;
			memcpy (&cmd->ucmd, &LastUserCmd, sizeof(usercmd_t));
		}
	}
} 

BOOL stoprecording;

void Cmd_Stop (player_t *plyr, int argc, char **argv)
{
	stoprecording = true;
}

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player)
{
	static int clonecount = 0;
	static byte *clonepos = NULL;

	if (stoprecording)			// use "stop" console command to end demo recording
		G_CheckDemoStatus ();

	// [RH] Write any special "ticcmds" for this player to the demo
	if (DemoQueue[player].size) {
		memcpy (demo_p, DemoQueue[player].queue, DemoQueue[player].size);
		demo_p += DemoQueue[player].size;
		Z_Free (DemoQueue[player].queue);
		DemoQueue[player].queue = NULL;
		DemoQueue[player].size = 0;
	}

	// [RH] Now write out a "normal" ticcmd. Use the clone command if this
	//		one is identical to the previous one.
	if (!memcmp (&LastUserCmd, &cmd->ucmd, sizeof(usercmd_t))) {
		if (!clonecount || clonecount == 256 || clonepos != demo_p) {
			WriteByte (DEM_USERCMDCLONE, &demo_p);
			WriteByte (0, &demo_p);
			clonecount = 1;
			clonepos = demo_p;
		} else {
			*(demo_p - 1) = clonecount++;
		}
	} else {
		WriteUserCmdMessage (&cmd->ucmd, &demo_p);
		clonecount = 0;
		memcpy (&LastUserCmd, &cmd->ucmd, sizeof(usercmd_t));
	}

	if (demo_p > demoend - 16) {
		// no more space
		G_CheckDemoStatus (); 
		return; 
	}

//	if (!clonecount)
//		G_ReadDemoTiccmd (cmd); 	// make SURE it is exactly the same
}



//
// G_RecordDemo
//
void G_RecordDemo (char* name)
{
	int	i;
	int	maxsize;

	usergame = false;
	strcpy (demoname, name);
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	maxsize = 0x20000;
	i = M_CheckParm ("-maxdemo");
	if (i && i<myargc-1) {
		maxsize = atoi(myargv[i+1])*1024;
		if (maxsize < 0x20000)
			maxsize = 0x20000;
	}
	demobuffer = Z_Malloc (maxsize,PU_STATIC,NULL);
	demoend = demobuffer + maxsize;

	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (void)
{
	int i;

	MakeEmptyUserCmd();

	olddemo = false;
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

char*	defdemoname;

void G_DeferedPlayDemo (char* name)
{
	defdemoname = name;
	gameaction = ga_playdemo;
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

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT.

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

				ReadWord (&demo_p);			// ZDoom version demo was created with
				if (ReadWord (&demo_p) > GAMEVER) {		// Minimum ZDoom version
					Printf ("Demo requires a newer version of ZDoom!\n");
					return true;
				}
				memcpy (mapname, demo_p, 8);	// Read map name
				mapname[8] = 0;
				demo_p += 8;
				rngseed = ReadLong (&demo_p);
				consoleplayer = *demo_p++;
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
		netgame = netdemo = true;

	return false;
}

void G_DoPlayDemo (void)
{
	skill_t skill;
	int		i;
	int		demoversion = 5;
	char	mapname[9];

	gameaction = ga_nothing;
	demobuffer = demo_p = W_CacheLumpName (defdemoname, PU_STATIC);

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo
	MakeEmptyUserCmd ();

	if (ReadLong (&demo_p) != FORM_ID) {
		olddemo = true;
		demo_p -= 4;
		demoversion = *demo_p++;

		if (demoversion > 4 && demoversion < 104) {
			Printf ("Demo is from unknown game version!\n");
			gameaction = ga_nothing;
			return;
		}

		if (demoversion <= 4) {
			skill = (float)demoversion;
			strcpy (mapname, CalcMapName (demo_p[0], demo_p[1]));
			demo_p += 2;
			consoleplayer = 0;
			SetCVarFloat (deathmatch, 0);
			SetCVarFloat (dmflagsvar, DF_NO_FREELOOK|DF_NO_JUMP);
		} else {
			int flags = DF_NO_FREELOOK|DF_NO_JUMP;

			skill = (float)(*demo_p++);
			strcpy (mapname, CalcMapName (demo_p[0], demo_p[1]));
			if (demo_p[2]) {
				SetCVarFloat (deathmatch, 1.0f);
				if (demo_p[2] == 2)
					flags |= DF_ITEMS_RESPAWN;
				else
					flags |= DF_WEAPONS_STAY;
			}
			if (demo_p[3])
				flags |= DF_MONSTERS_RESPAWN;
			if (demo_p[4])
				flags |= DF_FAST_MONSTERS;
			if (demo_p[5])
				flags |= DF_NO_MONSTERS;
			consoleplayer = demo_p[6];
			demo_p += 7;
			SetCVarFloat (dmflagsvar, (float)flags);
		}

		// Old demos only have four players
		for (i=0 ; i < 4 ; i++)
			playeringame[i] = demo_p[i];

		demo_p += i;

		if (playeringame[1]) {
			netgame = true;
			netdemo = true;
		}

		SetCVarFloat (gameskill, skill);

		AddCommandString ("set fraglimit 0; set timelimit 0; set cheats 0, sv_gravity 800");

		{
			// Setup compatible userinfo
			static char greenPlayer[] = "\\name\\Green\\autoaim\\50000\\color\\74 fc 6c";
			static char indigoPlayer[] = "\\name\\Indigo\\autoaim\\50000\\color\\80 80 80";
			static char brownPlayer[] = "\\name\\Brown\\autoaim\\50000\\color\\bc 78 48";
			static char redPlayer[] = "\\name\\Red\\autoaim\\50000\\color\\fc 00 00";
			byte *stream;

			stream = greenPlayer;
			D_ReadUserInfoStrings (0, &stream, false);
			stream = indigoPlayer;
			D_ReadUserInfoStrings (1, &stream, false);
			stream = brownPlayer;
			D_ReadUserInfoStrings (2, &stream, false);
			stream = redPlayer;
			D_ReadUserInfoStrings (3, &stream, false);

			R_BuildCompatiblePlayerTranslations ();
		}
	} else {
		if (G_ProcessIFFDemo (mapname)) {
			gameaction = ga_nothing;
			return;
		}
	}

	// don't spend a lot of time in loadlevel 
	precache = false;
	G_InitNew (mapname);
	precache = true;

	usergame = false;
	demoplayback = true;
}

//
// G_TimeDemo
//
void G_TimeDemo (char* name)
{
	nodrawers = M_CheckParm ("-nodraw");
	noblit = M_CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	defdemoname = name;
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

BOOL G_CheckDemoStatus (void)
{
	if (timingdemo)
	{
		extern int starttime;
		int endtime;

		endtime = I_GetTimeReally () - starttime;
		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

		I_Error ("timed %i gametics in %i realtics (%.1f fps)",gametic 
				 , endtime, (float)gametic/(float)endtime*(float)TICRATE);
	}

	if (demoplayback)
	{
		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed

		if (singledemo)
			I_Quit ();

		Z_ChangeTag (demobuffer, PU_CACHE);
		olddemo = false;
		demoplayback = false;
		netdemo = false;
		netgame = false;
//		deathmatch = 0;
		{
			int i;

			for (i = 1; i < MAXPLAYERS; i++)
				playeringame[i] = 0;
		}
//		respawnparm = false;
//		fastparm = 0;
//		nomonsters = false;
		consoleplayer = 0;
		D_AdvanceDemo (); 

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
		Z_Free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		I_Error ("Demo %s recorded",demoname); 
	}

	return false; 
}
