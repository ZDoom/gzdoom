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
// DESCRIPTION:
//		Status bar code.
//		Does the face/direction indicator animatin.
//		Does palette indicators as well (red pain/berserk, bright pickup)
//		[RH] Widget coordinates are relative to the console, not the screen!
//
//-----------------------------------------------------------------------------



#include <stdio.h>

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "m_random.h"
#include "w_wad.h"
#include "doomdef.h"
#include "g_game.h"
#include "st_stuff.h"
#include "st_lib.h"
#include "r_local.h"
#include "p_local.h"
#include "p_inter.h"
#include "am_map.h"
#include "m_cheat.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_text.h"
#include "doomstat.h"
#include "dstrings.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "version.h"

CVAR (idmypos, "0", 0)
BEGIN_CUSTOM_CVAR (st_scale, "0", CVAR_ARCHIVE)		// Stretch status bar to full screen width?
{
	if (var.value)
	{
		// Stretch status bar to fill width of screen

		ST_WIDTH = screen->width;
		if (ST_WIDTH == 320)
		{
			// Do not scale height for 320 x 2X0 screens
			ST_HEIGHT = 32;
		}
		else
		{
			ST_HEIGHT = (32 * screen->height) / 200;
		}
	}
	else
	{
		// Do not stretch status bar

		ST_WIDTH = 320;
		ST_HEIGHT = 32;
	}

	ST_X = (screen->width-ST_WIDTH)/2;
	ST_Y = screen->height - ST_HEIGHT;

	setsizeneeded = true;
	SB_state = -1;
}
END_CUSTOM_CVAR (st_scale)

// [RH] Needed when status bar scale changes
extern BOOL setsizeneeded;
extern BOOL automapactive;

// [RH] Status bar background
DCanvas *stbarscreen;
// [RH] Active status bar
DCanvas *stnumscreen;

BOOL DrawNewHUD;		// [RH] Draw the new HUD?


// functions in st_new.c
void ST_initNew (void);
void ST_unloadNew (void);
void ST_newDraw (void);

// [RH] Base blending values (for e.g. underwater)
int BaseBlendR, BaseBlendG, BaseBlendB;
float BaseBlendA;


//
// STATUS BAR DATA
//


// N/256*100% probability
//	that the normal face state will change
#define ST_FACEPROBABILITY		96

// For Responder
#define ST_TOGGLECHAT			KEY_ENTER

// Location of status bar face
#define ST_FX					(142)

// Should be set to patch width
//	for tall numbers later on
#define ST_TALLNUMWIDTH 		(tallnum[0]->width)

// Number of status faces.
#define ST_NUMPAINFACES 		5
#define ST_NUMSTRAIGHTFACES 	3
#define ST_NUMTURNFACES 		2
#define ST_NUMSPECIALFACES		3

#define ST_FACESTRIDE \
		  (ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES)

#define ST_NUMEXTRAFACES		2

#define ST_NUMFACES \
		  (ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES)

#define ST_TURNOFFSET			(ST_NUMSTRAIGHTFACES)
#define ST_OUCHOFFSET			(ST_TURNOFFSET + ST_NUMTURNFACES)
#define ST_EVILGRINOFFSET		(ST_OUCHOFFSET + 1)
#define ST_RAMPAGEOFFSET		(ST_EVILGRINOFFSET + 1)
#define ST_GODFACE				(ST_NUMPAINFACES*ST_FACESTRIDE)
#define ST_DEADFACE 			(ST_GODFACE+1)

#define ST_FACESX				(143)
#define ST_FACESY				(0)

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT			(1*TICRATE)
#define ST_OUCHCOUNT			(1*TICRATE)
#define ST_RAMPAGEDELAY 		(2*TICRATE)

#define ST_MUCHPAIN 			20


// Location and size of statistics,
//	justified according to widget type.
// Problem is, within which space? STbar? Screen?
// Note: this could be read in by a lump.
//		 Problem is, is the stuff rendered
//		 into a buffer,
//		 or into the frame buffer?

// AMMO number pos.
#define ST_AMMOWIDTH			3
#define ST_AMMOX				(44)
#define ST_AMMOY				(3)

// HEALTH number pos.
#define ST_HEALTHWIDTH			3
#define ST_HEALTHX				(90)
#define ST_HEALTHY				(3)

// Weapon pos.
#define ST_ARMSX				(111)
#define ST_ARMSY				(4)
#define ST_ARMSBGX				(104)
#define ST_ARMSBGY				(0)
#define ST_ARMSXSPACE			12
#define ST_ARMSYSPACE			10

// Frags pos.
#define ST_FRAGSX				(138)
#define ST_FRAGSY				(3)
#define ST_FRAGSWIDTH			2

// ARMOR number pos.
#define ST_ARMORWIDTH			3
#define ST_ARMORX				(221)
#define ST_ARMORY				(3)

// Key icon positions.
#define ST_KEY0WIDTH			8
#define ST_KEY0HEIGHT			5
#define ST_KEY0X				(239)
#define ST_KEY0Y				(3)
#define ST_KEY1WIDTH			ST_KEY0WIDTH
#define ST_KEY1X				(239)
#define ST_KEY1Y				(13)
#define ST_KEY2WIDTH			ST_KEY0WIDTH
#define ST_KEY2X				(239)
#define ST_KEY2Y				(23)

