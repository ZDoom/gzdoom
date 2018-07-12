//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <memory>
#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#endif

#include "i_time.h"
#include "templates.h"
#include "version.h"
#include "doomdef.h" 
#include "doomstat.h"
#include "d_protocol.h"
#include "d_netinf.h"
#include "intermission/intermission.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu/menu.h"
#include "m_crc32.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "c_console.h"
#include "c_bind.h"
#include "c_dispatch.h"
#include "w_wad.h"
#include "p_local.h" 
#include "gstrings.h"
#include "r_sky.h"
#include "g_game.h"
#include "sbar.h"
#include "m_png.h"
#include "a_keys.h"
#include "cmdlib.h"
#include "d_net.h"
#include "d_event.h"
#include "p_acs.h"
#include "p_effect.h"
#include "m_joy.h"
#include "r_renderer.h"
#include "r_utility.h"
#include "a_morph.h"
#include "p_spec.h"
#include "serializer.h"
#include "vm.h"

#include "g_hub.h"
#include "g_levellocals.h"
#include "events.h"


static FRandom pr_dmspawn ("DMSpawn");
static FRandom pr_pspawn ("PlayerSpawn");

bool	G_CheckDemoStatus (void);
void	G_ReadDemoTiccmd (ticcmd_t *cmd, int player);
void	G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf);
void	G_PlayerReborn (int player);

void	G_DoNewGame (void);
void	G_DoLoadGame (void);
void	G_DoPlayDemo (void);
void	G_DoCompleted (void);
void	G_DoVictory (void);
void	G_DoWorldDone (void);
void	G_DoSaveGame (bool okForQuicksave, FString filename, const char *description);
void	G_DoAutoSave ();

void STAT_Serialize(FSerializer &file);
bool WriteZip(const char *filename, TArray<FString> &filenames, TArray<FCompressedBuffer> &content);

