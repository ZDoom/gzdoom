/* m_options.c
**
** New options menu code.
**
** Sorry this got so convoluted. It was originally much cleaner until
** I started adding all sorts of gadgets to the menus. I might someday
** make a project of rewriting the entire menu system using Amiga-style
** taglists to describe each menu item. We'll see...
*/

#include "m_alloc.h"

#include "doomdef.h"
#include "dstrings.h"

#include "c_consol.h"
#include "c_dispch.h"
#include "c_bind.h"

#include "d_main.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

#include "m_misc.h"

// Data.
#include "m_menu.h"

//
// defaulted values
//
cvar_t 					*mouseSensitivity;		// has default

// Show messages has default, 0 = off, 1 = on
cvar_t 					*showMessages;
		
cvar_t 					*screenblocks;			// has default

extern BOOL				OptionsActive;

extern int				screenSize;
extern short			skullAnimCounter;

extern cvar_t			*cl_run;
extern cvar_t			*invertmouse;
extern cvar_t			*lookspring;
extern cvar_t			*lookstrafe;
extern cvar_t			*crosshair;
extern cvar_t			*freelook;

void M_ChangeMessages(void);
void M_SizeDisplay(float diff);
void M_StartControlPanel(void);

int  M_StringHeight(char *string);
void M_ClearMenus (void);



static value_t YesNo[2] = {
	{ 0.0, "No" },
	{ 1.0, "Yes" }
};

static value_t NoYes[2] = {
	{ 0.0, "Yes" },
	{ 1.0, "No" }
};

static value_t OnOff[2] = {
	{ 0.0, "Off" },
	{ 1.0, "On" }
};

menu_t  *CurrentMenu;
int		CurrentItem;
static BOOL	WaitingForKey;
static char	   *OldMessage;
static itemtype OldType;

/*=======================================
 *
 * Options Menu
 *
 *=======================================*/

static void CustomizeControls (void);
static void GameplayOptions (void);
static void VideoOptions (void);
static void GoToConsole (void);
void Reset2Defaults (void);
void Reset2Saved (void);

extern cvar_t *snd_MidiVolume;

static void SetVidMode (void);

static menuitem_t OptionItems[] = {
	{ more,		"Customize Controls",	NULL,					0.0, 0.0,	0.0, (value_t *)CustomizeControls },
	{ more,		"Go to console",		NULL,					0.0, 0.0,	0.0, (value_t *)GoToConsole },
	{ more,		"Gameplay Options",		NULL,					0.0, 0.0,	0.0, (value_t *)GameplayOptions },
	{ more,		"Display Options",		NULL,					0.0, 0.0,	0.0, (value_t *)VideoOptions },
	{ more,		"Set video mode",		NULL,					0.0, 0.0,	0.0, (value_t *)SetVidMode },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ slider,	"Mouse speed",			&mouseSensitivity,		0.5, 2.5,	0.1, NULL },
	{ slider,	"MIDI music volume",	&snd_MidiVolume,		0.0, 1.0,	0.05, NULL },
	{ slider,	"MOD music volume",		&snd_MusicVolume,		0.0, 64.0,	1.0, NULL },
	{ slider,	"Sound volume",			&snd_SfxVolume,			0.0, 15.0,	1.0, NULL },
	{ discrete,	"Always Run",			&cl_run,				2.0, 0.0,	0.0, OnOff },
	{ discrete, "Always Mouselook",		&freelook,				2.0, 0.0,	0.0, OnOff },
	{ discrete, "Invert Mouse",			&invertmouse,			2.0, 0.0,	0.0, OnOff },
	{ discrete, "Lookspring",			&lookspring,			2.0, 0.0,	0.0, OnOff },
	{ discrete, "Lookstrafe",			&lookstrafe,			2.0, 0.0,	0.0, OnOff },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ more,		"Reset to defaults",	NULL,					0.0, 0.0,	0.0, (value_t *)Reset2Defaults },
	{ more,		"Reset to last saved",	NULL,					0.0, 0.0,	0.0, (value_t *)Reset2Saved }
};

menu_t OptionMenu = {
	"M_OPTTTL",
	0,
	16,
	177,
	OptionItems,
};


/*=======================================
 *
 * Controls Menu
 *
 *=======================================*/

