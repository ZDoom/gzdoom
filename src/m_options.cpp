/*
** m_options.cpp
** New options menu code
**
**---------------------------------------------------------------------------
** Copyright 1998-2009 Randy Heit
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

#include "i_music.h"
#include "i_input.h"
#include "m_joy.h"

#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "gi.h"

#include "r_local.h"
#include "v_palette.h"
#include "gameconfigfile.h"

#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

#include "m_misc.h"
#include "hardware.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "d_event.h"

#include "sbar.h"

// Data.
#include "m_menu.h"

extern FButtonStatus MenuButtons[NUM_MKEYS];

EXTERN_CVAR(Bool, nomonsterinterpolation)
EXTERN_CVAR(Int, showendoom)
EXTERN_CVAR(Bool, hud_althud)
EXTERN_CVAR(Int, compatmode)
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR(Bool, displaynametags)
EXTERN_CVAR (Int, snd_channels)

//
// defaulted values
//
CVAR (Float, mouse_sensitivity, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// Show messages has default, 0 = off, 1 = on
CVAR (Bool, show_messages, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, show_obituaries, true, CVAR_ARCHIVE)
EXTERN_CVAR (Bool, longsavemessages)
EXTERN_CVAR (Bool, screenshot_quiet)

EXTERN_CVAR (Bool, cl_run)
EXTERN_CVAR (Int, crosshair)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Int, sv_smartaim)
EXTERN_CVAR (Int, am_colorset)
EXTERN_CVAR (Int, vid_aspect)

static void CalcIndent (menu_t *menu);

void M_ChangeMessages ();
void M_SizeDisplay (int diff);

int  M_StringHeight (char *string);

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

value_t OffOn[2] = {
	{ 0.0, "On" },
	{ 1.0, "Off" }
};

value_t CompatModes[] = {
	{ 0.0, "Default" },
	{ 1.0, "Doom" },
	{ 2.0, "Doom (strict)" },
	{ 3.0, "Boom" },
	{ 6.0, "Boom (strict)" },
	{ 5.0, "MBF" },
	{ 4.0, "ZDoom 2.0.63" },
};

menu_t  *CurrentMenu;
int		CurrentItem;
static const char	*OldMessage;
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
 * Confirm Menu - Used by safemore
 *
 *=======================================*/
static void ActivateConfirm (const char *text, void (*func)());
static void ConfirmIsAGo ();

static menuitem_t ConfirmItems[] = {
	{ whitetext,NULL,									{NULL}, {0}, {0}, {0}, {NULL} },
	{ redtext,	"Do you really want to do this?",		{NULL}, {0}, {0}, {0}, {NULL} },
	{ redtext,	" ",									{NULL}, {0}, {0}, {0}, {NULL} },
	{ rightmore,"Yes",									{NULL}, {0}, {0}, {0}, {(value_t*)ConfirmIsAGo} },
	{ rightmore,"No",									{NULL}, {0}, {0}, {0}, {(value_t*)M_PopMenuStack} },
};

static menu_t ConfirmMenu = {
	"PLEASE CONFIRM",
	3,
	countof(ConfirmItems),
	140,
	ConfirmItems,
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
	{ safemore,	"Reset to defaults",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Defaults} },
	{ safemore,	"Reset to last saved",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)Reset2Saved} },
	{ more,		"Go to console",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)GoToConsole} },
};

menu_t OptionMenu =
{
	"OPTIONS",
	0,
	countof(OptionItems),
	0,
	OptionItems,
};

/*=======================================
 *
 * Mouse Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, use_mouse)
EXTERN_CVAR (Bool, smooth_mouse)
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
	{ slider,	"Overall sensitivity",	{&mouse_sensitivity},	{0.5}, {2.5},	{0.1f}, {NULL} },
	{ discrete,	"Prescale mouse movement",{&m_noprescale},		{2.0}, {0.0},	{0.0}, {NoYes} },
	{ discrete, "Smooth mouse movement",{&smooth_mouse},		{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Turning speed",		{&m_yaw},				{0.0}, {2.5},	{0.1f}, {NULL} },
	{ slider,	"Mouselook speed",		{&m_pitch},				{0.0}, {2.5},	{0.1f}, {NULL} },
	{ slider,	"Forward/Backward speed",{&m_forward},			{0.0}, {2.5},	{0.1f}, {NULL} },
	{ slider,	"Strafing speed",		{&m_side},				{0.0}, {2.5},	{0.1f}, {NULL} },
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
	countof(MouseItems),
	0,
	MouseItems,
};

/*=======================================
 *
 * Joystick Menu
 *
 *=======================================*/

EXTERN_CVAR(Bool, use_joystick)
EXTERN_CVAR(Bool, joy_ps2raw)
EXTERN_CVAR(Bool, joy_dinput)
EXTERN_CVAR(Bool, joy_xinput)

static TArray<IJoystickConfig *> Joysticks;
static TArray<menuitem_t> JoystickItems;

menu_t JoystickMenu =
{
	"CONTROLLER OPTIONS",
};

/*=======================================
 *
 * Joystick Config Menu
 *
 *=======================================*/

IJoystickConfig *SELECTED_JOYSTICK;

static value_t JoyAxisMapNames[6] =
{
	{ (float)JOYAXIS_None, "None" },
	{ (float)JOYAXIS_Yaw, "Turning" },
	{ (float)JOYAXIS_Pitch, "Looking Up/Down" },
	{ (float)JOYAXIS_Forward, "Moving Forward" },
	{ (float)JOYAXIS_Side, "Strafing" },
	{ (float)JOYAXIS_Up, "Moving Up/Down" }
};

static value_t Inversion[2] =
{
	{ 0.0, "Not Inverted" },
	{ 1.0, "Inverted" }
};

static TArray<menuitem_t> JoystickConfigItems;

menu_t JoystickConfigMenu =
{
	"CONFIGURE CONTROLLER",
};


/*=======================================
 *
 * Controls Menu
 *
 *=======================================*/