FIntCVar gameskill ("skill", 2, CVAR_SERVERINFO|CVAR_LATCH);
CVAR(Bool, save_formatted, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)	// use formatted JSON for saves (more readable but a larger files and a bit slower.
CVAR (Int, deathmatch, 0, CVAR_SERVERINFO|CVAR_LATCH);
CVAR (Bool, chasedemo, false, 0);
CVAR (Bool, storesavepic, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, longsavemessages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, save_dir, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
CVAR (Bool, cl_waitforsave, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
EXTERN_CVAR (Float, con_midtime);

//==========================================================================
//
// CVAR displaynametags
//
// Selects whether to display name tags or not when changing weapons/items
//
//==========================================================================

CUSTOM_CVAR (Int, displaynametags, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 3)
	{
		self = 0;
	}
}

CVAR(Int, nametagcolor, CR_GOLD, CVAR_ARCHIVE)


gameaction_t	gameaction;
gamestate_t 	gamestate = GS_STARTUP;
FName			SelectedSlideshow;		// what to start when ga_slideshow

int 			paused;
bool			pauseext;
bool 			sendpause;				// send a pause event next tic 
bool			sendsave;				// send a save event next tic 
bool			sendturn180;			// [RH] send a 180 degree turn next tic
bool 			usergame;				// ok to save / end game
bool			insave;					// Game is saving - used to block exit commands

bool			timingdemo; 			// if true, exit with report on completion 
bool 			nodrawers;				// for comparative timing purposes 
bool 			noblit; 				// for comparative timing purposes 

bool	 		viewactive;

bool 			netgame;				// only true if packets are broadcast 
bool			multiplayer;
bool			multiplayernext = false;		// [SP] Map coop/dm implementation
player_t		players[MAXPLAYERS];
bool			playeringame[MAXPLAYERS];

int 			consoleplayer;			// player taking events
int 			gametic;

CVAR(Bool, demo_compress, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG);
FString			newdemoname;
FString			newdemomap;
FString			demoname;
bool 			demorecording;
bool 			demoplayback;
bool			demonew;				// [RH] Only used around G_InitNew for demos
int				demover;
uint8_t*			demobuffer;
uint8_t*			demo_p;
uint8_t*			democompspot;
uint8_t*			demobodyspot;
size_t			maxdemosize;
uint8_t*			zdemformend;			// end of FORM ZDEM chunk
uint8_t*			zdembodyend;			// end of ZDEM BODY chunk
bool 			singledemo; 			// quit after playing a demo from cmdline 
 
bool 			precache = true;		// if true, load all graphics at start 
 
wbstartstruct_t wminfo; 				// parms for world map / intermission 
 
short			consistancy[MAXPLAYERS][BACKUPTICS];
 
 
#define MAXPLMOVE				(forwardmove[1]) 
 
#define TURBOTHRESHOLD	12800

float	 		normforwardmove[2] = {0x19, 0x32};		// [RH] For setting turbo from console
float	 		normsidemove[2] = {0x18, 0x28};			// [RH] Ditto

int				forwardmove[2], sidemove[2];
int		 		angleturn[4] = {640, 1280, 320, 320};		// + slow turn
int				flyspeed[2] = {1*256, 3*256};
int				lookspeed[2] = {450, 512};

#define SLOWTURNTICS	6 

CVAR (Bool,		cl_run,			false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always run?
CVAR (Bool,		invertmouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Invert mouse look down/up?
CVAR (Bool,		freelook,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always mlook?
CVAR (Bool,		lookstrafe,		false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Always strafe with mouse?
CVAR (Float,	m_pitch,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)		// Mouse speeds
CVAR (Float,	m_yaw,			1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_forward,		1.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
CVAR (Float,	m_side,			2.f,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)
 
int 			turnheld;								// for accelerative turning 
 
// mouse values are used once 
int 			mousex;
int 			mousey; 		

FString			savegamefile;
FString			savedescription;

// [RH] Name of screenshot file to generate (usually NULL)
FString			shotfile;

AActor* 		bodyque[BODYQUESIZE]; 
int 			bodyqueslot; 

FString savename;
FString BackupSaveName;

bool SendLand;
const AInventory *SendItemUse, *SendItemDrop;
int SendItemDropAmount;

EXTERN_CVAR (Int, team)

CVAR (Bool, teamplay, false, CVAR_SERVERINFO)

// [RH] Allow turbo setting anytime during game
CUSTOM_CVAR (Float, turbo, 100.f, 0)
{
	if (self < 10.f)
	{
		self = 10.f;
	}
	else if (self > 255.f)
	{
		self = 255.f;
	}
	else
	{
		double scale = self * 0.01;

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
		Printf ("Current turn speeds: %d %d %d %d\n", angleturn[0],
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
		int slot = atoi (argv[1]);

		if (slot < NUM_WEAPON_SLOTS)
		{
			SendItemUse = players[consoleplayer].weapons.Slots[slot].PickWeapon (&players[consoleplayer], 
				!(dmflags2 & DF2_DONTCHECKAMMO));
		}
	}
}

CCMD (centerview)
{
	Net_WriteByte (DEM_CENTERVIEW);
}

CCMD(crouch)
{
	Net_WriteByte(DEM_CROUCH);
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
	SendItemUse = players[consoleplayer].weapons.PickNextWeapon (&players[consoleplayer]);
	// [BC] Option to display the name of the weapon being cycled to.
	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
	{
		StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(SmallFont, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
	}
	if (SendItemUse != players[consoleplayer].ReadyWeapon)
	{
		S_Sound(CHAN_AUTO, "misc/weaponchange", 1.0, ATTN_NONE);
	}
}

CCMD (weapprev)
{
	SendItemUse = players[consoleplayer].weapons.PickPrevWeapon (&players[consoleplayer]);
	// [BC] Option to display the name of the weapon being cycled to.
	if ((displaynametags & 2) && StatusBar && SmallFont && SendItemUse)
	{
		StatusBar->AttachMessage(Create<DHUDMessageFadeOut>(SmallFont, SendItemUse->GetTag(),
			1.5f, 0.90f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID( 'W', 'E', 'P', 'N' ));
	}
	if (SendItemUse != players[consoleplayer].ReadyWeapon)
	{
		S_Sound(CHAN_AUTO, "misc/weaponchange", 1.0, ATTN_NONE);
	}
}

CCMD (invnext)
{
	AInventory *next;

	if (who == NULL)
		return;

	auto old = who->InvSel;
	if (who->InvSel != NULL)
	{
		if ((next = who->InvSel->NextInv()) != NULL)
		{
			who->InvSel = next;
		}
		else
		{
			// Select the first item in the inventory
			if (!(who->Inventory->ItemFlags & IF_INVBAR))
			{
				who->InvSel = who->Inventory->NextInv();
			}
			else
			{
				who->InvSel = who->Inventory;
			}
		}
		if ((displaynametags & 1) && StatusBar && SmallFont && who->InvSel)
			StatusBar->AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, who->InvSel->GetTag(),
			1.5f, 0.80f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('S','I','N','V'));
	}
	who->player->inventorytics = 5*TICRATE;
	if (old != who->InvSel)
	{
		S_Sound(CHAN_AUTO, "misc/invchange", 1.0, ATTN_NONE);
	}
}

CCMD (invprev)
{
	AInventory *item, *newitem;

	if (who == NULL)
		return;

	auto old = who->InvSel;
	if (who->InvSel != NULL)
	{
		if ((item = who->InvSel->PrevInv()) != NULL)
		{
			who->InvSel = item;
		}
		else
		{
			// Select the last item in the inventory
			item = who->InvSel;
			while ((newitem = item->NextInv()) != NULL)
			{
				item = newitem;
			}
			who->InvSel = item;
		}
		if ((displaynametags & 1) && StatusBar && SmallFont && who->InvSel)
			StatusBar->AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, who->InvSel->GetTag(),
			1.5f, 0.80f, 0, 0, (EColorRange)*nametagcolor, 2.f, 0.35f), MAKE_ID('S','I','N','V'));
	}
	who->player->inventorytics = 5*TICRATE;
	if (old != who->InvSel)
	{
		S_Sound(CHAN_AUTO, "misc/invchange", 1.0, ATTN_NONE);
	}
}

CCMD (invuseall)
{
	SendItemUse = (const AInventory *)1;
}

CCMD (invuse)
{
	if (players[consoleplayer].inventorytics == 0)
	{
		if (players[consoleplayer].mo) SendItemUse = players[consoleplayer].mo->InvSel;
	}
	players[consoleplayer].inventorytics = 0;
}

CCMD(invquery)
{
	AInventory *inv = players[consoleplayer].mo->InvSel;
	if (inv != NULL)
	{
		Printf(PRINT_HIGH, "%s (%dx)\n", inv->GetTag(), inv->Amount);
	}
}

CCMD (use)
{
	if (argv.argc() > 1 && who != NULL)
	{
		SendItemUse = who->FindInventory(argv[1]);
	}
}

CCMD (invdrop)
{
	if (players[consoleplayer].mo)
	{
		SendItemDrop = players[consoleplayer].mo->InvSel;
		SendItemDropAmount = -1;
	}
}

CCMD (weapdrop)
{
	SendItemDrop = players[consoleplayer].ReadyWeapon;
	SendItemDropAmount = -1;
}

CCMD (drop)
{
	if (argv.argc() > 1 && who != NULL)
	{
		SendItemDrop = who->FindInventory(argv[1]);
		SendItemDropAmount = argv.argc() > 2 ? atoi(argv[2]) : -1;
	}
}

PClassActor *GetFlechetteType(AActor *other);

CCMD (useflechette)
{ // Select from one of arti_poisonbag1-3, whichever the player has
	static const ENamedName bagnames[3] =
	{
		NAME_ArtiPoisonBag3,	// use type 3 first because that's the default when the player has none specified.
		NAME_ArtiPoisonBag1,
		NAME_ArtiPoisonBag2
	};

	if (who == NULL)
		return;

	PClassActor *type = who->FlechetteType;
	if (type != NULL)
	{
		AInventory *item;
		if ( (item = who->FindInventory (type) ))
		{
			SendItemUse = item;
			return;
		}
	}

	// The default flechette could not be found, or the player had no default. Try all 3 types then.
	for (int j = 0; j < 3; ++j)
	{
		AInventory *item;
		if ( (item = who->FindInventory (bagnames[j])) )
		{
			SendItemUse = item;
			break;
		}
	}
}

CCMD (select)
{
	if (argv.argc() > 1)
	{
		AInventory *item = who->FindInventory(argv[1]);
		if (item != NULL)
		{
			who->InvSel = item;
		}
	}
	who->player->inventorytics = 5*TICRATE;
}

static inline int joyint(double val)
{
	if (val >= 0)
	{
		return int(ceil(val));
	}
	else
	{
		return int(floor(val));
	}
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
	int			fly;

	ticcmd_t	*base;

	base = I_BaseTiccmd (); 			// empty, or external driver
	*cmd = *base;

	cmd->consistancy = consistancy[consoleplayer][(maketic/ticdup)%BACKUPTICS];

	strafe = Button_Strafe.bDown;
	speed = Button_Speed.bDown ^ (int)cl_run;

	forward = side = fly = 0;

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
			tspeed += 2;		// slow turn
		
		if (Button_Right.bDown)
		{
			G_AddViewAngle (angleturn[tspeed]);
		}
		if (Button_Left.bDown)
		{
			G_AddViewAngle (-angleturn[tspeed]);
		}
	}

	if (Button_LookUp.bDown)
	{
		G_AddViewPitch (lookspeed[speed]);
	}
	if (Button_LookDown.bDown)
	{
		G_AddViewPitch (-lookspeed[speed]);
	}

	if (Button_MoveUp.bDown)
		fly += flyspeed[speed];
	if (Button_MoveDown.bDown)
		fly -= flyspeed[speed];

	if (Button_Klook.bDown)
	{
		if (Button_Forward.bDown)
			G_AddViewPitch (lookspeed[speed]);
		if (Button_Back.bDown)
			G_AddViewPitch (-lookspeed[speed]);
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
	if (Button_Attack.bDown)		cmd->ucmd.buttons |= BT_ATTACK;
	if (Button_AltAttack.bDown)		cmd->ucmd.buttons |= BT_ALTATTACK;
	if (Button_Use.bDown)			cmd->ucmd.buttons |= BT_USE;
	if (Button_Jump.bDown)			cmd->ucmd.buttons |= BT_JUMP;
	if (Button_Crouch.bDown)		cmd->ucmd.buttons |= BT_CROUCH;
	if (Button_Zoom.bDown)			cmd->ucmd.buttons |= BT_ZOOM;
	if (Button_Reload.bDown)		cmd->ucmd.buttons |= BT_RELOAD;

	if (Button_User1.bDown)			cmd->ucmd.buttons |= BT_USER1;
	if (Button_User2.bDown)			cmd->ucmd.buttons |= BT_USER2;
	if (Button_User3.bDown)			cmd->ucmd.buttons |= BT_USER3;
	if (Button_User4.bDown)			cmd->ucmd.buttons |= BT_USER4;

	if (Button_Speed.bDown)			cmd->ucmd.buttons |= BT_SPEED;
	if (Button_Strafe.bDown)		cmd->ucmd.buttons |= BT_STRAFE;
	if (Button_MoveRight.bDown)		cmd->ucmd.buttons |= BT_MOVERIGHT;
	if (Button_MoveLeft.bDown)		cmd->ucmd.buttons |= BT_MOVELEFT;
	if (Button_LookDown.bDown)		cmd->ucmd.buttons |= BT_LOOKDOWN;
	if (Button_LookUp.bDown)		cmd->ucmd.buttons |= BT_LOOKUP;
	if (Button_Back.bDown)			cmd->ucmd.buttons |= BT_BACK;
	if (Button_Forward.bDown)		cmd->ucmd.buttons |= BT_FORWARD;
	if (Button_Right.bDown)			cmd->ucmd.buttons |= BT_RIGHT;
	if (Button_Left.bDown)			cmd->ucmd.buttons |= BT_LEFT;
	if (Button_MoveDown.bDown)		cmd->ucmd.buttons |= BT_MOVEDOWN;
	if (Button_MoveUp.bDown)		cmd->ucmd.buttons |= BT_MOVEUP;
	if (Button_ShowScores.bDown)	cmd->ucmd.buttons |= BT_SHOWSCORES;

	// Handle joysticks/game controllers.
	float joyaxes[NUM_JOYAXIS];

	I_GetAxes(joyaxes);

	// Remap some axes depending on button state.
	if (Button_Strafe.bDown || (Button_Mlook.bDown && lookstrafe))
	{
		joyaxes[JOYAXIS_Side] = joyaxes[JOYAXIS_Yaw];
		joyaxes[JOYAXIS_Yaw] = 0;
	}
	if (Button_Mlook.bDown)
	{
		joyaxes[JOYAXIS_Pitch] = joyaxes[JOYAXIS_Forward];
		joyaxes[JOYAXIS_Forward] = 0;
	}

	if (joyaxes[JOYAXIS_Pitch] != 0)
	{
		G_AddViewPitch(joyint(joyaxes[JOYAXIS_Pitch] * 2048));
	}
	if (joyaxes[JOYAXIS_Yaw] != 0)
	{
		G_AddViewAngle(joyint(-1280 * joyaxes[JOYAXIS_Yaw]));
	}

	side -= joyint(sidemove[speed] * joyaxes[JOYAXIS_Side]);
	forward += joyint(joyaxes[JOYAXIS_Forward] * forwardmove[speed]);
	fly += joyint(joyaxes[JOYAXIS_Up] * 2048);

	// Handle mice.
	if (!Button_Mlook.bDown && !freelook)
	{
		forward += (int)((float)mousey * m_forward);
	}

	cmd->ucmd.pitch = LocalViewPitch >> 16;

	if (SendLand)
	{
		SendLand = false;
		fly = -32768;
	}

	if (strafe || lookstrafe)
		side += (int)((float)mousex * m_side);

	mousex = mousey = 0;

	// Build command.
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
	cmd->ucmd.yaw = LocalViewAngle >> 16;
	cmd->ucmd.upmove = fly;
	LocalViewAngle = 0;
	LocalViewPitch = 0;

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
		savegamefile = "";
	}
	if (SendItemUse == (const AInventory *)1)
	{
		Net_WriteByte (DEM_INVUSEALL);
		SendItemUse = NULL;
	}
	else if (SendItemUse != NULL)
	{
		Net_WriteByte (DEM_INVUSE);
		Net_WriteLong (SendItemUse->InventoryID);
		SendItemUse = NULL;
	}
	if (SendItemDrop != NULL)
	{
		Net_WriteByte (DEM_INVDROP);
		Net_WriteLong (SendItemDrop->InventoryID);
		Net_WriteLong(SendItemDropAmount);
		SendItemDrop = NULL;
	}

	cmd->ucmd.forwardmove <<= 8;
	cmd->ucmd.sidemove <<= 8;
}

//[Graf Zahl] This really helps if the mouse update rate can't be increased!
CVAR (Bool,		smooth_mouse,	false,	CVAR_GLOBALCONFIG|CVAR_ARCHIVE)

void G_AddViewPitch (int look, bool mouse)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	look <<= 16;
	if (players[consoleplayer].playerstate != PST_DEAD &&		// No adjustment while dead.
		players[consoleplayer].ReadyWeapon != NULL &&			// No adjustment if no weapon.
		players[consoleplayer].ReadyWeapon->FOVScale > 0)		// No adjustment if it is non-positive.
	{
		look = int(look * players[consoleplayer].ReadyWeapon->FOVScale);
	}
	if (!level.IsFreelookAllowed())
	{
		LocalViewPitch = 0;
	}
	else if (look > 0)
	{
		// Avoid overflowing
		if (LocalViewPitch > INT_MAX - look)
		{
			LocalViewPitch = 0x78000000;
		}
		else
		{
			LocalViewPitch = MIN(LocalViewPitch + look, 0x78000000);
		}
	}
	else if (look < 0)
	{
		// Avoid overflowing
		if (LocalViewPitch < INT_MIN - look)
		{
			LocalViewPitch = -0x78000000;
		}
		else
		{
			LocalViewPitch = MAX(LocalViewPitch + look, -0x78000000);
		}
	}
	if (look != 0)
	{
		LocalKeyboardTurner = (!mouse || smooth_mouse);
	}
}

void G_AddViewAngle (int yaw, bool mouse)
{
	if (gamestate == GS_TITLELEVEL)
	{
		return;
	}
	yaw <<= 16;
	if (players[consoleplayer].playerstate != PST_DEAD &&	// No adjustment while dead.
		players[consoleplayer].ReadyWeapon != NULL &&		// No adjustment if no weapon.
		players[consoleplayer].ReadyWeapon->FOVScale > 0)	// No adjustment if it is non-positive.
	{
		yaw = int(yaw * players[consoleplayer].ReadyWeapon->FOVScale);
	}
	LocalViewAngle -= yaw;
	if (yaw != 0)
	{
		LocalKeyboardTurner = (!mouse || smooth_mouse);
	}
}

CVAR (Bool, bot_allowspy, false, 0)


enum {
	SPY_CANCEL = 0,
	SPY_NEXT,
	SPY_PREV,
};

// [RH] Spy mode has been separated into two console commands.
//		One goes forward; the other goes backward.
static void ChangeSpy (int changespy)
{
	// If you're not in a level, then you can't spy.
	if (gamestate != GS_LEVEL)
	{
		return;
	}

	// If not viewing through a player, return your eyes to your own head.
	if (players[consoleplayer].camera->player == NULL)
	{
		// When watching demos, you will just have to wait until your player
		// has done this for you, since it could desync otherwise.
		if (!demoplayback)
		{
			Net_WriteByte(DEM_REVERTCAMERA);
		}
		return;
	}

	// We may not be allowed to spy on anyone.
	if (dmflags2 & DF2_DISALLOW_SPYING)
		return;

	// Otherwise, cycle to the next player.
	bool checkTeam = !demoplayback && deathmatch;
	int pnum = consoleplayer;
	if (changespy != SPY_CANCEL) 
	{
		player_t *player = players[consoleplayer].camera->player;
		// only use the camera as starting index if it's a valid player.
		if (player != NULL) pnum = int(players[consoleplayer].camera->player - players);

		int step = (changespy == SPY_NEXT) ? 1 : -1;

		do
		{
			pnum += step;
			pnum &= MAXPLAYERS-1;
			if (playeringame[pnum] &&
				(!checkTeam || players[pnum].mo->IsTeammate (players[consoleplayer].mo) ||
				(bot_allowspy && players[pnum].Bot != NULL)))
			{
				break;
			}
		} while (pnum != consoleplayer);
	}

	players[consoleplayer].camera = players[pnum].mo;
	S_UpdateSounds(players[consoleplayer].camera);
	StatusBar->AttachToPlayer (&players[pnum]);
	if (demoplayback || multiplayer)
	{
		StatusBar->ShowPlayerName ();
	}
}

CCMD (spynext)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_NEXT);
}

CCMD (spyprev)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_PREV);
}