static menuitem_t ControlsItems[] = {
	{ whitetext,"ENTER to change, BACKSPACE to clear", NULL, 0.0, 0.0, 0.0, NULL },
	{ control,	"Attack",				NULL, 0.0, 0.0, 0.0, (value_t *)"+attack" },
	{ control,	"Next Weapon",			NULL, 0.0, 0.0, 0.0, (value_t *)"weapnext" },	// Was already here
	{ control,	"Previous Weapon",		NULL, 0.0, 0.0, 0.0, (value_t *)"weapprev" },	// TIJ
	{ control,	"Use / Open",			NULL, 0.0, 0.0, 0.0, (value_t *)"+use" },
	{ control,	"Jump",					NULL, 0.0, 0.0, 0.0, (value_t *)"+jump" },
	{ control,	"Walk forward",			NULL, 0.0, 0.0, 0.0, (value_t *)"+forward" },
	{ control,	"Backpedal",			NULL, 0.0, 0.0, 0.0, (value_t *)"+back" },
	{ control,	"Turn left",			NULL, 0.0, 0.0, 0.0, (value_t *)"+left" },
	{ control,	"Turn right",			NULL, 0.0, 0.0, 0.0, (value_t *)"+right" },
	{ control,	"Run",					NULL, 0.0, 0.0, 0.0, (value_t *)"+speed" },
	{ control,	"Step left",			NULL, 0.0, 0.0, 0.0, (value_t *)"+moveleft" },
	{ control,	"Step right",			NULL, 0.0, 0.0, 0.0, (value_t *)"+moveright" },
	{ control,	"Sidestep",				NULL, 0.0, 0.0, 0.0, (value_t *)"+strafe" },
	{ control,	"Look up",				NULL, 0.0, 0.0, 0.0, (value_t *)"+lookup" },
	{ control,	"Look down",			NULL, 0.0, 0.0, 0.0, (value_t *)"+lookdown" },
	{ control,	"Center view",			NULL, 0.0, 0.0, 0.0, (value_t *)"centerview" },
	{ control,	"Mouse look",			NULL, 0.0, 0.0, 0.0, (value_t *)"+mlook" },
	{ control,	"Keyboard look",		NULL, 0.0, 0.0, 0.0, (value_t *)"+klook" },
	{ control,	"Toggle automap",		NULL, 0.0, 0.0, 0.0, (value_t *)"togglemap" },
	{ control,	"Chasecam",				NULL, 0.0, 0.0, 0.0, (value_t *)"chase" }
};

menu_t ControlsMenu = {
	"M_CONTRO",
	1,
	21,	// TIJ	Was: 19
		//		Now: 21 (2 more Control menu items: Prev and Next Weapon)
	0,
	ControlsItems,
};

/*=======================================
 *
 * Display Options Menu
 *
 *=======================================*/
static void StartMessagesMenu (void);

extern cvar_t *am_rotate,
			  *am_overlay,
			  *st_scale,
			  *am_usecustomcolors,
			  *r_detail,
			  *r_stretchsky,
			  *r_columnmethod,
			  *r_drawfuzz,
			  *cl_rockettrails,
			  *cl_pufftype,
			  *cl_bloodtype,
			  *wipetype,
			  *vid_palettehack;

static value_t Crosshairs[] = {
	{ 0.0, "None" },
	{ 1.0, "Cross 1" },
	{ 2.0, "Cross 2" },
	{ 3.0, "X" },
	{ 4.0, "Diamond" },
	{ 5.0, "Dot" },
	{ 6.0, "Box" },
	{ 7.0, "Angle" },
	{ 8.0, "Big Thing" }
};

static value_t DetailModes[] = {
	{ 0.0, "Normal" },
	{ 1.0, "Double Horizontally" },
	{ 2.0, "Double Vertically" },
	{ 3.0, "Double Horiz and Vert" }
};

static value_t ColumnMethods[] = {
	{ 0.0, "Original" },
	{ 1.0, "Optimized" }
};

static value_t BloodTypes[] = {
	{ 0.0, "Sprites" },
	{ 1.0, "Sprites and Particles" },
	{ 2.0, "Particles" }
};

static value_t PuffTypes[] = {
	{ 0.0, "Sprites" },
	{ 1.0, "Particles" }
};

static value_t Wipes[] = {
	{ 0.0, "None" },
	{ 1.0, "Melt" },
	{ 2.0, "Burn" },
	{ 3.0, "Crossfade" }
};

static menuitem_t VideoItems[] = {
	{ more,		"Messages",				NULL,					0.0, 0.0,	0.0, (value_t *)StartMessagesMenu },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ slider,	"Screen size",			&screenblocks,			3.0, 12.0,	1.0, NULL },
	{ slider,	"Brightness",			(cvar_t **)&gammalevel, 1.0, 3.0,	0.1, NULL },
	{ discrete,	"Crosshair",			(cvar_t **)&crosshair,	9.0, 0.0,	0.0, Crosshairs },
	{ discrete, "Column render mode",	(cvar_t **)&r_columnmethod,2.0,0.0,	0.0, ColumnMethods },
	{ discrete, "Detail mode",			(cvar_t **)&r_detail,	4.0, 0.0,	0.0, DetailModes },
	{ discrete, "Stretch short skies",	(cvar_t **)&r_stretchsky, 2.0,0.0,	0.0, OnOff },
	{ discrete, "Stretch status bar",	(cvar_t **)&st_scale,	2.0, 0.0,	0.0, OnOff },
	{ discrete, "Screen wipe style",	(cvar_t **)&wipetype,	4.0, 0.0,	0.0, Wipes },
#ifdef _WIN32
	{ discrete, "DirectDraw palette hack", (cvar_t **)&vid_palettehack, 2.0,0.0,0.0,OnOff },