// Ammunition counter.
#define ST_AMMO0WIDTH			3
#define ST_AMMO0HEIGHT			6
#define ST_AMMO0X				(288)
#define ST_AMMO0Y				(5)
#define ST_AMMO1WIDTH			ST_AMMO0WIDTH
#define ST_AMMO1X				(288)
#define ST_AMMO1Y				(11)
#define ST_AMMO2WIDTH			ST_AMMO0WIDTH
#define ST_AMMO2X				(288)
#define ST_AMMO2Y				(23)
#define ST_AMMO3WIDTH			ST_AMMO0WIDTH
#define ST_AMMO3X				(288)
#define ST_AMMO3Y				(17)

// Indicate maximum ammunition.
// Only needed because backpack exists.
#define ST_MAXAMMO0WIDTH		3
#define ST_MAXAMMO0HEIGHT		5
#define ST_MAXAMMO0X			(314)
#define ST_MAXAMMO0Y			(5)
#define ST_MAXAMMO1WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO1X			(314)
#define ST_MAXAMMO1Y			(11)
#define ST_MAXAMMO2WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO2X			(314)
#define ST_MAXAMMO2Y			(23)
#define ST_MAXAMMO3WIDTH		ST_MAXAMMO0WIDTH
#define ST_MAXAMMO3X			(314)
#define ST_MAXAMMO3Y			(17)

// pistol
#define ST_WEAPON0X 			(110)
#define ST_WEAPON0Y 			(4)

// shotgun
#define ST_WEAPON1X 			(122)
#define ST_WEAPON1Y 			(4)

// chain gun
#define ST_WEAPON2X 			(134)
#define ST_WEAPON2Y 			(4)

// missile launcher
#define ST_WEAPON3X 			(110)
#define ST_WEAPON3Y 			(13)

// plasma gun
#define ST_WEAPON4X 			(122)
#define ST_WEAPON4Y 			(13)

 // bfg
#define ST_WEAPON5X 			(134)
#define ST_WEAPON5Y 			(13)

// WPNS title
#define ST_WPNSX				(109)
#define ST_WPNSY				(23)

 // DETH title
#define ST_DETHX				(109)
#define ST_DETHY				(23)

//Incoming messages window location
//UNUSED
// #define ST_MSGTEXTX	   (viewwindowx)
// #define ST_MSGTEXTY	   (viewwindowy+realviewheight-18)
#define ST_MSGTEXTX 			0
#define ST_MSGTEXTY 			0
// Dimensions given in characters.
#define ST_MSGWIDTH 			52
// Or shall I say, in lines?
#define ST_MSGHEIGHT			1

#define ST_OUTTEXTX 			0
#define ST_OUTTEXTY 			6

// Width, in characters again.
#define ST_OUTWIDTH 			52
 // Height, in lines. 
#define ST_OUTHEIGHT			1


// [RH] Turned these into variables
// Size of statusbar.
// Now ([RH] truly) sensitive for scaling.
int						ST_HEIGHT;
int						ST_WIDTH;
int						ST_X;
int						ST_Y;

int						SB_state = -1;


// main player in game
// [RH] not static
player_t*				plyr; 

// ST_Start() has just been called
BOOL					st_firsttime;

// used to execute ST_Init() only once
static int				veryfirsttime = 1;

// used for timing
static unsigned int 	st_clock;

// used for making messages go away
static int				st_msgcounter=0;

// used when in chat 
static st_chatstateenum_t		st_chatstate;

// whether in automap or first-person
static st_stateenum_t	st_gamestate;

// whether left-side main status bar is active
static BOOL			st_statusbaron;

// whether status bar chat is active
static BOOL			st_chat;

// value of st_chat before message popped up
static BOOL			st_oldchat;

// whether chat window has the cursor on
static BOOL			st_cursoron;

// !deathmatch
static BOOL			st_notdeathmatch; 

// !deathmatch && st_statusbaron
static BOOL			st_armson;

// !deathmatch
static BOOL			st_fragson; 

// main bar left
static patch_t* 		sbar;

// 0-9, tall numbers
// [RH] no longer static
patch_t*		 		tallnum[10];

// tall % sign
// [RH] no longer static
patch_t*		 		tallpercent;

// 0-9, short, yellow (,different!) numbers
static patch_t* 		shortnum[10];

// 3 key-cards, 3 skulls, [RH] 3 combined
patch_t* 				keys[NUMCARDS+NUMCARDS/2]; 

// face status patches [RH] no longer static
patch_t* 				faces[ST_NUMFACES];

// face background
static patch_t* 		faceback;

 // main bar right
static patch_t* 		armsbg;

// weapon ownership patches
static patch_t* 		arms[6][2]; 

// ready-weapon widget
static st_number_t		w_ready;

 // in deathmatch only, summary of frags stats
static st_number_t		w_frags;

// health widget
static st_percent_t 	w_health;

// arms background
static st_binicon_t 	w_armsbg; 


// weapon ownership widgets
static st_multicon_t	w_arms[6];

// face status widget
static st_multicon_t	w_faces; 

// keycard widgets
static st_multicon_t	w_keyboxes[3];

// armor widget
static st_percent_t 	w_armor;

// ammo widgets
static st_number_t		w_ammo[4];

// max ammo widgets
static st_number_t		w_maxammo[4]; 



 // number of frags so far in deathmatch