menuitem_t ControlsItems[] =
{
	{ redtext,"ENTER to change, BACKSPACE to clear", {NULL}, {0.0}, {0.0}, {0.0}, {NULL} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Controls",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Fire",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+attack"} },
	{ control,	"Secondary Fire",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+altattack"} },
	{ control,	"Use / Open",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+use"} },
	{ control,	"Move forward",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+forward"} },
	{ control,	"Move backward",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+back"} },
	{ control,	"Strafe left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveleft"} },
	{ control,	"Strafe right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+moveright"} },
	{ control,	"Turn left",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+left"} },
	{ control,	"Turn right",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+right"} },
	{ control,	"Jump",					{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+jump"} },
	{ control,	"Crouch",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+crouch"} },
	{ control,	"Crouch Toggle",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"crouch"} },
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
	{ control,	"Show Scoreboard",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"+showscores"} },
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
	{ control,	"Drop item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invdrop"} },
	{ control,	"Query item",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"invquery"} },
	{ control,	"Drop weapon",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"weapdrop"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Other",				{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Toggle automap",		{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"togglemap"} },
	{ control,	"Chasecam",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"chase"} },
	{ control,	"Coop spy",				{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"spynext"} },
	{ control,	"Screenshot",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"screenshot"} },
	{ control,  "Open console",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"toggleconsole"} },
	{ redtext,	" ",					{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ whitetext,"Strife Popup Screens",	{NULL},	{0.0}, {0.0}, {0.0}, {NULL} },
	{ control,	"Mission objectives",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 1"} },
	{ control,	"Keys list",			{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 2"} },
	{ control,	"Weapons/ammo/stats",	{NULL}, {0.0}, {0.0}, {0.0}, {(value_t *)"showpop 3"} },
};

static TArray<menuitem_t> CustomControlsItems (0);

menu_t ControlsMenu =
{
	"CUSTOMIZE CONTROLS",
	3,
	countof(ControlsItems),
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
static void StartAutomapMenu (void);
static void StartScoreboardMenu (void);
static void InitCrosshairsList();

EXTERN_CVAR (Bool, st_scale)
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
EXTERN_CVAR (Int, r_fakecontrast)

static TArray<valuestring_t> Crosshairs;

static value_t ColumnMethods[] = {
	{ 0.0, "Original" },
	{ 1.0, "Optimized" }
};

static value_t RocketTrailTypes[] = {
	{ 0.0, "Off" },
	{ 1.0, "Particles" },
	{ 2.0, "Sprites" },
	{ 3.0, "Sprites & Particles" }
};

static value_t BloodTypes[] = {
	{ 0.0, "Sprites" },
	{ 1.0, "Sprites & Particles" },
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

static value_t Endoom[] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Only modified" }
};

static value_t Contrast[] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Smooth" }
};

static menuitem_t VideoItems[] = {
	{ more,		"Message Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartMessagesMenu} },
	{ more,		"Automap Options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartAutomapMenu} },
	{ more,		"Scoreboard Options",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)StartScoreboardMenu} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ slider,	"Screen size",			{&screenblocks},	   	{3.0}, {12.0},	{1.0}, {NULL} },
	{ slider,	"Brightness",			{&Gamma},			   	{1.0}, {3.0},	{0.1f}, {NULL} },
	{ discrete, "Vertical Sync",		{&vid_vsync},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discretes,"Crosshair",			{&crosshair},		   	{8.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Column render mode",	{&r_columnmethod},		{2.0}, {0.0},	{0.0}, {ColumnMethods} },
	{ discrete, "Stretch short skies",	{&r_stretchsky},	   	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Stretch status bar",	{&st_scale},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Alternative HUD",		{&hud_althud},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Screen wipe style",	{&wipetype},			{4.0}, {0.0},	{0.0}, {Wipes} },
#ifdef _WIN32
	{ discrete,	"Show ENDOOM screen",	{&showendoom},			{3.0}, {0.0},	{0.0}, {Endoom} },
	{ discrete, "DirectDraw palette hack", {&vid_palettehack},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Use attached surfaces", {&vid_attachedsurfaces},{2.0}, {0.0},	{0.0}, {OnOff} },
#endif
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Use fuzz effect",		{&r_drawfuzz},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Use fake contrast",	{&r_fakecontrast},		{3.0}, {0.0},	{0.0}, {Contrast} },
	{ discrete, "Rocket Trails",		{&cl_rockettrails},		{4.0}, {0.0},	{0.0}, {RocketTrailTypes} },
	{ discrete, "Blood Type",			{&cl_bloodtype},	   	{3.0}, {0.0},	{0.0}, {BloodTypes} },
	{ discrete, "Bullet Puff Type",		{&cl_pufftype},			{2.0}, {0.0},	{0.0}, {PuffTypes} },
	{ discrete, "Display nametags",		{&displaynametags},		{2.0}, {0.0},	{0.0}, {YesNo} },
};

#define CROSSHAIR_INDEX 7

menu_t VideoMenu =
{
	"DISPLAY OPTIONS",
	0,
	countof(VideoItems),
	0,
	VideoItems,
};

/*=======================================
 *
 * Automap Menu
 *
 *=======================================*/
static void StartMapColorsMenu (void);

EXTERN_CVAR (Int, am_rotate)
EXTERN_CVAR (Int, am_overlay)
EXTERN_CVAR (Bool, am_showitems)
EXTERN_CVAR (Bool, am_showmonsters)
EXTERN_CVAR (Bool, am_showsecrets)
EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Int, am_map_secrets)
EXTERN_CVAR (Bool, am_showtotaltime)
EXTERN_CVAR (Bool, am_drawmapback)

static value_t MapColorTypes[] = {
	{ 0, "Custom" },
	{ 1, "Traditional Doom" },
	{ 2, "Traditional Strife" },
	{ 3, "Traditional Raven" }
};

static value_t SecretTypes[] = {
	{ 0, "Never" },
	{ 1, "Only when found" },
	{ 2, "Always" },
};

static value_t RotateTypes[] = {
	{ 0, "Off" },
	{ 1, "On" },
	{ 2, "On for overlay only" }
};

static value_t OverlayTypes[] = {
	{ 0, "Off" },
	{ 1, "Overlay+Normal" },
	{ 2, "Overlay Only" }
};

static menuitem_t AutomapItems[] = {
	{ discrete, "Map color set",		{&am_colorset},			{4.0}, {0.0},	{0.0}, {MapColorTypes} },
	{ more,		"Set custom colors",	{NULL},					{0.0}, {0.0},	{0.0}, {(value_t*)StartMapColorsMenu} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Rotate automap",		{&am_rotate},		   	{3.0}, {0.0},	{0.0}, {RotateTypes} },
	{ discrete, "Overlay automap",		{&am_overlay},			{3.0}, {0.0},	{0.0}, {OverlayTypes} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Show item counts",		{&am_showitems},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show monster counts",	{&am_showmonsters},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show secret counts",	{&am_showsecrets},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show time elapsed",	{&am_showtime},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show total time elapsed",	{&am_showtotaltime},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ discrete, "Show secrets on map",		{&am_map_secrets},		{3.0}, {0.0},	{0.0}, {SecretTypes} },
	{ discrete, "Draw map background",	{&am_drawmapback},		{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t AutomapMenu =
{
	"AUTOMAP OPTIONS",
	0,
	countof(AutomapItems),
	0,
	AutomapItems,
};

/*=======================================
 *
 * Map Colors Menu
 *
 *=======================================*/
static void DefaultCustomColors();

EXTERN_CVAR (Color, am_backcolor)
EXTERN_CVAR (Color, am_yourcolor)
EXTERN_CVAR (Color, am_wallcolor)
EXTERN_CVAR (Color, am_secretwallcolor)
EXTERN_CVAR (Color, am_tswallcolor)
EXTERN_CVAR (Color, am_fdwallcolor)
EXTERN_CVAR (Color, am_cdwallcolor)
EXTERN_CVAR (Color, am_thingcolor)
EXTERN_CVAR (Color, am_gridcolor)
EXTERN_CVAR (Color, am_xhaircolor)
EXTERN_CVAR (Color, am_notseencolor)
EXTERN_CVAR (Color, am_lockedcolor)
EXTERN_CVAR (Color, am_ovyourcolor)
EXTERN_CVAR (Color, am_ovwallcolor)
EXTERN_CVAR (Color, am_ovthingcolor)
EXTERN_CVAR (Color, am_ovotherwallscolor)
EXTERN_CVAR (Color, am_ovunseencolor)
EXTERN_CVAR (Color, am_ovtelecolor)
EXTERN_CVAR (Color, am_intralevelcolor)
EXTERN_CVAR (Color, am_interlevelcolor)
EXTERN_CVAR (Color, am_secretsectorcolor)
EXTERN_CVAR (Color, am_ovsecretsectorcolor)
EXTERN_CVAR (Color, am_thingcolor_friend)
EXTERN_CVAR (Color, am_thingcolor_monster)
EXTERN_CVAR (Color, am_thingcolor_item)
EXTERN_CVAR (Color, am_ovthingcolor_friend)
EXTERN_CVAR (Color, am_ovthingcolor_monster)
EXTERN_CVAR (Color, am_ovthingcolor_item)

static menuitem_t MapColorsItems[] = {
	{ rsafemore,   "Restore default custom colors",				{NULL},					{0}, {0}, {0}, {(value_t*)DefaultCustomColors} },
	{ redtext,	   " ",											{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "Background",								{&am_backcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "You",										{&am_yourcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "1-sided walls",								{&am_wallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls with different floors",		{&am_fdwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls with different ceilings",		{&am_cdwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Map grid",									{&am_gridcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Center point",								{&am_xhaircolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Not-yet-seen walls",						{&am_notseencolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Locked doors",								{&am_lockedcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter to the same map",				{&am_intralevelcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter to a different map",				{&am_interlevelcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Secret sector",								{&am_secretsectorcolor},	{0}, {0}, {0}, {0} },
	{ redtext,		" ",										{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "Invisible 2-sided walls (for cheat)",		{&am_tswallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Secret walls (for cheat)",					{&am_secretwallcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Actors (for cheat)",						{&am_thingcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Monsters (for cheat)",						{&am_thingcolor_monster},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Friends (for cheat)",						{&am_thingcolor_friend},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Items (for cheat)",							{&am_thingcolor_item},			{0}, {0}, {0}, {0} },
	{ redtext,		" ",										{NULL},					{0}, {0}, {0}, {0} },
	{ colorpicker, "You (overlay)",								{&am_ovyourcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "1-sided walls (overlay)",					{&am_ovwallcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "2-sided walls (overlay)",					{&am_ovotherwallscolor},{0}, {0}, {0}, {0} },
	{ colorpicker, "Not-yet-seen walls (overlay)",				{&am_ovunseencolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Teleporter (overlay)",						{&am_ovtelecolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Secret sector (overlay)",					{&am_ovsecretsectorcolor},	{0}, {0}, {0}, {0} },
	{ colorpicker, "Actors (overlay) (for cheat)",				{&am_ovthingcolor},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Monsters (overlay) (for cheat)",			{&am_ovthingcolor_monster},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Friends (overlay) (for cheat)",				{&am_ovthingcolor_friend},		{0}, {0}, {0}, {0} },
	{ colorpicker, "Items (overlay) (for cheat)",				{&am_ovthingcolor_item},		{0}, {0}, {0}, {0} },
};

menu_t MapColorsMenu =
{
	"CUSTOMIZE MAP COLORS",
	0,
	countof(MapColorsItems),
	48,
	MapColorsItems,
};

/*=======================================
 *
 * Color Picker Sub-menu
 *
 *=======================================*/
static void StartColorPickerMenu (const char *colorname, FColorCVar *cvar);
static void ColorPickerReset ();
static int CurrColorIndex;
static int SelColorIndex;
static void UpdateSelColor (int index);


static menuitem_t ColorPickerItems[] = {
	{ redtext,		NULL,					{NULL},		{0},  {0},   {0},  {0} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ intslider,	"Red",					{NULL},		{0},  {255}, {15}, {0} },
	{ intslider,	"Green",				{NULL},		{0},  {255}, {15}, {0} },
	{ intslider,	"Blue",					{NULL},		{0},  {255}, {15}, {0} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ more,			"Undo changes",			{NULL},		{0},  {0},   {0},  {(value_t*)ColorPickerReset} },
	{ redtext,		" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{0},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{1},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{2},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{3},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{4},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{5},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{6},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{7},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{8},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{9},  {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{10}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{11}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{12}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{13}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{14}, {0},   {0},  {0} },
	{ palettegrid,	" ",					{NULL},		{15}, {0},   {0},  {0} },
};

menu_t ColorPickerMenu =
{
	"SELECT COLOR",
	2,
	countof(ColorPickerItems),
	0,
	ColorPickerItems,
};

/*=======================================
 *
 * Messages Menu
 *
 *=======================================*/
EXTERN_CVAR (Int,  con_scaletext)
EXTERN_CVAR (Bool, con_centernotify)
EXTERN_CVAR (Int,  msg0color)
EXTERN_CVAR (Int,  msg1color)
EXTERN_CVAR (Int,  msg2color)
EXTERN_CVAR (Int,  msg3color)
EXTERN_CVAR (Int,  msg4color)
EXTERN_CVAR (Int,  msgmidcolor)
EXTERN_CVAR (Int,  msglevel)

static value_t ScaleValues[] =
{
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Double" }
};

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
	{ 10.0, "yellow" },
	{ 11.0, "default" },
	{ 12.0, "black" },
	{ 13.0, "light blue" },
	{ 14.0, "cream" },
	{ 15.0, "olive" },
	{ 16.0, "dark green" },
	{ 17.0, "dark red" },
	{ 18.0, "dark brown" },
	{ 19.0, "purple" },
	{ 20.0, "dark gray" },
};

static value_t MessageLevels[] = {
	{ 0.0, "Item Pickup" },
	{ 1.0, "Obituaries" },
	{ 2.0, "Critical Messages" }
};

static menuitem_t MessagesItems[] = {
	{ discrete, "Show messages",		{&show_messages},		{2.0}, {0.0},   {0.0}, {OnOff} },
	{ discrete, "Show obituaries",		{&show_obituaries},		{2.0}, {0.0},   {0.0}, {OnOff} },
	{ discrete,	"Scale text in high res", {&con_scaletext},		{3.0}, {0.0}, 	{0.0}, {ScaleValues} },
	{ discrete, "Minimum message level", {&msglevel},		   	{3.0}, {0.0},   {0.0}, {MessageLevels} },
	{ discrete, "Center messages",		{&con_centernotify},	{2.0}, {0.0},	{0.0}, {OnOff} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext, "Message Colors",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ cdiscrete, "Item Pickup",			{&msg0color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Obituaries",			{&msg1color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Critical Messages",	{&msg2color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Chat Messages",		{&msg3color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Team Messages",		{&msg4color},		   	{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Centered Messages",	{&msgmidcolor},			{21.0}, {0.0},	{0.0}, {TextColors} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Screenshot messages",	{&screenshot_quiet},	{2.0}, {0.0},	{0.0}, {OffOn} },
	{ discrete, "Detailed save messages",{&longsavemessages},	{2.0}, {0.0},	{0.0}, {OnOff} },
};

menu_t MessagesMenu =
{
	"MESSAGES",
	0,
	countof(MessagesItems),
	0,
	MessagesItems,
};


/*=======================================
 *
 * Scoreboard Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, sb_cooperative_enable)
EXTERN_CVAR (Int, sb_cooperative_headingcolor)
EXTERN_CVAR (Int, sb_cooperative_yourplayercolor)
EXTERN_CVAR (Int, sb_cooperative_otherplayercolor)

EXTERN_CVAR (Bool, sb_deathmatch_enable)
EXTERN_CVAR (Int, sb_deathmatch_headingcolor)
EXTERN_CVAR (Int, sb_deathmatch_yourplayercolor)
EXTERN_CVAR (Int, sb_deathmatch_otherplayercolor)

EXTERN_CVAR (Bool, sb_teamdeathmatch_enable)
EXTERN_CVAR (Int, sb_teamdeathmatch_headingcolor)

static menuitem_t ScoreboardItems[] = {
	{ whitetext, "Cooperative Options",		{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Enable Scoreboard",		{&sb_cooperative_enable},				{21.0}, {0.0},	{0.0}, {YesNo} },
	{ cdiscrete, "Header Color",			{&sb_cooperative_headingcolor},			{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Your Player Color",		{&sb_cooperative_yourplayercolor},		{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Other Players' Color",	{&sb_cooperative_otherplayercolor},		{21.0}, {0.0},	{0.0}, {TextColors} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext, "Deathmatch Options",		{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Enable Scoreboard",		{&sb_deathmatch_enable},				{21.0}, {0.0},	{0.0}, {YesNo} },
	{ cdiscrete, "Header Color",			{&sb_deathmatch_headingcolor},			{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Your Player Color",		{&sb_deathmatch_yourplayercolor},		{21.0}, {0.0},	{0.0}, {TextColors} },
	{ cdiscrete, "Other Players' Color",	{&sb_deathmatch_otherplayercolor},		{21.0}, {0.0},	{0.0}, {TextColors} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext, "Team Deathmatch Options",	{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",						{NULL},									{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Enable Scoreboard",		{&sb_teamdeathmatch_enable},			{21.0}, {0.0},	{0.0}, {YesNo} },
	{ cdiscrete, "Header Color",			{&sb_teamdeathmatch_headingcolor},			{21.0}, {0.0},	{0.0}, {TextColors} }
};

menu_t ScoreboardMenu =
{
	"SCOREBOARD OPTIONS",
	2,
	countof(ScoreboardItems),
	0,
	ScoreboardItems,
};


/*=======================================
 *
 * Video Modes Menu
 *
 *=======================================*/

extern bool setmodeneeded;
extern int NewWidth, NewHeight, NewBits;
extern int DisplayBits;

int testingmode;		// Holds time to revert to old mode
int OldWidth, OldHeight, OldBits;

void M_FreeModesList ();
static void BuildModesList (int hiwidth, int hiheight, int hi_id);
static bool GetSelectedSize (int line, int *width, int *height);
static void SetModesMenu (int w, int h, int bits);

EXTERN_CVAR (Int, vid_defwidth)
EXTERN_CVAR (Int, vid_defheight)
EXTERN_CVAR (Int, vid_defbits)

static FIntCVar DummyDepthCvar (NULL, 0, 0);

EXTERN_CVAR (Bool, fullscreen)

static value_t Depths[22];

EXTERN_CVAR (Bool, vid_tft)		// Defined below
CUSTOM_CVAR (Int, menu_screenratios, 0, CVAR_ARCHIVE)
{
	if (self < 0 || self > 4)
	{
		self = 3;
	}
	else if (self == 4 && !vid_tft)
	{
		self = 3;
	}
	else
	{
		BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
	}
}

static value_t ForceRatios[] =
{
	{ 0.0, "Off" },
	{ 3.0, "4:3" },
	{ 1.0, "16:9" },
	{ 2.0, "16:10" },
	{ 4.0, "5:4" }
};
static value_t Ratios[] =
{
	{ 0.0, "4:3" },
	{ 1.0, "16:9" },
	{ 2.0, "16:10" },
	{ 3.0, "All" }
};
static value_t RatiosTFT[] =
{
	{ 0.0, "4:3" },
	{ 4.0, "5:4" },
	{ 1.0, "16:9" },
	{ 2.0, "16:10" },
	{ 3.0, "All" }
};

static char VMEnterText[] = "Press ENTER to set mode";
static char VMTestText[] = "T to test mode for 5 seconds";

static menuitem_t ModesItems[] = {
//	{ discrete, "Screen mode",			{&DummyDepthCvar},		{0.0}, {0.0},	{0.0}, {Depths} },
	{ discrete, "Force aspect ratio",	{&vid_aspect},			{5.0}, {0.0},	{0.0}, {ForceRatios} },
	{ discrete, "Aspect ratio",			{&menu_screenratios},	{4.0}, {0.0},	{0.0}, {Ratios} },
	{ discrete, "Fullscreen",			{&fullscreen},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ discrete, "Enable 5:4 aspect ratio",{&vid_tft},			{2.0}, {0.0},	{0.0}, {YesNo} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ screenres,NULL,					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
//	{ whitetext,"Note: Only 8 bpp modes are supported",{NULL},	{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMEnterText,			{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,  VMTestText,				{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
};

#define VM_ASPECTITEM	1
#define VM_RESSTART		5
#define VM_ENTERLINE	15
#define VM_TESTLINE		17

menu_t ModesMenu =
{
	"VIDEO MODE",
	2,
	countof(ModesItems),
	0,
	ModesItems,
};

CUSTOM_CVAR (Bool, vid_tft, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self)
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 5.f;
		ModesItems[VM_ASPECTITEM].e.values = RatiosTFT;
	}
	else
	{
		ModesItems[VM_ASPECTITEM].b.numvalues = 4.f;
		ModesItems[VM_ASPECTITEM].e.values = Ratios;
		if (menu_screenratios == 4)
		{
			menu_screenratios = 0;
		}
	}
	setsizeneeded = true;
	if (StatusBar != NULL)
	{
		StatusBar->ScreenSizeChanged();
	}	
}

/*=======================================
 *
 * Gameplay Options (dmflags) Menu
 *
 *=======================================*/
value_t SmartAim[4] = {
	{ 0.0, "Off" },
	{ 1.0, "On" },
	{ 2.0, "Never friends" },
	{ 3.0, "Only monsters" }
};

value_t FallingDM[4] = {
	{ 0, "Off" },
	{ DF_FORCE_FALLINGZD, "Old" },
	{ DF_FORCE_FALLINGHX, "Hexen" },
	{ DF_FORCE_FALLINGZD|DF_FORCE_FALLINGHX, "Strife" }
};

value_t DF_Jump[3] = {
	{ 0, "Default" },
	{ DF_NO_JUMP, "Off" },
	{ DF_YES_JUMP, "On" }
};

value_t DF_Crouch[3] = {
	{ 0, "Default" },
	{ DF_NO_CROUCH, "Off" },
	{ DF_YES_CROUCH, "On" }
};


static menuitem_t DMFlagsItems[] = {
	{ discrete, "Teamplay",				{&teamplay},	{2.0}, {0.0}, {0.0}, {OnOff} },
	{ slider,	"Team damage scalar",	{&teamdamage},	{0.0}, {1.0}, {0.05f},{NULL} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ discrete, "Smart Autoaim",		{&sv_smartaim},	{4.0}, {0.0}, {0.0}, {SmartAim} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ bitmask,	"Falling damage",		{&dmflags},		{4.0}, {DF_FORCE_FALLINGZD|DF_FORCE_FALLINGHX}, {0}, {FallingDM} },
	{ bitflag,	"Drop weapon",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_WEAPONDROP} },
	{ bitflag,	"Double ammo",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DOUBLEAMMO} },
	{ bitflag,	"Infinite ammo",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_INFINITE_AMMO} },
	{ bitflag,	"Infinite inventory",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_INFINITE_INVENTORY} },
	{ bitflag,	"No monsters",			{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_NO_MONSTERS} },
	{ bitflag,	"No monsters to exit",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_KILL_MONSTERS} },
	{ bitflag,	"Monsters respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_MONSTERS_RESPAWN} },
	{ bitflag,	"No respawn",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_RESPAWN} },
	{ bitflag,	"Items respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_ITEMS_RESPAWN} },
	{ bitflag,	"Big powerups respawn",	{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_RESPAWN_SUPER} },
	{ bitflag,	"Fast monsters",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FAST_MONSTERS} },
	{ bitflag,	"Degeneration",			{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_DEGENERATION} },
	{ bitflag,	"Allow Autoaim",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NOAUTOAIM} },
	{ bitflag,	"Allow Suicide",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NOSUICIDE} },
	{ bitmask,	"Allow jump",			{&dmflags},		{3.0}, {DF_NO_JUMP|DF_YES_JUMP}, {0}, {DF_Jump} },
	{ bitmask,	"Allow crouch",			{&dmflags},		{3.0}, {DF_NO_CROUCH|DF_YES_CROUCH}, {0}, {DF_Crouch} },
	{ bitflag,	"Allow freelook",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FREELOOK} },
	{ bitflag,	"Allow FOV",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_FOV} },
	{ bitflag,	"Allow BFG aiming",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_FREEAIMBFG} },
	{ bitflag,	"Allow automap",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_AUTOMAP} },
	{ bitflag,	"Automap allies",		{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_NO_AUTOMAP_ALLIES} },
	{ bitflag,	"Allow spying",			{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_DISALLOW_SPYING} },
	{ bitflag,	"Chasecam cheat",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_CHASECAM} },
	{ bitflag,	"Check ammo for weapon switch",	{&dmflags2},	{1}, {0}, {0}, {(value_t *)DF2_DONTCHECKAMMO} },
	{ bitflag,	"Killing boss brain kills all its monsters",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_KILLBOSSMONST} },

	{ redtext,	" ",					{NULL},			{0}, {0}, {0}, {NULL} },
	{ whitetext,"Deathmatch Settings",	{NULL},			{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Weapons stay",			{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_WEAPONS_STAY} },
	{ bitflag,	"Allow powerups",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ITEMS} },
	{ bitflag,	"Allow health",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_HEALTH} },
	{ bitflag,	"Allow armor",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_ARMOR} },
	{ bitflag,	"Spawn farthest",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SPAWN_FARTHEST} },
	{ bitflag,	"Same map",				{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_SAME_LEVEL} },
	{ bitflag,	"Force respawn",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_FORCE_RESPAWN} },
	{ bitflag,	"Allow exit",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_EXIT} },
	{ bitflag,	"Barrels respawn",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_BARRELS_RESPAWN} },
	{ bitflag,	"Respawn protection",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_RESPAWN_INVUL} },
	{ bitflag,	"Lose frag if fragged",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_LOSEFRAG} },
	{ bitflag,	"Keep frags gained",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_YES_KEEPFRAGS} },
	{ bitflag,	"No team switching",	{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_NO_TEAM_SWITCH} },

	{ redtext,	" ",					{NULL},			{0}, {0}, {0}, {NULL} },
	{ whitetext,"Cooperative Settings",	{NULL},			{0}, {0}, {0}, {NULL} },
	{ bitflag,	"Spawn multi. weapons", {&dmflags},		{1}, {0}, {0}, {(value_t *)DF_NO_COOP_WEAPON_SPAWN} },
	{ bitflag,	"Lose entire inventory",{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_COOP_LOSE_INVENTORY} },
	{ bitflag,	"Keep keys",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_KEYS} },
	{ bitflag,	"Keep weapons",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_WEAPONS} },
	{ bitflag,	"Keep armor",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_ARMOR} },
	{ bitflag,	"Keep powerups",		{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_POWERUPS} },
	{ bitflag,	"Keep ammo",			{&dmflags},		{1}, {0}, {0}, {(value_t *)DF_COOP_LOSE_AMMO} },
	{ bitflag,	"Lose half ammo",		{&dmflags},		{0}, {0}, {0}, {(value_t *)DF_COOP_HALVE_AMMO} },
	{ bitflag,	"Spawn where died",		{&dmflags2},	{0}, {0}, {0}, {(value_t *)DF2_SAME_SPAWN_SPOT} },
};

static menu_t DMFlagsMenu =
{
	"GAMEPLAY OPTIONS",
	0,
	countof(DMFlagsItems),
	0,
	DMFlagsItems,
};

/*=======================================
 *
 * Compatibility Options Menu
 *
 *=======================================*/

static menuitem_t CompatibilityItems[] = {
	{ discrete, "Compatibility mode",						{&compatmode},	{7.0}, {1.0},	{0.0}, {CompatModes} },
	{ redtext,	" ",					{NULL},			{0.0}, {0.0}, {0.0}, {NULL} },
	{ bitflag,	"Find shortest textures like Doom",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SHORTTEX} },
	{ bitflag,	"Use buggier stair building",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_STAIRINDEX} },
	{ bitflag,	"Find neighboring light like Doom",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_LIGHT} },
	{ bitflag,	"Limit Pain Elementals' Lost Souls",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_LIMITPAIN} },
	{ bitflag,	"Don't let others hear your pickups",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SILENTPICKUP} },
	{ bitflag,	"Actors are infinitely tall",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NO_PASSMOBJ} },
	{ bitflag,	"Enable wall running",						{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_WALLRUN} },
	{ bitflag,	"Spawn item drops on the floor",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NOTOSSDROPS} },
	{ bitflag,  "All special lines can block <use>",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_USEBLOCKING} },
	{ bitflag,	"Disable BOOM door light effect",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NODOORLIGHT} },
	{ bitflag,	"Raven scrollers use original speed",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_RAVENSCROLL} },
	{ bitflag,	"Use original sound target handling",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SOUNDTARGET} },
	{ bitflag,	"DEH health settings like Doom2.exe",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DEHHEALTH} },
	{ bitflag,	"Self ref. sectors don't block shots",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_TRACE} },
	{ bitflag,	"Monsters get stuck over dropoffs",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_DROPOFF} },
	{ bitflag,	"Monsters cannot cross dropoffs",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_CROSSDROPOFF} },
	{ bitflag,	"Monsters see invisible players",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_INVISIBILITY} },
	{ bitflag,	"Boom scrollers are additive",				{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_BOOMSCROLL} },
	{ bitflag,	"Inst. moving floors are not silent",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SILENT_INSTANT_FLOORS} },
	{ bitflag,  "Sector sounds use center as source",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SECTORSOUNDS} },
	{ bitflag,  "Use Doom heights for missile clipping",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MISSILECLIP} },
	{ bitflag,  "Allow any bossdeath for level special",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_ANYBOSSDEATH} },
	{ bitflag,  "No Minotaur floor flames in water",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MINOTAUR} },
	{ bitflag,  "Original A_Mushroom speed in DEH mods",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MUSHROOM} },
	{ bitflag,	"Monster movement is affected by effects",	{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MBFMONSTERMOVE} },
	{ bitflag,	"Crushed monsters can be resurrected",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_CORPSEGIBS} },
	{ bitflag,	"Friendly monsters aren't blocked",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_NOBLOCKFRIENDS} },
	{ bitflag,	"Invert sprite sorting",					{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_SPRITESORT} },
	{ bitflag,	"Use Doom code for hitscan checks",			{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_HITSCAN} },
	{ bitflag,	"Cripple sound for silent BFG trick",		{&compatflags}, {0}, {0}, {0}, {(value_t *)COMPATF_MAGICSILENCE} },
	
	{ discrete, "Interpolate monster movement",	{&nomonsterinterpolation},		{2.0}, {0.0},	{0.0}, {NoYes} },
};