#endif
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ discrete, "Use fuzz effect",		(cvar_t **)&r_drawfuzz, 2.0, 0.0, 0.0, YesNo },
	{ discrete, "Rocket Trails",		(cvar_t **)&cl_rockettrails, 2.0, 0.0, 0.0, OnOff },
	{ discrete, "Blood Type",			(cvar_t **)&cl_bloodtype, 3.0, 0.0, 0.0, BloodTypes },
	{ discrete, "Bullet Puff Type",		(cvar_t **)&cl_pufftype, 2.0, 0.0,  0.0, PuffTypes },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ discrete, "Rotate automap",		(cvar_t **)&am_rotate,	2.0, 0.0,	0.0, OnOff },
	{ discrete, "Overlay automap",		(cvar_t **)&am_overlay,	2.0, 0.0,	0.0, OnOff },
	{ discrete, "Standard map colors", (cvar_t **)&am_usecustomcolors, 2.0, 0.0, 0.0, NoYes },
};

menu_t VideoMenu = {
	"M_VIDEO",
	0,
#ifdef _WIN32
	20,
#else
	19,
#endif
	0,
	VideoItems,
};

/*=======================================
 *
 * Messages Menu
 *
 *=======================================*/
extern cvar_t *con_scaletext,
			  *msg0color, *msg1color, *msg2color, *msg3color, *msg4color,
			  *msgmidcolor, *msglevel;

static value_t TextColors[] = {
	{ 0.0, "brick" },
	{ 1.0, "tan" },
	{ 2.0, "gray" },
	{ 3.0, "green" },
	{ 4.0, "brown" },
	{ 5.0, "gold" },
	{ 6.0, "red" },
	{ 7.0, "blue" }
};

static value_t MessageLevels[] = {
	{ 0.0, "Item Pickup" },
	{ 1.0, "Obituaries" },
	{ 2.0, "Critical Messages" }
};

static menuitem_t MessagesItems[] = {
	{ discrete,	"Scale text in high res", (cvar_t **)&con_scaletext,2.0,0.0, 0.0, OnOff },
	{ discrete, "Minimum message level", (cvar_t **)&msglevel,	3.0, 0.0,   0.0, MessageLevels },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ whitetext, "Message Colors",		NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ cdiscrete, "Item Pickup",			(cvar_t **)&msg0color,	8.0, 0.0,	0.0, TextColors },
	{ cdiscrete, "Obituaries",			(cvar_t **)&msg1color,	8.0, 0.0,	0.0, TextColors },
	{ cdiscrete, "Critical Messages",	(cvar_t **)&msg2color,	8.0, 0.0,	0.0, TextColors },
	{ cdiscrete, "Chat Messages",		(cvar_t **)&msg3color,	8.0, 0.0,	0.0, TextColors },
	{ cdiscrete, "Team Messages",		(cvar_t **)&msg4color,	8.0, 0.0,	0.0, TextColors },
	{ cdiscrete, "Centered Messages",	(cvar_t **)&msgmidcolor,8.0, 0.0,	0.0, TextColors }
};

menu_t MessagesMenu = {
	"M_MESS",
	0,
	11,
	0,
	MessagesItems,
};


/*=======================================
 *
 * Video Modes Menu
 *
 *=======================================*/

extern BOOL setmodeneeded;
extern int NewWidth, NewHeight, NewID;
extern int DisplayID;
extern char *IdStrings[22];

int testingmode;		// Holds time to revert to old mode
int OldWidth, OldHeight, OldID;
float OldFS;

static void BuildModesList (int hiwidth, int hiheight, int hi_id);
static BOOL GetSelectedSize (int line, int *width, int *height);
static void SetModesMenu (int w, int h, int id);

// Found in i_video.c.
void I_StartModeIterator (int id);
BOOL I_NextMode (int *width, int *height);

extern cvar_t *vid_defwidth, *vid_defheight, *vid_defid;

static cvar_t DummyDepthCvar;
static cvar_t *DummyDepthPtr;

#ifndef DJGPP
extern cvar_t *fullscreen;
static cvar_t *DummyFSPtr;
static cvar_t DummyFSCvar;
#endif

static value_t Depths[22];

static char VMEnterText[] = "Press ENTER to set mode";
static char VMTestText[] = "T to test mode for 5 seconds";

static menuitem_t ModesItems[] = {
	{ discrete, "Screen mode",			(cvar_t **)&DummyDepthPtr,0.0, 0.0,0.0, Depths },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
#ifdef DJGPP
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
#else
	{ discrete, "Fullscreen",			(cvar_t **)&DummyFSPtr,2.0, 0.0,	0.0, YesNo },
#endif
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ screenres,NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ whitetext,"Note: Only 8 bpp modes are supported",NULL,	0.0, 0.0,	0.0, NULL },
	{ redtext,  VMEnterText,			NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,  VMTestText,				NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,  NULL,					NULL,					0.0, 0.0,	0.0, NULL },
	{ redtext,  NULL,					NULL,					0.0, 0.0,	0.0, NULL }
};