static int		st_fragscount;

// used to use appopriately pained face
static int		st_oldhealth = -1;

// used for evil grin
static int		oldweaponsowned[NUMWEAPONS]; 

 // count until face changes
static int		st_facecount = 0;

// current face index, used by w_faces
// [RH] not static anymore
int				st_faceindex = 0;

// holds key-type for each key box on bar
static int		keyboxes[3]; 

// a random number per tick
static int		st_randomnumber;  



// Massive bunches of cheat shit
//	to keep it from being easy to figure them out.
// Yeah, right...
unsigned char	cheat_mus_seq[] =
{
	0xb2, 0x26, 0xb6, 0xae, 0xea, 1, 0, 0, 0xff // idmus
};

unsigned char	cheat_choppers_seq[] =
{
	0xb2, 0x26, 0xe2, 0x32, 0xf6, 0x2a, 0x2a, 0xa6, 0x6a, 0xea, 0xff // id...
};

unsigned char	cheat_god_seq[] =
{
	0xb2, 0x26, 0x26, 0xaa, 0x26, 0xff	// iddqd
};

unsigned char	cheat_ammo_seq[] =
{
	0xb2, 0x26, 0xf2, 0x66, 0xa2, 0xff	// idkfa
};

unsigned char	cheat_ammonokey_seq[] =
{
	0xb2, 0x26, 0x66, 0xa2, 0xff		// idfa
};


// Smashing Pumpkins Into Small Piles Of Putrid Debris. 
unsigned char	cheat_noclip_seq[] =
{
	0xb2, 0x26, 0xea, 0x2a, 0xb2,		// idspispopd
	0xea, 0x2a, 0xf6, 0x2a, 0x26, 0xff
};

//
unsigned char	cheat_commercial_noclip_seq[] =
{
	0xb2, 0x26, 0xe2, 0x36, 0xb2, 0x2a, 0xff	// idclip
}; 



unsigned char	cheat_powerup_seq[7][10] =
{
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6e, 0xff }, 	// beholdv
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xea, 0xff }, 	// beholds
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xb2, 0xff }, 	// beholdi
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x6a, 0xff }, 	// beholdr
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xa2, 0xff }, 	// beholda
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0x36, 0xff }, 	// beholdl
	{ 0xb2, 0x26, 0x62, 0xa6, 0x32, 0xf6, 0x36, 0x26, 0xff }			// behold
};


unsigned char	cheat_clev_seq[] =
{
	0xb2, 0x26,  0xe2, 0x36, 0xa6, 0x6e, 1, 0, 0, 0xff	// idclev
};


// my position cheat
unsigned char	cheat_mypos_seq[] =
{
	0xb2, 0x26, 0xb6, 0xba, 0x2a, 0xf6, 0xea, 0xff		// idmypos
}; 


// Now what?
cheatseq_t		cheat_mus = { cheat_mus_seq, 0 };
cheatseq_t		cheat_god = { cheat_god_seq, 0 };
cheatseq_t		cheat_ammo = { cheat_ammo_seq, 0 };
cheatseq_t		cheat_ammonokey = { cheat_ammonokey_seq, 0 };
cheatseq_t		cheat_noclip = { cheat_noclip_seq, 0 };
cheatseq_t		cheat_commercial_noclip = { cheat_commercial_noclip_seq, 0 };

cheatseq_t		cheat_powerup[7] =
{
	{ cheat_powerup_seq[0], 0 },
	{ cheat_powerup_seq[1], 0 },
	{ cheat_powerup_seq[2], 0 },
	{ cheat_powerup_seq[3], 0 },
	{ cheat_powerup_seq[4], 0 },
	{ cheat_powerup_seq[5], 0 },
	{ cheat_powerup_seq[6], 0 }
};

cheatseq_t		cheat_choppers = { cheat_choppers_seq, 0 };
cheatseq_t		cheat_clev = { cheat_clev_seq, 0 };
cheatseq_t		cheat_mypos = { cheat_mypos_seq, 0 };


//
// STATUS BAR CODE
//
void ST_Stop(void);

void ST_refreshBackground(void)
{
	if (st_statusbaron)
	{
		// [RH] If screen is wider than the status bar,
		//      draw stuff around status bar.
		if (FG->width > ST_WIDTH)
		{
			R_DrawBorder (0, ST_Y, ST_X, FG->height);
			R_DrawBorder (FG->width - ST_X, ST_Y, FG->width, FG->height);
		}

		BG->DrawPatch (sbar, 0, 0);

		if (multiplayer)
		{
			// [RH] Always draw faceback with the player's color
			//		using a translation rather than a different patch.
			V_ColorMap = translationtables + (plyr - players)*256;
			BG->DrawTranslatedPatch (faceback, ST_FX, 0);
		}

		BG->Blit (0, 0, 320, 32, stnumscreen, 0, 0, 320, 32);

		if (!st_scale.value || ST_WIDTH == 320)
			stnumscreen->Blit (0, 0, 320, 32,
				FG, ST_X, ST_Y, ST_WIDTH, ST_HEIGHT);
	}
}


BOOL CheckCheatmode (void);

