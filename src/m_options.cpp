/*
** m_options.cpp
** New options menu code
**
**---------------------------------------------------------------------------
** Copyright 1998-2002 Randy Heit
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

#ifdef _WIN32
#include "i_music.h"
#include "i_input.h"
#endif

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
EXTERN_CVAR (Int, crosshair)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_3d)
EXTERN_CVAR (Bool, snd_waterreverb)

static void CalcIndent (menu_t *menu);

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

int flagsvar;
enum
{
	SHOW_DMFlags = 1,
	SHOW_DMFlags2 = 2,
	SHOW_CompatFlags = 4
};

/*=======================================
 *
 * Options Menu
 *
 *=======================================*/

static void CustomizeControls (void);
static void GameplayOptions (void);
static void CompatibilityOptions (void);
static void VideoOptions (void);
static void SoundOptions (void);
static void MouseOptions (void);
static void JoystickOptions (void);
static void GoToConsole (void);
void M_PlayerSetup (void);
void Reset2Defaults (void);
void Reset2Saved (void);

static void SetVidMode (void);

static menuitem_t OptionItems[] =
{
	{ more,		"Customize Controls",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)CustomizeControls} },
	{ more,		"Mouse options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)MouseOptions} },
	{ more,		"Joystick options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)JoystickOptions} },
	{ discrete,	"Always Run",			{&cl_run},				{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Player Setup",			{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)M_PlayerSetup} },
	{ more,		"Gameplay Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GameplayOptions} },
	{ more,		"Compatibility Options",{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)CompatibilityOptions} },
	{ more,		"Sound Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SoundOptions} },
	{ more,		"Display Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)VideoOptions} },
	{ more,		"Set video mode",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)SetVidMode} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Reset to defaults",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Defaults} },
	{ more,		"Reset to last saved",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Saved} },
	{ more,		"Go to console",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GoToConsole} },
};

menu_t OptionMenu =
{
	"OPTIONS",
	0,
	sizeof(OptionItems)/sizeof(OptionItems[0]),
	0,
	OptionItems,
};

/*=======================================
 *
 * Mouse Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, use_mouse)
EXTERN_CVAR (Bool, m_filter)
EXTERN_CVAR (Float, m_forward)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_side)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookspring)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Bool, m_noprescale)

static menuitem_t MouseItems[] =
{
	{ discrete,	"Enable mouse",			{&use_mouse},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Overall sensitivity",	{&mouse_sensitivity},	{0.5}, {2.5},	{0.1}, {NULL} },
	{ discrete,	"Prescale mouse movement",{&m_noprescale},		{2.0}, {0.0},	{0.0}, {NoYes} },
	{ discrete, "Smooth mouse movement",{&m_filter},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Turning speed",		{&m_yaw},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Mouselook speed",		{&m_pitch},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Forward/Backward speed",{&m_forward},			{0.5}, {2.5},	{0.1}, {NULL} },
	{ slider,	"Strafing speed",		{&m_side},				{0.5}, {2.5},	{0.1}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Always Mouselook",		{&freelook},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Invert Mouse",			{&invertmouse},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookspring",			{&lookspring},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Lookstrafe",			{&lookstrafe},			{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t MouseMenu =
{
	"MOUSE OPTIONS",
	0,
	sizeof(MouseItems)/sizeof(MouseItems[0]),
	0,
	MouseItems,
};

/*=======================================
 *
 * Joystick Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, use_joystick)
EXTERN_CVAR (Float, joy_speedmultiplier)
EXTERN_CVAR (Int, joy_xaxis)
EXTERN_CVAR (Int, joy_yaxis)
EXTERN_CVAR (Int, joy_zaxis)
EXTERN_CVAR (Int, joy_xrot)
EXTERN_CVAR (Int, joy_yrot)
EXTERN_CVAR (Int, joy_zrot)
EXTERN_CVAR (Int, joy_slider)
EXTERN_CVAR (Int, joy_dial)
EXTERN_CVAR (Float, joy_xthreshold)
EXTERN_CVAR (Float, joy_ythreshold)
EXTERN_CVAR (Float, joy_zthreshold)
EXTERN_CVAR (Float, joy_xrotthreshold)
EXTERN_CVAR (Float, joy_yrotthreshold)
EXTERN_CVAR (Float, joy_zrotthreshold)
EXTERN_CVAR (Float, joy_sliderthreshold)
EXTERN_CVAR (Float, joy_dialthreshold)
EXTERN_CVAR (Float, joy_yawspeed)
EXTERN_CVAR (Float, joy_pitchspeed)
EXTERN_CVAR (Float, joy_forwardspeed)
EXTERN_CVAR (Float, joy_sidespeed)
EXTERN_CVAR (Float, joy_upspeed)
EXTERN_CVAR (GUID, joy_guid)

static value_t JoyAxisMapNames[6] =
{
	{ 0.0, "None" },
	{ 1.0, "Turning" },
	{ 2.0, "Looking Up/Down" },
	{ 3.0, "Moving Forward" },
	{ 4.0, "Strafing" },
	{ 5.0, "Moving Up/Down" }
};

static value_t Inversion[2] =
{
	{ 0.0, "Not Inverted" },
	{ 1.0, "Inverted" }
};

static menuitem_t JoystickItems[] =
{
	{ discrete,	"Enable joystick",		{&use_joystick},		{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete_guid,"Active joystick",	{&joy_guid},			{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Overall sensitivity",	{&joy_speedmultiplier},	{0.9}, {2.0},	{0.2}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext,"Axis Assignments",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

menu_t JoystickMenu =
{
	"JOYSTICK OPTIONS",
	0,
	sizeof(JoystickItems)/sizeof(JoystickItems[0]),
	0,
	JoystickItems,
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

menu_t ControlsMenu =
{
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

menu_t VideoMenu =
{
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

menu_t MessagesMenu =
{
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
};

#define VM_DEPTHITEM	0
#define VM_RESSTART		4
#define VM_ENTERLINE	14
#define VM_TESTLINE		16

menu_t ModesMenu =
{
	"VIDEO MODE",
	2,
	17,
	130,
	ModesItems,
};

/*=======================================
 *
 * Gameplay Options (dmflags) Menu
 *
 *=======================================*/

static menuitem_t DMFlagsItems[] = {
	{ discrete, "Teamplay",				{&teamplay},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ slider,	"Team damage scalar",	{&teamdamage},	{0.0}, {1.0}, {0.05},{NULL} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ bitflag,	"Falling damage (old)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGZD} },
	{ bitflag,	"Falling damage (Hexen)",{&dmflags},	{0}, {0}, {0}, {(value_t *)DF_FORCE_FALLINGHX} },
	{ bitflag,	"Weapons stay (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_WEAPONS_STAY} },
	{ bitflag,	"Allow powerups (DM)",	{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ITEMS} },
	{ bitflag,	"Allow health (DM)",	{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_HEALTH} },
	{ bitflag,	"Allow armor (DM)",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ARMOR} },
	{ bitflag,	"Spawn farthest (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SPAWN_FARTHEST} },
	{ bitflag,	"Same map (DM)",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SAME_LEVEL} },
	{ bitflag,	"Force respawn (DM)",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_RESPAWN} },
	{ bitflag,	"Allow exit (DM)",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_EXIT} },
	{ bitflag,	"Barrels respawn (DM)",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_BARRELS_RESPAWN} },
	{ bitflag,	"Respawn protection (DM)",{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_INVUL} },
	{ bitflag,	"Drop weapons",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_WEAPONDROP} },
	{ bitflag,	"Infinite ammo",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_INFINITE_AMMO} },
	{ bitflag,	"No monsters",			{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_NO_MONSTERS} },
	{ bitflag,	"Monsters respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_MONSTERS_RESPAWN} },
	{ bitflag,	"Items respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_ITEMS_RESPAWN} },
	{ bitflag,	"Mega powerups respawn",{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_RESPAWN_SUPER} },
	{ bitflag,	"Fast monsters",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FAST_MONSTERS} },
	{ bitflag,	"Allow jump",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_JUMP} },
	{ bitflag,	"Allow freelook",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FREELOOK} },
	{ bitflag,	"Allow FOV",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FOV} },
	{ bitflag,	"Allow BFG aiming",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_FREEAIMBFG} }
};