#define VM_DEPTHITEM	0
#define VM_RESSTART		4
#define VM_ENTERLINE	14
#define VM_TESTLINE		16
#define VM_MAKEDEFLINE	18
#define VM_CURDEFLINE	19

menu_t ModesMenu = {
	"M_VIDMOD",
#ifndef DJGPP
	2,
#else
	4,
#endif
	20,
	130,
	ModesItems,
};

extern byte IdTobpp[22];

static int IdToBpp (int id) {
	id -= 1000;
	if (id < 0 || id > 21)
		return 0;
	else
		return IdTobpp[id];
}


/*=======================================
 *
 * Gameplay Options (dmflags) Menu
 *
 *=======================================*/

static cvar_t *flagsvar;

static menuitem_t DMFlagsItems[] = {
	{ bitflag,	"Falling damage",		(cvar_t **)DF_YES_FALLING,		0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Weapons stay (DM)",	(cvar_t **)DF_WEAPONS_STAY,		0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow powerups (DM)",	(cvar_t **)DF_NO_ITEMS,			1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow health (DM)",	(cvar_t **)DF_NO_HEALTH,		1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow armor (DM)",		(cvar_t **)DF_NO_ARMOR,			1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Spawn farthest (DM)",	(cvar_t **)DF_SPAWN_FARTHEST,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Same map (DM)",		(cvar_t **)DF_SAME_LEVEL,		0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Force respawn (DM)",	(cvar_t **)DF_FORCE_RESPAWN,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow exit (DM)",		(cvar_t **)DF_NO_EXIT,			1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Infinite ammo",		(cvar_t **)DF_INFINITE_AMMO,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"No monsters",			(cvar_t **)DF_NO_MONSTERS,		0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Monsters respawn",		(cvar_t **)DF_MONSTERS_RESPAWN,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Items respawn",		(cvar_t **)DF_ITEMS_RESPAWN,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Fast monsters",		(cvar_t **)DF_FAST_MONSTERS,	0, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow jump",			(cvar_t **)DF_NO_JUMP,			1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Allow freelook",		(cvar_t **)DF_NO_FREELOOK,		1, 0, 0, (value_t *)&dmflags },
	{ bitflag,	"Friendly fire",		(cvar_t **)DF_NO_FRIENDLY_FIRE,	1, 0, 0, (value_t *)&dmflags },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ discrete, "Teamplay",				(cvar_t **)&teamplay,	2.0, 0.0,	0.0, OnOff }
};

static menu_t DMFlagsMenu = {
	"M_GMPLAY",
	0,
	19,
	0,
	DMFlagsItems,
};

void M_FreeValues (value_t **values, int num)
{
	int i;

	if (*values) {
		for (i = 0; i < num; i++) {
			if ((*values)[i].name)
				free ((*values)[i].name);
		}

		free (*values);
		*values = NULL;
	}
}

//
//		Set some stuff up for the video modes menu
//
static int IDTranslate[22];

void M_OptInit (void)
{
	int currval = 0, dummy1, dummy2, i;
	char name[24];

	DummyDepthPtr = &DummyDepthCvar;
#ifndef DJGPP
	DummyFSPtr = &DummyFSCvar;
#endif

	for (i = 1000; i < 1022; i++) {
		I_StartModeIterator (i);
		if (I_NextMode (&dummy1, &dummy2)) {
			Depths[currval].value = currval;
			sprintf (name, "%d bit (%s)", IdToBpp (i), IdStrings[i-1000]);
			Depths[currval].name = copystring (name);
			IDTranslate[currval] = i;
			currval++;
		}
	}

	ModesItems[VM_DEPTHITEM].b.min = (float)currval;
}


//
//		Toggle messages on/off
//
void M_ChangeMessages (void)
{
	if (showMessages->value) {
		Printf (128, "%s\n", MSGOFF);
		SetCVarFloat (showMessages, 0.0);
	} else {
		Printf (128, "%s\n", MSGON);
		SetCVarFloat (showMessages, 1.0);
	}
}

void Cmd_ToggleMessages (player_t *plyr, int argc, char **argv)
{
	M_ChangeMessages ();
}

void M_SizeDisplay (float diff)
{
	// changing screenblocks automatically resizes the display
	SetCVarFloat (screenblocks, screenblocks->value + diff);
}

void Cmd_Sizedown (void *plyr, int argc, char **argv)
{
	M_SizeDisplay (-1.0);
	S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
}

void Cmd_Sizeup (void *plyr, int argc, char **argv)
{
	M_SizeDisplay(1.0);
	S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
}

void M_BuildKeyList (menuitem_t *item, int numitems)
{
	int i;

	for (i = 0; i < numitems; i++, item++) {
		if (item->type == control)
			C_GetKeysForCommand (item->e.command, &item->b.key1, &item->c.key2);
	}
}