// Respond to keyboard input events, intercept cheats.
// [RH] Cheats eat the last keypress used to trigger them
BOOL ST_Responder (event_t *ev)
{
	  BOOL eat = false;
	  int i;
		
	  // Filter automap on/off.
	  if (ev->type == ev_keyup && ((ev->data1 & 0xffff0000) == AM_MSGHEADER))
	  {
		switch(ev->data1)
		{
		case AM_MSGENTERED:
			st_gamestate = AutomapState;
			st_firsttime = true;
			break;
			
		case AM_MSGEXITED:
			st_gamestate = FirstPersonState;
			break;
		}
	}

	// if a user keypress...
	else if (ev->type == ev_keydown)
	{
		// cheats are now allowed in netgames if
		// 'sv_cheats' cvar is set...

		// 'dqd' cheat for toggleable god mode
		if (cht_CheckCheat(&cheat_god, (char)ev->data2))
		{
			if (CheckCheatmode ())
				return false;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_IDDQD);
			eat = true;
		}

		// 'fa' cheat for killer fucking arsenal
		else if (cht_CheckCheat(&cheat_ammonokey, (char)ev->data2))
		{
			if (CheckCheatmode ())
				return false;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_IDFA);
			eat = true;
		}

		// 'kfa' cheat for key full ammo
		else if (cht_CheckCheat(&cheat_ammo, (char)ev->data2))
		{
			if (CheckCheatmode ())
				return false;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_IDKFA);
			eat = true;
		}

		// Simplified, accepting both "noclip" and "idspispopd".
		// no clipping mode cheat
		else if ( cht_CheckCheat(&cheat_noclip, (char)ev->data2) 
				|| cht_CheckCheat(&cheat_commercial_noclip,(char)ev->data2) )
		{
			if (CheckCheatmode ())
				return false;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_NOCLIP);
			eat = true;
		}
		// 'behold?' power-up cheats
		for (i=0;i<6;i++)
		{
			if (cht_CheckCheat(&cheat_powerup[i], (char)ev->data2))
			{
				if (CheckCheatmode ())
					return false;

				Net_WriteByte (DEM_GENERICCHEAT);
				Net_WriteByte ((byte)(CHT_BEHOLDV + i));
				eat = true;
			}
		}

		// 'behold' power-up menu
		if (cht_CheckCheat(&cheat_powerup[6], (char)ev->data2))
		{
			if (CheckCheatmode ())
				return false;

			Printf (PRINT_HIGH, "%s\n", STSTR_BEHOLD);
		}

		// 'choppers' invulnerability & chainsaw
		else if (cht_CheckCheat(&cheat_choppers, (char)ev->data2))
		{
			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_CHAINSAW);
			eat = true;
		}

		// 'mypos' for player position
		else if (cht_CheckCheat(&cheat_mypos, (char)ev->data2))
		{
			AddCommandString ("toggle idmypos");
			eat = true;
		}

		// 'clev' change-level cheat
		else if (cht_CheckCheat(&cheat_clev, (char)ev->data2))
		{
			char cmd[16];

			strcpy (cmd, "idclev ");
			cht_GetParam(&cheat_clev, &cmd[7]);
			cmd[9] = 0;
			AddCommandString (cmd);
			eat = true;
		}

		// 'idmus' change-music cheat
		else if (cht_CheckCheat(&cheat_mus, (char)ev->data2))
		{
			char buf[16];
				  
			cht_GetParam(&cheat_mus, buf);
			buf[2] = 0;

			sprintf (buf + 3, "idmus %s\n", buf);
			AddCommandString (buf + 3);
			eat = true;
		}
	}

	return eat;
}



int ST_calcPainOffset(void)
{
	int 		health;
	static int	lastcalc;
	static int	oldhealth = -1;
	
	health = plyr->health > 100 ? 100 : plyr->health;

	if (health != oldhealth)
	{
		lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
		oldhealth = health;
	}
	return lastcalc;
}