static menu_t CompatibilityMenu =
{
	"COMPATIBILITY OPTIONS",
	0,
	countof(CompatibilityItems),
	0,
	CompatibilityItems,
};

/*=======================================
 *
 * Sound Options Menu
 *
 *=======================================*/

#ifdef _WIN32
EXTERN_CVAR (Float, snd_movievolume)
#endif
EXTERN_CVAR (Bool, snd_flipstereo)
EXTERN_CVAR (Bool, snd_pitched)
EXTERN_CVAR (String, snd_output_format)
EXTERN_CVAR (String, snd_speakermode)
EXTERN_CVAR (String, snd_resampler)
EXTERN_CVAR (String, snd_output)
EXTERN_CVAR (Int, snd_buffersize)
EXTERN_CVAR (Int, snd_buffercount)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_hrtf)
EXTERN_CVAR (Bool, snd_waterreverb)
EXTERN_CVAR (Float, snd_waterlp)
EXTERN_CVAR (Int, snd_mididevice)

static void MakeSoundChanges ();
static void AdvSoundOptions ();
static void ModReplayerOptions ();

static value_t SampleRates[] =
{
	{ 0.f,		"Default" },
	{ 4000.f,	"4000 Hz" },
	{ 8000.f,	"8000 Hz" },
	{ 11025.f,	"11025 Hz" },
	{ 22050.f,	"22050 Hz" },
	{ 32000.f,	"32000 Hz" },
	{ 44100.f,	"44100 Hz" },
	{ 48000.f,	"48000 Hz" }
};

