/*
** m_options.cpp
** New options menu code
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** Sorry this got so convoluted. It was originally much cleaner until
** I started adding all sorts of gadgets to the menus. I might someday
** make a project of rewriting the entire menu system using Amiga-style
** taglists to describe each menu item. We'll see... (Probably not.)
*/

#include "m_alloc.h"
#include "templates.h"

#include "doomdef.h"
#include "gstrings.h"

#include "c_console.h"
#include "c_dispatch.h"
#include "c_bind.h"

#include "d_main.h"
#include "d_gui.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "gi.h"

#include "r_local.h"
#include "gameconfigfile.h"

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
CVAR (Float, mouse_sensitivity, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// Show messages has default, 0 = off, 1 = on
CVAR (Bool, show_messages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

extern bool	OptionsActive;

extern int	skullAnimCounter;

EXTERN_CVAR (Bool, cl_run)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookspring)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Int, crosshair)
EXTERN_CVAR (Bool, freelook)

void M_ChangeMessages ();
void M_SizeDisplay (int diff);

int  M_StringHeight (char *string);
void M_ClearMenus ();

EColorRange LabelColor;
EColorRange ValueColor;
EColorRange MoreColor;

static bool CanScrollUp;
static bool CanScrollDown;
static int VisBottom;

value_t YesNo[2] = {
	{ 0.0, "No" },
	{ 1.0, "Yes" }
};

value_t NoYes[2] = {
	{ 0.0, "Yes" },
	{ 1.0, "No" }
};

value_t OnOff[2] = {
	{ 0.0, "Off" },
	{ 1.0, "On" }
};

menu_t  *CurrentMenu;
int		CurrentItem;
int		WaitingForKey;
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

EXTERN_CVAR (Float, snd_midivolume)

static void SetVidMode (void);