//
// This is a not-very-pretty routine which handles
//	the face states and their timing.
// the precedence of expressions is:
//	dead > evil grin > turned head > straight ahead
//
void ST_updateFaceWidget(void)
{
	int 		i;
	angle_t 	badguyangle;
	angle_t 	diffang;
	static int	lastattackdown = -1;
	static int	priority = 0;
	BOOL	 	doevilgrin;

	if (priority < 10)
	{
		// dead
		if (!plyr->health)
		{
			priority = 9;
			st_faceindex = ST_DEADFACE;
			st_facecount = 1;
		}
	}

	if (priority < 9)
	{
		if (plyr->bonuscount)
		{
			// picking up bonus
			doevilgrin = false;

			for (i=0;i<NUMWEAPONS;i++)
			{
				if (oldweaponsowned[i] != plyr->weaponowned[i])
				{
					doevilgrin = true;
					oldweaponsowned[i] = plyr->weaponowned[i];
				}
			}
			if (doevilgrin) 
			{
				// evil grin if just picked up weapon
				priority = 8;
				st_facecount = ST_EVILGRINCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_EVILGRINOFFSET;
			}
		}

	}
  
	if (priority < 8)
	{
		if (plyr->damagecount
			&& plyr->attacker
			&& plyr->attacker != plyr->mo)
		{
			// being attacked
			priority = 7;
			
			if (plyr->health - st_oldhealth > ST_MUCHPAIN)
			{
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				badguyangle = R_PointToAngle2(plyr->mo->x,
											  plyr->mo->y,
											  plyr->attacker->x,
											  plyr->attacker->y);
				
				if (badguyangle > plyr->mo->angle)
				{
					// whether right or left
					diffang = badguyangle - plyr->mo->angle;
					i = diffang > ANG180; 
				}
				else
				{
					// whether left or right
					diffang = plyr->mo->angle - badguyangle;
					i = diffang <= ANG180; 
				} // confusing, aint it?

				
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset();
				
				if (diffang < ANG45)
				{
					// head-on	  
					st_faceindex += ST_RAMPAGEOFFSET;
				}
				else if (i)
				{
					// turn face right
					st_faceindex += ST_TURNOFFSET;
				}
				else
				{
					// turn face left
					st_faceindex += ST_TURNOFFSET+1;
				}
			}
		}
	}
  
	if (priority < 7)
	{
		// getting hurt because of your own damn stupidity
		if (plyr->damagecount)
		{
			if (plyr->health - st_oldhealth > ST_MUCHPAIN)
			{
				priority = 7;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_OUCHOFFSET;
			}
			else
			{
				priority = 6;
				st_facecount = ST_TURNCOUNT;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
			}

		}

	}
  
	if (priority < 6)
	{
		// rapid firing
		if (plyr->attackdown)
		{
			if (lastattackdown==-1)
				lastattackdown = ST_RAMPAGEDELAY;
			else if (!--lastattackdown)
			{
				priority = 5;
				st_faceindex = ST_calcPainOffset() + ST_RAMPAGEOFFSET;
				st_facecount = 1;
				lastattackdown = 1;
			}
		}
		else
			lastattackdown = -1;

	}
  
	if (priority < 5)
	{
		// invulnerability
		if ((plyr->cheats & CF_GODMODE)
			|| plyr->powers[pw_invulnerability])
		{
			priority = 4;

			st_faceindex = ST_GODFACE;
			st_facecount = 1;

		}
	}

	// look left or look right if the facecount has timed out
	if (!st_facecount)
	{
		st_faceindex = ST_calcPainOffset() + (st_randomnumber % 3);
		st_facecount = ST_STRAIGHTFACECOUNT;
		priority = 0;
	}

	st_facecount--;

}

void ST_updateWidgets(void)
{
	static int	largeammo = 1994; // means "n/a"
	int 		i;

	// must redirect the pointer if the ready weapon has changed.
	//	if (w_ready.data != plyr->readyweapon)
	//	{
	if (weaponinfo[plyr->readyweapon].ammo == am_noammo)
		w_ready.num = &largeammo;
	else
		w_ready.num = &plyr->ammo[weaponinfo[plyr->readyweapon].ammo];
	//{
	// static int tic=0;
	// static int dir=-1;
	// if (!(tic&15))
	//	 plyr->ammo[weaponinfo[plyr->readyweapon].ammo]+=dir;
	// if (plyr->ammo[weaponinfo[plyr->readyweapon].ammo] == -100)
	//	 dir = 1;
	// tic++;
	// }
	w_ready.data = plyr->readyweapon;

	// if (*w_ready.on)
	//	STlib_updateNum(&w_ready, true);
	// refresh weapon change
	//	}

	// update keycard multiple widgets
	for (i=0;i<3;i++)
	{
		keyboxes[i] = plyr->cards[i] ? i : -1;

		// [RH] show multiple keys per box, too
		if (plyr->cards[i+3])
			keyboxes[i] = (keyboxes[i] == -1) ? i+3 : i+6;
	}

	// refresh everything if this is him coming back to life
	ST_updateFaceWidget();

	// used by the w_armsbg widget
	st_notdeathmatch = !((int)deathmatch.value);
	
	// used by w_arms[] widgets
	st_armson = st_statusbaron && !((int)deathmatch.value);

	// used by w_frags widget
	st_fragson = (int)deathmatch.value && st_statusbaron; 
	st_fragscount = plyr->fragcount;	// [RH] Just use cumulative total

	// get rid of chat window if up because of message
	if (!--st_msgcounter)
		st_chat = st_oldchat;

}

void ST_Ticker (void)
{
//FIXME
//return;
	st_clock++;
	st_randomnumber = M_Random();
	ST_updateWidgets();
	st_oldhealth = plyr->health;

}

static float st_palette[4];

// [RH] Amount of red flash for up to 114 damage points. Calculated by hand
//		using a logarithmic scale and my trusty HP48G.
static byte damageToAlpha[114] = {
	  0,   8,  16,  23,  30,  36,  42,  47,  53,  58,  62,  67,  71,  75,  79,
	 83,  87,  90,  94,  97, 100, 103, 107, 109, 112, 115, 118, 120, 123, 125,
	128, 130, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157,
	159, 160, 162, 164, 165, 167, 169, 170, 172, 173, 175, 176, 178, 179, 181,
	182, 183, 185, 186, 187, 189, 190, 191, 192, 194, 195, 196, 197, 198, 200,
	201, 202, 203, 204, 205, 206, 207, 209, 210, 211, 212, 213, 214, 215, 216,
	217, 218, 219, 220, 221, 221, 222, 223, 224, 225, 226, 227, 228, 229, 229,
	230, 231, 232, 233, 234, 235, 235, 236, 237
};