void M_SwitchMenu (menu_t *menu)
{
	int i, widest = 0, thiswidth;
	menuitem_t *item;

	MenuStack[MenuStackDepth].menu.new = menu;
	MenuStack[MenuStackDepth].isNewStyle = true;
	MenuStack[MenuStackDepth].drawSkull = false;
	MenuStackDepth++;

	CurrentMenu = menu;
	CurrentItem = menu->lastOn;

	if (!menu->indent) {
		for (i = 0; i < menu->numitems; i++) {
			item = menu->items + i;
			if (item->type != whitetext && item->type != redtext) {
				thiswidth = V_StringWidth (item->label);
				if (thiswidth > widest)
					widest = thiswidth;
			}
		}
		menu->indent = widest + 6;
	}

	flagsvar = NULL;
}

BOOL M_StartOptionsMenu (void)
{
	M_SwitchMenu (&OptionMenu);
	return true;
}

void M_DrawSlider (int x, int y, float min, float max, float cur)
{
	float range;
	int i;

	range = max - min;

	if (cur > max)
		cur = max;
	else if (cur < min)
		cur = min;

	cur -= min;

	V_DrawPatchClean (x, y, &screen, W_CacheLumpName ("LSLIDE", PU_CACHE));
	for (i = 1; i < 11; i++)
		V_DrawPatchClean (x + i*8, y, &screen, W_CacheLumpName ("MSLIDE", PU_CACHE));
	V_DrawPatchClean (x + 88, y, &screen, W_CacheLumpName ("RSLIDE", PU_CACHE));

	V_DrawPatchClean (x + 5 + (int)((cur * 78.0) / range), y, &screen, W_CacheLumpName ("CSLIDE", PU_CACHE));
}