CCMD (spycancel)
{
	// allow spy mode changes even during the demo
	ChangeSpy (SPY_CANCEL);
}

//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder (event_t *ev)
{
	// any other key pops up menu if in demos
	// [RH] But only if the key isn't bound to a "special" command
	if (gameaction == ga_nothing && 
		(demoplayback || gamestate == GS_DEMOSCREEN || gamestate == GS_TITLELEVEL))
	{
		const char *cmd = Bindings.GetBind (ev->data1);

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
				M_StartControlPanel(true);
				M_SetMenu(NAME_Mainmenu, -1);
				return true;
			}
			else
			{
				return C_DoKey (ev, &Bindings, &DoubleBindings);
			}
		}
		if (cmd && cmd[0] == '+')
			return C_DoKey (ev, &Bindings, &DoubleBindings);

		return false;
	}

	if (CT_Responder (ev))
		return true;			// chat ate the event

	if (gamestate == GS_LEVEL)
	{
		if (ST_Responder (ev))
			return true;		// status window ate it
		if (!viewactive && AM_Responder (ev, false))
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
		if (C_DoKey (ev, &Bindings, &DoubleBindings))
			return true;
		break;

	case EV_KeyUp:
		C_DoKey (ev, &Bindings, &DoubleBindings);
		break;

	// [RH] mouse buttons are sent as key up/down events
	case EV_Mouse: 
		mousex = (int)(ev->x * mouse_sensitivity);
		mousey = (int)(ev->y * mouse_sensitivity);
		break;
	}

	// [RH] If the view is active, give the automap a chance at
	// the events *last* so that any bound keys get precedence.

	if (gamestate == GS_LEVEL && viewactive)
		return AM_Responder (ev, true);

	return (ev->type == EV_KeyDown ||
			ev->type == EV_Mouse);
}



//
// G_Ticker
// Make ticcmd_ts for the players.
//
extern FTexture *Page;


void G_Ticker ()
{
	int i;
	gamestate_t	oldgamestate;

	// do player reborns if needed
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			if (players[i].playerstate == PST_GONE)
			{
				G_DoPlayerPop(i);
			}
			if (players[i].playerstate == PST_REBORN || players[i].playerstate == PST_ENTER)
			{
				G_DoReborn(i, false);
			}
		}
	}

	if (ToggleFullscreen)
	{
		static char toggle_fullscreen[] = "toggle fullscreen";
		ToggleFullscreen = false;
		AddCommandString (toggle_fullscreen);
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
			G_DoLoadLevel (-1, false);
			break;
		case ga_recordgame:
			G_CheckDemoStatus();
			G_RecordDemo(newdemoname);
			G_BeginRecording(newdemomap);
		case ga_newgame2:	// Silence GCC (see above)
		case ga_newgame:
			G_DoNewGame ();
			break;
		case ga_loadgame:
		case ga_loadgamehidecon:
		case ga_autoloadgame:
			G_DoLoadGame ();
			break;
		case ga_savegame:
			G_DoSaveGame (true, savegamefile, savedescription);
			gameaction = ga_nothing;
			savegamefile = "";
			savedescription = "";
			break;
		case ga_autosave:
			G_DoAutoSave ();
			gameaction = ga_nothing;
			break;
		case ga_loadgameplaydemo:
			G_DoLoadGame ();
			// fallthrough
		case ga_playdemo:
			G_DoPlayDemo ();
			break;
		case ga_completed:
			G_DoCompleted ();
			break;
		case ga_slideshow:
			if (gamestate == GS_LEVEL) F_StartIntermission(SelectedSlideshow, FSTATE_InLevel);
			break;
		case ga_worlddone:
			G_DoWorldDone ();
			break;
		case ga_screenshot:
			M_ScreenShot (shotfile);
			shotfile = "";
			gameaction = ga_nothing;
			break;
		case ga_fullconsole:
			C_FullConsole ();
			gameaction = ga_nothing;
			break;
		case ga_togglemap:
			AM_ToggleMap ();
			gameaction = ga_nothing;
			break;
		case ga_nothing:
			break;
		}
		C_AdjustBottom ();
	}

	if (oldgamestate != gamestate)
	{
		if (oldgamestate == GS_DEMOSCREEN && Page != NULL)
		{
			Page->Unload();
			Page = NULL;
		}
		else if (oldgamestate == GS_FINALE)
		{
			F_EndFinale ();
		}
	}

	// get commands, check consistancy, and build new consistancy check
	int buf = (gametic/ticdup)%BACKUPTICS;

	// [RH] Include some random seeds and player stuff in the consistancy
	// check, not just the player's x position like BOOM.
	uint32_t rngsum = FRandom::StaticSumSeeds ();

	//Added by MC: For some of that bot stuff. The main bot function.
	bglobal.Main ();

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			ticcmd_t *cmd = &players[i].cmd;
			ticcmd_t *newcmd = &netcmds[i][buf];

			if ((gametic % ticdup) == 0)
			{
				RunNetSpecs (i, buf);
			}
			if (demorecording)
			{
				G_WriteDemoTiccmd (newcmd, i, buf);
			}
			players[i].oldbuttons = cmd->ucmd.buttons;
			// If the user alt-tabbed away, paused gets set to -1. In this case,
			// we do not want to read more demo commands until paused is no
			// longer negative.
			if (demoplayback)
			{
				G_ReadDemoTiccmd (cmd, i);
			}
			else
			{
				memcpy(cmd, newcmd, sizeof(ticcmd_t));
			}

			// check for turbo cheats
			if (multiplayer && turbo > 100.f && cmd->ucmd.forwardmove > TURBOTHRESHOLD &&
				!(gametic&31) && ((gametic>>5)&(MAXPLAYERS-1)) == i )
			{
				Printf ("%s is turbo!\n", players[i].userinfo.GetName());
			}

			if (netgame && players[i].Bot == NULL && !demoplayback && (gametic%ticdup) == 0)
			{
				//players[i].inconsistant = 0;
				if (gametic > BACKUPTICS*ticdup && consistancy[i][buf] != cmd->consistancy)
				{
					players[i].inconsistant = gametic - BACKUPTICS*ticdup;
				}
				if (players[i].mo)
				{
					uint32_t sum = rngsum + int((players[i].mo->X() + players[i].mo->Y() + players[i].mo->Z())*257) + players[i].mo->Angles.Yaw.BAMs() + players[i].mo->Angles.Pitch.BAMs();
					sum ^= players[i].health;
					consistancy[i][buf] = sum;
				}
				else
				{
					consistancy[i][buf] = rngsum;
				}
			}
		}
	}

	// [ZZ] also tick the UI part of the events
	E_UiTick();

	// do main actions
	switch (gamestate)
	{
	case GS_LEVEL:
		P_Ticker ();
		AM_Ticker ();
		break;

	case GS_TITLELEVEL:
		P_Ticker ();
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

	case GS_STARTUP:
		if (gameaction == ga_nothing)
		{
			gamestate = GS_FULLCONSOLE;
			gameaction = ga_fullconsole;
		}
		break;

	default:
		break;
	}

	// [MK] Additional ticker for UI events right after all others
	E_PostUiTick();
}