/*
=============
SV_AddBlend
[RH] This is from Q2.
=============
*/
void SV_AddBlend (float r, float g, float b, float a, float *v_blend)
{
	float a2, a3;

	if (a <= 0)
		return;
	a2 = v_blend[3] + (1-v_blend[3])*a;	// new total alpha
	a3 = v_blend[3]/a2;		// fraction of color from old

	v_blend[0] = v_blend[0]*a3 + r*(1-a3);
	v_blend[1] = v_blend[1]*a3 + g*(1-a3);
	v_blend[2] = v_blend[2]*a3 + b*(1-a3);
	v_blend[3] = a2;
}

void ST_doPaletteStuff (void)
{
	float blend[4];
	int cnt;

	blend[0] = blend[1] = blend[2] = blend[3] = 0;

	SV_AddBlend (BaseBlendR / 255.0f, BaseBlendG / 255.0f, BaseBlendB / 255.0f, BaseBlendA, blend);
	if (plyr->powers[pw_ironfeet] > 4*32 || plyr->powers[pw_ironfeet]&8)
		SV_AddBlend (0.0f, 1.0f, 0.0f, 0.125f, blend);
	if (plyr->bonuscount) {
		cnt = plyr->bonuscount << 3;
		SV_AddBlend (0.8431f, 0.7294f, 0.2706f, cnt > 128 ? 0.5f : cnt / 255.0f, blend);
	}

	if (plyr->damagecount < 114)
		cnt = damageToAlpha[plyr->damagecount];
	else
		cnt = damageToAlpha[113];

	if (plyr->powers[pw_strength])
	{
		// slowly fade the berzerk out
		int bzc = 128 - ((plyr->powers[pw_strength]>>3) & (~0x1f));

		if (bzc > cnt)
			cnt = bzc;
	}
		
	if (cnt)
	{
		if (cnt > 228)
			cnt = 228;

		SV_AddBlend (1.0f, 0.0f, 0.0f, cnt / 255.0f, blend);
	}

	if (memcmp (blend, st_palette, sizeof(blend))) {
		memcpy (st_palette, blend, sizeof(blend));
		V_SetBlend ((int)(blend[0] * 255.0f), (int)(blend[1] * 255.0f),
					(int)(blend[2] * 255.0f), (int)(blend[3] * 256.0f));
	}
}

void ST_drawWidgets(BOOL refresh)
{
	int i;

	// used by w_arms[] widgets
	st_armson = st_statusbaron && !((int)deathmatch.value);

	// used by w_frags widget
	st_fragson = (int)deathmatch.value && st_statusbaron; 

	STlib_updateNum (&w_ready, refresh);

	for (i = 0; i < 4; i++)
	{
		STlib_updateNum (&w_ammo[i], refresh);
		STlib_updateNum (&w_maxammo[i], refresh);
	}

	STlib_updatePercent (&w_health, refresh);
	STlib_updatePercent (&w_armor, refresh);

	STlib_updateBinIcon(&w_armsbg, refresh);

	for (i = 0; i < 6; i++)
		STlib_updateMultIcon (&w_arms[i], refresh);

	STlib_updateMultIcon (&w_faces, refresh);

	for (i = 0; i < 3; i++)
		STlib_updateMultIcon (&w_keyboxes[i], refresh);

	STlib_updateNum (&w_frags, refresh);

	if (st_scale.value && ST_WIDTH != 320 && st_statusbaron)
		stnumscreen->Blit (0, 0, 320, 32,
			FG, ST_X, ST_Y, ST_WIDTH, ST_HEIGHT);
}

void ST_doRefresh(void)
{

	st_firsttime = false;

	// draw status bar background to off-screen buff
	ST_refreshBackground();

	// and refresh all widgets
	ST_drawWidgets(true);

}

void ST_diffDraw(void)
{
	// update all widgets
	ST_drawWidgets(false);
}

void ST_Drawer (void)
{
	if (noisedebug.value)
		S_NoiseDebug ();

	if (demoplayback && demover != GAMEVER)
		screen->DrawTextClean (CR_TAN, 0, ST_Y - 40 * CleanYfac,
			"Demo was recorded with a different version\n"
			"of ZDoom. Expect it to go out of sync.");

//FIXME
//return;
	if (realviewheight == screen->height && viewactive)
	{
		if (DrawNewHUD)
			ST_newDraw ();
		SB_state = -1;
	}
	else
	{
		stbarscreen->Lock ();
		stnumscreen->Lock ();

		if (SB_state)
		{
			ST_doRefresh ();
			SB_state = 0;
		}
		else
		{
			ST_diffDraw ();
		}

		stnumscreen->Unlock ();
		stbarscreen->Unlock ();
	}

		
	if (viewheight <= ST_Y)
		ST_nameDraw (ST_Y - 11 * CleanYfac);
	else
		ST_nameDraw (screen->height - 11 * CleanYfac);

	// Do red-/gold-shifts from damage/items
	ST_doPaletteStuff();

	// [RH] Hey, it's somewhere to put the idmypos stuff!
	if (idmypos.value)
		Printf (PRINT_HIGH, "ang=%d;x,y=(%d,%d)\n",
				players[consoleplayer].camera->angle/FRACUNIT,
				players[consoleplayer].camera->x/FRACUNIT,
				players[consoleplayer].camera->y/FRACUNIT);
}