static menu_t DMFlagsMenu =
{
	"GAMEPLAY OPTIONS",
	0,
	sizeof(DMFlagsItems)/sizeof(DMFlagsItems[0]),
	0,
	DMFlagsItems,
};

/*=======================================
 *
 * Compatibility Options Menu
 *
 *=======================================*/

static menuitem_t CompatibilityItems[] = {
	{ bitflag,	"Find shortest textures like Doom",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SHORTTEX} },
	{ bitflag,	"Use buggier stair building",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_STAIRINDEX} },
	{ bitflag,	"Limit Pain Elementals to 20 Lost Souls",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_LIMITPAIN} },
	{ bitflag,	"Don't let others hear your pickups",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SILENTPICKUP} },
	{ bitflag,	"Actors are infinitely tall",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NO_PASSMOBJ} },
	{ bitflag,	"Allow silent BFG trick",					{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MAGICSILENCE} },
	{ bitflag,	"Enable wall running",						{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_WALLRUN} },
	{ bitflag,	"Spawn item drops on the floor",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NOTOSSDROPS} },
	{ bitflag,  "All special lines can block use lines",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_USEBLOCKING} },
};

static menu_t CompatibilityMenu =
{
	"COMPATIBILITY OPTIONS",
	0,
	sizeof(CompatibilityItems)/sizeof(CompatibilityItems[0]),
	0,
	CompatibilityItems,
};