//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Mobj
//

//
// G_PlayerFinishLevel
// Called when a player completes a level.
//
// flags is checked for RESETINVENTORY and RESETHEALTH only.

void G_PlayerFinishLevel (int player, EFinishLevelType mode, int flags)
{
	AInventory *item, *next;
	player_t *p;

	p = &players[player];

	if (p->morphTics != 0)
	{ // Undo morph
		P_UndoPlayerMorph (p, p, 0, true);
	}

	// Strip all current powers, unless moving in a hub and the power is okay to keep.
	item = p->mo->Inventory;
	auto ptype = PClass::FindActor(NAME_Powerup);
	while (item != NULL)
	{
		next = item->Inventory;
		if (item->IsKindOf (ptype))
		{
			if (deathmatch || ((mode != FINISH_SameHub || !(item->ItemFlags & IF_HUBPOWER))
				&& !(item->ItemFlags & IF_PERSISTENTPOWER))) // Keep persistent powers in non-deathmatch games
			{
				item->Destroy ();
			}
		}
		item = next;
	}
	if (p->ReadyWeapon != NULL &&
		p->ReadyWeapon->WeaponFlags&WIF_POWERED_UP &&
		p->PendingWeapon == p->ReadyWeapon->SisterWeapon)
	{
		// Unselect powered up weapons if the unpowered counterpart is pending
		p->ReadyWeapon=p->PendingWeapon;
	}
	// reset invisibility to default
	if (p->mo->GetDefault()->flags & MF_SHADOW)
	{
		p->mo->flags |= MF_SHADOW;
	}
	else
	{
		p->mo->flags &= ~MF_SHADOW;
	}
	p->mo->RenderStyle = p->mo->GetDefault()->RenderStyle;
	p->mo->Alpha = p->mo->GetDefault()->Alpha;
	p->extralight = 0;					// cancel gun flashes
	p->fixedcolormap = NOFIXEDCOLORMAP;	// cancel ir goggles
	p->fixedlightlevel = -1;
	p->damagecount = 0; 				// no palette changes
	p->bonuscount = 0;
	p->poisoncount = 0;
	p->inventorytics = 0;

	if (mode != FINISH_SameHub)
	{
		// Take away flight and keys (and anything else with IF_INTERHUBSTRIP set)
		item = p->mo->Inventory;
		while (item != NULL)
		{
			next = item->Inventory;
			if (item->InterHubAmount < 1)
			{
				item->Destroy ();
			}
			item = next;
		}
	}

	if (mode == FINISH_NoHub && !(level.flags2 & LEVEL2_KEEPFULLINVENTORY))
	{ // Reduce all owned (visible) inventory to defined maximum interhub amount
		TArray<AInventory*> todelete;
		for (item = p->mo->Inventory; item != NULL; item = item->Inventory)
		{
			// If the player is carrying more samples of an item than allowed, reduce amount accordingly
			if (item->ItemFlags & IF_INVBAR && item->Amount > item->InterHubAmount)
			{
				item->Amount = item->InterHubAmount;
				if ((level.flags3 & LEVEL3_REMOVEITEMS) && !(item->ItemFlags & IF_UNDROPPABLE))
				{
					todelete.Push(item);
				}
			}
		}
		for (auto it : todelete)
		{
			if (!(it->ObjectFlags & OF_EuthanizeMe))
			{
				it->DepleteOrDestroy();
			}
		}
	}

	// Resets player health to default if not dead.
	if ((flags & CHANGELEVEL_RESETHEALTH) && p->playerstate != PST_DEAD)
	{
		p->health = p->mo->health = p->mo->SpawnHealth();
	}

	// Clears the entire inventory and gives back the defaults for starting a game
	if ((flags & CHANGELEVEL_RESETINVENTORY) && p->playerstate != PST_DEAD)
	{
		p->mo->ClearInventory();
		p->mo->GiveDefaultInventory();
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
	int 		frags[MAXPLAYERS];
	int			fragcount;	// [RH] Cumulative frags
	int 		killcount;
	int 		itemcount;
	int 		secretcount;
	int			chasecam;
	uint8_t		currclass;
	userinfo_t  userinfo;	// [RH] Save userinfo
	APlayerPawn *actor;
	PClassActor *cls;
	FString		log;
	DBot		*Bot;		//Added by MC:

	p = &players[player];

	memcpy (frags, p->frags, sizeof(frags));
	fragcount = p->fragcount;
	killcount = p->killcount;
	itemcount = p->itemcount;
	secretcount = p->secretcount;
	currclass = p->CurrentPlayerClass;
	userinfo.TransferFrom(p->userinfo);
	actor = p->mo;
	cls = p->cls;
	log = p->LogText;
	chasecam = p->cheats & CF_CHASECAM;
	Bot = p->Bot;			//Added by MC:

	// Reset player structure to its defaults
	p->~player_t();
	::new(p) player_t;

	memcpy (p->frags, frags, sizeof(p->frags));
	p->health = actor->health;
	p->fragcount = fragcount;
	p->killcount = killcount;
	p->itemcount = itemcount;
	p->secretcount = secretcount;
	p->CurrentPlayerClass = currclass;
	p->userinfo.TransferFrom(userinfo);
	p->mo = actor;
	p->cls = cls;
	p->LogText = log;
	p->cheats |= chasecam;
	p->Bot = Bot;			//Added by MC:

	p->oldbuttons = ~0, p->attackdown = true; p->usedown = true;	// don't do anything immediately
	p->original_oldbuttons = ~0;
	p->playerstate = PST_LIVE;

	if (gamestate != GS_TITLELEVEL)
	{
		// [GRB] Give inventory specified in DECORATE
		actor->GiveDefaultInventory ();
		p->ReadyWeapon = p->PendingWeapon;
	}

	//Added by MC: Init bot structure.
	if (p->Bot != NULL)
	{
		botskill_t skill = p->Bot->skill;
		p->Bot->Clear ();
		p->Bot->player = p;
		p->Bot->skill = skill;
	}
}

//
// G_CheckSpot	
// Returns false if the player cannot be respawned
// at the given mapthing spot  
// because something is occupying it 
//

bool G_CheckSpot (int playernum, FPlayerStart *mthing)
{
	DVector3 spot;
	double oldz;
	int i;

	if (mthing->type == 0) return false;

	spot = mthing->pos;

	if (!(level.flags & LEVEL_USEPLAYERSTARTZ))
	{
		spot.Z = 0;
	}
	spot.Z += P_PointInSector (spot)->floorplane.ZatPoint (spot);

	if (!players[playernum].mo)
	{ // first spawn of level, before corpses
		for (i = 0; i < playernum; i++)
			if (players[i].mo && players[i].mo->X() == spot.X && players[i].mo->Y() == spot.Y)
				return false;
		return true;
	}

	oldz = players[playernum].mo->Z();	// [RH] Need to save corpse's z-height
	players[playernum].mo->SetZ(spot.Z);		// [RH] Checks are now full 3-D

	// killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
	// corpse to detect collisions with other players in DM starts
	//
	// Old code:
	// if (!P_CheckPosition (players[playernum].mo, x, y))
	//    return false;

	players[playernum].mo->flags |=  MF_SOLID;
	i = P_CheckPosition(players[playernum].mo, spot);
	players[playernum].mo->flags &= ~MF_SOLID;
	players[playernum].mo->SetZ(oldz);	// [RH] Restore corpse's height
	if (!i)
		return false;

	return true;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//

// [RH] Returns the distance of the closest player to the given mapthing
static double PlayersRangeFromSpot (FPlayerStart *spot)
{
	double closest = INT_MAX;
	double distance;
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || !players[i].mo || players[i].health <= 0)
			continue;

		distance = players[i].mo->Distance2D(spot->pos.X, spot->pos.Y);

		if (distance < closest)
			closest = distance;
	}

	return closest;
}

// [RH] Select the deathmatch spawn spot farthest from everyone.
static FPlayerStart *SelectFarthestDeathmatchSpot (size_t selections)
{
	double bestdistance = 0;
	FPlayerStart *bestspot = NULL;
	unsigned int i;

	for (i = 0; i < selections; i++)
	{
		double distance = PlayersRangeFromSpot (&level.deathmatchstarts[i]);

		if (distance > bestdistance)
		{
			bestdistance = distance;
			bestspot = &level.deathmatchstarts[i];
		}
	}

	return bestspot;
}