static patch_t *LoadFaceGraphic (char *name, int namespc)
{
	char othername[9];
	int lump;

	lump = W_CheckNumForName (name, namespc);
	if (lump == -1)
	{
		strcpy (othername, name);
		othername[0] = 'S'; othername[1] = 'T'; othername[2] = 'F';
		lump = W_GetNumForName (othername);
	}
	return (patch_t *)W_CacheLumpNum (lump, PU_STATIC);
}

void ST_loadGraphics(void)
{
	playerskin_t *skin;
	int i, j;
	int namespc;
	int facenum;
	char namebuf[9];

	namebuf[8] = 0;
	if (plyr)
		skin = &skins[plyr->userinfo.skin];
	else
		skin = &skins[players[consoleplayer].userinfo.skin];

	// Load the numbers, tall and short
	for (i=0;i<10;i++)
	{
		sprintf(namebuf, "STTNUM%d", i);
		tallnum[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);

		sprintf(namebuf, "STYSNUM%d", i);
		shortnum[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
	}

	// Load percent key.
	//Note: why not load STMINUS here, too?
	tallpercent = (patch_t *) W_CacheLumpName("STTPRCNT", PU_STATIC);

	// key cards
	for (i=0;i<NUMCARDS+NUMCARDS/2;i++)
	{
		sprintf(namebuf, "STKEYS%d", i);
		keys[i] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);
	}

	// arms background
	armsbg = (patch_t *) W_CacheLumpName("STARMS", PU_STATIC);

	// arms ownership widgets
	for (i=0;i<6;i++)
	{
		sprintf(namebuf, "STGNUM%d", i+2);

		// gray #
		arms[i][0] = (patch_t *) W_CacheLumpName(namebuf, PU_STATIC);

		// yellow #
		arms[i][1] = shortnum[i+2]; 
	}

	// face backgrounds for different color players
	// [RH] only one face background used for all players
	//		different colors are accomplished with translations
	faceback = (patch_t *) W_CacheLumpName("STFBANY", PU_STATIC);

	// status bar background bits
	sbar = (patch_t *) W_CacheLumpName("STBAR", PU_STATIC);

	// face states
	facenum = 0;

	// [RH] Use face specified by "skin"
	if (skin->face[0]) {
		// The skin has its own face
		strncpy (namebuf, skin->face, 3);
		namespc = skin->namespc;
	} else {
		// The skin doesn't have its own face; use the normal one
		namebuf[0] = 'S'; namebuf[1] = 'T'; namebuf[2] = 'F';
		namespc = ns_global;
	}

	for (i = 0; i < ST_NUMPAINFACES; i++)
	{
		for (j = 0; j < ST_NUMSTRAIGHTFACES; j++)
		{
			sprintf(namebuf+3, "ST%d%d", i, j);
			faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
		}
		sprintf(namebuf+3, "TR%d0", i);		// turn right
		faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
		sprintf(namebuf+3, "TL%d0", i);		// turn left
		faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
		sprintf(namebuf+3, "OUCH%d", i);		// ouch!
		faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
		sprintf(namebuf+3, "EVL%d", i);		// evil grin ;)
		faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
		sprintf(namebuf+3, "KILL%d", i);		// pissed off
		faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
	}
	strcpy (namebuf+3, "GOD0");
	faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
	strcpy (namebuf+3, "DEAD0");
	faces[facenum++] = LoadFaceGraphic (namebuf, namespc);
}

void ST_loadData (void)
{
	ST_loadGraphics();
}

void ST_unloadGraphics (void)
{

	int i;

	// unload the numbers, tall and short
	for (i=0;i<10;i++)
	{
		Z_ChangeTag(tallnum[i], PU_CACHE);
		Z_ChangeTag(shortnum[i], PU_CACHE);
	}
	// unload tall percent
	Z_ChangeTag(tallpercent, PU_CACHE); 

	// unload arms background
	Z_ChangeTag(armsbg, PU_CACHE); 

	// unload gray #'s
	for (i=0;i<6;i++)
		Z_ChangeTag(arms[i][0], PU_CACHE);
	
	// unload the key cards
	for (i=0;i<NUMCARDS+NUMCARDS/2;i++)
		Z_ChangeTag(keys[i], PU_CACHE);

	Z_ChangeTag(sbar, PU_CACHE);
	Z_ChangeTag(faceback, PU_CACHE);

	for (i=0;i<ST_NUMFACES;i++)
		Z_ChangeTag(faces[i], PU_CACHE);

	// Note: nobody ain't seen no unloading
	//	 of stminus yet. Dude.
	

}

void ST_unloadData(void)
{
	ST_unloadGraphics();
	ST_unloadNew();
}

void ST_initData(void)
{

	int i;

	st_firsttime = true;
	if (players[consoleplayer].camera && players[consoleplayer].camera->player)
		plyr = players[consoleplayer].camera->player;		// [RH] use camera
	else
		plyr = &players[consoleplayer];

	st_clock = 0;
	st_chatstate = StartChatState;
	st_gamestate = FirstPersonState;

	st_statusbaron = true;
	st_oldchat = st_chat = false;
	st_cursoron = false;

	st_faceindex = 0;
	memset (st_palette, 255, sizeof(st_palette));

	st_oldhealth = -1;

	for (i=0;i<NUMWEAPONS;i++)
		oldweaponsowned[i] = plyr->weaponowned[i];

	for (i=0;i<3;i++)
		keyboxes[i] = -1;

	STlib_init();
	ST_initNew();

}