static value_t BufferSizes[] =
{
	{    0.f, "Default" },
	{   64.f, "64 samples" },
	{  128.f, "128 samples" },
	{  256.f, "256 samples" },
	{  512.f, "512 samples" },
	{ 1024.f, "1024 samples" },
	{ 2048.f, "2048 samples" },
	{ 4096.f, "4096 samples" }
};

static value_t BufferCounts[] =
{
	{    0.f, "Default" },
	{    2.f, "2" },
	{    3.f, "3" },
	{    4.f, "4" },
	{    5.f, "5" },
	{    6.f, "6" },
	{    7.f, "7" },
	{    8.f, "8" },
	{    9.f, "9" },
	{   10.f, "10" },
	{   11.f, "11" },
	{   12.f, "12" }
};

static valueenum_t Outputs[] =
{
	{ "Default",		"Default" },
#if defined(_WIN32)
	{ "DirectSound",	"DirectSound" },
	{ "WASAPI",			"Vista WASAPI" },
	{ "ASIO",			"ASIO" },
	{ "WaveOut",		"WaveOut" },
	{ "OpenAL",			"OpenAL (very beta)" },
#elif defined(unix)
	{ "OSS",			"OSS" },
	{ "ALSA",			"ALSA" },
	{ "SDL",			"SDL" },
	{ "ESD",			"ESD" },
#elif defined(__APPLE__)
	{ "Sound Manager",	"Sound Manager" },
	{ "Core Audio",		"Core Audio" },
#endif
	{ "No sound",		"No sound" }
};

static valueenum_t OutputFormats[] =
{
	{ "PCM-8",		"8-bit" },
	{ "PCM-16",		"16-bit" },
	{ "PCM-24",		"24-bit" },
	{ "PCM-32",		"32-bit" },
	{ "PCM-Float",	"32-bit float" }
};

static valueenum_t SpeakerModes[] =
{
	{ "Auto",		"Auto" },
	{ "Mono",		"Mono" },
	{ "Stereo",		"Stereo" },
	{ "Prologic",	"Dolby Prologic Decoder" },
	{ "Quad",		"Quad" },
	{ "Surround",	"5 speakers" },
	{ "5.1",		"5.1 speakers" },
	{ "7.1",		"7.1 speakers" }
};

static valueenum_t Resamplers[] =
{
	{ "NoInterp",	"No interpolation" },
	{ "Linear",		"Linear" },
	{ "Cubic",		"Cubic" },
	{ "Spline",		"Spline" }
};