// [RH] Select a deathmatch spawn spot at random (original mechanism)
static FPlayerStart *SelectRandomDeathmatchSpot (int playernum, unsigned int selections)
{
	unsigned int i, j;

	for (j = 0; j < 20; j++)
	{
		i = pr_dmspawn() % selections;
		if (G_CheckSpot (playernum, &level.deathmatchstarts[i]) )
		{
			return &level.deathmatchstarts[i];
		}
	}

	// [RH] return a spot anyway, since we allow telefragging when a player spawns
	return &level.deathmatchstarts[i];
}

DEFINE_ACTION_FUNCTION(DObject, G_PickDeathmatchStart)
{
	PARAM_PROLOGUE;
	unsigned int selections = level.deathmatchstarts.Size();
	DVector3 pos;
	int angle;
	if (selections == 0)
	{
		angle = INT_MAX;
		pos = DVector3(0, 0, 0);
	}
	else
	{
		unsigned int i = pr_dmspawn() % selections;
		angle = level.deathmatchstarts[i].angle;
		pos = level.deathmatchstarts[i].pos;
	}

	if (numret > 1)
	{
		ret[1].SetInt(angle);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetVector(pos);
	}
	return numret;
}

void G_DeathMatchSpawnPlayer (int playernum)
{
	unsigned int selections;
	FPlayerStart *spot;

	selections = level.deathmatchstarts.Size ();
	// [RH] We can get by with just 1 deathmatch start
	if (selections < 1)
		I_Error ("No deathmatch starts");

	// At level start, none of the players have mobjs attached to them,
	// so we always use the random deathmatch spawn. During the game,
	// though, we use whatever dmflags specifies.
	if ((dmflags & DF_SPAWN_FARTHEST) && players[playernum].mo)
		spot = SelectFarthestDeathmatchSpot (selections);
	else
		spot = SelectRandomDeathmatchSpot (playernum, selections);

	if (spot == NULL)
	{ // No good spot, so the player will probably get stuck.
	  // We were probably using select farthest above, and all
	  // the spots were taken.
		spot = G_PickPlayerStart(playernum, PPS_FORCERANDOM);
		if (!G_CheckSpot(playernum, spot))
		{ // This map doesn't have enough coop spots for this player
		  // to use one.
			spot = SelectRandomDeathmatchSpot(playernum, selections);
			if (spot == NULL)
			{ // We have a player 1 start, right?
				spot = &level.playerstarts[0];
				if (spot->type == 0)
				{ // Fine, whatever.
					spot = &level.deathmatchstarts[0];
				}
			}
		}
	}
	AActor *mo = P_SpawnPlayer(spot, playernum);
	if (mo != NULL) P_PlayerStartStomp(mo);
}


//
// G_PickPlayerStart
//
FPlayerStart *G_PickPlayerStart(int playernum, int flags)
{
	if (level.AllPlayerStarts.Size() == 0) // No starts to pick
	{
		return NULL;
	}

	if ((level.flags2 & LEVEL2_RANDOMPLAYERSTARTS) || (flags & PPS_FORCERANDOM) ||
		level.playerstarts[playernum].type == 0)
	{
		if (!(flags & PPS_NOBLOCKINGCHECK))
		{
			TArray<FPlayerStart *> good_starts;
			unsigned int i;

			// Find all unblocked player starts.
			for (i = 0; i < level.AllPlayerStarts.Size(); ++i)
			{
				if (G_CheckSpot(playernum, &level.AllPlayerStarts[i]))
				{
					good_starts.Push(&level.AllPlayerStarts[i]);
				}
			}
			if (good_starts.Size() > 0)
			{ // Pick an open spot at random.
				return good_starts[pr_pspawn(good_starts.Size())];
			}
		}
		// Pick a spot at random, whether it's open or not.
		return &level.AllPlayerStarts[pr_pspawn(level.AllPlayerStarts.Size())];
	}
	return &level.playerstarts[playernum];
}

DEFINE_ACTION_FUNCTION(DObject, G_PickPlayerStart)
{
	PARAM_PROLOGUE;
	PARAM_INT(playernum);
	PARAM_INT_DEF(flags);
	auto ps = G_PickPlayerStart(playernum, flags);
	if (numret > 1)
	{
		ret[1].SetInt(ps? ps->angle : 0);
		numret = 2;
	}
	if (numret > 0)
	{
		ret[0].SetVector(ps ? ps->pos : DVector3(0, 0, 0));
	}
	return numret;
}

//
// G_QueueBody
//
static void G_QueueBody (AActor *body)
{
	// flush an old corpse if needed
	int modslot = bodyqueslot%BODYQUESIZE;

	if (bodyqueslot >= BODYQUESIZE && bodyque[modslot] != NULL)
	{
		bodyque[modslot]->Destroy ();
	}
	bodyque[modslot] = body;

	// Copy the player's translation, so that if they change their color later, only
	// their current body will change and not all their old corpses.
	if (GetTranslationType(body->Translation) == TRANSLATION_Players ||
		GetTranslationType(body->Translation) == TRANSLATION_PlayersExtra)
	{
		*translationtables[TRANSLATION_PlayerCorpses][modslot] = *TranslationToTable(body->Translation);
		body->Translation = TRANSLATION(TRANSLATION_PlayerCorpses,modslot);
		translationtables[TRANSLATION_PlayerCorpses][modslot]->UpdateNative();
	}

	const int skinidx = body->player->userinfo.GetSkin();

	if (0 != skinidx && !(body->flags4 & MF4_NOSKIN))
	{
		// Apply skin's scale to actor's scale, it will be lost otherwise
		const AActor *const defaultActor = body->GetDefault();
		const FPlayerSkin &skin = Skins[skinidx];

		body->Scale.X *= skin.Scale.X / defaultActor->Scale.X;
		body->Scale.Y *= skin.Scale.Y / defaultActor->Scale.Y;
	}

	bodyqueslot++;
}

//
// G_DoReborn
//
EXTERN_CVAR(Bool, sv_singleplayerrespawn)
void G_DoReborn (int playernum, bool freshbot)
{
	if (!multiplayer && !(level.flags2 & LEVEL2_ALLOWRESPAWN) && !sv_singleplayerrespawn &&
		!G_SkillProperty(SKILLP_PlayerRespawn))
	{
		if (BackupSaveName.Len() > 0 && FileExists (BackupSaveName.GetChars()))
		{ // Load game from the last point it was saved
			savename = BackupSaveName;
			gameaction = ga_autoloadgame;
		}
		else
		{ // Reload the level from scratch
			bool indemo = demoplayback;
			BackupSaveName = "";
			G_InitNew (level.MapName, false);
			demoplayback = indemo;
//			gameaction = ga_loadlevel;
		}
	}
	else
	{
		bool isUnfriendly = players[playernum].mo && !(players[playernum].mo->flags & MF_FRIENDLY);

		// respawn at the start
		// first disassociate the corpse
		if (players[playernum].mo)
		{
			G_QueueBody (players[playernum].mo);
			players[playernum].mo->player = NULL;
		}

		// spawn at random spot if in deathmatch
		if ((deathmatch || isUnfriendly) && (level.deathmatchstarts.Size () > 0))
		{
			G_DeathMatchSpawnPlayer (playernum);
			return;
		}

		if (!(level.flags2 & LEVEL2_RANDOMPLAYERSTARTS) &&
			level.playerstarts[playernum].type != 0 &&
			G_CheckSpot (playernum, &level.playerstarts[playernum]))
		{
			AActor *mo = P_SpawnPlayer(&level.playerstarts[playernum], playernum);
			if (mo != NULL) P_PlayerStartStomp(mo, true);
		}
		else
		{ // try to spawn at any random player's spot
			FPlayerStart *start = G_PickPlayerStart(playernum, PPS_FORCERANDOM);
			AActor *mo = P_SpawnPlayer(start, playernum);
			if (mo != NULL) P_PlayerStartStomp(mo, true);
		}
	}
}

//
// G_DoReborn
//
void G_DoPlayerPop(int playernum)
{
	playeringame[playernum] = false;

	if (deathmatch)
	{
		Printf("%s left the game with %d frags\n",
			players[playernum].userinfo.GetName(),
			players[playernum].fragcount);
	}
	else
	{
		Printf("%s left the game\n", players[playernum].userinfo.GetName());
	}

	// [RH] Revert each player to their own view if spying through the player who left
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (playeringame[ii] && players[ii].camera == players[playernum].mo)
		{
			players[ii].camera = players[ii].mo;
			if (ii == consoleplayer && StatusBar != NULL)
			{
				StatusBar->AttachToPlayer(&players[ii]);
			}
		}
	}

	// [RH] Make the player disappear
	FBehavior::StaticStopMyScripts(players[playernum].mo);
	// [ZZ] fire player disconnect hook
	E_PlayerDisconnected(playernum);
	// [RH] Let the scripts know the player left
	FBehavior::StaticStartTypedScripts(SCRIPT_Disconnect, players[playernum].mo, true, playernum, true);
	if (players[playernum].mo != NULL)
	{
		P_DisconnectEffect(players[playernum].mo);
		players[playernum].mo->player = NULL;
		players[playernum].mo->Destroy();
		if (!(players[playernum].mo->ObjectFlags & OF_EuthanizeMe))
		{ // We just destroyed a morphed player, so now the original player
			// has taken their place. Destroy that one too.
			players[playernum].mo->Destroy();
		}
		players[playernum].mo = NULL;
		players[playernum].camera = NULL;
	}

	players[playernum].DestroyPSprites();
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
void G_LoadGame (const char* name, bool hidecon)
{
	if (name != NULL)
	{
		savename = name;
		gameaction = !hidecon ? ga_loadgame : ga_loadgamehidecon;
	}
}