/*=======================================
 *
 * Sound Options Menu
 *
 *=======================================*/

#ifdef _WIN32
EXTERN_CVAR (Float, snd_midivolume)
EXTERN_CVAR (Float, snd_movievolume)
#endif
EXTERN_CVAR (Bool, snd_flipstereo)
EXTERN_CVAR (Bool, snd_pitched)

static void MakeSoundChanges ();
static void AdvSoundOptions ();
static void ChooseMIDI ();

static value_t SampleRates[] =
{
	{ 4000.f, "4000 Hz" },
	{ 8000.f, "8000 Hz" },
	{ 11025.f, "11025 Hz" },
	{ 22050.f, "22050 Hz" },
	{ 32000.f, "32000 Hz" },
	{ 44100.f, "44100 Hz" },
	{ 48000.f, "48000 Hz" },
	{ 65535.f, "65535 Hz" }
};

static value_t BufferSizes[] =
{
	{   0.f, "Default" },
	{  20.f, "20 ms" },
	{  40.f, "40 ms" },
	{  60.f, "60 ms" },
	{  80.f, "80 ms" },
	{ 100.f, "100 ms" },
	{ 120.f, "120 ms" },
	{ 140.f, "140 ms" },
	{ 160.f, "160 ms" },
	{ 180.f, "180 ms" },
	{ 200.f, "200 ms" },
};