static menuitem_t SoundItems[] =
{
	{ slider,	"Sounds volume",		{&snd_sfxvolume},		{0.0}, {1.0},	{0.05f}, {NULL} },
	{ slider,	"Music volume",			{&snd_musicvolume},		{0.0}, {1.0},	{0.05f}, {NULL} },
	{ discrete, "MIDI device",			{&snd_mididevice},		{0.0}, {0.0},	{0.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Underwater reverb",	{&snd_waterreverb},		{2.0}, {0.0},	{0.0}, {OnOff} },
	{ slider,	"Underwater cutoff",	{&snd_waterlp},			{0.0}, {2000.0},{50.0}, {NULL} },
	{ discrete, "Randomize pitches",	{&snd_pitched},			{2.0}, {0.0},	{0.0}, {OnOff} },
	{ slider,	"Sound channels",		{&snd_channels},		{8.0}, {256.0},	{8.0}, {NULL} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Restart sound",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)MakeSoundChanges} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ ediscrete,"Output system",		{&snd_output},			{countof(Outputs)}, {0.0}, {0.0}, {(value_t *)Outputs} },
	{ ediscrete,"Output format",		{&snd_output_format},	{5.0}, {0.0},	{0.0}, {(value_t *)OutputFormats} },
	{ ediscrete,"Speaker mode",			{&snd_speakermode},		{8.0}, {0.0},	{0.0}, {(value_t *)SpeakerModes} },
	{ ediscrete,"Resampler",			{&snd_resampler},		{4.0}, {0.0},	{0.0}, {(value_t *)Resamplers} },
	{ discrete, "HRTF filter",			{&snd_hrtf},			{2.0}, {0.0},	{0.0}, {(value_t *)OnOff} },

	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ more,		"Advanced options",		{NULL},					{0.0}, {0.0},	{0.0}, {(value_t *)AdvSoundOptions} },
	{ more,		"Module replayer options", {NULL},				{0.0}, {0.0},	{0.0}, {(value_t *)ModReplayerOptions} },
};

static menu_t SoundMenu =
{
	"SOUND OPTIONS",
	0,
	countof(SoundItems),
	0,
	SoundItems,
};

#define MIDI_DEVICE_ITEM 2

/*=======================================
 *
 * Advanced Sound Options Menu
 *
 *=======================================*/

EXTERN_CVAR (Bool, opl_onechip)

static menuitem_t AdvSoundItems[] =
{
	{ discrete, "Sample rate",			{&snd_samplerate},		{8.0}, {0.0},	{0.0}, {SampleRates} },
	{ discrete, "Buffer size",			{&snd_buffersize},		{8.0}, {0.0},	{0.0}, {BufferSizes} },
	{ discrete, "Buffer count",			{&snd_buffercount},		{12.0}, {0.0},	{0.0}, {BufferCounts} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ whitetext,"OPL Synthesis",		{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Only emulate one OPL chip", {&opl_onechip},	{2.0}, {0.0},	{0.0}, {OnOff} },
};

static menu_t AdvSoundMenu =
{
	"ADVANCED SOUND OPTIONS",
	0,
	countof(AdvSoundItems),
	0,
	AdvSoundItems,
};

/*=======================================
 *
 * Module Replayer Options Menu
 *
 *=======================================*/

EXTERN_CVAR(Bool, mod_dumb)
EXTERN_CVAR(Int, mod_samplerate)
EXTERN_CVAR(Int, mod_volramp)
EXTERN_CVAR(Int, mod_interp)
EXTERN_CVAR(Bool, mod_autochip)
EXTERN_CVAR(Int, mod_autochip_size_force)
EXTERN_CVAR(Int, mod_autochip_size_scan)
EXTERN_CVAR(Int, mod_autochip_scan_threshold)

static value_t ModReplayers[] =
{
	{ 0.0, "FMOD" },
	{ 1.0, "foo_dumb" }
};

static value_t ModInterpolations[] =
{
	{ 0.0, "None" },
	{ 1.0, "Linear" },
	{ 2.0, "Cubic" }
};

static value_t ModVolumeRamps[] =
{
	{ 0.0, "None" },
	{ 1.0, "Logarithmic" },
	{ 2.0, "Linear" },
	{ 3.0, "XM=lin, else none" },
	{ 4.0, "XM=lin, else log" }
};

static menuitem_t ModReplayerItems[] =
{
	{ discrete, "Replayer engine",		{&mod_dumb},			{2.0}, {0.0},	{0.0}, {ModReplayers} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Sample rate",			{&mod_samplerate},		{8.0}, {0.0},	{0.0}, {SampleRates} },
	{ discrete, "Interpolation",		{&mod_interp},			{3.0}, {0.0},	{0.0}, {ModInterpolations} },
	{ discrete, "Volume ramping",		{&mod_volramp},			{5.0}, {0.0},	{0.0}, {ModVolumeRamps} },
	{ redtext,	" ",					{NULL},					{0.0}, {0.0},	{0.0}, {NULL} },
	{ discrete, "Chip-o-matic",			{&mod_autochip},		{2.0}, {0.0},	{0.0}, {OnOff} },
	// TODO if the menu system is ever rewritten: Provide a decent
	// mechanism to edit the chip-o-matic settings like you can with
	// the foo_dumb preferences in foobar2000.
};

static menu_t ModReplayerMenu =
{
	"MODULE REPLAYER OPTIONS",
	0,
	countof(ModReplayerItems),
	0,
	ModReplayerItems,
};


//===========================================================================
static void ActivateConfirm (const char *text, void (*func)())
{
	ConfirmItems[0].label = text;
	ConfirmItems[0].e.mfunc = func;
	ConfirmMenu.lastOn = 3;
	M_SwitchMenu (&ConfirmMenu);
}

static void ConfirmIsAGo ()
{
	M_PopMenuStack ();
	ConfirmItems[0].e.mfunc ();
}

//
//		Set some stuff up for the video modes menu
//
static BYTE BitTranslate[32];

void M_OptInit (void)
{
	if (gameinfo.gametype & GAME_DoomChex)
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

void M_InitVideoModesMenu ()
{
	int dummy1, dummy2;
	size_t currval = 0;

	M_RefreshModesList();

	for (unsigned int i = 1; i <= 32 && currval < countof(Depths); i++)
	{
		Video->StartModeIterator (i, screen->IsFullscreen());
		if (Video->NextMode (&dummy1, &dummy2, NULL))
		{
			/*
			Depths[currval].value = currval;
			mysnprintf (name, countof(name), "%d bit", i);
			Depths[currval].name = copystring (name);
			*/
			BitTranslate[currval++] = i;
		}
	}

	//ModesItems[VM_DEPTHITEM].b.min = (float)currval;

	switch (Video->GetDisplayType ())
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
}


//
//		Toggle messages on/off
//
void M_ChangeMessages ()
{
	if (show_messages)
	{
		Printf (128, "%s\n", GStrings("MSGOFF"));
		show_messages = false;
	}
	else
	{
		Printf (128, "%s\n", GStrings("MSGON"));
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
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
}

CCMD (sizeup)
{
	M_SizeDisplay (1);
	S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
}

// Draws a string in the console font, scaled to the 8x8 cells
// used by the default console font.
void M_DrawConText (int color, int x, int y, const char *str)
{
	int len = (int)strlen(str);

	screen->DrawText (ConFont, color, x, y, str,
		DTA_CellX, 8 * CleanXfac_1,
		DTA_CellY, 8 * CleanYfac_1,
		TAG_DONE);
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
		if (item->type != whitetext && item->type != redtext && item->type != screenres &&
			item->type != joymore && (item->type != discrete || item->c.discretecenter != 1))
		{
			thiswidth = SmallFont->StringWidth (item->label);
			if (thiswidth > widest)
				widest = thiswidth;
		}
	}
	menu->indent = widest + 4;
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

// Draw a slider. Set fracdigits negative to not display the current value numerically.
static void M_DrawSlider (int x, int y, double min, double max, double cur,int fracdigits)
{
	double range;

	range = max - min;
	double ccur = clamp(cur, min, max) - min;

	M_DrawConText(CR_WHITE, x, y, "\x10\x11\x11\x11\x11\x11\x11\x11\x11\x11\x11\x12");
	M_DrawConText(CR_ORANGE, x + int((5 + ((ccur * 78) / range)) * CleanXfac_1), y, "\x13");

	if (fracdigits >= 0)
	{
		char textbuf[16];
		mysnprintf(textbuf, countof(textbuf), "%.*f", fracdigits, cur);
		screen->DrawText(SmallFont, CR_DARKGRAY, x + (12*8 + 4) * CleanXfac_1, y, textbuf, DTA_CleanNoMove_1, true, TAG_DONE);
	}
}

int M_FindCurVal (float cur, value_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

int M_FindCurVal (float cur, valuestring_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

const char *M_FindCurVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v].name;
		}
	}
	return cur;
}

const char *M_FindPrevVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v == 0 ? numvals - 1 : v - 1].value;
		}
	}
	return values[0].value;
}

const char *M_FindNextVal(const char *cur, valueenum_t *values, int numvals)
{
	for (int v = 0; v < numvals; ++v)
	{
		if (stricmp(values[v].value, cur) == 0)
		{
			return values[v == numvals - 1 ? 0 : v + 1].value;
		}
	}
	return values[0].value;
}

void M_OptDrawer ()
{
	EColorRange color;
	int y, width, i, x, ytop, fontheight;
	menuitem_t *item;
	UCVarValue value;
	DWORD overlay;
	int labelofs;
	int indent;
	int cursorspace;

	if (!CurrentMenu->DontDim)
	{
		screen->Dim ();
	}

	if (CurrentMenu->PreDraw !=  NULL)
	{
		if (CurrentMenu->PreDraw ()) return;
	}

	if (CurrentMenu->y != 0)
	{
		y = CurrentMenu->y;
	}
	else
	{
		if (BigFont && CurrentMenu->texttitle)
		{
			screen->DrawText (BigFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
				(screen->GetWidth() - BigFont->StringWidth(CurrentMenu->texttitle) * CleanXfac_1) / 2, 10*CleanYfac_1,
				CurrentMenu->texttitle, DTA_CleanNoMove_1, true, TAG_DONE);
			y = 15 + BigFont->GetHeight();
		}
		else
		{
			y = 15;
		}
	}
	if (gameinfo.gametype & GAME_Raven)
	{
		labelofs = 2 * CleanXfac_1;
		y -= 2;
		fontheight = 9;
	}
	else
	{
		labelofs = 0;
		fontheight = 8;
	}
	cursorspace = 14 * CleanXfac_1;
	y *= CleanYfac_1;
	fontheight *= CleanYfac_1;
	ytop = y + CurrentMenu->scrolltop * 8 * CleanYfac_1;
	int lastrow = screen->GetHeight() - SmallFont->GetHeight() * CleanYfac_1;

	for (i = 0; i < CurrentMenu->numitems && y <= lastrow; i++, y += fontheight)
	{
		if (i == CurrentMenu->scrolltop)
		{
			i += CurrentMenu->scrollpos;
		}

		item = CurrentMenu->items + i;
		overlay = 0;
		if (item->type == discrete && item->c.discretecenter == 1)
		{
			indent = screen->GetWidth() / 2;
		}
		else if (item->type == joymore)
		{
			indent = 4 * CleanXfac_1;
		}
		else
		{
			indent = CurrentMenu->indent;
			if (indent > 280)
			{ // kludge for the compatibility options with their extremely long labels
				if (indent + 40 <= CleanWidth_1)
				{
					indent = (screen->GetWidth() - ((indent + 40) * CleanXfac_1)) / 2 + indent * CleanXfac_1;
				}
				else
				{
					indent = screen->GetWidth() - 40 * CleanXfac_1;
				}
			}
			else
			{
				indent = (indent - 160) * CleanXfac_1 + screen->GetWidth() / 2;
			}
		}

		if (item->type != screenres)
		{
			FString somestring;
			const char *label;
			if (item->type != joymore)
			{
				label = item->label;
			}
			else
			{
				if (Joysticks.Size() == 0)
				{
					label = "No devices connected";
				}
				else
				{
					somestring = Joysticks[item->a.joyselection]->GetName();
					label = somestring;
				}
			}
			width = SmallFont->StringWidth(label) * CleanXfac_1;
			switch (item->type)
			{
			case more:
			case safemore:
				x = indent - width;
				color = MoreColor;
				break;

			case joymore:
				x = 20 * CleanXfac_1;
				color = MoreColor;
				break;

			case numberedmore:
			case rsafemore:
			case rightmore:
				x = indent + cursorspace;
				color = item->type != rightmore ? CR_GREEN : MoreColor;
				break;

			case redtext:
				x = screen->GetWidth() / 2 - width / 2;
				color = LabelColor;
				break;

			case whitetext:
				x = screen->GetWidth() / 2 - width / 2;
				color = CR_GOLD;//ValueColor;
				break;

			case listelement:
				x = indent + cursorspace;
				color = LabelColor;
				break;

			case colorpicker:
				x = indent + cursorspace;
				color = MoreColor;
				break;

			case discrete:
				if (item->d.graycheck != NULL && !(**item->d.graycheck))
				{
					overlay = MAKEARGB(128,0,0,0);
				}
				// Intentional fall-through

			default:
				x = indent - width;
				color = (item->type == control && menuactive == MENU_WaitKey && i == CurrentItem)
					? CR_YELLOW : LabelColor;
				break;
			}
			screen->DrawText (SmallFont, color, x, y, label, DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);

			switch (item->type)
			{
			case numberedmore:
				if (item->b.position != 0)
				{
					char tbuf[16];

					mysnprintf (tbuf, countof(tbuf), "%d.", item->b.position);
					x = indent - SmallFont->StringWidth (tbuf) * CleanXfac_1;
					screen->DrawText (SmallFont, CR_GREY, x, y, tbuf, DTA_CleanNoMove_1, true, TAG_DONE);
				}
				break;

			case bitmask:
			{
				int v, vals;

				value = item->a.cvar->GetGenericRep (CVAR_Int);
				value.Float = float(value.Int & int(item->c.max));
				vals = (int)item->b.numvalues;

				v = M_FindCurVal (value.Float, item->e.values, vals);

				if (v == vals)
				{
					screen->DrawText (SmallFont, ValueColor, indent + cursorspace, y, "Unknown",
						DTA_CleanNoMove_1, true, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
						indent + cursorspace, y, item->e.values[v].name,
						DTA_CleanNoMove_1, true, TAG_DONE);
				}

			}
			break;

			case discretes:
			case discrete:
			case cdiscrete:
			case inverter:
			case joy_map:
			{
				int v, vals;

				overlay = 0;
				if (item->type == joy_map)
				{
					value.Float = (float)SELECTED_JOYSTICK->GetAxisMap(item->a.joyselection);
				}
				else
				{
					value = item->a.cvar->GetGenericRep (CVAR_Float);
				}
				if (item->type == inverter)
				{
					value.Float = (value.Float < 0.f);
					vals = 2;
				}
				else
				{
					vals = (int)item->b.numvalues;
				}
				if (item->type != discretes)
				{
					v = M_FindCurVal (value.Float, item->e.values, vals);
				}
				else
				{
					v = M_FindCurVal (value.Float, item->e.valuestrings, vals);
				}
				if (item->type == discrete)
				{
					if (item->d.graycheck != NULL && !(**item->d.graycheck))
					{
						overlay = MAKEARGB(96,48,0,0);
					}
				}

				if (v == vals)
				{
					screen->DrawText (SmallFont, ValueColor, indent + cursorspace, y, "Unknown",
						DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
				}
				else
				{
					screen->DrawText (SmallFont, item->type == cdiscrete ? v : ValueColor,
						indent + cursorspace, y,
						item->type != discretes ? item->e.values[v].name : item->e.valuestrings[v].name.GetChars(),
						DTA_CleanNoMove_1, true, DTA_ColorOverlay, overlay, TAG_DONE);
				}

			}
			break;

			case ediscrete:
			{
				const char *v;

				value = item->a.cvar->GetGenericRep (CVAR_String);
				v = M_FindCurVal(value.String, item->e.enumvalues, (int)item->b.numvalues);
				screen->DrawText(SmallFont, ValueColor, indent + cursorspace, y, v, DTA_CleanNoMove_1, true, TAG_DONE);
			}
			break;

			case nochoice:
				screen->DrawText (SmallFont, CR_GOLD, indent + cursorspace, y,
					(item->e.values[(int)item->b.min]).name, DTA_CleanNoMove_1, true, TAG_DONE);
				break;

			case joy_sens:
				value.Float = SELECTED_JOYSTICK->GetSensitivity();
				M_DrawSlider (indent + cursorspace, y + labelofs, item->b.min, item->c.max, value.Float, 1);
				break;

			case joy_slider:
				if (item->e.joyslidernum == 0)
				{
					value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				}
				else
				{
					assert(item->e.joyslidernum == 1);
					value.Float = SELECTED_JOYSTICK->GetAxisDeadZone(item->a.joyselection);
				}
				M_DrawSlider (indent + cursorspace, y + labelofs, item->b.min, item->c.max, fabs(value.Float), 3);
				break;

			case joy_inverter:
				assert(item->e.joyslidernum == 0);
				value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				screen->DrawText(SmallFont, ValueColor, indent + cursorspace, y,
					(value.Float < 0) ? "Yes" : "No",
					DTA_CleanNoMove_1, true, TAG_DONE);
				break;

			case slider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (indent + cursorspace, y + labelofs, item->b.min, item->c.max, value.Float, 1);
				break;

			case absslider:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				M_DrawSlider (indent + cursorspace, y + labelofs, item->b.min, item->c.max, fabs(value.Float), 1);
				break;

			case intslider:
				M_DrawSlider (indent + cursorspace, y + labelofs, item->b.min, item->c.max, item->a.fval, 0);
				break;

			case control:
			{
				char description[64];

				C_NameKeys (description, item->b.key1, item->c.key2);
				if (description[0])
				{
					M_DrawConText(CR_WHITE, indent + cursorspace, y-1+labelofs, description);
				}
				else
				{
					screen->DrawText(SmallFont, CR_BLACK, indent + cursorspace, y + labelofs, "---",
						DTA_CleanNoMove_1, true, TAG_DONE);
				}
			}
			break;

			case colorpicker:
			{
				int box_x, box_y;
				box_x = indent - 35 * CleanXfac_1;
				box_y = (gameinfo.gametype & GAME_Raven) ? y - CleanYfac_1 : y;
				screen->Clear (box_x, box_y, box_x + 32*CleanXfac_1, box_y + fontheight-CleanYfac_1,
					item->a.colorcvar->GetIndex(), 0);
			}
			break;

			case palettegrid:
			{
				int box_x, box_y;
				int x1, p;
				const int w = fontheight;
				const int h = fontheight;

				box_y = y - 2 * CleanYfac_1;
				p = 0;
				box_x = indent - 32 * CleanXfac_1;
				for (x1 = 0, p = int(item->b.min * 16); x1 < 16; ++p, ++x1)
				{
					screen->Clear (box_x, box_y, box_x + w, box_y + h, p, 0);
					if (p == CurrColorIndex || (i == CurrentItem && x1 == SelColorIndex))
					{
						int r, g, b;
						DWORD col;
						double blinky;
						if (i == CurrentItem && x1 == SelColorIndex)
						{
							r = 255, g = 128, b = 0;
						}
						else
						{
							r = 200, g = 200, b = 255;
						}
						// Make sure the cursors stand out against similar colors
						// by pulsing them.
						blinky = fabs(sin(I_MSTime()/1000.0)) * 0.5 + 0.5;
						col = MAKEARGB(255,int(r*blinky),int(g*blinky),int(b*blinky));

						screen->Clear (box_x, box_y, box_x + w, box_y + 1, -1, col);
						screen->Clear (box_x, box_y + h-1, box_x + w, box_y + h, -1, col);
						screen->Clear (box_x, box_y, box_x + 1, box_y + h, -1, col);
						screen->Clear (box_x + w - 1, box_y, box_x + w, box_y + h, -1, col);
					}
					box_x += w;
				}
			}
			break;

			case bitflag:
			{
				value_t *value;
				const char *str;

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

				screen->DrawText (SmallFont, ValueColor,
					indent + cursorspace, y, str, DTA_CleanNoMove_1, true, TAG_DONE);
			}
			break;

			default:
				break;
			}

			if (item->type != palettegrid &&	// Palette grids draw their own cursor
				i == CurrentItem &&
				(skullAnimCounter < 6 || menuactive == MENU_WaitKey))
			{
				M_DrawConText(CR_RED, indent + 3 * CleanXfac_1, y-CleanYfac_1+labelofs, "\xd");
			}
		}
		else
		{
			char *str = NULL;
			int colwidth = screen->GetWidth() / 3;

			for (x = 0; x < 3; x++)
			{
				switch (x)
				{
				case 0: str = item->b.res1; break;
				case 1: str = item->c.res2; break;
				case 2: str = item->d.res3; break;
				}
				if (str)
				{
					if (x == item->e.highlight)
 						color = CR_GOLD;	//ValueColor;
					else
						color = CR_BRICK;	//LabelColor;

					screen->DrawText (SmallFont, color, colwidth * x + 20 * CleanXfac_1, y, str, DTA_CleanNoMove_1, true, TAG_DONE);
				}
			}

			if (i == CurrentItem && ((item->a.selmode != -1 && (skullAnimCounter < 6 || menuactive == MENU_WaitKey)) || testingmode))
			{
				M_DrawConText(CR_RED, item->a.selmode * colwidth + 8 * CleanXfac_1, y - CleanYfac_1 + labelofs, "\xd");
			}
		}
	}

	CanScrollUp = (CurrentMenu->scrollpos > 0);
	CanScrollDown = (i < CurrentMenu->numitems);
	VisBottom = i - 1;

	if (CanScrollUp)
	{
		M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, ytop + labelofs, "\x1a");
	}
	if (CanScrollDown)
	{
		M_DrawConText(CR_ORANGE, 3 * CleanXfac_1, y - 8*CleanYfac_1 + labelofs, "\x1b");
	}

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
					fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "    ");
				}
				printed = true;
				fillptr += mysnprintf (fillptr, countof(flagsblah) - (fillptr - flagsblah), "%s = %d", vars[i]->GetName (), **vars[i]);
			}
		}
		screen->DrawText (SmallFont, ValueColor,
			(screen->GetWidth() - SmallFont->StringWidth (flagsblah) * CleanXfac_1) / 2, 0, flagsblah,
			DTA_CleanNoMove_1, true, TAG_DONE);
	}
}