static bool CheckSingleWad (const char *name, bool &printRequires, bool printwarn)
{
	if (name == NULL)
	{
		return true;
	}
	if (Wads.CheckIfWadLoaded (name) < 0)
	{
		if (printwarn)
		{
			if (!printRequires)
			{
				Printf ("This savegame needs these wads:\n%s", name);
			}
			else
			{
				Printf (", %s", name);
			}
		}
		printRequires = true;
		return false;
	}
	return true;
}

// Return false if not all the needed wads have been loaded.
bool G_CheckSaveGameWads (FSerializer &arc, bool printwarn)
{
	bool printRequires = false;
	FString text;

	arc("Game WAD", text);
	CheckSingleWad (text, printRequires, printwarn);
	arc("Map WAD", text);
	CheckSingleWad (text, printRequires, printwarn);

	if (printRequires)
	{
		if (printwarn)
		{
			Printf ("\n");
		}
		return false;
	}

	return true;
}


void G_DoLoadGame ()
{
	bool hidecon;

	if (gameaction != ga_autoloadgame)
	{
		demoplayback = false;
	}
	hidecon = gameaction == ga_loadgamehidecon;
	gameaction = ga_nothing;

	std::unique_ptr<FResourceFile> resfile(FResourceFile::OpenResourceFile(savename.GetChars(), true, true));
	if (resfile == nullptr)
	{
		Printf ("Could not read savegame '%s'\n", savename.GetChars());
		return;
	}
	FResourceLump *info = resfile->FindLump("info.json");
	if (info == nullptr)
	{
		Printf("'%s' is not a valid savegame: Missing 'info.json'.\n", savename.GetChars());
		return;
	}

	SaveVersion = 0;

	void *data = info->CacheLump();
	FSerializer arc;
	if (!arc.OpenReader((const char *)data, info->LumpSize))
	{
		Printf("Failed to access savegame info\n");
		return;
	}

	// Check whether this savegame actually has been created by a compatible engine.
	// Since there are ZDoom derivates using the exact same savegame format but
	// with mutual incompatibilities this check simplifies things significantly.
	FString savever, engine, map;
	arc("Save Version", SaveVersion);
	arc("Engine", engine);
	arc("Current Map", map);

	if (engine.CompareNoCase(GAMESIG) != 0)
	{
		// Make a special case for the message printed for old savegames that don't
		// have this information.
		if (engine.IsEmpty())
		{
			Printf("Savegame is from an incompatible version\n");
		}
		else
		{
			Printf("Savegame is from another ZDoom-based engine: %s\n", engine.GetChars());
		}
		return;
	}

	if (SaveVersion < MINSAVEVER || SaveVersion > SAVEVER)
	{
		Printf("Savegame is from an incompatible version");
		if (SaveVersion < MINSAVEVER)
		{
			Printf(": %d (%d is the oldest supported)", SaveVersion, MINSAVEVER);
		}
		else
		{
			Printf(": %d (%d is the highest supported)", SaveVersion, SAVEVER);
		}
		Printf("\n");
		return;
	}

	if (!G_CheckSaveGameWads(arc, true))
	{
		return;
	}

	if (map.IsEmpty())
	{
		Printf("Savegame is missing the current map\n");
		return;
	}

	// Now that it looks like we can load this save, hide the fullscreen console if it was up
	// when the game was selected from the menu.
	if (hidecon && gamestate == GS_FULLCONSOLE)
	{
		gamestate = GS_HIDECONSOLE;
	}
	// we are done with info.json.
	arc.Close();

	info = resfile->FindLump("globals.json");
	if (info == nullptr)
	{
		Printf("'%s' is not a valid savegame: Missing 'globals.json'.\n", savename.GetChars());
		return;
	}

	data = info->CacheLump();
	if (!arc.OpenReader((const char *)data, info->LumpSize))
	{
		Printf("Failed to access savegame info\n");
		return;
	}


	// Read intermission data for hubs
	G_SerializeHub(arc);

	bglobal.RemoveAllBots(true);

	FString cvar;
	arc("importantcvars", cvar);
	if (!cvar.IsEmpty())
	{
		uint8_t *vars_p = (uint8_t *)cvar.GetChars();
		C_ReadCVars(&vars_p);
	}

	uint32_t time[2] = { 1,0 };

	arc("ticrate", time[0])
		("leveltime", time[1]);
	// dearchive all the modifications
	level.time = Scale(time[1], TICRATE, time[0]);

	G_ReadSnapshots(resfile.get());
	resfile.reset(nullptr);	// we no longer need the resource file below this point
	G_ReadVisited(arc);

	// load a base level
	savegamerestore = true;		// Use the player actors in the savegame
	bool demoplaybacksave = demoplayback;
	G_InitNew(map, false);
	demoplayback = demoplaybacksave;
	savegamerestore = false;

	STAT_Serialize(arc);
	FRandom::StaticReadRNGState(arc);
	P_ReadACSDefereds(arc);
	P_ReadACSVars(arc);

	NextSkill = -1;
	arc("nextskill", NextSkill);

	if (level.info != nullptr)
		level.info->Snapshot.Clean();

	BackupSaveName = savename;

	// At this point, the GC threshold is likely a lot higher than the
	// amount of memory in use, so bring it down now by starting a
	// collection.
	GC::StartCollection();
}


//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//
void G_SaveGame (const char *filename, const char *description)
{
	if (sendsave || gameaction == ga_savegame)
	{
		Printf ("A game save is still pending.\n");
		return;
	}
    else if (!usergame)
	{
        Printf ("not in a saveable game\n");
    }
    else if (gamestate != GS_LEVEL)
	{
        Printf ("not in a level\n");
    }
    else if (players[consoleplayer].health <= 0 && !multiplayer)
    {
        Printf ("player is dead in a single-player game\n");
    }
	else
	{
		savegamefile = filename;
		savedescription = description;
		sendsave = true;
	}
}

FString G_BuildSaveName (const char *prefix, int slot)
{
	FString name;
	FString leader;
	const char *slash = "";

	leader = Args->CheckValue ("-savedir");
	if (leader.IsEmpty())
	{
		leader = save_dir;
		if (leader.IsEmpty())
		{
			leader = M_GetSavegamesPath();
		}
	}
	size_t len = leader.Len();
	if (leader[0] != '\0' && leader[len-1] != '\\' && leader[len-1] != '/')
	{
		slash = "/";
	}
	name << leader << slash;
	name = NicePath(name);
	CreatePath(name);
	name << prefix;
	if (slot >= 0)
	{
		name.AppendFormat("%d." SAVEGAME_EXT, slot);
	}
	return name;
}

CVAR (Int, autosavenum, 0, CVAR_NOSET|CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
static int nextautosave = -1;
CVAR (Int, disableautosave, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, saveloadconfirmation, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG) // [mxd]
CUSTOM_CVAR (Int, autosavecount, 4, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0)
		self = 0;
}

extern void P_CalcHeight (player_t *);

void G_DoAutoSave ()
{
	FString description;
	FString file;
	// Keep up to four autosaves at a time
	UCVarValue num;
	const char *readableTime;
	int count = autosavecount != 0 ? autosavecount : 1;
	
	if (nextautosave == -1) 
	{
		nextautosave = (autosavenum + 1) % count;
	}

	num.Int = nextautosave;
	autosavenum.ForceSet (num, CVAR_Int);

	file = G_BuildSaveName ("auto", nextautosave);

	if (!(level.flags2 & LEVEL2_NOAUTOSAVEHINT))
	{
		nextautosave = (nextautosave + 1) % count;
	}
	else
	{
		// This flag can only be used once per level
		level.flags2 &= ~LEVEL2_NOAUTOSAVEHINT;
	}

	readableTime = myasctime ();
	description.Format("Autosave %.12s", readableTime + 4);
	G_DoSaveGame (false, file, description);
}


static void PutSaveWads (FSerializer &arc)
{
	const char *name;

	// Name of IWAD
	name = Wads.GetWadName (Wads.GetIwadNum());
	arc.AddString("Game WAD", name);

	// Name of wad the map resides in
	if (Wads.GetLumpFile (level.lumpnum) > Wads.GetIwadNum())
	{
		name = Wads.GetWadName (Wads.GetLumpFile (level.lumpnum));
		arc.AddString("Map WAD", name);
	}
}

static void PutSaveComment (FSerializer &arc)
{
	char comment[256];
	const char *readableTime;
	uint16_t len;
	int levelTime;

	// Get the current date and time
	readableTime = myasctime ();

	strncpy (comment, readableTime, 10);
	strncpy (comment+10, readableTime+19, 5);
	strncpy (comment+15, readableTime+10, 9);
	comment[24] = 0;

	arc.AddString("Creation Time", comment);

	// Get level name
	//strcpy (comment, level.level_name);
	mysnprintf(comment, countof(comment), "%s - %s", level.MapName.GetChars(), level.LevelName.GetChars());
	len = (uint16_t)strlen (comment);
	comment[len] = '\n';

	// Append elapsed time
	levelTime = level.time / TICRATE;
	mysnprintf (comment + len + 1, countof(comment) - len - 1, "time: %02d:%02d:%02d",
		levelTime/3600, (levelTime%3600)/60, levelTime%60);
	comment[len+16] = 0;

	// Write out the comment
	arc.AddString("Comment", comment);
}

static void PutSavePic (FileWriter *file, int width, int height)
{
	if (width <= 0 || height <= 0 || !storesavepic)
	{
		M_CreateDummyPNG (file);
	}
	else
	{
		screen->WriteSavePic(&players[consoleplayer], file, width, height);
	}
}