static menuitem_t OptionItems[] =
{
	{ more,		"Customize Controls",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)CustomizeControls} },
	{ more,		"Go to console",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GoToConsole} },
	{ more,		"Gameplay Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GameplayOptions} },
	{ more,		"Display Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)VideoOptions} },
	{ more,		"Set video mode",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SetVidMode} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Mouse speed",			{&mouse_sensitivity},	{0.5}, {2.5},	{0.1}, {NULL} },
#ifdef _WIN32
	{ slider,	"MIDI music volume",	{&snd_midivolume},		{0.0}, {1.0},	{0.05}, {NULL} },
#endif
	{ slider,	"MOD music volume",		{&snd_musicvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ slider,	"Sound volume",			{&snd_sfxvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ discrete,	"Always Run",			{&cl_run},				{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Always Mouselook",		{&freelook},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Invert Mouse",			{&invertmouse},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookspring",			{&lookspring},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookstrafe",			{&lookstrafe},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Reset to defaults",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Defaults} },
	{ more,		"Reset to last saved",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Saved} }
};

menu_t OptionMenu = {
	{ 'M','_','O','P','T','T','T','L' },
	"OPTIONS",
	0,
	sizeof(OptionItems)/sizeof(OptionItems[0]),
	177,
	OptionItems,
};


/*=======================================
 *
 * Controls Menu
 *
 *=======================================*/

static menuitem_t ControlsItems[] =
{
	{ redtext,"ENTER to change, BACKSPACE to clear", {NULL}, {0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Controls",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Fire",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+attack"} },
	{ control,	"Use / Open",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+use"} },
	{ control,	"Move forward",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+forward"} },
	{ control,	"Move backward",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+back"} },
	{ control,	"Strafe left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveleft"} },
	{ control,	"Strafe right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveright"} },
	{ control,	"Turn left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+left"} },
	{ control,	"Turn right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+right"} },
	{ control,	"Jump",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+jump"} },
	{ control,	"Fly / Swim up",		{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"+moveup"} },
	{ control,	"Fly / Swim down",		{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"+movedown"} },
	{ control,	"Stop flying",			{NULL},	{0.0}, {0.0}, {0.0}, {(value_t *)"land"} },
	{ control,	"Mouse look",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+mlook"} },
	{ control,	"Keyboard look",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+klook"} },
	{ control,	"Look up",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+lookup"} },
	{ control,	"Look down",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+lookdown"} },
	{ control,	"Center view",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"centerview"} },
	{ control,	"Run",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+speed"} },
	{ control,	"Strafe",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+strafe"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Chat",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Say",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"messagemode"} },
	{ control,	"Team say",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"messagemode2"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Weapons",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Next weapon",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapnext"} },
	{ control,	"Previous weapon",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapprev"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Inventory",			{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Activate item",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invuse"} },
	{ control,	"Activate all items",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invuseall"} },
	{ control,	"Next item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invnext"} },
	{ control,	"Previous item",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invprev"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Other",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Toggle automap",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"togglemap"} },
	{ control,	"Chasecam",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"chase"} },
	{ control,	"Add a bot",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"addbot"} },
	{ control,	"Coop spy",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"spynext"} },
	{ control,	"Screenshot",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"screenshot"} },
	{ control,  "Open console",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"toggleconsole"} }
};

static TArray<menuitem_t> CustomControlsItems (0);

menu_t ControlsMenu = {
	{ 'M','_','C','O','N','T','R','O' },
	"CUSTOMIZE CONTROLS",
	3,
	sizeof(ControlsItems)/sizeof(ControlsItems[0]),
	0,
	ControlsItems,
	2,
};

/*=======================================
 *
 * Display Options Menu
 *
 *=======================================*/
static void StartMessagesMenu (void);

EXTERN_CVAR (Bool, am_rotate)
EXTERN_CVAR (Bool, am_overlay)
EXTERN_CVAR (Bool, st_scale)
EXTERN_CVAR (Bool, am_usecustomcolors)
EXTERN_CVAR (Int,  r_detail)
EXTERN_CVAR (Bool, r_stretchsky)
EXTERN_CVAR (Int,  r_columnmethod)
EXTERN_CVAR (Bool, r_drawfuzz)
EXTERN_CVAR (Int,  cl_rockettrails)
EXTERN_CVAR (Int,  cl_pufftype)
EXTERN_CVAR (Int,  cl_bloodtype)
EXTERN_CVAR (Int,  wipetype)
EXTERN_CVAR (Bool, vid_palettehack)
EXTERN_CVAR (Bool, vid_attachedsurfaces)
EXTERN_CVAR (Int,  screenblocks)

static value_t Crosshairs[] =
{
	{ 0.0, "None" },
	{ 1.0, "Cross 1" },
	{ 2.0, "Cross 2" },
	{ 3.0, "X" },
	{ 4.0, "Circle" },
	{ 5.0, "Angle" },
	{ 6.0, "Triangle" },
	{ 7.0, "Dot" }
};

static value_t DetailModes[] =
{
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
	{ more,		"Messages",				{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartMessagesMenu} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Screen size",			{&screenblocks},	   	{3.0}, {12.0},	{1.0}, {NULL} },
	{ slider,	"Brightness",			{&Gamma},			   	{1.0}, {3.0},	{0.1}, {NULL} },
	{ discrete,	"Crosshair",			{&crosshair},		   	{8.0}, {0.0},	{0.0}, {Crosshairs} },
	{ discrete, "Column render mode",	{&r_columnmethod},		{2.0}, {0.0},	{0.0}, {ColumnMethods} },
	{ discrete, "Detail mode",			{&r_detail},		   	{4.0}, {0.0},	{0.0}, {DetailModes} },
	{ discrete, "Stretch short skies",	{&r_stretchsky},	   	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Stretch status bar",	{&st_scale},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Screen wipe style",	{&wipetype},			{4.0}, {0.0},	{0.0}, {Wipes} },
#ifdef _WIN32
	{ discrete, "DirectDraw palette hack", {&vid_palettehack},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Use attached surfaces", {&vid_attachedsurfaces},{2.0}, {0.0},	{0.0}, {OnOff} },
#endif
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Use fuzz effect",		{&r_drawfuzz},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Rocket Trails",		{&cl_rockettrails},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Blood Type",			{&cl_bloodtype},	   	{3.0}, {0.0},	{0.0}, {BloodTypes} },
	{ discrete, "Bullet Puff Type",		{&cl_pufftype},			{2.0}, {0.0},	{0.0}, {PuffTypes} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Rotate automap",		{&am_rotate},		   	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Overlay automap",		{&am_overlay},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Standard map colors",	{&am_usecustomcolors},	{2.0}, {0.0},	{0.0}, {NoYes} },
};

menu_t VideoMenu = {
	"M_VIDEO",
	"DISPLAY OPTIONS",
	0,
#ifdef _WIN32
	21,
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
EXTERN_CVAR (Bool, con_scaletext)
EXTERN_CVAR (Bool, con_centernotify)
EXTERN_CVAR (Int,  msg0color)
EXTERN_CVAR (Int,  msg1color)
EXTERN_CVAR (Int,  msg2color)
EXTERN_CVAR (Int,  msg3color)
EXTERN_CVAR (Int,  msg4color)
EXTERN_CVAR (Int,  msgmidcolor)
EXTERN_CVAR (Int,  msglevel)

static value_t TextColors[] =
{
	{ 0.0, "brick" },
	{ 1.0, "tan" },
	{ 2.0, "gray" },
	{ 3.0, "green" },
	{ 4.0, "brown" },
	{ 5.0, "gold" },
	{ 6.0, "red" },
	{ 7.0, "blue" },
	{ 8.0, "orange" },
	{ 9.0, "white" },
	{ 10.0, "yellow" }
};

static value_t MessageLevels[] = {
	{ 0.0, "Item Pickup" },
	{ 1.0, "Obituaries" },
	{ 2.0, "Critical Messages" }
};

static menuitem_t MessagesItems[] = {
	{ discrete,	"Scale text in high res", {&con_scaletext},		{2.0}, {0.0}, 	{0.0}, {OnOff} },
	{ discrete, "Minimum message level", {&msglevel},		   	{3.0}, {0.0},   {0.0}, {MessageLevels} },
	{ discrete, "Center messages",		{&con_centernotify},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext, "Message Colors",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ cdiscrete, "Item Pickup",			{&msg0color},		   	{11.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Obituaries",			{&msg1color},		   	{11.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Critical Messages",	{&msg2color},		   	{11.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Chat Messages",		{&msg3color},		   	{11.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Team Messages",		{&msg4color},		   	{11.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Centered Messages",	{&msgmidcolor},			{11.0}, {0.0},	{0.0}, {TextColors} }
};

menu_t MessagesMenu = {
	"M_MESS",
	"MESSAGES",
	0,
	12,
	0,
	MessagesItems,
};


/*=======================================
 *
 * Video Modes Menu
 *
 *=======================================*/

extern BOOL setmodeneeded;
extern int NewWidth, NewHeight, NewBits;
extern int DisplayBits;

int testingmode;		// Holds time to revert to old mode
int OldWidth, OldHeight, OldBits;

static void BuildModesList (int hiwidth, int hiheight, int hi_id);
static BOOL GetSelectedSize (int line, int *width, int *height);
static void SetModesMenu (int w, int h, int bits);

EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Int, vid_defbits)

static FIntCVar DummyDepthCvar (NULL, 0, 0);

EXTERN_CVAR (Bool, fullscreen)

static value_t Depths[22];

static char VMEnterText[] = "Press ENTER to set mode";
static char VMTestText[] = "T to test mode for 5 seconds";

static menuitem_t ModesItems[] = {
	{ discrete, "Screen mode",			{&DummyDepthCvar},		{0.0}, {0.0},	{0.0}, {Depths} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Fullscreen",			{&fullscreen},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,{NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext,"Note: Only 8 bpp modes are supported",{NULL},	{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMEnterText,			{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMTestText,				{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  {NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  {NULL},					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} }
};

#define VM_DEPTHITEM	0
#define VM_RESSTART		4
#define VM_ENTERLINE	14
#define VM_TESTLINE		16
#define VM_MAKEDEFLINE	18
#define VM_CURDEFLINE	19

menu_t ModesMenu = {
	{ 'M','_','V','I','D','M','O','D' },
	"VIDEO MODE",
	2,
	20,
	130,
	ModesItems,
};

/*=======================================
 *
 * Gameplay Options (dmflags) Menu
 *
 *=======================================*/

static FIntCVar *flagsvar;

static menuitem_t DMFlagsItems[] = {
	{ bitflag,	"Falling damage (old)",	{(FBaseCVar *)DF_FORCE_FALLINGZD},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Falling damage (Hexen)", {(FBaseCVar *)DF_FORCE_FALLINGHX},{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Weapons stay (DM)",	{(FBaseCVar *)DF_WEAPONS_STAY},		{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow powerups (DM)",	{(FBaseCVar *)DF_NO_ITEMS},			{1}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow health (DM)",	{(FBaseCVar *)DF_NO_HEALTH},		{1}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow armor (DM)",		{(FBaseCVar *)DF_NO_ARMOR},			{1}, {0}, {0}, {NULL} },
	{ bitflag,	"Spawn farthest (DM)",	{(FBaseCVar *)DF_SPAWN_FARTHEST},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Same map (DM)",		{(FBaseCVar *)DF_SAME_LEVEL},		{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Force respawn (DM)",	{(FBaseCVar *)DF_FORCE_RESPAWN},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow exit (DM)",		{(FBaseCVar *)DF_NO_EXIT},			{1}, {0}, {0}, {NULL} },
	{ bitflag,	"Infinite ammo",		{(FBaseCVar *)DF_INFINITE_AMMO},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"No monsters",			{(FBaseCVar *)DF_NO_MONSTERS},		{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Monsters respawn",		{(FBaseCVar *)DF_MONSTERS_RESPAWN},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Items respawn",		{(FBaseCVar *)DF_ITEMS_RESPAWN},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Fast monsters",		{(FBaseCVar *)DF_FAST_MONSTERS},	{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow jump",			{(FBaseCVar *)DF_NO_JUMP},			{1}, {0}, {0}, {NULL} },
	{ bitflag,	"Allow freelook",		{(FBaseCVar *)DF_NO_FREELOOK},		{1}, {0}, {0}, {NULL} },
	{ redtext,	" ",					{NULL},								{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Team damage scalar",	{&teamdamage},						{0.0}, {1.0},	{0.05},{NULL} },
	{ discrete, "Teamplay",				{&teamplay},						{2.0}, {0.0},	{0.0}, {OnOff} }
};

static menu_t DMFlagsMenu = {
	{ 'M','_','G','M','P','L','A','Y' },
	"GAMEPLAY OPTIONS",
	0,
	20,
	0,
	DMFlagsItems,
};

void M_FreeValues (value_t **values, int num)
{
	int i;

	if (*values)
	{
		for (i = 0; i < num; i++)
		{
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
static byte BitTranslate[16];

void M_OptInit (void)
{
	int currval = 0, dummy1, dummy2, i;
	char name[24];

	for (i = 1; i < 32; i++)
	{
		I_StartModeIterator (i);
		if (I_NextMode (&dummy1, &dummy2))
		{
			Depths[currval].value = currval;
			sprintf (name, "%d bit", i);
			Depths[currval].name = copystring (name);
			BitTranslate[currval] = i;
			currval++;
		}
	}

	ModesItems[VM_DEPTHITEM].b.min = (float)currval;

	switch (I_DisplayType ())
	{
	case DISPLAY_FullscreenOnly:
		ModesItems[2].type = nochoice;
		ModesItems[2].b.min = 1.f;
		break;
	case DISPLAY_WindowOnly:
		ModesItems[2].type = nochoice;
		ModesItems[2].b.min = 0.f;
		break;
	default:
		break;
	}

	if (gameinfo.gametype == GAME_Doom)
	{
		LabelColor = CR_UNTRANSLATED;
		ValueColor = CR_GRAY;
		MoreColor = CR_GRAY;
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		LabelColor = CR_GREEN;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}
	else // Hexen
	{
		LabelColor = CR_RED;
		ValueColor = CR_UNTRANSLATED;
		MoreColor = CR_UNTRANSLATED;
	}
}


//
//		Toggle messages on/off
//
void M_ChangeMessages ()
{
	if (show_messages)
	{
		Printf (128, "%s\n", GStrings(MSGOFF));
		show_messages = false;
	}
	else
	{
		Printf (128, "%s\n", GStrings(MSGON));
		show_messages = true;
	}
}

CCMD (togglemessages)
{
	M_ChangeMessages ();
}

void M_SizeDisplay (int diff)
{
	// changing screenblocks automatically resizes the display
	screenblocks = screenblocks + diff;
}

CCMD (sizedown)
{
	M_SizeDisplay (-1);
	S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
}

CCMD (sizeup)
{
	M_SizeDisplay (1);
	S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
}

void M_BuildKeyList (menuitem_t *item, int numitems)
{
	int i;

	for (i = 0; i < numitems; i++, item++)
	{
		if (item->type == control)
			C_GetKeysForCommand (item->e.command, &item->b.key1, &item->c.key2);
	}
}

void M_SwitchMenu (menu_t *menu)
{
	int i, widest = 0, thiswidth;
	menuitem_t *item;

	MenuStack[MenuStackDepth].menu.newmenu = menu;
	MenuStack[MenuStackDepth].isNewStyle = true;
	MenuStack[MenuStackDepth].drawSkull = false;
	MenuStackDepth++;

	CanScrollUp = false;
	CanScrollDown = false;
	CurrentMenu = menu;
	CurrentItem = menu->lastOn;

	if (!menu->indent)
	{
		for (i = 0; i < menu->numitems; i++)
		{
			item = menu->items + i;
			if (item->type != whitetext && item->type != redtext)
			{
				thiswidth = screen->StringWidth (item->label);
				if (thiswidth > widest)
					widest = thiswidth;
			}
		}
		menu->indent = widest + 6;
	}

	flagsvar = NULL;
}

bool M_StartOptionsMenu (void)
{
	M_SwitchMenu (&OptionMenu);
	return true;
}

void M_DrawSlider (int x, int y, float min, float max, float cur)
{
	float range;

	range = max - min;

	if (cur > max)
		cur = max;
	else if (cur < min)
		cur = min;

	cur -= min;

	screen->SetFont (ConFont);
	screen->DrawTextCleanMove (CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
	screen->DrawTextCleanMove (CR_ORANGE, x + 5 + (int)((cur * 78.f) / range), y, "\x13");
	screen->SetFont (SmallFont);
}

int M_FindCurVal (float cur, value_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

void M_OptDrawer ()
{
	EColorRange color;
	int y, width, i, x, ytop;
	menuitem_t *item;
	UCVarValue value;

	i = W_CheckNumForName (CurrentMenu->title);
	if (gameinfo.gametype == GAME_Doom && i >= 0)
	{
		patch_t *title = (patch_t *)W_CacheLumpNum (i, PU_CACHE);
		screen->DrawPatchClean (title, 160-SHORT(title->width)/2, 10);
		y = 15 + title->height;
	}
	else if (BigFont && CurrentMenu->texttitle)
	{
		screen->SetFont (BigFont);
		screen->DrawTextCleanMove (CR_UNTRANSLATED,
			160-screen->StringWidth (CurrentMenu->texttitle)/2, 10,
			CurrentMenu->texttitle);
		screen->SetFont (SmallFont);
		y = 15 + BigFont->GetHeight ();
	}
	else
	{
		y = 15;
	}

	ytop = y + CurrentMenu->scrolltop * 8;
	for (i = 0; i < CurrentMenu->numitems &&
		y <= 200 - SmallFont->GetHeight (); i++, y += 8)
	{
		if (i == CurrentMenu->scrolltop)
		{
			i += CurrentMenu->scrollpos;
		}

		item = CurrentMenu->items + i;

		if (item->type != screenres)
		{
			width = screen->StringWidth (item->label);
			switch (item->type)
			{
			case more:
				x = CurrentMenu->indent - width;
				color = MoreColor;
				break;

			case redtext:
				x = 160 - width / 2;
				color = LabelColor;
				break;

			case whitetext:
				x = 160 - width / 2;
				color = ValueColor;
				break;

			case listelement:
				x = CurrentMenu->indent + 14;
				color = LabelColor;
				break;

			default:
				x = CurrentMenu->indent - width;
				color = (item->type == control && WaitingForKey && i == CurrentItem)
					? CR_YELLOW : LabelColor;
				break;
			}
			screen->DrawTextCleanMove (color, x, y, item->label);

			switch (item->type)
			{
			case discrete:
			case cdiscrete:
			{
				int v, vals;

				value = item->a.cvar->GetGenericRep (CVAR_Float);
				vals = (int)item->b.min;
				v = M_FindCurVal (value.Float, item->e.values, vals);

				if (v == vals)
				{
					screen->DrawTextCleanMove (ValueColor, CurrentMenu->indent + 14, y, "Unknown");
				}
				else
				{
					if (item->type == cdiscrete)
						screen->DrawTextCleanMove (v, CurrentMenu->indent + 14, y, item->e.values[v].name);
					else
						screen->DrawTextCleanMove (ValueColor, CurrentMenu->indent + 14, y, item->e.values[v].name);
				}

			}
			break;

			case nochoice:
				screen->DrawTextCleanMove (CR_GOLD, CurrentMenu->indent + 14, y,
										   (item->e.values[(int)item->b.min]).name);
				break;

			case slider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (CurrentMenu->indent + 14, y, item->b.min, item->c.max, value.Float);
				break;

			case control:
			{
				char description[64];

				C_NameKeys (description, item->b.key1, item->c.key2);
				screen->SetFont (ConFont);
				screen->DrawTextCleanMove (CR_WHITE,
					CurrentMenu->indent + 14, y-1, description);
				screen->SetFont (SmallFont);
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

				if (flagsvar)
				{
					if (*flagsvar & item->a.flagmask)
						str = value[1].name;
					else
						str = value[0].name;
				}
				else
				{
					str = "???";
				}

				screen->DrawTextCleanMove (ValueColor,
					CurrentMenu->indent + 14, y, str);
			}
			break;

			default:
				break;
			}

			if (i == CurrentItem && (skullAnimCounter < 6 || WaitingForKey))
			{
				screen->SetFont (ConFont);
				screen->DrawTextCleanMove (CR_RED, CurrentMenu->indent + 3, y-1, "\xd");
				screen->SetFont (SmallFont);
			}
		}
		else
		{
			char *str = NULL;

			for (x = 0; x < 3; x++)
			{
				switch (x)
				{
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
				if (str)
				{
					if (x == item->e.highlight)
						color = ValueColor;
					else
						color = LabelColor;

					screen->DrawTextCleanMove (color, 104 * x + 20, y, str);
				}
			}

			if (i == CurrentItem && ((item->a.selmode != -1 && (skullAnimCounter < 6 || WaitingForKey)) || testingmode))
			{
				screen->SetFont (ConFont);
				screen->DrawTextCleanMove (CR_RED, item->a.selmode * 104 + 8, y, "\xd");
				screen->SetFont (SmallFont);
			}
		}
	}

	CanScrollUp = (CurrentMenu->scrollpos != 0);
	CanScrollDown = (i < CurrentMenu->numitems);
	VisBottom = i - 1;

	screen->SetFont (ConFont);
	if (CanScrollUp)
	{
		screen->DrawTextCleanMove (CR_ORANGE, 3, ytop, "\x1a");
	}
	if (CanScrollDown)
	{
		screen->DrawTextCleanMove (CR_ORANGE, 3, y - 8, "\x1b");
	}
	screen->SetFont (SmallFont);

	if (flagsvar)
	{
		char flagsblah[64];

		sprintf (flagsblah, "%s = %d", flagsvar->GetName (), **flagsvar);
		screen->DrawTextCleanMove (ValueColor,
			160 - (screen->StringWidth (flagsblah) >> 1), 190, flagsblah);
	}
}

void M_OptResponder (event_t *ev)
{
	menuitem_t *item;
	int ch = tolower (ev->data1);
	UCVarValue value;

	item = CurrentMenu->items + CurrentItem;

	if (WaitingForKey && ev->type == EV_KeyDown)
	{
		if (ev->data1 != KEY_ESCAPE)
		{
			C_ChangeBinding (item->e.command, ev->data1);
			M_BuildKeyList (CurrentMenu->items, CurrentMenu->numitems);
		}
		WaitingForKey = 0;
		CurrentMenu->items[0].label = OldMessage;
		CurrentMenu->items[0].type = OldType;
		return;
	}

	if (ev->subtype == EV_GUI_KeyRepeat)
	{
		if (ch != GK_LEFT && ch != GK_RIGHT && ch != GK_UP && ch != GK_DOWN)
		{
			return;
		}
	}
	else if (ev->subtype != EV_GUI_KeyDown)
	{
		return;
	}
	
	if (item->type == bitflag && flagsvar &&
		(ch == GK_LEFT || ch == GK_RIGHT || ch == '\r')
		&& !demoplayback)
	{
		*flagsvar = *flagsvar ^ item->a.flagmask;
		return;
	}

	switch (ch)
	{
	case GK_DOWN:
		{
			int modecol;

			if (item->type == screenres)
			{
				modecol = item->a.selmode;
				item->a.selmode = -1;
			}
			else
			{
				modecol = 0;
			}

			do
			{
				CurrentItem++;
				if (CanScrollDown && CurrentItem == VisBottom)
				{
					CurrentMenu->scrollpos++;
					VisBottom++;
				}
				if (CurrentItem == CurrentMenu->numitems)
				{
					CurrentMenu->scrollpos = 0;
					CurrentItem = 0;
				}
			} while (CurrentMenu->items[CurrentItem].type == redtext ||
					 CurrentMenu->items[CurrentItem].type == whitetext ||
					 (CurrentMenu->items[CurrentItem].type == screenres &&
					  !CurrentMenu->items[CurrentItem].b.res1));

			if (CurrentMenu->items[CurrentItem].type == screenres)
				CurrentMenu->items[CurrentItem].a.selmode = modecol;

			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_UP:
		{
			int modecol;

			if (item->type == screenres)
			{
				modecol = item->a.selmode;
				item->a.selmode = -1;
			}
			else
			{
				modecol = 0;
			}

			do
			{
				CurrentItem--;
				if (CanScrollUp &&
					CurrentItem == CurrentMenu->scrolltop + CurrentMenu->scrollpos)
				{
					CurrentMenu->scrollpos--;
				}
				if (CurrentItem < 0)
				{
					CurrentMenu->scrollpos = MAX (0,CurrentMenu->numitems - 22 + CurrentMenu->scrolltop);
					CurrentItem = CurrentMenu->numitems - 1;
				}
			} while (CurrentMenu->items[CurrentItem].type == redtext ||
					 CurrentMenu->items[CurrentItem].type == whitetext ||
					 (CurrentMenu->items[CurrentItem].type == screenres &&
					  !CurrentMenu->items[CurrentItem].b.res1));

			if (CurrentMenu->items[CurrentItem].type == screenres)
				CurrentMenu->items[CurrentItem].a.selmode = modecol;

			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_PGUP:
		if (CanScrollUp)
		{
			CurrentMenu->scrollpos -= VisBottom - CurrentMenu->scrollpos - CurrentMenu->scrolltop;
			if (CurrentMenu->scrollpos < 0)
			{
				CurrentMenu->scrollpos = 0;
			}
			CurrentItem = CurrentMenu->scrolltop + CurrentMenu->scrollpos + 1;
			while (CurrentMenu->items[CurrentItem].type == redtext ||
				   CurrentMenu->items[CurrentItem].type == whitetext ||
				   (CurrentMenu->items[CurrentItem].type == screenres &&
					!CurrentMenu->items[CurrentItem].b.res1))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_PGDN:
		if (CanScrollDown)
		{
			int pagesize = VisBottom - CurrentMenu->scrollpos - CurrentMenu->scrolltop;
			CurrentMenu->scrollpos += pagesize;
			if (CurrentMenu->scrollpos + CurrentMenu->scrolltop + pagesize > CurrentMenu->numitems)
			{
				CurrentMenu->scrollpos = CurrentMenu->numitems - CurrentMenu->scrolltop - pagesize;
			}
			CurrentItem = CurrentMenu->scrolltop + CurrentMenu->scrollpos + 1;
			while (CurrentMenu->items[CurrentItem].type == redtext ||
				   CurrentMenu->items[CurrentItem].type == whitetext ||
				   (CurrentMenu->items[CurrentItem].type == screenres &&
					!CurrentMenu->items[CurrentItem].b.res1))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case GK_LEFT:
		switch (item->type)
		{
			case slider:
				{
					UCVarValue newval;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					newval.Float = value.Float - item->d.step;

					if (newval.Float < item->b.min)
						newval.Float = item->b.min;

					if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case discrete:
			case cdiscrete:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Float);
					cur = M_FindCurVal (value.Float, item->e.values, numvals);
					if (--cur < 0)
						cur = numvals - 1;

					value.Float = item->e.values[cur].value;
					item->a.cvar->SetGenericRep (value, CVAR_Float);

					// Hack hack. Rebuild list of resolutions
					if (item->e.values == Depths)
						BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case screenres:
				{
					int col;

					col = item->a.selmode - 1;
					if (col < 0)
					{
						if (CurrentItem > 0)
						{
							if (CurrentMenu->items[CurrentItem - 1].type == screenres)
							{
								item->a.selmode = -1;
								CurrentMenu->items[--CurrentItem].a.selmode = 2;
							}
						}
					}
					else
					{
						item->a.selmode = col;
					}
				}
				S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case GK_RIGHT:
		switch (item->type)
		{
			case slider:
				{
					UCVarValue newval;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					newval.Float = value.Float + item->d.step;

					if (newval.Float > item->c.max)
						newval.Float = item->c.max;

					if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case discrete:
			case cdiscrete:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Float);
					cur = M_FindCurVal (value.Float, item->e.values, numvals);
					if (++cur >= numvals)
						cur = 0;

					value.Float = item->e.values[cur].value;
					item->a.cvar->SetGenericRep (value, CVAR_Float);

					// Hack hack. Rebuild list of resolutions
					if (item->e.values == Depths)
						BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case screenres:
				{
					int col;

					col = item->a.selmode + 1;
					if ((col > 2) || (col == 2 && !item->d.res3) || (col == 1 && !item->c.res2))
					{
						if (CurrentMenu->numitems - 1 > CurrentItem)
						{
							if (CurrentMenu->items[CurrentItem + 1].type == screenres)
							{
								if (CurrentMenu->items[CurrentItem + 1].b.res1)
								{
									item->a.selmode = -1;
									CurrentMenu->items[++CurrentItem].a.selmode = 0;
								}
							}
						}
					}
					else
					{
						item->a.selmode = col;
					}
				}
				S_Sound (CHAN_VOICE, "menu/cursor", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case '\b':
		if (item->type == control)
		{
			C_UnbindACommand (item->e.command);
			item->b.key1 = item->c.key2 = 0;
		}
		break;

	case '\r':
		if (CurrentMenu == &ModesMenu)
		{
			if (!(item->type == screenres && GetSelectedSize (CurrentItem, &NewWidth, &NewHeight)))
			{
				NewWidth = SCREENWIDTH;
				NewHeight = SCREENHEIGHT;
			}
			NewBits = BitTranslate[DummyDepthCvar];
			setmodeneeded = true;
			S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
			SetModesMenu (NewWidth, NewHeight, NewBits);
		}
		else if (item->type == more && item->e.mfunc)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
			item->e.mfunc();
		}
		else if (item->type == discrete || item->type == cdiscrete)
		{
			int cur;
			int numvals;

			numvals = (int)item->b.min;
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			cur = M_FindCurVal (value.Float, item->e.values, numvals);
			if (++cur >= numvals)
				cur = 0;

			value.Float = item->e.values[cur].value;
			item->a.cvar->SetGenericRep (value, CVAR_Float);

			// Hack hack. Rebuild list of resolutions
			if (item->e.values == Depths)
				BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);

			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == control)
		{
			WaitingForKey = 1;
			OldMessage = CurrentMenu->items[0].label;
			OldType = CurrentMenu->items[0].type;
			CurrentMenu->items[0].label = "Press new key for control, ESC to cancel";
			CurrentMenu->items[0].type = redtext;
		}
		else if (item->type == listelement)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
			item->e.lfunc (CurrentItem);
		}
		else if (item->type == screenres)
		{
		}
		break;

	case GK_ESCAPE:
		CurrentMenu->lastOn = CurrentItem;
		M_PopMenuStack ();
		break;

	default:
		if (ch == 't')
		{
			// Test selected resolution
			if (CurrentMenu == &ModesMenu)
			{
				if (!(item->type == screenres &&
					GetSelectedSize (CurrentItem, &NewWidth, &NewHeight)))
				{
					NewWidth = SCREENWIDTH;
					NewHeight = SCREENHEIGHT;
				}
				OldWidth = SCREENWIDTH;
				OldHeight = SCREENHEIGHT;
				OldBits = DisplayBits;
				NewBits = BitTranslate[DummyDepthCvar];
				setmodeneeded = true;
				testingmode = I_GetTime() + 5 * TICRATE;
				S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
				SetModesMenu (NewWidth, NewHeight, NewBits);
			}
		}
		else if (ch == 'd' && CurrentMenu == &ModesMenu)
		{
			// Make current resolution the default
			vid_defwidth = SCREENWIDTH;
			vid_defheight = SCREENHEIGHT;
			vid_defbits = DisplayBits;
			SetModesMenu (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
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
	M_SizeDisplay (0);
}

void Reset2Defaults (void)
{
	C_SetDefaultBindings ();
	C_SetCVarsToDefaults ();
	UpdateStuff();
}

void Reset2Saved (void)
{
	GameConfig->DoGlobalSetup ();
	GameConfig->DoGameSetup (GameNames[gameinfo.gametype]);
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

CCMD (menu_keys)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	CustomizeControls ();
}

EXTERN_CVAR (Int, dmflags)

static void GameplayOptions (void)
{
	M_SwitchMenu (&DMFlagsMenu);
	flagsvar = &dmflags;
}

CCMD (menu_gameplay)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	GameplayOptions ();
}

static void VideoOptions (void)
{
	M_SwitchMenu (&VideoMenu);
}

CCMD (menu_display)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	M_SwitchMenu (&VideoMenu);
}

static void BuildModesList (int hiwidth, int hiheight, int hi_bits)
{
	char strtemp[32], **str;
	int	 i, c;
	int	 width, height, showbits;

	showbits = BitTranslate[DummyDepthCvar];

	I_StartModeIterator (showbits);

	for (i = VM_RESSTART; ModesItems[i].type == screenres; i++)
	{
		ModesItems[i].e.highlight = -1;
		for (c = 0; c < 3; c++)
		{
			if (c == 0)
			{
				str = &ModesItems[i].b.res1;
			}
			else if (c == 1)
			{
				str = &ModesItems[i].c.res2;
			}
			else
			{
				str = &ModesItems[i].d.res3;
			}
			if (I_NextMode (&width, &height))
			{
				if (/* hi_bits == showbits && */ width == hiwidth && height == hiheight)
					ModesItems[i].e.highlight = ModesItems[i].a.selmode = c;
				
				sprintf (strtemp, "%dx%d", width, height);
				ReplaceString (str, strtemp);
			}
			else
			{
				if (*str)
				{
					free (*str);
					*str = NULL;
				}
			}
		}
	}
}

void M_RefreshModesList ()
{
	BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
}

static BOOL GetSelectedSize (int line, int *width, int *height)
{
	int i, stopat;

	if (ModesItems[line].type != screenres)
	{
		return false;
	}
	else
	{
		I_StartModeIterator (BitTranslate[DummyDepthCvar]);

		stopat = (line - VM_RESSTART) * 3 + ModesItems[line].a.selmode + 1;

		for (i = 0; i < stopat; i++)
			if (!I_NextMode (width, height))
				return false;
	}

	return true;
}

static int FindBits (int bits)
{
	int i;

	for (i = 0; i < 22; i++)
	{
		if (BitTranslate[i] == bits)
			return i;
	}

	return 0;
}

static void SetModesMenu (int w, int h, int bits)
{
	char strtemp[64];

	DummyDepthCvar = FindBits (bits);

	if (!testingmode)
	{
		if (ModesItems[VM_ENTERLINE].label != VMEnterText)
			free (ModesItems[VM_ENTERLINE].label);
		ModesItems[VM_ENTERLINE].label = VMEnterText;
		ModesItems[VM_TESTLINE].label = VMTestText;

		sprintf (strtemp, "D to make %dx%dx%d the default", w, h, bits);
		ReplaceString (&ModesItems[VM_MAKEDEFLINE].label, strtemp);

		sprintf (strtemp, "Current default is %dx%dx%d",
				 *vid_defwidth,
				 *vid_defheight,
				 *vid_defbits);
		ReplaceString (&ModesItems[VM_CURDEFLINE].label, strtemp);
	}
	else
	{
		sprintf (strtemp, "TESTING %dx%dx%d", w, h, bits);
		ModesItems[VM_ENTERLINE].label = copystring (strtemp);
		ModesItems[VM_TESTLINE].label = "Please wait 5 seconds...";
		ModesItems[VM_MAKEDEFLINE].label = copystring (" ");
		ModesItems[VM_CURDEFLINE].label = copystring (" ");
	}

	BuildModesList (w, h, bits);
}

void M_RestoreMode (void)
{
	NewWidth = OldWidth;
	NewHeight = OldHeight;
	NewBits = OldBits;
	setmodeneeded = true;
	testingmode = 0;
	SetModesMenu (OldWidth, OldHeight, OldBits);
}

static void SetVidMode (void)
{
	SetModesMenu (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
	if (ModesMenu.items[ModesMenu.lastOn].type == screenres)
	{
		if (ModesMenu.items[ModesMenu.lastOn].a.selmode == -1)
		{
			ModesMenu.items[ModesMenu.lastOn].a.selmode++;
		}
	}
	M_SwitchMenu (&ModesMenu);
}

CCMD (menu_video)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	SetVidMode ();
}

void M_LoadKeys (const char *modname, bool dbl)
{
	char section[64];

	if (GameNames[gameinfo.gametype] == NULL)
		return;

	sprintf (section, "%s.%s%sBindings", GameNames[gameinfo.gametype], modname,
		dbl ? ".Double" : ".");
	if (GameConfig->SetSection (section))
	{
		const char *key, *value;
		while (GameConfig->NextInSection (key, value))
		{
			C_DoBind (key, value, dbl);
		}
	}
}

int M_DoSaveKeys (FConfigFile *config, char *section, int i, bool dbl)
{
	int most = CustomControlsItems.Size();

	config->SetSection (section, true);
	config->ClearCurrentSection ();
	for (++i; i < most; ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];
		if (item->type == control)
		{
			C_ArchiveBindings (config, dbl, item->e.command);
			continue;
		}
		break;
	}
	return i;
}

void M_SaveCustomKeys (FConfigFile *config, char *section, char *subsection)
{
	if (ControlsMenu.items == ControlsItems)
		return;

	// Start after the normal controls
	int i = sizeof(ControlsItems)/sizeof(ControlsItems[0]);
	int most = CustomControlsItems.Size();

	while (i < most)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext)
		{
			sprintf (subsection, "%s.Bindings", item->e.command);
			M_DoSaveKeys (config, section, i, false);
			sprintf (subsection, "%s.DoubleBindings", item->e.command);
			i = M_DoSaveKeys (config, section, i, true);
		}
		else
		{
			i++;
		}
	}
}

static int AddKeySpot;

CCMD (addkeysection)
{
	if (argv.argc() != 3)
	{
		Printf ("Usage: addkeysection <section name> <ini name>\n");
		return;
	}

	const int numStdControls = sizeof(ControlsItems)/sizeof(ControlsItems[0]);
	int i;

	if (ControlsMenu.items == ControlsItems)
	{ // No custom controls have been defined yet.
		for (i = 0; i < numStdControls; ++i)
		{
			CustomControlsItems.Push (ControlsItems[i]);
		}
	}

	// See if this section already exists
	int last = CustomControlsItems.Size();
	for (i = numStdControls; i < last; ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext &&
			stricmp (item->label, argv[1]) == 0)
		{ // found it
			break;
		}
	}

	if (i == last)
	{ // Add the new section
		// Limit the ini name to 32 chars
		if (strlen (argv[2]) > 32)
			argv[2][32] = 0;

		menuitem_t tempItem = { redtext, " " };

		// Add a blank line to the menu
		CustomControlsItems.Push (tempItem);

		// Add the section name to the menu
		tempItem.type = whitetext;
		tempItem.label = copystring (argv[1]);
		tempItem.e.command = copystring (argv[2]);	// Record ini section name in command field
		CustomControlsItems.Push (tempItem);
		ControlsMenu.items = &CustomControlsItems[0];

		// Load bindings for this section from the ini
		M_LoadKeys (argv[1], 0);
		M_LoadKeys (argv[1], 1);

		AddKeySpot = 0;
	}
	else
	{ // Add new keys to the end of this section
		do
		{
			i++;
		} while (i < last && CustomControlsItems[i].type == control);
		if (i < last)
		{
			AddKeySpot = i;
		}
		else
		{
			AddKeySpot = 0;
		}
	}
}

CCMD (addmenukey)
{
	if (argv.argc() != 3)
	{
		Printf ("Usage: addmenukey <description> <command>\n");
		return;
	}
	if (ControlsMenu.items == ControlsItems)
	{
		Printf ("You must use addkeysection first.\n");
		return;
	}

	menuitem_t newItem = { control, };
	newItem.label = copystring (argv[1]);
	newItem.e.command = copystring (argv[2]);
	if (AddKeySpot == 0)
	{ // Just add to the end of the menu
		CustomControlsItems.Push (newItem);
	}
	else
	{ // Add somewhere in the middle of the menu
		size_t movecount = CustomControlsItems.Size() - AddKeySpot;
		CustomControlsItems.Reserve (1);
		memmove (&CustomControlsItems[AddKeySpot+1],
				 &CustomControlsItems[AddKeySpot],
				 sizeof(menuitem_t)*movecount);
		CustomControlsItems[AddKeySpot++] = newItem;
	}
	ControlsMenu.numitems = CustomControlsItems.Size();
}