void M_OptResponder(event_t *ev)
{
	menuitem_t *item = CurrentMenu->items + CurrentItem;

	if (menuactive == MENU_WaitKey && ev->type == EV_KeyDown)
	{
		if (ev->data1 != KEY_ESCAPE)
		{
			C_ChangeBinding(item->e.command, ev->data1);
			M_BuildKeyList(CurrentMenu->items, CurrentMenu->numitems);
		}
		menuactive = MENU_On;
		CurrentMenu->items[0].label = OldMessage;
		CurrentMenu->items[0].type = OldType;
	}
	else if (ev->type == EV_GUI_Event && ev->subtype == EV_GUI_KeyDown)
	{
		if (CurrentMenu == &ModesMenu && (ev->data1 == 't' || ev->data1 == 'T'))
		{ // Test selected resolution
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
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			SetModesMenu (NewWidth, NewHeight, NewBits);
		}
		else if (ev->data1 >= '0' && ev->data1 <= '9')
		{ // Activate an item of type numberedmore
			int i;
			int num = ev->data1 == '0' ? 10 : ev->data1 - '0';

			for (i = 0; i < CurrentMenu->numitems; ++i)
			{
				menuitem_t *item = CurrentMenu->items + i;

				if (item->type == numberedmore && item->b.position == num)
				{
					CurrentItem = i;
					M_OptButtonHandler(MKEY_Enter, false);
					break;
				}
			}
		}
	}
}