int M_FindCurVal (float cur, value_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

void M_OptDrawer (void)
{
	int color;
	int y, width, i, x;
	menuitem_t *item;
	patch_t *title;

	title = W_CacheLumpName (CurrentMenu->title, PU_CACHE);
	
	V_DrawPatchClean (160-title->width/2,10,&screen,title);

//	for (i = 0, y = 20 + title->height; i < CurrentMenu->numitems; i++, y += 8)	{
	for (i = 0, y = 15 + title->height; i < CurrentMenu->numitems; i++, y += 8)	{	// TIJ
		item = CurrentMenu->items + i;

		if (item->type != screenres) {
			width = V_StringWidth (item->label);
			switch (item->type) {
				case more:
					x = CurrentMenu->indent - width;
					color = CR_GREY;
					break;

				case redtext:
					x = 160 - width / 2;
					color = CR_RED;
					break;

				case whitetext:
					x = 160 - width / 2;
					color = CR_GREY;
					break;

				case listelement:
					x = CurrentMenu->indent + 14;
					color = CR_RED;
					break;

				default:
					x = CurrentMenu->indent - width;
					color = CR_RED;
					break;
			}
			V_DrawTextCleanMove (color, x, y, item->label);

			switch (item->type) {
				case discrete:
				case cdiscrete:
					{
						int v, vals;

						vals = (int)item->b.min;
						v = M_FindCurVal ((*item->a.cvar)->value, item->e.values, vals);

						if (v == vals) {
							V_DrawTextCleanMove (CR_GREY, CurrentMenu->indent + 14, y, "Unknown");
						} else {
							if (item->type == cdiscrete)
								V_DrawTextCleanMove (v, CurrentMenu->indent + 14, y, item->e.values[v].name);
							else
								V_DrawTextCleanMove (CR_GREY, CurrentMenu->indent + 14, y, item->e.values[v].name);
						}
					}
					break;

				case slider:
					M_DrawSlider (CurrentMenu->indent + 14, y, item->b.min, item->c.max, (*item->a.cvar)->value);
					break;

				case control:
					{
						char description[64];

						C_NameKeys (description, item->b.key1, item->c.key2);
						V_DrawTextCleanMove (CR_GREY, CurrentMenu->indent + 14, y, description);
					}
					break;

				case bitflag:
					{
						value_t *value;
						char *str;

						if (item->b.min)
							value = NoYes;
						else
							value = YesNo;

						if (*item->e.flagint & item->a.flagmask)
							str = value[1].name;
						else
							str = value[0].name;

						V_DrawTextCleanMove (CR_GREY, CurrentMenu->indent + 14, y, str);
					}
					break;

				default:
					break;
			}

			if (i == CurrentItem && (skullAnimCounter < 6 || WaitingForKey)) {
				V_DrawPatchClean (CurrentMenu->indent + 3, y, &screen, W_CacheLumpName ("LITLCURS", PU_CACHE));
			}

		} else {

			char *str;

			for (x = 0; x < 3; x++) {
				switch (x) {
					case 0:
						str = item->b.res1;
						break;
					case 1:
						str = item->c.res2;
						break;
					case 2:
						str = item->d.res3;
						break;
				}
				if (str) {
					if (x == item->e.highlight)
						color = CR_GREY;
					else
						color = CR_RED;

					V_DrawTextCleanMove (color, 104 * x + 20, y, str);
				}
			}

			if (i == CurrentItem && ((item->a.selmode != -1 && (skullAnimCounter < 6 || WaitingForKey)) || testingmode)) {
				V_DrawPatchClean (item->a.selmode * 104 + 8, y, &screen, W_CacheLumpName ("LITLCURS", PU_CACHE));
			}

		}
	}

	if (flagsvar) {
		char flagsblah[64];

		sprintf (flagsblah, "%s = %s", flagsvar->name, flagsvar->string);
		V_DrawTextCleanMove (CR_GREY, 160 - (V_StringWidth (flagsblah) >> 1), 190, flagsblah);
	}
}

void M_OptResponder (event_t *ev)
{
	menuitem_t *item;
	int ch = ev->data1;
	
	item = CurrentMenu->items + CurrentItem;

	if (WaitingForKey) {
		if (ch != KEY_ESCAPE) {
			C_ChangeBinding (item->e.command, ch);
			M_BuildKeyList (CurrentMenu->items, CurrentMenu->numitems);
		}
		WaitingForKey = false;
		CurrentMenu->items[0].label = OldMessage;
		CurrentMenu->items[0].type = OldType;
		return;
	}

	if (item->type == bitflag && flagsvar &&
		(ch == KEY_LEFTARROW || ch == KEY_RIGHTARROW || ch == KEY_ENTER)
		&& !demoplayback) {
			int newflags = *item->e.flagint ^ item->a.flagmask;
			char val[16];

			sprintf (val, "%d", newflags);
			SetCVar (flagsvar, val);
			return;
	}

	switch (ch) {
		case KEY_DOWNARROW:
			{
				int modecol;

				if (item->type == screenres) {
					modecol = item->a.selmode;
					item->a.selmode = -1;
				} else {
					modecol = 0;
				}

				do {
					if (++CurrentItem == CurrentMenu->numitems)
						CurrentItem = 0;
				} while (CurrentMenu->items[CurrentItem].type == redtext ||
						 CurrentMenu->items[CurrentItem].type == whitetext ||
						 (CurrentMenu->items[CurrentItem].type == screenres &&
						  !CurrentMenu->items[CurrentItem].b.res1));

				if (CurrentMenu->items[CurrentItem].type == screenres)
					CurrentMenu->items[CurrentItem].a.selmode = modecol;

				S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
			}
			break;

		case KEY_UPARROW:
			{
				int modecol;

				if (item->type == screenres) {
					modecol = item->a.selmode;
					item->a.selmode = -1;
				} else {
					modecol = 0;
				}

				do {
					if (--CurrentItem < 0)
						CurrentItem = CurrentMenu->numitems - 1;
				} while (CurrentMenu->items[CurrentItem].type == redtext ||
						 CurrentMenu->items[CurrentItem].type == whitetext ||
						 (CurrentMenu->items[CurrentItem].type == screenres &&
						  !CurrentMenu->items[CurrentItem].b.res1));

				if (CurrentMenu->items[CurrentItem].type == screenres)
					CurrentMenu->items[CurrentItem].a.selmode = modecol;

				S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
			}
			break;

		case KEY_LEFTARROW:
			switch (item->type) {
				case slider:
					{
						float newval = (*item->a.cvar)->value - item->d.step;

						if (newval < item->b.min)
							newval = item->b.min;

						if (item->e.cfunc)
							item->e.cfunc (*item->a.cvar, newval);
						else
							SetCVarFloat (*item->a.cvar, newval);
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
					break;

				case discrete:
				case cdiscrete:
					{
						int cur;
						int numvals;

						numvals = (int)item->b.min;
						cur = M_FindCurVal ((*item->a.cvar)->value, item->e.values, numvals);
						if (--cur < 0)
							cur = numvals - 1;

						SetCVarFloat (*item->a.cvar, item->e.values[cur].value);

						// Hack hack. Rebuild list of resolutions
						if (item->e.values == Depths)
							BuildModesList (screen.width, screen.height, DisplayID);
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
					break;

				case screenres:
					{
						int col;

						col = item->a.selmode - 1;
						if (col < 0) {
							if (CurrentItem > 0) {
								if (CurrentMenu->items[CurrentItem - 1].type == screenres) {
									item->a.selmode = -1;
									CurrentMenu->items[--CurrentItem].a.selmode = 2;
								}
							}
						} else {
							item->a.selmode = col;
						}
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
					break;

				default:
					break;
			}
			break;

		case KEY_RIGHTARROW:
			switch (item->type) {
				case slider:
					{
						float newval = (*item->a.cvar)->value + item->d.step;

						if (newval > item->c.max)
							newval = item->c.max;

						if (item->e.cfunc)
							item->e.cfunc (*item->a.cvar, newval);
						else
							SetCVarFloat (*item->a.cvar, newval);
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
					break;

				case discrete:
				case cdiscrete:
					{
						int cur;
						int numvals;

						numvals = (int)item->b.min;
						cur = M_FindCurVal ((*item->a.cvar)->value, item->e.values, numvals);
						if (++cur >= numvals)
							cur = 0;

						SetCVarFloat (*item->a.cvar, item->e.values[cur].value);

						// Hack hack. Rebuild list of resolutions
						if (item->e.values == Depths)
							BuildModesList (screen.width, screen.height, DisplayID);
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
					break;

				case screenres:
					{
						int col;

						col = item->a.selmode + 1;
						if ((col > 2) || (col == 2 && !item->d.res3) || (col == 1 && !item->c.res2)) {
							if (CurrentMenu->numitems - 1 > CurrentItem) {
								if (CurrentMenu->items[CurrentItem + 1].type == screenres) {
									if (CurrentMenu->items[CurrentItem + 1].b.res1) {
										item->a.selmode = -1;
										CurrentMenu->items[++CurrentItem].a.selmode = 0;
									}
								}
							}
						} else {
							item->a.selmode = col;
						}
					}
					S_Sound (NULL, CHAN_VOICE, "plats/pt1_stop", 1, ATTN_NONE);
					break;

				default:
					break;
			}
			break;

		case KEY_BACKSPACE:
			if (item->type == control) {
				C_UnbindACommand (item->e.command);
				item->b.key1 = item->c.key2 = 0;
			}
			break;

		case KEY_ENTER:
			if (CurrentMenu == &ModesMenu) {
#ifndef DJGPP
				SetCVarFloat (fullscreen, DummyFSCvar.value);
#endif
				if (!(item->type == screenres && GetSelectedSize (CurrentItem, &NewWidth, &NewHeight))) {
					NewWidth = screen.width;
					NewHeight = screen.height;
				}
				NewID = IDTranslate[(int)DummyDepthCvar.value];
				setmodeneeded = true;
				S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
				SetModesMenu (NewWidth, NewHeight, NewID);
			} else if (item->type == more && item->e.mfunc) {
				CurrentMenu->lastOn = CurrentItem;
				S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
				item->e.mfunc();
			} else if (item->type == discrete || item->type == cdiscrete) {
				int cur;
				int numvals;

				numvals = (int)item->b.min;
				cur = M_FindCurVal ((*item->a.cvar)->value, item->e.values, numvals);
				if (++cur >= numvals)
					cur = 0;

				SetCVarFloat (*item->a.cvar, item->e.values[cur].value);

				// Hack hack. Rebuild list of resolutions
				if (item->e.values == Depths)
					BuildModesList (screen.width, screen.height, DisplayID);

				S_Sound (NULL, CHAN_VOICE, "plats/pt1_mid", 1, ATTN_NONE);
			} else if (item->type == control) {
				WaitingForKey = true;
				OldMessage = CurrentMenu->items[0].label;
				OldType = CurrentMenu->items[0].type;
				CurrentMenu->items[0].label = "Press new key for control or ESC to cancel";
				CurrentMenu->items[0].type = redtext;
			} else if (item->type == listelement) {
				CurrentMenu->lastOn = CurrentItem;
				S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
				item->e.lfunc (CurrentItem);
			} else if (item->type == screenres) {
			}
			break;

		case KEY_ESCAPE:
			CurrentMenu->lastOn = CurrentItem;
			M_PopMenuStack ();
			break;

		default:
			if (ev->data2 == 't') {
				// Test selected resolution
				if (CurrentMenu == &ModesMenu) {
#ifndef DJGPP
					OldFS = fullscreen->value;
					SetCVarFloat (fullscreen, DummyFSCvar.value);
#endif
					if (!(item->type == screenres && GetSelectedSize (CurrentItem, &NewWidth, &NewHeight))) {
						NewWidth = screen.width;
						NewHeight = screen.height;
					}
					OldWidth = screen.width;
					OldHeight = screen.height;
					OldID = DisplayID;
					NewID = IDTranslate[(int)DummyDepthCvar.value];
					setmodeneeded = true;
					testingmode = I_GetTime() + 5 * TICRATE;
					S_Sound (NULL, CHAN_VOICE, "weapons/pistol", 1, ATTN_NONE);
					SetModesMenu (NewWidth, NewHeight, NewID);
				}
			} else if (ev->data2 == 'd' && CurrentMenu == &ModesMenu) {
				// Make current resolution the default
				SetCVarFloat (vid_defwidth, (float)screen.width);
				SetCVarFloat (vid_defheight, (float)screen.height);
				SetCVar (vid_defid, IdStrings[DisplayID-1000]);
				SetModesMenu (screen.width, screen.height, DisplayID);
			}
			break;
	}
}

static void GoToConsole (void)
{
	M_ClearMenus ();
	C_ToggleConsole ();
}

static void UpdateStuff (void)
{
	M_SizeDisplay (0.0);
}

void Reset2Defaults (void)
{
	AddCommandString ("unbindall; binddefaults");
	C_SetCVarsToDefaults ();
	UpdateStuff();
}

void Reset2Saved (void)
{
	char *execcommand;
	char *configfile = GetConfigPath ();

	execcommand = Malloc (strlen (configfile) + 8);
	sprintf (execcommand, "exec \"%s\"", configfile);
	free (configfile);
	AddCommandString (execcommand);
	free (execcommand);
	UpdateStuff();
}

static void StartMessagesMenu (void)
{
	M_SwitchMenu (&MessagesMenu);
}

static void CustomizeControls (void)
{
	M_BuildKeyList (ControlsMenu.items, ControlsMenu.numitems);
	M_SwitchMenu (&ControlsMenu);
}

void Cmd_Menu_Keys (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	OptionsActive = true;
	CustomizeControls();
}

static void GameplayOptions (void)
{
	M_SwitchMenu (&DMFlagsMenu);
	flagsvar = dmflagsvar;
}

void Cmd_Menu_Gameplay (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	OptionsActive = true;
	GameplayOptions ();
}

static void VideoOptions (void)
{
	M_SwitchMenu (&VideoMenu);
}

void Cmd_Menu_Display (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	OptionsActive = true;
	M_SwitchMenu (&VideoMenu);
}

static void BuildModesList (int hiwidth, int hiheight, int hi_id)
{
	char strtemp[32], **str;
	int	 i, c;
	int	 width, height, showid;

	showid = IDTranslate[(int)DummyDepthCvar.value];

	I_StartModeIterator (showid);

	for (i = VM_RESSTART; ModesItems[i].type == screenres; i++) {
		ModesItems[i].e.highlight = -1;
		for (c = 0; c < 3; c++) {
			switch (c) {
				case 0:
					str = &ModesItems[i].b.res1;
					break;
				case 1:
					str = &ModesItems[i].c.res2;
					break;
				case 2:
					str = &ModesItems[i].d.res3;
					break;
			}
			if (I_NextMode (&width, &height)) {
				if (/* hi_id == showid && */ width == hiwidth && height == hiheight)
					ModesItems[i].e.highlight = ModesItems[i].a.selmode = c;
				
				sprintf (strtemp, "%dx%d", width, height);
				ReplaceString (str, strtemp);
			} else {
				if (*str) {
					free (*str);
					*str = NULL;
				}
			}
		}
	}
}

static BOOL GetSelectedSize (int line, int *width, int *height)
{
	int i, stopat;

	if (ModesItems[line].type != screenres) {
		return false;
	} else {
		I_StartModeIterator (IDTranslate[(int)DummyDepthCvar.value]);

		stopat = (line - VM_RESSTART) * 3 + ModesItems[line].a.selmode + 1;

		for (i = 0; i < stopat; i++)
			if (!I_NextMode (width, height))
				return false;
	}

	return true;
}

static int FindId (int id)
{
	int i;

	for (i = 0; i < 22; i++) {
		if (IDTranslate[i] == id)
			return i;
	}

	return 0;
}

static void SetModesMenu (int w, int h, int id)
{
	char strtemp[64];

	SetCVarFloat (&DummyDepthCvar, (float)FindId (id));
#ifndef DJGPP
	SetCVarFloat (&DummyFSCvar, fullscreen->value);
#endif

	if (!testingmode) {
		if (ModesItems[VM_ENTERLINE].label != VMEnterText)
			free (ModesItems[VM_ENTERLINE].label);
		ModesItems[VM_ENTERLINE].label = VMEnterText;
		ModesItems[VM_TESTLINE].label = VMTestText;

		sprintf (strtemp, "D to make %dx%d (%s) the default", w, h, IdStrings[id-1000]);
		ReplaceString (&ModesItems[VM_MAKEDEFLINE].label, strtemp);

		sprintf (strtemp, "Current default is %dx%d (%s)",
				 (int)vid_defwidth->value,
				 (int)vid_defheight->value,
				 vid_defid->string);
		ReplaceString (&ModesItems[VM_CURDEFLINE].label, strtemp);
	} else {
		sprintf (strtemp, "TESTING %dx%d (%s)", w, h, IdStrings[id-1000]);
		ModesItems[VM_ENTERLINE].label = copystring (strtemp);
		ModesItems[VM_TESTLINE].label = "Please wait 5 seconds...";
		ModesItems[VM_MAKEDEFLINE].label = copystring (" ");
		ModesItems[VM_CURDEFLINE].label = copystring (" ");
	}

	BuildModesList (w, h, id);
}

void M_RestoreMode (void)
{
	NewWidth = OldWidth;
	NewHeight = OldHeight;
	NewID = OldID;
	setmodeneeded = true;
	testingmode = 0;
#ifndef DJGPP
	SetCVarFloat (fullscreen, OldFS);
#endif
	SetModesMenu (OldWidth, OldHeight, OldID);
}

static void SetVidMode (void)
{
	SetModesMenu (screen.width, screen.height, DisplayID);
	if (ModesMenu.items[ModesMenu.lastOn].type == screenres) {
		if (ModesMenu.items[ModesMenu.lastOn].a.selmode == -1) {
			ModesMenu.items[ModesMenu.lastOn].a.selmode++;
		}
	}
	M_SwitchMenu (&ModesMenu);
}

void Cmd_Menu_Video (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_Sound (NULL, CHAN_VOICE, "switches/normbutn", 1, ATTN_NONE);
	OptionsActive = true;
	SetVidMode ();
}