void G_DoSaveGame (bool okForQuicksave, FString filename, const char *description)
{
	TArray<FCompressedBuffer> savegame_content;
	TArray<FString> savegame_filenames;

	char buf[100];

	// Do not even try, if we're not in a level. (Can happen after
	// a demo finishes playback.)
	if (level.lines.Size() == 0 || level.sectors.Size() == 0 || gamestate != GS_LEVEL)
	{
		return;
	}

	if (demoplayback)
	{
		filename = G_BuildSaveName ("demosave." SAVEGAME_EXT, -1);
	}

	if (cl_waitforsave)
		I_FreezeTime(true);

	insave = true;
	try
	{
		G_SnapshotLevel();
	}
	catch(CRecoverableError &err)
	{
		// delete the snapshot. Since the save failed it is broken.
		insave = false;
		level.info->Snapshot.Clean();
		Printf(PRINT_HIGH, "Save failed\n");
		Printf(PRINT_HIGH, "%s\n", err.GetMessage());
		// The time freeze must be reset if the save fails.
		if (cl_waitforsave)
			I_FreezeTime(false);
		return;
	}
	catch (...)
	{
		insave = false;
		if (cl_waitforsave)
			I_FreezeTime(false);
		throw;
	}

	BufferWriter savepic;
	FSerializer savegameinfo;		// this is for displayable info about the savegame
	FSerializer savegameglobals;	// and this for non-level related info that must be saved.

	savegameinfo.OpenWriter(true);
	savegameglobals.OpenWriter(save_formatted);

	SaveVersion = SAVEVER;
	PutSavePic(&savepic, SAVEPICWIDTH, SAVEPICHEIGHT);
	mysnprintf(buf, countof(buf), GAMENAME " %s", GetVersionString());
	// put some basic info into the PNG so that this isn't lost when the image gets extracted.
	M_AppendPNGText(&savepic, "Software", buf);
	M_AppendPNGText(&savepic, "Title", description);
	M_AppendPNGText(&savepic, "Current Map", level.MapName);
	M_FinishPNG(&savepic);

	int ver = SAVEVER;
	savegameinfo.AddString("Software", buf)
		.AddString("Engine", GAMESIG)
		("Save Version", ver)
		.AddString("Title", description)
		.AddString("Current Map", level.MapName);


	PutSaveWads (savegameinfo);
	PutSaveComment (savegameinfo);

	// Intermission stats for hubs
	G_SerializeHub(savegameglobals);

	{
		FString vars = C_GetMassCVarString(CVAR_SERVERINFO);
		savegameglobals.AddString("importantcvars", vars.GetChars());
	}

	if (level.time != 0 || level.maptime != 0)
	{
		int tic = TICRATE;
		savegameglobals("ticrate", tic);
		savegameglobals("leveltime", level.time);
	}

	STAT_Serialize(savegameglobals);
	FRandom::StaticWriteRNGState(savegameglobals);
	P_WriteACSDefereds(savegameglobals);
	P_WriteACSVars(savegameglobals);
	G_WriteVisited(savegameglobals);


	if (NextSkill != -1)
	{
		savegameglobals("nextskill", NextSkill);
	}

	auto picdata = savepic.GetBuffer();
	FCompressedBuffer bufpng = { picdata->Size(), picdata->Size(), METHOD_STORED, 0, static_cast<unsigned int>(crc32(0, &(*picdata)[0], picdata->Size())), (char*)&(*picdata)[0] };

	savegame_content.Push(bufpng);
	savegame_filenames.Push("savepic.png");
	savegame_content.Push(savegameinfo.GetCompressedOutput());
	savegame_filenames.Push("info.json");
	savegame_content.Push(savegameglobals.GetCompressedOutput());
	savegame_filenames.Push("globals.json");

	G_WriteSnapshots (savegame_filenames, savegame_content);
	

	WriteZip(filename, savegame_filenames, savegame_content);

	savegameManager.NotifyNewSave (filename, description, okForQuicksave);

	// delete the JSON buffers we created just above. Everything else will
	// either still be needed or taken care of automatically.
	savegame_content[1].Clean();
	savegame_content[2].Clean();

	// Check whether the file is ok by trying to open it.
	FResourceFile *test = FResourceFile::OpenResourceFile(filename, true);
	if (test != nullptr)
	{
		delete test;
		if (longsavemessages) Printf ("%s (%s)\n", GStrings("GGSAVED"), filename.GetChars());
		else Printf ("%s\n", GStrings("GGSAVED"));
	}
	else Printf(PRINT_HIGH, "Save failed\n");


	BackupSaveName = filename;

	// We don't need the snapshot any longer.
	level.info->Snapshot.Clean();
		
	insave = false;

	if (cl_waitforsave)
		I_FreezeTime(false);
}




//
// DEMO RECORDING
//

void G_ReadDemoTiccmd (ticcmd_t *cmd, int player)
{
	int id = DEM_BAD;

	while (id != DEM_USERCMD && id != DEM_EMPTYUSERCMD)
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

		case DEM_EMPTYUSERCMD:
			// leave cmd->ucmd unchanged
			break;

		case DEM_DROPPLAYER:
			{
				uint8_t i = ReadByte (&demo_p);
				if (i < MAXPLAYERS)
				{
					playeringame[i] = false;
				}
			}
			break;

		default:
			Net_DoCommand (id, &demo_p, player);
			break;
		}
	}
} 

bool stoprecording;

CCMD (stop)
{
	stoprecording = true;
}

extern uint8_t *lenspot;

void G_WriteDemoTiccmd (ticcmd_t *cmd, int player, int buf)
{
	uint8_t *specdata;
	int speclen;

	if (stoprecording)
	{ // use "stop" console command to end demo recording
		G_CheckDemoStatus ();
		if (!netgame)
		{
			gameaction = ga_fullconsole;
		}
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
		ptrdiff_t comp = democompspot - demobuffer;
		ptrdiff_t body = demobodyspot - demobuffer;
		// [RH] Allocate more space for the demo
		maxdemosize += 0x20000;
		demobuffer = (uint8_t *)M_Realloc (demobuffer, maxdemosize);
		demo_p = demobuffer + pos;
		lenspot = demobuffer + spot;
		democompspot = demobuffer + comp;
		demobodyspot = demobuffer + body;
	}
}



//
// G_RecordDemo
//
void G_RecordDemo (const char* name)
{
	usergame = false;
	demoname = name;
	FixPathSeperator (demoname);
	DefaultExtension (demoname, ".lmp");
	maxdemosize = 0x20000;
	demobuffer = (uint8_t *)M_Malloc (maxdemosize);
	demorecording = true; 
}


// [RH] Demos are now saved as IFF FORMs. I've also removed support
//		for earlier ZDEMs since I didn't want to bother supporting
//		something that probably wasn't used much (if at all).

void G_BeginRecording (const char *startmap)
{
	int i;

	if (startmap == NULL)
	{
		startmap = level.MapName;
	}
	demo_p = demobuffer;

	WriteLong (FORM_ID, &demo_p);			// Write FORM ID
	demo_p += 4;							// Leave space for len
	WriteLong (ZDEM_ID, &demo_p);			// Write ZDEM ID

	// Write header chunk
	StartChunk (ZDHD_ID, &demo_p);
	WriteWord (DEMOGAMEVERSION, &demo_p);	// Write ZDoom version
	*demo_p++ = 2;							// Write minimum version needed to use this demo.
	*demo_p++ = 3;							// (Useful?)

	strcpy((char*)demo_p, startmap);		// Write name of map demo was recorded on.
	demo_p += strlen(startmap) + 1;
	WriteLong(rngseed, &demo_p);			// Write RNG seed
	*demo_p++ = consoleplayer;
	FinishChunk (&demo_p);

	// Write player info chunks
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i])
		{
			StartChunk (UINF_ID, &demo_p);
			WriteByte ((uint8_t)i, &demo_p);
			D_WriteUserInfoStrings (i, &demo_p);
			FinishChunk (&demo_p);
		}
	}

	// It is possible to start a "multiplayer" game with only one player,
	// so checking the number of players when playing back the demo is not
	// enough.
	if (multiplayer)
	{
		StartChunk (NETD_ID, &demo_p);
		FinishChunk (&demo_p);
	}

	// Write cvars chunk
	StartChunk (VARS_ID, &demo_p);
	C_WriteCVars (&demo_p, CVAR_SERVERINFO|CVAR_DEMOSAVE);
	FinishChunk (&demo_p);

	// Write weapon ordering chunk
	StartChunk (WEAP_ID, &demo_p);
	P_WriteDemoWeaponsChunk(&demo_p);
	FinishChunk (&demo_p);

	// Indicate body is compressed
	StartChunk (COMP_ID, &demo_p);
	democompspot = demo_p;
	WriteLong (0, &demo_p);
	FinishChunk (&demo_p);

	// Begin BODY chunk
	StartChunk (BODY_ID, &demo_p);
	demobodyspot = demo_p;
}


//
// G_PlayDemo
//

FString defdemoname;

void G_DeferedPlayDemo (const char *name)
{
	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
}

UNSAFE_CCMD (playdemo)
{
	if (netgame)
	{
		Printf("End your current netgame first!\n");
		return;
	}
	if (demorecording)
	{
		Printf("End your current demo first!\n");
		return;
	}
	if (argv.argc() > 1)
	{
		G_DeferedPlayDemo (argv[1]);
		singledemo = true;
	}
}

UNSAFE_CCMD (timedemo)
{
	if (argv.argc() > 1)
	{
		G_TimeDemo (argv[1]);
		singledemo = true;
	}
}