void M_OptButtonHandler(EMenuKey key, bool repeat)
{
	menuitem_t *item;
	UCVarValue value;

	item = CurrentMenu->items + CurrentItem;

	if (item->type == bitflag &&
		(key == MKEY_Left || key == MKEY_Right || key == MKEY_Enter)
		&& !demoplayback)
	{
		*(item->a.intcvar) = (*(item->a.intcvar)) ^ item->e.flagmask;
		return;
	}

	// The controls that manipulate joystick interfaces can only be changed from the
	// keyboard, because I can't think of a good way to avoid problems otherwise.
	if (item->type == discrete && item->c.discretecenter == 2 && (key == MKEY_Left || key == MKEY_Right))
	{
		if (repeat)
		{
			return;
		}
		for (int i = 0; i < FButtonStatus::MAX_KEYS; ++i)
		{
			if (MenuButtons[key].Keys[i] >= KEY_FIRSTJOYBUTTON)
			{
				return;
			}
		}
	}

	switch (key)
	{
	default:
		break;		// Keep GCC quiet

	case MKEY_Down:
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
					  !CurrentMenu->items[CurrentItem].b.res1) ||
					 (CurrentMenu->items[CurrentItem].type == numberedmore &&
					  !CurrentMenu->items[CurrentItem].b.position));

			if (CurrentMenu->items[CurrentItem].type == screenres)
			{
				item = &CurrentMenu->items[CurrentItem];
				while ((modecol == 2 && !item->d.res3) || (modecol == 1 && !item->c.res2))
				{
					modecol--;
				}
				CurrentMenu->items[CurrentItem].a.selmode = modecol;
			}

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case MKEY_Up:
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
					int ytop, maxitems, rowheight;

					// Figure out how many lines of text fit on the menu
					if (CurrentMenu->y != 0)
					{
						ytop = CurrentMenu->y;
					}
					else if (BigFont && CurrentMenu->texttitle)
					{
						ytop = 15 + BigFont->GetHeight ();
					}
					else
					{
						ytop = 15;
					}
					if (!(gameinfo.gametype & GAME_DoomChex))
					{
						ytop -= 2;
						rowheight = 9;
					}
					else
					{
						rowheight = 8;
					}
					ytop *= CleanYfac_1;
					rowheight *= CleanYfac_1;
					maxitems = (screen->GetHeight() - rowheight - ytop) / rowheight + 1;

					CurrentMenu->scrollpos = MAX (0,CurrentMenu->numitems - maxitems + CurrentMenu->scrolltop);
					CurrentItem = CurrentMenu->numitems - 1;
				}
			} while (CurrentMenu->items[CurrentItem].type == redtext ||
					 CurrentMenu->items[CurrentItem].type == whitetext ||
					 (CurrentMenu->items[CurrentItem].type == screenres &&
					  !CurrentMenu->items[CurrentItem].b.res1) ||
					 (CurrentMenu->items[CurrentItem].type == numberedmore &&
					  !CurrentMenu->items[CurrentItem].b.position));

			if (CurrentMenu->items[CurrentItem].type == screenres)
				CurrentMenu->items[CurrentItem].a.selmode = modecol;

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case MKEY_PageUp:
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
					!CurrentMenu->items[CurrentItem].b.res1) ||
				   (CurrentMenu->items[CurrentItem].type == numberedmore &&
					!CurrentMenu->items[CurrentItem].b.position))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case MKEY_PageDown:
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
					!CurrentMenu->items[CurrentItem].b.res1) ||
				   (CurrentMenu->items[CurrentItem].type == numberedmore &&
					!CurrentMenu->items[CurrentItem].b.position))
			{
				++CurrentItem;
			}
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
		}
		break;

	case MKEY_Left:
		switch (item->type)
		{
			case slider:
			case absslider:
			case intslider:
				{
					UCVarValue newval;
					bool reversed;

					if (item->type == intslider)
						value.Float = item->a.fval;
					else
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

					if (item->type == intslider)
						item->a.fval = newval.Float;
					else if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_sens:
				value.Float = SELECTED_JOYSTICK->GetSensitivity() - item->d.step;
				if (value.Float < item->b.min)
					value.Float = item->b.min;
				SELECTED_JOYSTICK->SetSensitivity(value.Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_slider:
				if (item->e.joyslidernum == 0)
				{
					value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				}
				else
				{
					assert(item->e.joyslidernum == 1);
					value.Float = SELECTED_JOYSTICK->GetAxisDeadZone(item->a.joyselection);
				}
				if (value.Float >= 0)
				{
					value.Float -= item->d.step;
					if (value.Float < item->b.min)
						value.Float = item->b.min;
				}
				else
				{
					value.Float += item->d.step;
					if (value.Float < -item->c.max)
						value.Float = -item->c.max;
				}
				if (item->e.joyslidernum == 0)
				{
					SELECTED_JOYSTICK->SetAxisScale(item->a.joyselection, value.Float);
				}
				else
				{
					SELECTED_JOYSTICK->SetAxisDeadZone(item->a.joyselection, value.Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case palettegrid:
				SelColorIndex = (SelColorIndex - 1) & 15;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case discretes:
			case discrete:
			case cdiscrete:
			case joy_map:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.min;
					if (item->type == joy_map)
					{
						value.Float = (float)SELECTED_JOYSTICK->GetAxisMap(item->a.joyselection);
					}
					else
					{
						value = item->a.cvar->GetGenericRep (CVAR_Float);
					}
					if (item->type != discretes)
					{
						cur = M_FindCurVal (value.Float, item->e.values, numvals);
					}
					else
					{
						cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
					}
					if (--cur < 0)
						cur = numvals - 1;

					value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
					if (item->type == joy_map)
					{
						SELECTED_JOYSTICK->SetAxisMap(item->a.joyselection, (EJoyAxis)(int)value.Float);
					}
					else
					{
						item->a.cvar->SetGenericRep (value, CVAR_Float);
					}

					// Hack hack. Rebuild list of resolutions
					if (item->e.values == Depths)
						BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case ediscrete:
				value = item->a.cvar->GetGenericRep(CVAR_String);
				value.String = const_cast<char *>(M_FindPrevVal(value.String, item->e.enumvalues, (int)item->b.numvalues));
				item->a.cvar->SetGenericRep(value, CVAR_String);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case bitmask:
				{
					int cur;
					int numvals;
					int bmask = int(item->c.max);

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Int);
					
					cur = M_FindCurVal (float(value.Int & bmask), item->e.values, numvals);
					if (--cur < 0)
						cur = numvals - 1;

					value.Int = (value.Int & ~bmask) | int(item->e.values[cur].value);
					item->a.cvar->SetGenericRep (value, CVAR_Int);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_inverter:
				assert(item->e.joyslidernum == 0);
				value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				SELECTED_JOYSTICK->SetAxisScale(item->a.joyselection, -value.Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
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
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case MKEY_Right:
		switch (item->type)
		{
			case slider:
			case absslider:
			case intslider:
				{
					UCVarValue newval;
					bool reversed;

					if (item->type == intslider)
						value.Float = item->a.fval;
					else
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

					if (item->type == intslider)
						item->a.fval = newval.Float;
					else if (item->e.cfunc)
						item->e.cfunc (item->a.cvar, newval.Float);
					else
						item->a.cvar->SetGenericRep (newval, CVAR_Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_sens:
				value.Float = SELECTED_JOYSTICK->GetSensitivity() + item->d.step;
				if (value.Float > item->c.max)
					value.Float = item->c.max;
				SELECTED_JOYSTICK->SetSensitivity(value.Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_slider:
				if (item->e.joyslidernum == 0)
				{
					value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				}
				else
				{
					assert(item->e.joyslidernum == 1);
					value.Float = SELECTED_JOYSTICK->GetAxisDeadZone(item->a.joyselection);
				}
				if (value.Float >= 0)
				{
					value.Float += item->d.step;
					if (value.Float > item->c.max)
						value.Float = item->c.max;
				}
				else
				{
					value.Float -= item->d.step;
					if (value.Float > item->b.min)
						value.Float = -item->b.min;
				}
				if (item->e.joyslidernum == 0)
				{
					SELECTED_JOYSTICK->SetAxisScale(item->a.joyselection, value.Float);
				}
				else
				{
					SELECTED_JOYSTICK->SetAxisDeadZone(item->a.joyselection, value.Float);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case palettegrid:
				SelColorIndex = (SelColorIndex + 1) & 15;
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			case discretes:
			case discrete:
			case cdiscrete:
			case joy_map:
				{
					int cur;
					int numvals;

					numvals = (int)item->b.min;
					if (item->type == joy_map)
					{
						value.Float = (float)SELECTED_JOYSTICK->GetAxisMap(item->a.joyselection);
					}
					else
					{
						value = item->a.cvar->GetGenericRep (CVAR_Float);
					}
					if (item->type != discretes)
					{
						cur = M_FindCurVal (value.Float, item->e.values, numvals);
					}
					else
					{
						cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
					}
					if (++cur >= numvals)
						cur = 0;

					value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
					if (item->type == joy_map)
					{
						SELECTED_JOYSTICK->SetAxisMap(item->a.joyselection, (EJoyAxis)(int)value.Float);
					}
					else
					{
						item->a.cvar->SetGenericRep (value, CVAR_Float);
					}

					// Hack hack. Rebuild list of resolutions
					if (item->e.values == Depths)
						BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case ediscrete:
				value = item->a.cvar->GetGenericRep(CVAR_String);
				value.String = const_cast<char *>(M_FindNextVal(value.String, item->e.enumvalues, (int)item->b.numvalues));
				item->a.cvar->SetGenericRep(value, CVAR_String);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case bitmask:
				{
					int cur;
					int numvals;
					int bmask = int(item->c.max);

					numvals = (int)item->b.min;
					value = item->a.cvar->GetGenericRep (CVAR_Int);
					
					cur = M_FindCurVal (float(value.Int & bmask), item->e.values, numvals);
					if (++cur >= numvals)
						cur = 0;

					value.Int = (value.Int & ~bmask) | int(item->e.values[cur].value);
					item->a.cvar->SetGenericRep (value, CVAR_Int);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case inverter:
				value = item->a.cvar->GetGenericRep (CVAR_Float);
				value.Float = -value.Float;
				item->a.cvar->SetGenericRep (value, CVAR_Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
				break;

			case joy_inverter:
				assert(item->e.joyslidernum == 0);
				value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
				SELECTED_JOYSTICK->SetAxisScale(item->a.joyselection, -value.Float);
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
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
				S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", 1, ATTN_NONE);
				break;

			default:
				break;
		}
		break;

	case MKEY_Clear:
		if (item->type == control)
		{
			C_UnbindACommand (item->e.command);
			item->b.key1 = item->c.key2 = 0;
		}
		break;

	case MKEY_Enter:
		if (CurrentMenu == &ModesMenu && item->type == screenres)
		{
			if (!GetSelectedSize (CurrentItem, &NewWidth, &NewHeight))
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
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			SetModesMenu (NewWidth, NewHeight, NewBits);
		}
		else if ((item->type == more ||
				  item->type == numberedmore ||
				  item->type == rightmore ||
				  item->type == rsafemore ||
				  item->type == joymore ||
				  item->type == safemore)
				 && item->e.mfunc)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			if (item->type == safemore || item->type == rsafemore)
			{
				ActivateConfirm (item->label, item->e.mfunc);
			}
			else
			{
				item->e.mfunc();
			}
		}
		else if (item->type == discrete || item->type == cdiscrete || item->type == discretes)
		{
			int cur;
			int numvals;

			numvals = (int)item->b.min;
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			if (item->type != discretes)
			{
				cur = M_FindCurVal (value.Float, item->e.values, numvals);
			}
			else
			{
				cur = M_FindCurVal (value.Float, item->e.valuestrings, numvals);
			}
			if (++cur >= numvals)
				cur = 0;

			value.Float = item->type != discretes ? item->e.values[cur].value : item->e.valuestrings[cur].value;
			item->a.cvar->SetGenericRep (value, CVAR_Float);

			// Hack hack. Rebuild list of resolutions
			if (item->e.values == Depths)
				BuildModesList (SCREENWIDTH, SCREENHEIGHT, DisplayBits);

			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == control)
		{
			menuactive = MENU_WaitKey;
			OldMessage = CurrentMenu->items[0].label;
			OldType = CurrentMenu->items[0].type;
			CurrentMenu->items[0].label = "Press new key for control, ESC to cancel";
			CurrentMenu->items[0].type = redtext;
		}
		else if (item->type == listelement)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			item->e.lfunc (CurrentItem);
		}
		else if (item->type == inverter)
		{
			value = item->a.cvar->GetGenericRep (CVAR_Float);
			value.Float = -value.Float;
			item->a.cvar->SetGenericRep (value, CVAR_Float);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == joy_inverter)
		{
			assert(item->e.joyslidernum == 0);
			value.Float = SELECTED_JOYSTICK->GetAxisScale(item->a.joyselection);
			SELECTED_JOYSTICK->SetAxisScale(item->a.joyselection, -value.Float);
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/change", 1, ATTN_NONE);
		}
		else if (item->type == screenres)
		{
		}
		else if (item->type == colorpicker)
		{
			CurrentMenu->lastOn = CurrentItem;
			S_Sound (CHAN_VOICE | CHAN_UI, "menu/choose", 1, ATTN_NONE);
			StartColorPickerMenu (item->label, item->a.colorcvar);
		}
		else if (item->type == palettegrid)
		{
			UpdateSelColor (SelColorIndex + int(item->b.min * 16));
		}
		break;

	case MKEY_Back:
		CurrentMenu->lastOn = CurrentItem;
		if (CurrentMenu->EscapeHandler != NULL)
		{
			CurrentMenu->EscapeHandler ();
		}
		M_PopMenuStack ();
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

static void StartAutomapMenu (void)
{
	M_SwitchMenu (&AutomapMenu);
}

static void StartScoreboardMenu (void)
{
	M_SwitchMenu (&ScoreboardMenu);
}

CCMD (menu_messages)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartMessagesMenu ();
}

CCMD (menu_automap)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartAutomapMenu ();
}

CCMD (menu_scoreboard)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartScoreboardMenu ();
}

static void StartMapColorsMenu (void)
{
	M_SwitchMenu (&MapColorsMenu);
}

CCMD (menu_mapcolors)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	StartMapColorsMenu ();
}

static void DefaultCustomColors ()
{
	// Find the color cvars by scanning the MapColors menu.
	for (int i = 0; i < MapColorsMenu.numitems; ++i)
	{
		if (MapColorsItems[i].type == colorpicker)
		{
			MapColorsItems[i].a.colorcvar->ResetToDefault ();
		}
	}
}

static bool ColorPickerDrawer ()
{
	DWORD newColor = MAKEARGB(255,
		int(ColorPickerItems[2].a.fval),
		int(ColorPickerItems[3].a.fval),
		int(ColorPickerItems[4].a.fval));
	DWORD oldColor = DWORD(*ColorPickerItems[0].a.colorcvar) | 0xFF000000;

	int x = screen->GetWidth()*2/3;
	int y = (15 + BigFont->GetHeight() + SmallFont->GetHeight()*5 - 10) * CleanYfac_1;

	screen->Clear (x, y, x + 48*CleanXfac_1, y + 48*CleanYfac_1, -1, oldColor);
	screen->Clear (x + 48*CleanXfac_1, y, x + 48*2*CleanXfac_1, y + 48*CleanYfac_1, -1, newColor);

	y += 49*CleanYfac_1;
	screen->DrawText (SmallFont, CR_GRAY, x+(24-SmallFont->StringWidth("Old")/2)*CleanXfac_1, y,
		"Old", DTA_CleanNoMove_1, true, TAG_DONE);
	screen->DrawText (SmallFont, CR_WHITE, x+(48+24-SmallFont->StringWidth("New")/2)*CleanXfac_1, y,
		"New", DTA_CleanNoMove_1, true, TAG_DONE);
	return false;
}

static void SetColorPickerSliders ()
{
	FColorCVar *cvar = ColorPickerItems[0].a.colorcvar;
	ColorPickerItems[2].a.fval = float(RPART(DWORD(*cvar)));
	ColorPickerItems[3].a.fval = float(GPART(DWORD(*cvar)));
	ColorPickerItems[4].a.fval = float(BPART(DWORD(*cvar)));
	CurrColorIndex = cvar->GetIndex();
}

static void UpdateSelColor (int index)
{
	ColorPickerItems[2].a.fval = GPalette.BaseColors[index].r;
	ColorPickerItems[3].a.fval = GPalette.BaseColors[index].g;
	ColorPickerItems[4].a.fval = GPalette.BaseColors[index].b;
}

static void ColorPickerReset ()
{
	SetColorPickerSliders ();
}

static void ActivateColorChoice ()
{
	UCVarValue val;
	val.Int = MAKERGB
		(int(ColorPickerItems[2].a.fval),
		 int(ColorPickerItems[3].a.fval),
		 int(ColorPickerItems[4].a.fval));
	ColorPickerItems[0].a.colorcvar->SetGenericRep (val, CVAR_Int);
}

static void StartColorPickerMenu (const char *colorname, FColorCVar *cvar)
{
	ColorPickerMenu.PreDraw = ColorPickerDrawer;
	ColorPickerMenu.EscapeHandler = ActivateColorChoice;
	ColorPickerItems[0].label = colorname;
	ColorPickerItems[0].a.colorcvar = cvar;
	SetColorPickerSliders ();
    M_SwitchMenu (&ColorPickerMenu);
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

static bool DrawJoystickConfigMenuHeader()
{
	FString joyname = SELECTED_JOYSTICK->GetName();
	screen->DrawText(BigFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
		(screen->GetWidth() - BigFont->StringWidth(CurrentMenu->texttitle) * CleanXfac_1) / 2,
		5 * CleanYfac_1,
		CurrentMenu->texttitle, DTA_CleanNoMove_1, true, TAG_DONE);
	screen->DrawText(SmallFont, gameinfo.gametype & GAME_DoomChex ? CR_RED : CR_UNTRANSLATED,
		(screen->GetWidth() - SmallFont->StringWidth(joyname) * CleanXfac_1) / 2, (8 + BigFont->GetHeight()) * CleanYfac_1,
		joyname, DTA_CleanNoMove_1, true, TAG_DONE);
	return false;
}

static void UpdateJoystickConfigMenu(IJoystickConfig *joy)
{
	int i;
	menuitem_t item = { whitetext };

	JoystickConfigItems.Clear();
	if (joy == NULL)
	{
		item.type = redtext;
		item.label = "Invalid controller specified for menu";
		JoystickConfigItems.Push(item);
	}
	else
	{
		SELECTED_JOYSTICK = joy;

		item.type = joy_sens;
		item.label = "Overall sensitivity";
		item.b.min = 0;
		item.c.max = 2;
		item.d.step = 0.1f;
		JoystickConfigItems.Push(item);

		item.type = redtext;
		item.label = " ";
		JoystickConfigItems.Push(item);

		item.type = whitetext;
		if (joy->GetNumAxes() > 0)
		{
			item.label = "Axis Configuration";
			JoystickConfigItems.Push(item);

			for (i = 0; i < joy->GetNumAxes(); ++i)
			{
				item.type = redtext;
				item.label = " ";
				JoystickConfigItems.Push(item);

				item.type = joy_map;
				item.label = joy->GetAxisName(i);
				item.a.joyselection = i;
				item.b.numvalues = countof(JoyAxisMapNames);
				item.e.values = JoyAxisMapNames;
				JoystickConfigItems.Push(item);

				item.type = joy_slider;
				item.label = "Sensitivity";
				item.b.min = 0;
				item.c.max = 4;
				item.d.step = 0.1f;
				item.e.joyslidernum = 0;
				JoystickConfigItems.Push(item);

				item.type = joy_inverter;
				item.label = "Invert";
				JoystickConfigItems.Push(item);

				item.type = joy_slider;
				item.label = "Dead Zone";
				item.b.position = 1;
				item.c.max = 0.9f;
				item.d.step = 0.05f;
				item.e.joyslidernum = 1;
				JoystickConfigItems.Push(item);
			}
		}
		else
		{
			item.label = "No configurable axes";
			JoystickConfigItems.Push(item);
		}
	}
	JoystickConfigMenu.items = &JoystickConfigItems[0];
	JoystickConfigMenu.numitems = JoystickConfigItems.Size();
	JoystickConfigMenu.lastOn = 0;
	JoystickConfigMenu.scrollpos = 0;
	JoystickConfigMenu.y = 25 + BigFont->GetHeight();
	JoystickConfigMenu.PreDraw = DrawJoystickConfigMenuHeader;
	if (screen != NULL)
	{
		CalcIndent(&JoystickConfigMenu);
	}
}

static void StartJoystickConfigMenu()
{
	UpdateJoystickConfigMenu(Joysticks[JoystickItems[JoystickMenu.lastOn].a.joyselection]);
	M_SwitchMenu(&JoystickConfigMenu);
}

void UpdateJoystickMenu(IJoystickConfig *selected)
{
	int i;
	menuitem_t item = { whitetext };
	int itemnum = -1;

	JoystickItems.Clear();
	I_GetJoysticks(Joysticks);
	if ((unsigned)itemnum >= Joysticks.Size())
	{
		itemnum = Joysticks.Size() - 1;
	}
	if (selected != NULL)
	{
		for (i = 0; (unsigned)i < Joysticks.Size(); ++i)
		{
			if (Joysticks[i] == selected)
			{
				itemnum = i;
				break;
			}
		}
	}
	item.type = discrete;
	item.label = "Enable controller support";
	item.a.cvar = &use_joystick;
	item.b.numvalues = 2;
	item.c.discretecenter = 2;
	item.e.values = YesNo;
	JoystickItems.Push(item);

#ifdef _WIN32
	item.label = "Enable DirectInput controllers";
	item.a.cvar = &joy_dinput;
	JoystickItems.Push(item);

	item.label = "Enable XInput controllers";
	item.a.cvar = &joy_xinput;
	JoystickItems.Push(item);

	item.label = "Enable raw PlayStation 2 adapters";
	item.a.cvar = &joy_ps2raw;
	JoystickItems.Push(item);
#endif

	item.type = redtext;
	item.label = " ";
	item.c.discretecenter = 0;
	JoystickItems.Push(item);

	if (Joysticks.Size() == 0)
	{
		item.type = redtext;
		item.label = "No controllers detected";
		JoystickItems.Push(item);
		if (!use_joystick)
		{
			item.type = whitetext;
			item.label = "Controller support must be";
			JoystickItems.Push(item);

			item.label = "enabled to detect any";
			JoystickItems.Push(item);
		}
	}
	else
	{
		item.label = "Configure controllers:";
		JoystickItems.Push(item);

		item.type = joymore;
		item.e.mfunc = StartJoystickConfigMenu;
		for (int i = 0; i < (int)Joysticks.Size(); ++i)
		{
			item.a.joyselection = i;
			if (i == itemnum)
			{
				JoystickMenu.lastOn = JoystickItems.Size(); 
			}
			JoystickItems.Push(item);
		}
	}
	JoystickMenu.items = &JoystickItems[0];
	JoystickMenu.numitems = JoystickItems.Size();
	if (JoystickMenu.lastOn >= JoystickMenu.numitems)
	{
		JoystickMenu.lastOn = JoystickMenu.numitems - 1;
	}
	if (CurrentMenu == &JoystickMenu && CurrentItem >= JoystickMenu.numitems)
	{
		CurrentItem = JoystickMenu.lastOn;
	}
	if (screen != NULL)
	{
		CalcIndent(&JoystickMenu);
	}

	// If the joystick config menu is open, close it if the device it's
	// open for is gone.
	for (i = 0; (unsigned)i < Joysticks.Size(); ++i)
	{
		if (Joysticks[i] == SELECTED_JOYSTICK)
		{
			break;
		}
	}
	if (i == (int)Joysticks.Size())
	{
		SELECTED_JOYSTICK = NULL;
		if (CurrentMenu == &JoystickConfigMenu)
		{
			M_PopMenuStack();
		}
	}
}

static void JoystickOptions ()
{
	UpdateJoystickMenu (NULL);
	M_SwitchMenu (&JoystickMenu);
}

CCMD (menu_joystick)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	JoystickOptions ();
}

static void FreeMIDIMenuList()
{
	if (SoundItems[MIDI_DEVICE_ITEM].e.values != NULL)
	{
		delete[] SoundItems[MIDI_DEVICE_ITEM].e.values;
	}
}

static void SoundOptions ()
{
	I_BuildMIDIMenuList(&SoundItems[MIDI_DEVICE_ITEM].e.values, &SoundItems[MIDI_DEVICE_ITEM].b.min);
	atterm(FreeMIDIMenuList);
	M_SwitchMenu(&SoundMenu);
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

static void MakeSoundChanges (void)
{
	static char snd_reset[] = "snd_reset";
	AddCommandString (snd_reset);
}

static void ModReplayerOptions()
{
	for (size_t i = 2; i < countof(ModReplayerItems); ++i)
	{
		if (ModReplayerItems[i].type == discrete)
		{
			ModReplayerItems[i].d.graycheck = &mod_dumb;
		}
	}
	M_SwitchMenu(&ModReplayerMenu);
}

CCMD (menu_modreplayer)
{
	M_StartControlPanel(true);
	OptionsActive = true;
	ModReplayerOptions();
}

static void VideoOptions (void)
{
	InitCrosshairsList();
	M_SwitchMenu (&VideoMenu);
}

CCMD (menu_display)
{
	M_StartControlPanel (true);
	OptionsActive = true;
	InitCrosshairsList();
	M_SwitchMenu (&VideoMenu);
}

static void BuildModesList (int hiwidth, int hiheight, int hi_bits)
{
	char strtemp[32], **str;
	int	 i, c;
	int	 width, height, showbits;
	bool letterbox=false;
	int  ratiomatch;

	if (menu_screenratios >= 0 && menu_screenratios <= 4 && menu_screenratios != 3)
	{
		ratiomatch = menu_screenratios;
	}
	else
	{
		ratiomatch = -1;
	}
	showbits = BitTranslate[DummyDepthCvar];

	if (Video != NULL)
	{
		Video->StartModeIterator (showbits, screen->IsFullscreen());
	}

	for (i = VM_RESSTART; ModesItems[i].type == screenres; i++)
	{
		ModesItems[i].e.highlight = -1;
		for (c = 0; c < 3; c++)
		{
			bool haveMode = false;

			switch (c)
			{
			default: str = &ModesItems[i].b.res1; break;
			case 1:  str = &ModesItems[i].c.res2; break;
			case 2:  str = &ModesItems[i].d.res3; break;
			}
			if (Video != NULL)
			{
				while ((haveMode = Video->NextMode (&width, &height, &letterbox)) &&
					(ratiomatch >= 0 && CheckRatio (width, height) != ratiomatch))
				{
				}
			}

			if (haveMode)
			{
				if (/* hi_bits == showbits && */ width == hiwidth && height == hiheight)
					ModesItems[i].e.highlight = ModesItems[i].a.selmode = c;
				
				mysnprintf (strtemp, countof(strtemp), "%dx%d%s", width, height, letterbox?TEXTCOLOR_BROWN" LB":"");
				ReplaceString (str, strtemp);
			}
			else
			{
				if (*str)
				{
					delete[] *str;
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

void M_FreeModesList ()
{
	for (int i = VM_RESSTART; ModesItems[i].type == screenres; ++i)
	{
		for (int c = 0; c < 3; ++c)
		{
			char **str;

			switch (c)
			{
			default: str = &ModesItems[i].b.res1; break;
			case 1:  str = &ModesItems[i].c.res2; break;
			case 2:  str = &ModesItems[i].d.res3; break;
			}
			if (str != NULL)
			{
				delete[] *str;
				*str = NULL;
			}
		}
	}
}

static bool GetSelectedSize (int line, int *width, int *height)
{
	if (ModesItems[line].type != screenres)
	{
		return false;
	}
	else
	{
		char *res, *breakpt;
		long x, y;

		switch (ModesItems[line].a.selmode)
		{
		default: res = ModesItems[line].b.res1; break;
		case 1:  res = ModesItems[line].c.res2; break;
		case 2:  res = ModesItems[line].d.res3; break;
		}
		x = strtol (res, &breakpt, 10);
		y = strtol (breakpt+1, NULL, 10);

		*width = x;
		*height = y;
		return true;
	}
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
			free (const_cast<char *>(ModesItems[VM_ENTERLINE].label));
		ModesItems[VM_ENTERLINE].label = VMEnterText;
		ModesItems[VM_TESTLINE].label = VMTestText;
	}
	else
	{
		char strtemp[64];

		mysnprintf (strtemp, countof(strtemp), "TESTING %dx%dx%d", w, h, bits);
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

	mysnprintf (section, countof(section), "%s.%s%sBindings", GameNames[gameinfo.gametype], modname,
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

void M_SaveCustomKeys (FConfigFile *config, char *section, char *subsection, size_t sublen)
{
	if (ControlsMenu.items == ControlsItems)
		return;

	// Start after the normal controls
	unsigned int i = countof(ControlsItems);
	unsigned int most = CustomControlsItems.Size();

	while (i < most)
	{
		menuitem_t *item = &CustomControlsItems[i];

		if (item->type == whitetext)
		{
			assert (item->e.command != NULL);
			mysnprintf (subsection, sublen, "%s.Bindings", item->e.command);
			M_DoSaveKeys (config, section, (int)i, false);
			mysnprintf (subsection, sublen, "%s.DoubleBindings", item->e.command);
			i = M_DoSaveKeys (config, section, (int)i, true);
		}
		else
		{
			i++;
		}
	}
}

static int AddKeySpot;

void FreeKeySections()
{
	const unsigned int numStdControls = countof(ControlsItems);
	unsigned int i;

	for (i = numStdControls; i < CustomControlsItems.Size(); ++i)
	{
		menuitem_t *item = &CustomControlsItems[i];
		if (item->type == whitetext || item->type == control)
		{
			if (item->label != NULL)
			{
				delete[] item->label;
				item->label = NULL;
			}
			if (item->e.command != NULL)
			{
				delete[] item->e.command;
				item->e.command = NULL;
			}
		}
	}
}

CCMD (addkeysection)
{
	if (argv.argc() != 3)
	{
		Printf ("Usage: addkeysection <menu section name> <ini name>\n");
		return;
	}

	const int numStdControls = countof(ControlsItems);
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
	ControlsMenu.items = &CustomControlsItems[0];
	ControlsMenu.numitems = (int)CustomControlsItems.Size();
}

void M_Deinit ()
{
	// Free bitdepth names for the modes menu.
	for (size_t i = 0; i < countof(Depths); ++i)
	{
		if (Depths[i].name != NULL)
		{
			delete[] Depths[i].name;
			Depths[i].name = NULL;
		}
	}

	// Free resolutions from the modes menu.
	M_FreeModesList();
}

// Reads any XHAIRS lumps for the names of crosshairs and
// adds them to the display options menu.
void InitCrosshairsList()
{
	int lastlump, lump;
	valuestring_t value;

	lastlump = 0;

	Crosshairs.Clear();
	value.value = 0;
	value.name = "None";
	Crosshairs.Push(value);

	while ((lump = Wads.FindLump("XHAIRS", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetNumber())
		{
			value.value = float(sc.Number);
			sc.MustGetString();
			value.name = sc.String;
			if (value.value != 0)
			{ // Check if it already exists. If not, add it.
				unsigned int i;

				for (i = 1; i < Crosshairs.Size(); ++i)
				{
					if (Crosshairs[i].value == value.value)
					{
						break;
					}
				}
				if (i < Crosshairs.Size())
				{
					Crosshairs[i].name = value.name;
				}
				else
				{
					Crosshairs.Push(value);
				}
			}
		}
	}
	VideoItems[CROSSHAIR_INDEX].b.numvalues = float(Crosshairs.Size());
	VideoItems[CROSSHAIR_INDEX].e.valuestrings = &Crosshairs[0];
}