void ST_createWidgets(void)
{

	int i;

	// ready weapon ammo
	STlib_initNum(&w_ready,
				  ST_AMMOX,
				  ST_AMMOY,
				  tallnum,
				  &plyr->ammo[weaponinfo[plyr->readyweapon].ammo],
				  &st_statusbaron,
				  ST_AMMOWIDTH );

	// the last weapon type
	w_ready.data = plyr->readyweapon; 

	// health percentage
	STlib_initPercent(&w_health,
					  ST_HEALTHX,
					  ST_HEALTHY,
					  tallnum,
					  &plyr->health,
					  &st_statusbaron,
					  tallpercent);

	// arms background
	STlib_initBinIcon(&w_armsbg,
					  ST_ARMSBGX,
					  ST_ARMSBGY,
					  armsbg,
					  &st_notdeathmatch,
					  &st_statusbaron);

	// weapons owned
	for(i=0;i<6;i++)
	{
		STlib_initMultIcon(&w_arms[i],
						   ST_ARMSX+(i%3)*ST_ARMSXSPACE,
						   ST_ARMSY+(i/3)*ST_ARMSYSPACE,
						   arms[i], (int *) &plyr->weaponowned[i+1],
						   &st_armson);
	}

	// frags sum
	STlib_initNum(&w_frags,
				  ST_FRAGSX,
				  ST_FRAGSY,
				  tallnum,
				  &st_fragscount,
				  &st_fragson,
				  ST_FRAGSWIDTH);

	// faces
	STlib_initMultIcon(&w_faces,
					   ST_FACESX,
					   ST_FACESY,
					   faces,
					   &st_faceindex,
					   &st_statusbaron);

	// armor percentage - should be colored later
	STlib_initPercent(&w_armor,
					  ST_ARMORX,
					  ST_ARMORY,
					  tallnum,
					  &plyr->armorpoints,
					  &st_statusbaron, tallpercent);

	// keyboxes 0-2
	STlib_initMultIcon(&w_keyboxes[0],
					   ST_KEY0X,
					   ST_KEY0Y,
					   keys,
					   &keyboxes[0],
					   &st_statusbaron);
	
	STlib_initMultIcon(&w_keyboxes[1],
					   ST_KEY1X,
					   ST_KEY1Y,
					   keys,
					   &keyboxes[1],
					   &st_statusbaron);

	STlib_initMultIcon(&w_keyboxes[2],
					   ST_KEY2X,
					   ST_KEY2Y,
					   keys,
					   &keyboxes[2],
					   &st_statusbaron);

	// ammo count (all four kinds)
	STlib_initNum(&w_ammo[0],
				  ST_AMMO0X,
				  ST_AMMO0Y,
				  shortnum,
				  &plyr->ammo[0],
				  &st_statusbaron,
				  ST_AMMO0WIDTH);

	STlib_initNum(&w_ammo[1],
				  ST_AMMO1X,
				  ST_AMMO1Y,
				  shortnum,
				  &plyr->ammo[1],
				  &st_statusbaron,
				  ST_AMMO1WIDTH);

	STlib_initNum(&w_ammo[2],
				  ST_AMMO2X,
				  ST_AMMO2Y,
				  shortnum,
				  &plyr->ammo[2],
				  &st_statusbaron,
				  ST_AMMO2WIDTH);
	
	STlib_initNum(&w_ammo[3],
				  ST_AMMO3X,
				  ST_AMMO3Y,
				  shortnum,
				  &plyr->ammo[3],
				  &st_statusbaron,
				  ST_AMMO3WIDTH);

	// max ammo count (all four kinds)
	STlib_initNum(&w_maxammo[0],
				  ST_MAXAMMO0X,
				  ST_MAXAMMO0Y,
				  shortnum,
				  &plyr->maxammo[0],
				  &st_statusbaron,
				  ST_MAXAMMO0WIDTH);

	STlib_initNum(&w_maxammo[1],
				  ST_MAXAMMO1X,
				  ST_MAXAMMO1Y,
				  shortnum,
				  &plyr->maxammo[1],
				  &st_statusbaron,
				  ST_MAXAMMO1WIDTH);

	STlib_initNum(&w_maxammo[2],
				  ST_MAXAMMO2X,
				  ST_MAXAMMO2Y,
				  shortnum,
				  &plyr->maxammo[2],
				  &st_statusbaron,
				  ST_MAXAMMO2WIDTH);
	
	STlib_initNum(&w_maxammo[3],
				  ST_MAXAMMO3X,
				  ST_MAXAMMO3Y,
				  shortnum,
				  &plyr->maxammo[3],
				  &st_statusbaron,
				  ST_MAXAMMO3WIDTH);

}

static BOOL	st_stopped = true;


void ST_Start (void)
{
// FIXME
//return;
	if (!st_stopped)
		ST_Stop();

	ST_initData();
	ST_createWidgets();
	st_stopped = false;
	SB_state = -1;
	st_scale.Set (st_scale.string);
}

void ST_Stop (void)
{
	if (st_stopped)
		return;

	V_SetBlend (0,0,0,0);

	st_stopped = true;
}

void ST_Init (void)
{
//FIXME
//return;
	veryfirsttime = 0;

	stbarscreen = new DCanvas (320, 32, 8);
	stnumscreen = new DCanvas (320, 32, 8);

	ST_loadData();
}