static menuitem_t SoundItems[] =
{
	{ slider,	"Sound effects volume",	{&snd_sfxvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
#ifdef _WIN32
	{ slider,	"MIDI music volume",	{&snd_midivolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ slider,	"Other music volume",	{&snd_musicvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
	{ slider,	"Movie volume",			{&snd_movievolume},		{0.0}, {1.0},	{0.05}, {NULL} },
#else
	{ slider,	"Music volume",			{&snd_musicvolume},		{0.0}, {1.0},	{0.05}, {NULL} },
#endif
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Underwater EAX Reverb",{&snd_waterreverb},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Flip Stereo Channels",	{&snd_flipstereo},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Random Pitch Variations", {&snd_pitched},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Activate below settings", {NULL},			{0.0}, {0.0},	{0.0}, {(value_t *)MakeSoundChanges} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Sample Rate",			{&snd_samplerate},		{8.0}, {0.0},	{0.0}, {SampleRates} },
	{ discrete, "Buffer Size",			{&snd_buffersize},		{11.0}, {0.0},	{0.0}, {BufferSizes} },
	{ discrete, "3D Sound",				{&snd_3d},				{2.0}, {0.0},	{0.0}, {OnOff} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Advanced Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)AdvSoundOptions} },
#ifdef _WIN32
	{ more,		"Select MIDI Device",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)ChooseMIDI} },
#endif
};

static menu_t SoundMenu =
{
	"SOUND OPTIONS",
	0,
	sizeof(SoundItems)/sizeof(SoundItems[0]),
	0,
	SoundItems,
};

#ifdef _WIN32
/*=======================================
 *
 * MIDI Device Menu
 *
 *=======================================*/

EXTERN_CVAR (Int, snd_mididevice)

static menuitem_t MidiDeviceItems[] =
{
	{ discrete, "Device",				{&snd_mididevice},	{0.0}, {0.0},	{0.0}, {NULL} },
};

static menu_t MidiDeviceMenu =
{
	"SELECT MIDI DEVICE",
	0,
	1,
	0,
	MidiDeviceItems,
};
#endif

/*=======================================
 *
 * Advanced Sound Options Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, opl_enable)
EXTERN_CVAR (Int, opl_frequency)
EXTERN_CVAR (Bool, opl_onechip)


static menuitem_t AdvSoundItems[] =
{
	{ whitetext,"OPL Synthesis",			{NULL},				{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Use FM Synth for MUS music",{&opl_enable},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Only emulate one OPL chip", {&opl_onechip},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "OPL synth sample rate",	 {&opl_frequency},	{8.0}, {0.0},	{0.0}, {SampleRates} },


};

static menu_t AdvSoundMenu =
{
	"ADVANCED SOUND OPTIONS",
	1,
	sizeof(AdvSoundItems)/sizeof(AdvSoundItems[0]),
	0,
	AdvSoundItems,
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

static void CalcIndent (menu_t *menu)
{
	int i, widest = 0, thiswidth;
	menuitem_t *item;

	for (i = 0; i < menu->numitems; i++)
	{
		item = menu->items + i;
		if (item->type != whitetext && item->type != redtext)
		{
			thiswidth = SmallFont->StringWidth (item->label);
			if (thiswidth > widest)
				widest = thiswidth;
		}
	}
	menu->indent = widest + 6;
}

void M_SwitchMenu (menu_t *menu)
{
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
		CalcIndent (menu);
	}

	flagsvar = 0;
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
	screen->DrawText (CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12", DTA_Clean, true, TAG_DONE);
	screen->DrawText (CR_ORANGE, x + 5 + (int)((cur * 78.f) / range), y, "\x13", DTA_Clean, true, TAG_DONE);
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

int M_FindCurGUID (const GUID &guid, GUIDName *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (memcmp (&values[v].ID, &guid, sizeof(GUID)) == 0)
			break;

	return v;
}

void M_OptDrawer ()
{
	EColorRange color;
	int y, width, i, x, ytop, fontheight;
	menuitem_t *item;
	UCVarValue value;
	int labelofs;

	if (BigFont && CurrentMenu->texttitle)
	{
		screen->SetFont (BigFont);
		screen->DrawText (gameinfo.gametype == GAME_Doom ? CR_RED : CR_UNTRANSLATED,
			160-BigFont->StringWidth (CurrentMenu->texttitle)/2, 10,
			CurrentMenu->texttitle, DTA_Clean, true, TAG_DONE);
		screen->SetFont (SmallFont);
		y = 15 + BigFont->GetHeight ();
	}
	else
	{
		y = 15;
	}

	if (gameinfo.gametype != GAME_Doom)
	{
		labelofs = 2;
		y -= 2;
		fontheight = 9;
	}
	else
	{
		labelofs = 0;
		fontheight = 8;
	}

	ytop = y + CurrentMenu->scrolltop * 8;

	for (i = 0; i < CurrentMenu->numitems && y <= 200 - SmallFont->GetHeight(); i++, y += fontheight)
	{
		if (i == CurrentMenu->scrolltop)
		{
			i += CurrentMenu->scrollpos;
		}

		item = CurrentMenu->items + i;

		if (item->type != screenres)
		{
			width = SmallFont->StringWidth (item->label);
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
				color = CR_GOLD;//ValueColor;
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
			screen->DrawText (color, x, y, item->label, DTA_Clean, true, TAG_DONE);

			switch (item->type)
			{
			case discrete:
			case cdiscrete:
			case inverter:
			{
				int v, vals;

				value = item->a.cvar->GetGenericRep (CVAR_Float);
				if (item->type == inverter)
				{
					value.Float = (value.Float < 0.f);
					vals = 2;
				}
				else
				{
					vals = (int)item->b.numvalues;
				}
				v = M_FindCurVal (value.Float, item->e.values, vals);

				if (v == vals)
				{
					screen->DrawText (ValueColor, CurrentMenu->indent + 14, y, "Unknown",
						DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (item->type == cdiscrete ? v : ValueColor,
						CurrentMenu->indent + 14, y, item->e.values[v].name,
						DTA_Clean, true, TAG_DONE);
				}

			}
			break;

			case discrete_guid:
			{
				int v, vals;

				vals = (int)item->b.numvalues;
				v = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, vals);

				if (v == vals)
				{
					UCVarValue val = item->a.guidcvar->GetGenericRep (CVAR_String);
					screen->DrawText (ValueColor, CurrentMenu->indent + 14, y, val.String, DTA_Clean, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (ValueColor, CurrentMenu->indent + 14, y, item->e.guidvalues[v].Name,
						DTA_Clean, true, TAG_DONE);
				}

			}
			break;

			case nochoice:
				screen->DrawText (CR_GOLD, CurrentMenu->indent + 14, y,
					(item->e.values[(int)item->b.min]).name, DTA_Clean, true, TAG_DONE);
				break;

			case slider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (CurrentMenu->indent + 14, y + labelofs, item->b.min, item->c.max, value.Float);
				break;

			case absslider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (CurrentMenu->indent + 14, y + labelofs, item->b.min, item->c.max, fabs(value.Float));
				break;

			case control:
			{
				char description[64];

				C_NameKeys (description, item->b.key1, item->c.key2);
				screen->SetFont (ConFont);
				screen->DrawText (CR_WHITE,
					CurrentMenu->indent + 14, y-1+labelofs, description, DTA_Clean, true, TAG_DONE);
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

				if (item->a.cvar)
				{
					if ((*(item->a.intcvar)) & item->e.flagmask)
						str = value[1].name;
					else
						str = value[0].name;
				}
				else
				{
					str = "???";
				}

				screen->DrawText (ValueColor,
					CurrentMenu->indent + 14, y, str, DTA_Clean, true, TAG_DONE);
			}
			break;

			default:
				break;
			}

			if (i == CurrentItem && (skullAnimCounter < 6 || WaitingForKey))
			{
				screen->SetFont (ConFont);
				screen->DrawText (CR_RED, CurrentMenu->indent + 3, y-1+labelofs, "\xd",
					DTA_Clean, true, TAG_DONE);
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

					screen->DrawText (color, 104 * x + 20, y, str, DTA_Clean, true, TAG_DONE);
				}
			}

			if (i == CurrentItem && ((item->a.selmode != -1 && (skullAnimCounter < 6 || WaitingForKey)) || testingmode))
			{
				screen->SetFont (ConFont);
				screen->DrawText (CR_RED, item->a.selmode * 104 + 8, y-1 + labelofs, "\xd",
					DTA_Clean, true, TAG_DONE);
				screen->SetFont (SmallFont);
			}
		}
	}

	CanScrollUp = (CurrentMenu->scrollpos > 0);
	CanScrollDown = (i < CurrentMenu->numitems);
	VisBottom = i - 1;

	screen->SetFont (ConFont);
	if (CanScrollUp)
	{
		screen->DrawText (CR_ORANGE, 3, ytop + labelofs, "\x1a", DTA_Clean, true, TAG_DONE);
	}
	if (CanScrollDown)
	{
		screen->DrawText (CR_ORANGE, 3, y - 8 + labelofs, "\x1b", DTA_Clean, true, TAG_DONE);
	}
	screen->SetFont (SmallFont);

	if (flagsvar)
	{
		static const FIntCVar *const vars[3] = { &dmflags, &dmflags2, &compatflags };
		char flagsblah[256];
		char *fillptr = flagsblah;
		bool printed = false;

		for (int i = 0; i < 3; ++i)
		{
			if (flagsvar & (1 << i))
			{
				if (printed)
				{
					fillptr += sprintf (fillptr, "    ");
				}
				printed = true;
				fillptr += sprintf (fillptr, "%s = %d", vars[i]->GetName (), **vars[i]);
			}
		}
		screen->DrawText (ValueColor,
			160 - (SmallFont->StringWidth (flagsblah) >> 1), 0, flagsblah,
			DTA_Clean, true, TAG_DONE);
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
	
	if (item->type == bitflag &&
		(ch == GK_LEFT || ch == GK_RIGHT || ch == '\r')
		&& !demoplayback)
	{
		*(item->a.intcvar) = (*(item->a.intcvar)) ^ item->e.flagmask;
		return;
	}

	switch (ch)
	{
	case GK_DOWN:
		if (CurrentMenu->numitems > 1)
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
		if (CurrentMenu->numitems > 1)
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
				if (CurrentMenu->scrollpos > 0 &&
					CurrentItem == CurrentMenu->scrolltop + CurrentMenu->scrollpos)
				{
					CurrentMenu->scrollpos--;
				}
				if (CurrentItem < 0)
				{
					int maxitems, rowheight;

					// Figure out how many lines of text fit on the menu
					if (BigFont && CurrentMenu->texttitle)
					{
						maxitems = 15 + BigFont->GetHeight ();
					}
					else
					{
						maxitems = 15;
					}
					if (gameinfo.gametype != GAME_Doom)
					{
						maxitems -= 2;
						rowheight = 9;
					}
					else
					{
						rowheight = 8;
					}
					maxitems = (200 - SmallFont->GetHeight () - maxitems) / rowheight + 1;

					CurrentMenu->scrollpos = MAX (0,CurrentMenu->numitems - maxitems + CurrentMenu->scrolltop);
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
		if (CurrentMenu->scrollpos > 0)
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
			case absslider:
				{
					UCVarValue newval;
					bool reversed;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					reversed = item->type == absslider && value.Float < 0.f;
					newval.Float = (reversed ? -value.Float : value.Float) - item->d.step;

					if (newval.Float < item->b.min)
						newval.Float = item->b.min;
					else if (newval.Float > item->c.max)
						newval.Float = item->c.max;

					if (reversed)
					{
						newval.Float = -newval.Float;
					}

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

			case discrete_guid:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.numvalues;
					cur = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, numvals);
					if (--cur < 0)
						cur = numvals - 1;

					*(item->a.guidcvar) = item->e.guidvalues[cur].ID;
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
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
			case absslider:
				{
					UCVarValue newval;
					bool reversed;

					value = item->a.cvar->GetGenericRep (CVAR_Float);
					reversed = item->type == absslider && value.Float < 0.f;
					newval.Float = (reversed ? -value.Float : value.Float) + item->d.step;

					if (newval.Float > item->c.max)
						newval.Float = item->c.max;
					else if (newval.Float < item->b.min)
						newval.Float = item->b.min;

					if (reversed)
					{
						newval.Float = -newval.Float;
					}

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

			case discrete_guid:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.numvalues;
					cur = M_FindCurGUID (*(item->a.guidcvar), item->e.guidvalues, numvals);
					if (++cur >= numvals)
						cur = 0;

					*(item->a.guidcvar) = item->e.guidvalues[cur].ID;
				}
				S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
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
			else
			{
				testingmode = 1;
				setmodeneeded = true;
				NewBits = BitTranslate[DummyDepthCvar];
			}
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
		else if (item->type == inverter)
		{
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			value.Float = -value.Float;
			item->a.cvar->SetGenericRep (value, CVAR_Float);
			S_Sound (CHAN_VOICE, "menu/change", 1, ATTN_NONE);
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
				testingmode = I_GetTime(false) + 5 * TICRATE;
				S_Sound (CHAN_VOICE, "menu/choose", 1, ATTN_NONE);
				SetModesMenu (NewWidth, NewHeight, NewBits);
			}
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
	flagsvar = SHOW_DMFlags | SHOW_DMFlags2;
}

CCMD (menu_gameplay)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	GameplayOptions ();
}

static void CompatibilityOptions (void)
{
	M_SwitchMenu (&CompatibilityMenu);
	flagsvar = SHOW_CompatFlags;
}

CCMD (menu_compatibility)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	CompatibilityOptions ();
}

static void MouseOptions ()
{
	M_SwitchMenu (&MouseMenu);
}

CCMD (menu_mouse)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	MouseOptions ();
}

void UpdateJoystickMenu ()
{
	static FIntCVar * const cvars[8] =
	{
		&joy_xaxis, &joy_yaxis, &joy_zaxis,
		&joy_xrot, &joy_yrot, &joy_zrot,
		&joy_slider, &joy_dial
	};
	static FFloatCVar * const cvars2[5] =
	{
		&joy_yawspeed, &joy_pitchspeed, &joy_forwardspeed,
		&joy_sidespeed, &joy_upspeed
	};
	static FFloatCVar * const cvars3[8] =
	{
		&joy_xthreshold, &joy_ythreshold, &joy_zthreshold,
		&joy_xrotthreshold, &joy_yrotthreshold, &joy_zrotthreshold,
		&joy_sliderthreshold, &joy_dialthreshold
	};

	int i, line;

	if (JoystickNames.Size() == 0)
	{
		JoystickItems[0].type = redtext;
		JoystickItems[0].label = "No joysticks connected";
		line = 1;
	}
	else
	{
		JoystickItems[0].type = discrete;
		JoystickItems[0].label = "Enable joystick";

		JoystickItems[1].b.numvalues = float(JoystickNames.Size());
		JoystickItems[1].e.guidvalues = &JoystickNames[0];

		line = 5;

		for (i = 0; i < 8; ++i)
		{
			if (JoyAxisNames[i] != NULL)
			{
				JoystickItems[line].label = JoyAxisNames[i];
				JoystickItems[line].type = discrete;
				JoystickItems[line].a.intcvar = cvars[i];
				JoystickItems[line].b.numvalues = 6.f;
				JoystickItems[line].e.values = JoyAxisMapNames;
				line++;
			}
		}

		JoystickItems[line].type = redtext;
		JoystickItems[line].label = " ";
		line++;

		JoystickItems[line].type = whitetext;
		JoystickItems[line].label = "Axis Sensitivity";
		line++;

		for (i = 0; i < 5; ++i)
		{
			JoystickItems[line].type = absslider;
			JoystickItems[line].label = JoyAxisMapNames[i+1].name;
			JoystickItems[line].a.cvar = cvars2[i];
			JoystickItems[line].b.min = 0.0;
			JoystickItems[line].c.max = 4.0;
			JoystickItems[line].d.step = 0.2;
			line++;

			JoystickItems[line].type = inverter;
			JoystickItems[line].label = JoyAxisMapNames[i+1].name;
			JoystickItems[line].a.cvar = cvars2[i];
			JoystickItems[line].e.values = Inversion;
			line++;
		}

		JoystickItems[line].type = redtext;
		JoystickItems[line].label = " ";
		line++;

		JoystickItems[line].type = whitetext;
		JoystickItems[line].label = "Axis Dead Zones";
		line++;

		for (i = 0; i < 8; ++i)
		{
			if (JoyAxisNames[i] != NULL)
			{
				JoystickItems[line].label = JoyAxisNames[i];
				JoystickItems[line].type = slider;
				JoystickItems[line].a.cvar = cvars3[i];
				JoystickItems[line].b.min = 0.0;
				JoystickItems[line].c.max = 0.9;
				JoystickItems[line].d.step = 0.05;
				line++;
			}
		}
	}

	JoystickMenu.numitems = line;
	if (JoystickMenu.lastOn >= line)
	{
		JoystickMenu.lastOn = line - 1;
	}
	if (screen != NULL)
	{
		CalcIndent (&JoystickMenu);
	}
}

static void JoystickOptions ()
{
	UpdateJoystickMenu ();
	M_SwitchMenu (&JoystickMenu);
}

CCMD (menu_joystick)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	JoystickOptions ();
}

static void SoundOptions ()
{
	M_SwitchMenu (&SoundMenu);
}

CCMD (menu_sound)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	SoundOptions ();
}

static void AdvSoundOptions ()
{
	M_SwitchMenu (&AdvSoundMenu);
}

CCMD (menu_advsound)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	AdvSoundOptions ();
}

#ifdef _WIN32
static void ChooseMIDI ()
{
	I_BuildMIDIMenuList (&MidiDeviceItems[0].e.values,
						 &MidiDeviceItems[0].b.min);
	M_SwitchMenu (&MidiDeviceMenu);
}

CCMD (menu_mididevice)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	ChooseMIDI ();
}
#endif

static void MakeSoundChanges (void)
{
	AddCommandString ("snd_reset");
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
	DummyDepthCvar = FindBits (bits);

	if (testingmode <= 1)
	{
		if (ModesItems[VM_ENTERLINE].label != VMEnterText)
			free (ModesItems[VM_ENTERLINE].label);
		ModesItems[VM_ENTERLINE].label = VMEnterText;
		ModesItems[VM_TESTLINE].label = VMTestText;
	}
	else
	{
		char strtemp[64];

		sprintf (strtemp, "TESTING %dx%dx%d", w, h, bits);
		ModesItems[VM_ENTERLINE].label = copystring (strtemp);
		ModesItems[VM_TESTLINE].label = "Please wait 5 seconds...";
	}

	BuildModesList (w, h, bits);
}

void M_RestoreMode ()
{
	NewWidth = OldWidth;
	NewHeight = OldHeight;
	NewBits = OldBits;
	setmodeneeded = true;
	testingmode = 0;
	SetModesMenu (OldWidth, OldHeight, OldBits);
}

void M_SetDefaultMode ()
{
	// Make current resolution the default
	vid_defwidth = SCREENWIDTH;
	vid_defheight = SCREENHEIGHT;
	vid_defbits = DisplayBits;
	testingmode = 0;
	SetModesMenu (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
}

static void SetVidMode ()
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
	int most = (int)CustomControlsItems.Size();

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
	size_t i = sizeof(ControlsItems)/sizeof(ControlsItems[0]);
	size_t most = CustomControlsItems.Size();

	while (i < most)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext)
		{
			sprintf (subsection, "%s.Bindings", item->e.command);
			M_DoSaveKeys (config, section, (int)i, false);
			sprintf (subsection, "%s.DoubleBindings", item->e.command);
			i = M_DoSaveKeys (config, section, (int)i, true);
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
		Printf ("Usage: addkeysection <menu section name> <ini name>\n");
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
	int last = (int)CustomControlsItems.Size();
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
		M_LoadKeys (argv[2], 0);
		M_LoadKeys (argv[2], 1);

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
	ControlsMenu.numitems = (int)CustomControlsItems.Size();
}