// [RH] Process all the information in a FORM ZDEM
//		until a BODY chunk is entered.
bool G_ProcessIFFDemo (FString &mapname)
{
	bool headerHit = false;
	bool bodyHit = false;
	int numPlayers = 0;
	int id, len, i;
	uLong uncompSize = 0;
	uint8_t *nextchunk;

	demoplayback = true;

	for (i = 0; i < MAXPLAYERS; i++)
		playeringame[i] = 0;

	len = ReadLong (&demo_p);
	zdemformend = demo_p + len + (len & 1);

	// Check to make sure this is a ZDEM chunk file.
	// TODO: Support multiple FORM ZDEMs in a CAT. Might be useful.

	id = ReadLong (&demo_p);
	if (id != ZDEM_ID)
	{
		Printf ("Not a " GAMENAME " demo file!\n");
		return true;
	}

	// Process all chunks until a BODY chunk is encountered.

	while (demo_p < zdemformend && !bodyHit)
	{
		id = ReadLong (&demo_p);
		len = ReadLong (&demo_p);
		nextchunk = demo_p + len + (len & 1);
		if (nextchunk > zdemformend)
		{
			Printf ("Demo is mangled!\n");
			return true;
		}

		switch (id)
		{
		case ZDHD_ID:
			headerHit = true;

			demover = ReadWord (&demo_p);	// ZDoom version demo was created with
			if (demover < MINDEMOVERSION)
			{
				Printf ("Demo requires an older version of " GAMENAME "!\n");
				//return true;
			}
			if (ReadWord (&demo_p) > DEMOGAMEVERSION)	// Minimum ZDoom version
			{
				Printf ("Demo requires a newer version of " GAMENAME "!\n");
				return true;
			}
			if (demover >= 0x21a)
			{
				mapname = (char*)demo_p;
				demo_p += mapname.Len() + 1;
			}
			else
			{
				mapname = FString((char*)demo_p, 8);
				demo_p += 8;
			}
			rngseed = ReadLong (&demo_p);
			// Only reset the RNG if this demo is not in conjunction with a savegame.
			if (mapname[0] != 0)
			{
				FRandom::StaticClearRandom ();
			}
			consoleplayer = *demo_p++;
			break;

		case VARS_ID:
			C_ReadCVars (&demo_p);
			break;

		case UINF_ID:
			i = ReadByte (&demo_p);
			if (!playeringame[i])
			{
				playeringame[i] = 1;
				numPlayers++;
			}
			D_ReadUserInfoStrings (i, &demo_p, false);
			break;

		case NETD_ID:
			multiplayer = true;
			break;

		case WEAP_ID:
			P_ReadDemoWeaponsChunk(&demo_p);
			break;

		case BODY_ID:
			bodyHit = true;
			zdembodyend = demo_p + len;
			break;

		case COMP_ID:
			uncompSize = ReadLong (&demo_p);
			break;
		}

		if (!bodyHit)
			demo_p = nextchunk;
	}

	if (!headerHit)
	{
		Printf ("Demo has no header!\n");
		return true;
	}

	if (!numPlayers)
	{
		Printf ("Demo has no players!\n");
		return true;
	}

	if (!bodyHit)
	{
		zdembodyend = NULL;
		Printf ("Demo has no BODY chunk!\n");
		return true;
	}

	if (numPlayers > 1)
		multiplayer = netgame = true;

	if (uncompSize > 0)
	{
		uint8_t *uncompressed = (uint8_t*)M_Malloc(uncompSize);
		int r = uncompress (uncompressed, &uncompSize, demo_p, uLong(zdembodyend - demo_p));
		if (r != Z_OK)
		{
			Printf ("Could not decompress demo! %s\n", M_ZLibError(r).GetChars());
			M_Free(uncompressed);
			return true;
		}
		M_Free (demobuffer);
		zdembodyend = uncompressed + uncompSize;
		demobuffer = demo_p = uncompressed;
	}

	return false;
}

void G_DoPlayDemo (void)
{
	FString mapname;
	int demolump;

	gameaction = ga_nothing;

	// [RH] Allow for demos not loaded as lumps
	demolump = Wads.CheckNumForFullName (defdemoname, true);
	if (demolump >= 0)
	{
		int demolen = Wads.LumpLength (demolump);
		demobuffer = (uint8_t *)M_Malloc(demolen);
		Wads.ReadLump (demolump, demobuffer);
	}
	else
	{
		FixPathSeperator (defdemoname);
		DefaultExtension (defdemoname, ".lmp");
		FileReader fr;
		if (!fr.OpenFile(defdemoname))
		{
			I_Error("Unable to open demo '%s'", defdemoname.GetChars());
		}
		auto len = fr.GetLength();
		demobuffer = (uint8_t*)M_Malloc(len);
		if (fr.Read(demobuffer, len) != len)
		{
			I_Error("Unable to read demo '%s'", defdemoname.GetChars());
		}
	}
	demo_p = demobuffer;

	Printf ("Playing demo %s\n", defdemoname.GetChars());

	C_BackupCVars ();		// [RH] Save cvars that might be affected by demo

	if (ReadLong (&demo_p) != FORM_ID)
	{
		const char *eek = "Cannot play non-" GAMENAME " demos.\n";

		C_ForgetCVars();
		M_Free(demobuffer);
		demo_p = demobuffer = NULL;
		if (singledemo)
		{
			I_Error ("%s", eek);
		}
		else
		{
			Printf (PRINT_BOLD, "%s", eek);
			gameaction = ga_nothing;
		}
	}
	else if (G_ProcessIFFDemo (mapname))
	{
		C_RestoreCVars();
		gameaction = ga_nothing;
		demoplayback = false;
	}
	else
	{
		// don't spend a lot of time in loadlevel 
		precache = false;
		demonew = true;
		if (mapname.Len() != 0)
		{
			G_InitNew (mapname, false);
		}
		else if (level.sectors.Size() == 0)
		{
			I_Error("Cannot play demo without its savegame\n");
		}
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
void G_TimeDemo (const char* name)
{
	nodrawers = !!Args->CheckParm ("-nodraw");
	noblit = !!Args->CheckParm ("-noblit");
	timingdemo = true;
	singletics = true;

	defdemoname = name;
	gameaction = (gameaction == ga_loadgame) ? ga_loadgameplaydemo : ga_playdemo;
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

bool G_CheckDemoStatus (void)
{
	if (!demorecording)
	{ // [RH] Restore the player's userinfo settings.
		D_SetupUserInfo();
	}

	if (demoplayback)
	{
		extern int starttime;
		int endtime = 0;

		if (timingdemo)
			endtime = I_GetTime () - starttime;

		C_RestoreCVars ();		// [RH] Restore cvars demo might have changed
		M_Free (demobuffer);
		demobuffer = NULL;

		P_SetupWeapons_ntohton();
		demoplayback = false;
		netgame = false;
		multiplayer = false;
		singletics = false;
		for (int i = 1; i < MAXPLAYERS; i++)
			playeringame[i] = 0;
		consoleplayer = 0;
		players[0].camera = NULL;
		if (StatusBar != NULL)
		{
			StatusBar->AttachToPlayer (&players[0]);
		}
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
		uint8_t *formlen;

		WriteByte (DEM_STOP, &demo_p);

		if (demo_compress)
		{
			// Now that the entire BODY chunk has been created, replace it with
			// a compressed version. If the BODY successfully compresses, the
			// contents of the COMP chunk will be changed to indicate the
			// uncompressed size of the BODY.
			uLong len = uLong(demo_p - demobodyspot);
			uLong outlen = (len + len/100 + 12);
			Byte *compressed = new Byte[outlen];
			int r = compress2 (compressed, &outlen, demobodyspot, len, 9);
			if (r == Z_OK && outlen < len)
			{
				formlen = democompspot;
				WriteLong (len, &democompspot);
				memcpy (demobodyspot, compressed, outlen);
				demo_p = demobodyspot + outlen;
			}
			delete[] compressed;
		}
		FinishChunk (&demo_p);
		formlen = demobuffer + 4;
		WriteLong (int(demo_p - demobuffer - 8), &formlen);

		auto fw = FileWriter::Open(demoname);
		bool saved = false;
		if (fw != nullptr)
		{
			const size_t size = demo_p - demobuffer;
			saved = fw->Write(demobuffer, size) == size;
			delete fw;
			if (!saved) remove(demoname);
		}
		M_Free (demobuffer); 
		demorecording = false;
		stoprecording = false;
		if (saved)
		{
			Printf ("Demo %s recorded\n", demoname.GetChars()); 
		}
		else
		{
			Printf ("Demo %s could not be saved\n", demoname.GetChars());
		}
	}

	return false; 
}

void G_StartSlideshow(FName whichone)
{
	gameaction = ga_slideshow;
	SelectedSlideshow = whichone == NAME_None ? level.info->slideshow : whichone;
}

DEFINE_ACTION_FUNCTION(FLevelLocals, StartSlideshow)
{
	PARAM_PROLOGUE;
	PARAM_NAME_DEF(whichone);
	G_StartSlideshow(whichone);
	return 0;
}

DEFINE_GLOBAL(players)
DEFINE_GLOBAL(playeringame)
DEFINE_GLOBAL(PlayerClasses)
DEFINE_GLOBAL_NAMED(Skins, PlayerSkins)
DEFINE_GLOBAL(consoleplayer)
DEFINE_GLOBAL_NAMED(PClassActor::AllActorClasses, AllActorClasses)
DEFINE_GLOBAL(validcount)
DEFINE_GLOBAL(multiplayer)
DEFINE_GLOBAL(gameaction)
DEFINE_GLOBAL(gamestate)
DEFINE_GLOBAL(skyflatnum)
DEFINE_GLOBAL_NAMED(bglobal.freeze, globalfreeze)
DEFINE_GLOBAL(gametic)
DEFINE_GLOBAL(demoplayback)
DEFINE_GLOBAL(automapactive);
DEFINE_GLOBAL(Net_Arbitrator);
