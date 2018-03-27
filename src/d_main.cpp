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
//
// DESCRIPTION:
//		DOOM main program (D_DoomMain) and game loop (D_DoomLoop),
//		plus functions to determine game mode (shareware, registered),
//		parse command line parameters, configure game parameters (turbo),
//		and call the startup functions.
//
//-----------------------------------------------------------------------------

// HEADER FILES ------------------------------------------------------------

#ifdef _WIN32
#include <direct.h>
#define mkdir(a,b) _mkdir (a)
#else
#include <sys/stat.h>
#endif

#ifdef HAVE_FPU_CONTROL
#include <fpu_control.h>
#endif
#include <float.h>

#if defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#endif

#include <time.h>
#include <math.h>
#include <assert.h>
#include <sys/stat.h>

#include "doomerrors.h"

#include "i_time.h"
#include "d_gui.h"
#include "m_random.h"
#include "doomdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "w_wad.h"
#include "s_sound.h"
#include "v_video.h"
#include "intermission/intermission.h"
#include "f_wipe.h"
#include "m_argv.h"
#include "m_misc.h"
#include "menu/menu.h"
#include "c_console.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "wi_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_setup.h"
#include "r_utility.h"
#include "r_sky.h"
#include "d_main.h"
#include "d_dehacked.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_swap.h"
#include "v_text.h"
#include "gi.h"
#include "b_bot.h"		//Added by MC:
#include "stats.h"
#include "gameconfigfile.h"
#include "sbar.h"
#include "decallib.h"
#include "version.h"
#include "v_text.h"
#include "st_start.h"
#include "templates.h"
#include "teaminfo.h"
#include "hardware.h"
#include "sbarinfo.h"
#include "d_net.h"
#include "g_level.h"
#include "d_event.h"
#include "d_netinf.h"
#include "v_palette.h"
#include "m_cheat.h"
#include "compatibility.h"
#include "m_joy.h"
#include "sc_man.h"
#include "po_man.h"
#include "resourcefiles/resourcefile.h"
#include "r_renderer.h"
#include "p_local.h"
#include "autosegs.h"
#include "fragglescript/t_fs.h"
#include "g_levellocals.h"
#include "events.h"
#include "r_utility.h"
#include "vm.h"
#include "types.h"
#include "r_data/r_vanillatrans.h"

EXTERN_CVAR(Bool, hud_althud)
void DrawHUD();
void D_DoAnonStats();


// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void I_SetWindowTitle(const char* caption);
extern void ReadStatistics();
extern void M_RestoreMode ();
extern void M_SetDefaultMode ();
extern void G_NewInit ();
extern void SetupPlayerClasses ();
extern void HUD_InitHud();
void gl_PatchMenu();	// remove modern OpenGL options on old hardware.
void DeinitMenus();
const FIWADInfo *D_FindIWAD(TArray<FString> &wadfiles, const char *iwad, const char *basewad);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void D_CheckNetGame ();
void D_ProcessEvents ();
void G_BuildTiccmd (ticcmd_t* cmd);
void D_DoAdvanceDemo ();
void D_AddWildFile (TArray<FString> &wadfiles, const char *pattern);
void D_LoadWadSettings ();
void ParseGLDefs();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void D_DoomLoop ();
const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir=false);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

EXTERN_CVAR (Float, turbo)
EXTERN_CVAR (Bool, freelook)
EXTERN_CVAR (Float, m_pitch)
EXTERN_CVAR (Float, m_yaw)
EXTERN_CVAR (Bool, invertmouse)
EXTERN_CVAR (Bool, lookstrafe)
EXTERN_CVAR (Int, screenblocks)
EXTERN_CVAR (Bool, sv_cheats)
EXTERN_CVAR (Bool, sv_unlimited_pickup)
EXTERN_CVAR (Bool, I_FriendlyWindowTitle)

extern int testingmode;
extern bool setmodeneeded;
extern int NewWidth, NewHeight, NewBits, DisplayBits;
extern bool gameisdead;
extern bool demorecording;
extern bool M_DemoNoPlay;	// [RH] if true, then skip any demos in the loop
extern bool insave;


// PUBLIC DATA DEFINITIONS -------------------------------------------------

CUSTOM_CVAR (Int, fraglimit, 0, CVAR_SERVERINFO)
{
	// Check for the fraglimit being hit because the fraglimit is being
	// lowered below somebody's current frag count.
	if (deathmatch && self > 0)
	{
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && self <= D_GetFragCount(&players[i]))
			{
				Printf ("%s\n", GStrings("TXT_FRAGLIMIT"));
				G_ExitLevel (0, false);
				break;
			}
		}
	}
}

CVAR (Float, timelimit, 0.f, CVAR_SERVERINFO);
CVAR (Int, wipetype, 1, CVAR_ARCHIVE);
CVAR (Int, snd_drawoutput, 0, 0);
CUSTOM_CVAR (String, vid_cursor, "None", CVAR_ARCHIVE | CVAR_NOINITCALL)
{
	bool res = false;

	if (!stricmp(self, "None" ) && gameinfo.CursorPic.IsNotEmpty())
	{
		res = I_SetCursor(TexMan[gameinfo.CursorPic]);
	}
	else
	{
		res = I_SetCursor(TexMan[self]);
	}
	if (!res)
	{
		I_SetCursor(TexMan["cursor"]);
	}
}

// Controlled by startup dialog
CVAR (Bool, disableautoload, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, autoloadbrightmaps, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, autoloadlights, false, CVAR_ARCHIVE | CVAR_NOINITCALL | CVAR_GLOBALCONFIG)
CVAR (Bool, r_debug_disable_vis_filter, false, 0)

bool wantToRestart;
bool DrawFSHUD;				// [RH] Draw fullscreen HUD?
TArray<FString> allwads;
bool devparm;				// started game with -devparm
const char *D_DrawIcon;	// [RH] Patch name of icon to draw on next refresh
int NoWipe;				// [RH] Allow wipe? (Needs to be set each time)
bool singletics = false;	// debug flag to cancel adaptiveness
FString startmap;
bool autostart;
FString StoredWarp;
bool advancedemo;
FILE *debugfile;
FILE *hashfile;
event_t events[MAXEVENTS];
int eventhead;
int eventtail;
gamestate_t wipegamestate = GS_DEMOSCREEN;	// can be -1 to force a wipe
bool PageBlank;
FTexture *Page;
FTexture *Advisory;
bool nospriterename;
FStartupInfo DoomStartupInfo;
FString lastIWAD;
int restart = 0;
bool batchrun;	// just run the startup and collect all error messages in a logfile, then quit without any interaction

cycle_t FrameCycles;

// [SP] Store the capabilities of the renderer in a global variable, to prevent excessive per-frame processing
uint32_t r_renderercaps = 0;


// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int demosequence;
static int pagetic;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// D_ProcessEvents
//
// Send all the events of the given timestamp down the responder chain.
// Events are asynchronous inputs generally generated by the game user.
// Events can be discarded if no responder claims them
//
//==========================================================================

void D_ProcessEvents (void)
{
	event_t *ev;
		
	// [RH] If testing mode, do not accept input until test is over
	if (testingmode)
	{
		if (testingmode == 1)
		{
			M_SetDefaultMode ();
		}
		else if (testingmode <= I_GetTime())
		{
			M_RestoreMode ();
		}
		return;
	}

	for (; eventtail != eventhead ; eventtail = (eventtail+1)&(MAXEVENTS-1))
	{
		ev = &events[eventtail];
		if (ev->type == EV_None)
			continue;
		if (ev->type == EV_DeviceChange)
			UpdateJoystickMenu(I_UpdateDeviceList());
		if (C_Responder (ev))
			continue;				// console ate the event
		if (M_Responder (ev))
			continue;				// menu ate the event
		// check events
		if (ev->type != EV_Mouse && E_Responder(ev)) // [ZZ] ZScript ate the event // update 07.03.17: mouse events are handled directly
			continue;
		G_Responder (ev);
	}
}

//==========================================================================
//
// D_PostEvent
//
// Called by the I/O functions when input is detected.
//
//==========================================================================

void D_PostEvent (const event_t *ev)
{
	// Do not post duplicate consecutive EV_DeviceChange events.
	if (ev->type == EV_DeviceChange && events[eventhead].type == EV_DeviceChange)
	{
		return;
	}
	events[eventhead] = *ev;
	if (ev->type == EV_Mouse && menuactive == MENU_Off && ConsoleState != c_down && ConsoleState != c_falling && !E_Responder(ev) && !paused)
	{
		if (Button_Mlook.bDown || freelook)
		{
			int look = int(ev->y * m_pitch * mouse_sensitivity * 16.0);
			if (invertmouse)
				look = -look;
			G_AddViewPitch (look, true);
			events[eventhead].y = 0;
		}
		if (!Button_Strafe.bDown && !lookstrafe)
		{
			G_AddViewAngle (int(ev->x * m_yaw * mouse_sensitivity * 8.0), true);
			events[eventhead].x = 0;
		}
		if ((events[eventhead].x | events[eventhead].y) == 0)
		{
			return;
		}
	}
	eventhead = (eventhead+1)&(MAXEVENTS-1);
}

//==========================================================================
//
// D_RemoveNextCharEvent
//
// Removes the next EV_GUI_Char event in the input queue. Used by the menu,
// since it (generally) consumes EV_GUI_KeyDown events and not EV_GUI_Char
// events, and it needs to ensure that there is no left over input when it's
// done. If there are multiple EV_GUI_KeyDowns before the EV_GUI_Char, then
// there are dead chars involved, so those should be removed, too. We do
// this by changing the message type to EV_None rather than by actually
// removing the event from the queue.
// 
//==========================================================================

void D_RemoveNextCharEvent()
{
	assert(events[eventtail].type == EV_GUI_Event && events[eventtail].subtype == EV_GUI_KeyDown);
	for (int evnum = eventtail; evnum != eventhead; evnum = (evnum+1) & (MAXEVENTS-1))
	{
		event_t *ev = &events[evnum];
		if (ev->type != EV_GUI_Event)
			break;
		if (ev->subtype == EV_GUI_KeyDown || ev->subtype == EV_GUI_Char)
		{
			ev->type = EV_None;
			if (ev->subtype == EV_GUI_Char)
				break;
		}
		else
		{
			break;
		}
	}
}

//==========================================================================
//
// CVAR dmflags
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags, 0, CVAR_SERVERINFO)
{
	// In case DF_NO_FREELOOK was changed, reinitialize the sky
	// map. (If no freelook, then no need to stretch the sky.)
	if (sky1texture.isValid())
		R_InitSkyMap ();

	if (self & DF_NO_FREELOOK)
	{
		Net_WriteByte (DEM_CENTERVIEW);
	}
	// If nofov is set, force everybody to the arbitrator's FOV.
	if ((self & DF_NO_FOV) && consoleplayer == Net_Arbitrator)
	{
		float fov;

		Net_WriteByte (DEM_FOV);

		// If the game is started with DF_NO_FOV set, the arbitrator's
		// DesiredFOV will not be set when this callback is run, so
		// be sure not to transmit a 0 FOV.
		fov = players[consoleplayer].DesiredFOV;
		if (fov == 0)
		{
			fov = 90;
		}
		Net_WriteFloat (fov);
	}
}

CVAR (Flag, sv_nohealth,		dmflags, DF_NO_HEALTH);
CVAR (Flag, sv_noitems,			dmflags, DF_NO_ITEMS);
CVAR (Flag, sv_weaponstay,		dmflags, DF_WEAPONS_STAY);
CVAR (Flag, sv_falldamage,		dmflags, DF_FORCE_FALLINGHX);
CVAR (Flag, sv_oldfalldamage,	dmflags, DF_FORCE_FALLINGZD);
CVAR (Flag, sv_samelevel,		dmflags, DF_SAME_LEVEL);
CVAR (Flag, sv_spawnfarthest,	dmflags, DF_SPAWN_FARTHEST);
CVAR (Flag, sv_forcerespawn,	dmflags, DF_FORCE_RESPAWN);
CVAR (Flag, sv_noarmor,			dmflags, DF_NO_ARMOR);
CVAR (Flag, sv_noexit,			dmflags, DF_NO_EXIT);
CVAR (Flag, sv_infiniteammo,	dmflags, DF_INFINITE_AMMO);
CVAR (Flag, sv_nomonsters,		dmflags, DF_NO_MONSTERS);
CVAR (Flag, sv_monsterrespawn,	dmflags, DF_MONSTERS_RESPAWN);
CVAR (Flag, sv_itemrespawn,		dmflags, DF_ITEMS_RESPAWN);
CVAR (Flag, sv_fastmonsters,	dmflags, DF_FAST_MONSTERS);
CVAR (Flag, sv_nojump,			dmflags, DF_NO_JUMP);
CVAR (Flag, sv_allowjump,		dmflags, DF_YES_JUMP);
CVAR (Flag, sv_nofreelook,		dmflags, DF_NO_FREELOOK);
CVAR (Flag, sv_allowfreelook,	dmflags, DF_YES_FREELOOK);
CVAR (Flag, sv_nofov,			dmflags, DF_NO_FOV);
CVAR (Flag, sv_noweaponspawn,	dmflags, DF_NO_COOP_WEAPON_SPAWN);
CVAR (Flag, sv_nocrouch,		dmflags, DF_NO_CROUCH);
CVAR (Flag, sv_allowcrouch,		dmflags, DF_YES_CROUCH);
CVAR (Flag, sv_cooploseinventory,	dmflags, DF_COOP_LOSE_INVENTORY);
CVAR (Flag, sv_cooplosekeys,	dmflags, DF_COOP_LOSE_KEYS);
CVAR (Flag, sv_cooploseweapons,	dmflags, DF_COOP_LOSE_WEAPONS);
CVAR (Flag, sv_cooplosearmor,	dmflags, DF_COOP_LOSE_ARMOR);
CVAR (Flag, sv_cooplosepowerups,	dmflags, DF_COOP_LOSE_POWERUPS);
CVAR (Flag, sv_cooploseammo,	dmflags, DF_COOP_LOSE_AMMO);
CVAR (Flag, sv_coophalveammo,	dmflags, DF_COOP_HALVE_AMMO);

// Some (hopefully cleaner) interface to these settings.
CVAR (Mask, sv_crouch,			dmflags, DF_NO_CROUCH|DF_YES_CROUCH);
CVAR (Mask, sv_jump,			dmflags, DF_NO_JUMP|DF_YES_JUMP);
CVAR (Mask, sv_fallingdamage,	dmflags, DF_FORCE_FALLINGHX|DF_FORCE_FALLINGZD);
CVAR (Mask, sv_freelook,		dmflags, DF_NO_FREELOOK|DF_YES_FREELOOK);

//==========================================================================
//
// CVAR dmflags2
//
// [RH] From Skull Tag. Some of these were already done as separate cvars
// (such as bfgaiming), but I collected them here like Skull Tag does.
//
//==========================================================================

CUSTOM_CVAR (Int, dmflags2, 0, CVAR_SERVERINFO)
{
	// Stop the automap if we aren't allowed to use it.
	if ((self & DF2_NO_AUTOMAP) && automapactive)
		AM_Stop ();

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		player_t *p = &players[i];

		if (!playeringame[i])
			continue;

		// Revert our view to our own eyes if spying someone else.
		if (self & DF2_DISALLOW_SPYING)
		{
			// The player isn't looking through its own eyes, so make it.
			if (p->camera != p->mo)
			{
				p->camera = p->mo;

				S_UpdateSounds (p->camera);
				StatusBar->AttachToPlayer (p);

				if (demoplayback || multiplayer)
					StatusBar->ShowPlayerName ();
			}
		}

		// Come out of chasecam mode if we're not allowed to use chasecam.
		if (!(dmflags2 & DF2_CHASECAM) && CheckCheatmode(false))
		{
			// Take us out of chasecam mode only.
			if (p->cheats & CF_CHASECAM)
				cht_DoCheat (p, CHT_CHASECAM);
		}
	}
}

CVAR (Flag, sv_weapondrop,			dmflags2, DF2_YES_WEAPONDROP);
CVAR (Flag, sv_noteamswitch,		dmflags2, DF2_NO_TEAM_SWITCH);
CVAR (Flag, sv_doubleammo,			dmflags2, DF2_YES_DOUBLEAMMO);
CVAR (Flag, sv_degeneration,		dmflags2, DF2_YES_DEGENERATION);
CVAR (Flag, sv_nobfgaim,			dmflags2, DF2_NO_FREEAIMBFG);
CVAR (Flag, sv_barrelrespawn,		dmflags2, DF2_BARRELS_RESPAWN);
CVAR (Flag, sv_keepfrags,			dmflags2, DF2_YES_KEEPFRAGS);
CVAR (Flag, sv_norespawn,			dmflags2, DF2_NO_RESPAWN);
CVAR (Flag, sv_losefrag,			dmflags2, DF2_YES_LOSEFRAG);
CVAR (Flag, sv_respawnprotect,		dmflags2, DF2_YES_RESPAWN_INVUL);
CVAR (Flag, sv_samespawnspot,		dmflags2, DF2_SAME_SPAWN_SPOT);
CVAR (Flag, sv_infiniteinventory,	dmflags2, DF2_INFINITE_INVENTORY);
CVAR (Flag, sv_killallmonsters,		dmflags2, DF2_KILL_MONSTERS);
CVAR (Flag, sv_noautomap,			dmflags2, DF2_NO_AUTOMAP);
CVAR (Flag, sv_noautomapallies,		dmflags2, DF2_NO_AUTOMAP_ALLIES);
CVAR (Flag, sv_disallowspying,		dmflags2, DF2_DISALLOW_SPYING);
CVAR (Flag, sv_chasecam,			dmflags2, DF2_CHASECAM);
CVAR (Flag, sv_disallowsuicide,		dmflags2, DF2_NOSUICIDE);
CVAR (Flag, sv_noautoaim,			dmflags2, DF2_NOAUTOAIM);
CVAR (Flag, sv_dontcheckammo,		dmflags2, DF2_DONTCHECKAMMO);
CVAR (Flag, sv_killbossmonst,		dmflags2, DF2_KILLBOSSMONST);
CVAR (Flag, sv_nocountendmonst,		dmflags2, DF2_NOCOUNTENDMONST);
CVAR (Flag, sv_respawnsuper,		dmflags2, DF2_RESPAWN_SUPER);

//==========================================================================
//
// CVAR compatflags
//
//==========================================================================

int i_compatflags, i_compatflags2;	// internal compatflags composed from the compatflags CVAR and MAPINFO settings
int ii_compatflags, ii_compatflags2, ib_compatflags;

EXTERN_CVAR(Int, compatmode)

static int GetCompatibility(int mask)
{
	if (level.info == NULL) return mask;
	else return (mask & ~level.info->compatmask) | (level.info->compatflags & level.info->compatmask);
}

static int GetCompatibility2(int mask)
{
	return (level.info == NULL) ? mask
		: (mask & ~level.info->compatmask2) | (level.info->compatflags2 & level.info->compatmask2);
}

CUSTOM_CVAR (Int, compatflags, 0, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	int old = i_compatflags;
	i_compatflags = GetCompatibility(self) | ii_compatflags;
	if ((old ^ i_compatflags) & COMPATF_POLYOBJ)
	{
		FPolyObj::ClearAllSubsectorLinks();
	}
}

CUSTOM_CVAR (Int, compatflags2, 0, CVAR_ARCHIVE|CVAR_SERVERINFO)
{
	i_compatflags2 = GetCompatibility2(self) | ii_compatflags2;
}

CUSTOM_CVAR(Int, compatmode, 0, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	int v, w = 0;

	switch (self)
	{
	default:
	case 0:
		v = 0;
		break;

	case 1:	// Doom2.exe compatible with a few relaxed settings
		v = COMPATF_SHORTTEX|COMPATF_STAIRINDEX|COMPATF_USEBLOCKING|COMPATF_NODOORLIGHT|COMPATF_SPRITESORT|
			COMPATF_TRACE|COMPATF_MISSILECLIP|COMPATF_SOUNDTARGET|COMPATF_DEHHEALTH|COMPATF_CROSSDROPOFF|
			COMPATF_LIGHT|COMPATF_MASKEDMIDTEX;
		w= COMPATF2_FLOORMOVE;
		break;

	case 2:	// same as 1 but stricter (NO_PASSMOBJ and INVISIBILITY are also set)
		v = COMPATF_SHORTTEX|COMPATF_STAIRINDEX|COMPATF_USEBLOCKING|COMPATF_NODOORLIGHT|COMPATF_SPRITESORT|
			COMPATF_TRACE|COMPATF_MISSILECLIP|COMPATF_SOUNDTARGET|COMPATF_NO_PASSMOBJ|COMPATF_LIMITPAIN|
			COMPATF_DEHHEALTH|COMPATF_INVISIBILITY|COMPATF_CROSSDROPOFF|COMPATF_CORPSEGIBS|COMPATF_HITSCAN|
			COMPATF_WALLRUN|COMPATF_NOTOSSDROPS|COMPATF_LIGHT|COMPATF_MASKEDMIDTEX;
		w = COMPATF2_BADANGLES|COMPATF2_FLOORMOVE|COMPATF2_POINTONLINE;
		break;

	case 3: // Boom compat mode
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_MASKEDMIDTEX;
		break;

	case 4: // Old ZDoom compat mode
		v = COMPATF_SOUNDTARGET | COMPATF_LIGHT;
		w = COMPATF2_MULTIEXIT | COMPATF2_TELEPORT | COMPATF2_PUSHWINDOW;
		break;

	case 5: // MBF compat mode
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_MUSHROOM|
			COMPATF_MBFMONSTERMOVE|COMPATF_NOBLOCKFRIENDS|COMPATF_MASKEDMIDTEX;
		break;

	case 6:	// Boom with some added settings to reenable some 'broken' behavior
		v = COMPATF_TRACE|COMPATF_SOUNDTARGET|COMPATF_BOOMSCROLL|COMPATF_MISSILECLIP|COMPATF_NO_PASSMOBJ|
			COMPATF_INVISIBILITY|COMPATF_CORPSEGIBS|COMPATF_HITSCAN|COMPATF_WALLRUN|COMPATF_NOTOSSDROPS|COMPATF_MASKEDMIDTEX;
		w = COMPATF2_POINTONLINE;
		break;

	}
	compatflags = v;
	compatflags2 = w;
}

CVAR (Flag, compat_shortTex,			compatflags,  COMPATF_SHORTTEX);
CVAR (Flag, compat_stairs,				compatflags,  COMPATF_STAIRINDEX);
CVAR (Flag, compat_limitpain,			compatflags,  COMPATF_LIMITPAIN);
CVAR (Flag, compat_silentpickup,		compatflags,  COMPATF_SILENTPICKUP);
CVAR (Flag, compat_nopassover,			compatflags,  COMPATF_NO_PASSMOBJ);
CVAR (Flag, compat_soundslots,			compatflags,  COMPATF_MAGICSILENCE);
CVAR (Flag, compat_wallrun,				compatflags,  COMPATF_WALLRUN);
CVAR (Flag, compat_notossdrops,			compatflags,  COMPATF_NOTOSSDROPS);
CVAR (Flag, compat_useblocking,			compatflags,  COMPATF_USEBLOCKING);
CVAR (Flag, compat_nodoorlight,			compatflags,  COMPATF_NODOORLIGHT);
CVAR (Flag, compat_ravenscroll,			compatflags,  COMPATF_RAVENSCROLL);
CVAR (Flag, compat_soundtarget,			compatflags,  COMPATF_SOUNDTARGET);
CVAR (Flag, compat_dehhealth,			compatflags,  COMPATF_DEHHEALTH);
CVAR (Flag, compat_trace,				compatflags,  COMPATF_TRACE);
CVAR (Flag, compat_dropoff,				compatflags,  COMPATF_DROPOFF);
CVAR (Flag, compat_boomscroll,			compatflags,  COMPATF_BOOMSCROLL);
CVAR (Flag, compat_invisibility,		compatflags,  COMPATF_INVISIBILITY);
CVAR (Flag, compat_silentinstantfloors,	compatflags,  COMPATF_SILENT_INSTANT_FLOORS);
CVAR (Flag, compat_sectorsounds,		compatflags,  COMPATF_SECTORSOUNDS);
CVAR (Flag, compat_missileclip,			compatflags,  COMPATF_MISSILECLIP);
CVAR (Flag, compat_crossdropoff,		compatflags,  COMPATF_CROSSDROPOFF);
CVAR (Flag, compat_anybossdeath,		compatflags,  COMPATF_ANYBOSSDEATH);
CVAR (Flag, compat_minotaur,			compatflags,  COMPATF_MINOTAUR);
CVAR (Flag, compat_mushroom,			compatflags,  COMPATF_MUSHROOM);
CVAR (Flag, compat_mbfmonstermove,		compatflags,  COMPATF_MBFMONSTERMOVE);
CVAR (Flag, compat_corpsegibs,			compatflags,  COMPATF_CORPSEGIBS);
CVAR (Flag, compat_noblockfriends,		compatflags,  COMPATF_NOBLOCKFRIENDS);
CVAR (Flag, compat_spritesort,			compatflags,  COMPATF_SPRITESORT);
CVAR (Flag, compat_hitscan,				compatflags,  COMPATF_HITSCAN);
CVAR (Flag, compat_light,				compatflags,  COMPATF_LIGHT);
CVAR (Flag, compat_polyobj,				compatflags,  COMPATF_POLYOBJ);
CVAR (Flag, compat_maskedmidtex,		compatflags,  COMPATF_MASKEDMIDTEX);
CVAR (Flag, compat_badangles,			compatflags2, COMPATF2_BADANGLES);
CVAR (Flag, compat_floormove,			compatflags2, COMPATF2_FLOORMOVE);
CVAR (Flag, compat_soundcutoff,			compatflags2, COMPATF2_SOUNDCUTOFF);
CVAR (Flag, compat_pointonline,			compatflags2, COMPATF2_POINTONLINE);
CVAR (Flag, compat_multiexit,			compatflags2, COMPATF2_MULTIEXIT);
CVAR (Flag, compat_teleport,			compatflags2, COMPATF2_TELEPORT);
CVAR (Flag, compat_pushwindow,			compatflags2, COMPATF2_PUSHWINDOW);

//==========================================================================
//
// D_Display
//
// Draw current display, possibly wiping it from the previous
//
//==========================================================================

void D_Display ()
{
	bool wipe;
	bool hw2d;

	if (nodrawers || screen == NULL)
		return; 				// for comparative timing / profiling
	
	cycle_t cycles;
	
	cycles.Reset();
	cycles.Clock();

	r_UseVanillaTransparency = UseVanillaTransparency(); // [SP] Cache UseVanillaTransparency() call
	r_renderercaps = Renderer->GetCaps(); // [SP] Get the current capabilities of the renderer

	if (players[consoleplayer].camera == NULL)
	{
		players[consoleplayer].camera = players[consoleplayer].mo;
	}

	if (viewactive)
	{
		DAngle fov = 90.f;
		AActor *cam = players[consoleplayer].camera;
		if (cam)
		{
			if (cam->player)
				fov = cam->player->FOV;
			else fov = cam->CameraFOV;
		}
		R_SetFOV(r_viewpoint, fov);
	}

	// [RH] change the screen mode if needed
	if (setmodeneeded)
	{
		int oldrenderer;
		extern int currentrenderer;
		EXTERN_CVAR(Int, vid_renderer)
		oldrenderer = vid_renderer; // [SP] Save pending vid_renderer setting (hack)
		if (currentrenderer != vid_renderer)
			vid_renderer = currentrenderer;

		// Change screen mode.
		if (Video->SetResolution (NewWidth, NewHeight, NewBits))
		{
			// Recalculate various view parameters.
			setsizeneeded = true;
			// Let the status bar know the screen size changed
			if (StatusBar != NULL)
			{
				StatusBar->CallScreenSizeChanged ();
			}
			// Refresh the console.
			C_NewModeAdjust ();
			// Reload crosshair if transitioned to a different size
			ST_LoadCrosshair (true);
			AM_NewResolution ();
			// Reset the mouse cursor in case the bit depth changed
			vid_cursor.Callback();
		}
		vid_renderer = oldrenderer; // [SP] Restore pending vid_renderer setting
	}

	// change the view size if needed
	if (setsizeneeded && StatusBar != NULL)
	{
		R_ExecuteSetViewSize (r_viewpoint, r_viewwindow);
	}
	setmodeneeded = false;

	if (screen->Lock (false))
	{
		V_SetBorderNeedRefresh();
	}

	// [RH] Allow temporarily disabling wipes
	if (NoWipe)
	{
		V_SetBorderNeedRefresh();
		NoWipe--;
		wipe = false;
		wipegamestate = gamestate;
	}
	else if (gamestate != wipegamestate && gamestate != GS_FULLCONSOLE && gamestate != GS_TITLELEVEL)
	{ // save the current screen if about to wipe
		V_SetBorderNeedRefresh();
		switch (wipegamestate)
		{
		default:
			wipe = screen->WipeStartScreen (wipetype);
			break;

		case GS_FORCEWIPEFADE:
			wipe = screen->WipeStartScreen (wipe_Fade);
			break;

		case GS_FORCEWIPEBURN:
			wipe = screen->WipeStartScreen (wipe_Burn);
			break;

		case GS_FORCEWIPEMELT:
			wipe = screen->WipeStartScreen (wipe_Melt);
			break;
		}
		wipegamestate = gamestate;
	}
	else
	{
		wipe = false;
	}

	hw2d = false;


	{
		screen->FrameTime = I_msTimeFS();
		TexMan.UpdateAnimations(screen->FrameTime);
		R_UpdateSky(screen->FrameTime);
		switch (gamestate)
		{
		case GS_FULLCONSOLE:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			C_DrawConsole (false);
			M_Drawer ();
			screen->Update ();
			return;

		case GS_LEVEL:
		case GS_TITLELEVEL:
			if (!gametic)
			{
				if (!screen->HasBegun2D())
				{
					screen->Begin2D(false);
				}
				break;
			}

			if (StatusBar != NULL)
			{
				float blend[4] = { 0, 0, 0, 0 };
				StatusBar->BlendView (blend);
			}
			screen->SetBlendingRect(viewwindowx, viewwindowy,
				viewwindowx + viewwidth, viewwindowy + viewheight);

			// [ZZ] execute event hook that we just started the frame
			//E_RenderFrame();
			//
			Renderer->RenderView(&players[consoleplayer]);

			if ((hw2d = screen->Begin2D(viewactive)))
			{
				// Redraw everything every frame when using 2D accel
				V_SetBorderNeedRefresh();
			}
			Renderer->DrawRemainingPlayerSprites();
			screen->DrawBlendingRect();
			if (automapactive)
			{
				AM_Drawer (hud_althud? viewheight : StatusBar->GetTopOfStatusbar());
			}
			if (!automapactive || viewactive)
			{
				V_RefreshViewBorder ();
			}

			// for timing the statusbar code.
			//cycle_t stb;
			//stb.Reset();
			//stb.Clock();
			if (hud_althud && viewheight == SCREENHEIGHT && screenblocks > 10)
			{
				StatusBar->DrawBottomStuff (HUD_AltHud);
				if (DrawFSHUD || automapactive) DrawHUD();
				if (players[consoleplayer].camera && players[consoleplayer].camera->player && !automapactive)
				{
					StatusBar->DrawCrosshair();
				}
				StatusBar->CallDraw (HUD_AltHud);
				StatusBar->DrawTopStuff (HUD_AltHud);
			}
			else 
			if (viewheight == SCREENHEIGHT && viewactive && screenblocks > 10)
			{
				EHudState state = DrawFSHUD ? HUD_Fullscreen : HUD_None;
				StatusBar->DrawBottomStuff (state);
				StatusBar->CallDraw (state);
				StatusBar->DrawTopStuff (state);
			}
			else
			{
				StatusBar->DrawBottomStuff (HUD_StatusBar);
				StatusBar->CallDraw (HUD_StatusBar);
				StatusBar->DrawTopStuff (HUD_StatusBar);
			}
			//stb.Unclock();
			//Printf("Stbar = %f\n", stb.TimeMS());
			CT_Drawer ();
			break;

		case GS_INTERMISSION:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			WI_Drawer ();
			CT_Drawer ();
			break;

		case GS_FINALE:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			F_Drawer ();
			CT_Drawer ();
			break;

		case GS_DEMOSCREEN:
			screen->SetBlendingRect(0,0,0,0);
			hw2d = screen->Begin2D(false);
			D_PageDrawer ();
			CT_Drawer ();
			break;

		default:
			break;
		}
	}
	// draw pause pic
	if ((paused || pauseext) && menuactive == MENU_Off)
	{
		FTexture *tex;
		int x;
		FString pstring = "By ";

		tex = TexMan(gameinfo.PauseSign);
		x = (SCREENWIDTH - tex->GetScaledWidth() * CleanXfac)/2 +
			tex->GetScaledLeftOffset() * CleanXfac;
		screen->DrawTexture (tex, x, 4, DTA_CleanNoMove, true, TAG_DONE);
		if (paused && multiplayer)
		{
			pstring += players[paused - 1].userinfo.GetName();
			screen->DrawText(SmallFont, CR_RED,
				(screen->GetWidth() - SmallFont->StringWidth(pstring)*CleanXfac) / 2,
				(tex->GetScaledHeight() * CleanYfac) + 4, pstring, DTA_CleanNoMove, true, TAG_DONE);
		}
	}

	// [RH] Draw icon, if any
	if (D_DrawIcon)
	{
		FTextureID picnum = TexMan.CheckForTexture (D_DrawIcon, ETextureType::MiscPatch);

		D_DrawIcon = NULL;
		if (picnum.isValid())
		{
			FTexture *tex = TexMan[picnum];
			screen->DrawTexture (tex, 160 - tex->GetScaledWidth()/2, 100 - tex->GetScaledHeight()/2,
				DTA_320x200, true, TAG_DONE);
		}
		NoWipe = 10;
	}

	if (snd_drawoutput)
	{
		GSnd->DrawWaveDebug(snd_drawoutput);
	}

	if (!wipe || NoWipe < 0)
	{
		NetUpdate ();			// send out any new accumulation
		// normal update
		// draw ZScript UI stuff
		C_DrawConsole (hw2d);	// draw console
		M_Drawer ();			// menu is drawn even on top of everything
		FStat::PrintStat ();
		screen->Update ();		// page flip or blit buffer
	}
	else
	{
		// wipe update
		uint64_t wipestart, nowtime, diff;
		bool done;

		GSnd->SetSfxPaused(true, 1);
		I_FreezeTime(true);
		screen->WipeEndScreen ();

		wipestart = I_msTime();
		NetUpdate();		// send out any new accumulation

		do
		{
			do
			{
				I_WaitVBL(2);
				nowtime = I_msTime();
				diff = (nowtime - wipestart) * 40 / 1000;	// Using 35 here feels too slow.
			} while (diff < 1);
			wipestart = nowtime;
			done = screen->WipeDo (1);
			C_DrawConsole (hw2d);	// console and
			M_Drawer ();			// menu are drawn even on top of wipes
			screen->Update ();		// page flip or blit buffer
			NetUpdate ();			// [RH] not sure this is needed anymore
		} while (!done);
		screen->WipeCleanup();
		I_FreezeTime(false);
		GSnd->SetSfxPaused(false, 1);
	}
	screen->End2D();
	cycles.Unclock();
	FrameCycles = cycles;
}

//==========================================================================
//
// D_ErrorCleanup ()
//
// Cleanup after a recoverable error or a restart
//==========================================================================

void D_ErrorCleanup ()
{
	savegamerestore = false;
	screen->Unlock ();
	bglobal.RemoveAllBots (true);
	D_QuitNetGame ();
	if (demorecording || demoplayback)
		G_CheckDemoStatus ();
	Net_ClearBuffers ();
	G_NewInit ();
	M_ClearMenus ();
	singletics = false;
	playeringame[0] = 1;
	players[0].playerstate = PST_LIVE;
	gameaction = ga_fullconsole;
	if (gamestate == GS_DEMOSCREEN)
	{
		menuactive = MENU_Off;
	}
	if (gamestate == GS_INTERMISSION) gamestate = GS_DEMOSCREEN;
	insave = false;
}

//==========================================================================
//
// D_DoomLoop
//
// Manages timing and IO, calls all ?_Responder, ?_Ticker, and ?_Drawer,
// calls I_GetTime, I_StartFrame, and I_StartTic
//
//==========================================================================

void D_DoomLoop ()
{
	int lasttic = 0;

	// Clamp the timer to TICRATE until the playloop has been entered.
	r_NoInterpolate = true;
	Page = Advisory = NULL;

	vid_cursor.Callback();

	for (;;)
	{
		try
		{
			// frame syncronous IO operations
			if (gametic > lasttic)
			{
				lasttic = gametic;
				I_StartFrame ();
			}
			I_SetFrameTime();

			// process one or more tics
			if (singletics)
			{
				I_StartTic ();
				D_ProcessEvents ();
				G_BuildTiccmd (&netcmds[consoleplayer][maketic%BACKUPTICS]);
				if (advancedemo)
					D_DoAdvanceDemo ();
				C_Ticker ();
				M_Ticker ();
				G_Ticker ();
				// [RH] Use the consoleplayer's camera to update sounds
				S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
				gametic++;
				maketic++;
				GC::CheckGC ();
				Net_NewMakeTic ();
			}
			else
			{
				TryRunTics (); // will run at least one tic
			}
			// Update display, next frame, with current state.
			I_StartTic ();
			D_Display ();
			if (wantToRestart)
			{
				wantToRestart = false;
				return;
			}
		}
		catch (CRecoverableError &error)
		{
			if (error.GetMessage ())
			{
				Printf (PRINT_BOLD, "\n%s\n", error.GetMessage());
			}
			D_ErrorCleanup ();
		}
		catch (CVMAbortException &error)
		{
			error.MaybePrintMessage();
			Printf("%s", error.stacktrace.GetChars());
			D_ErrorCleanup();
		}
	}
}

//==========================================================================
//
// D_PageTicker
//
//==========================================================================

void D_PageTicker (void)
{
	if (--pagetic < 0)
		D_AdvanceDemo ();
}

//==========================================================================
//
// D_PageDrawer
//
//==========================================================================

void D_PageDrawer (void)
{
	screen->Clear(0, 0, SCREENWIDTH, SCREENHEIGHT, 0, 0);
	if (Page != NULL)
	{
		screen->DrawTexture (Page, 0, 0,
			DTA_Fullscreen, true,
			DTA_Masked, false,
			DTA_BilinearFilter, true,
			TAG_DONE);
	}
	else
	{
		if (!PageBlank)
		{
			screen->DrawText (SmallFont, CR_WHITE, 0, 0, "Page graphic goes here", TAG_DONE);
		}
	}
	if (Advisory != NULL)
	{
		screen->DrawTexture (Advisory, 4, 160, DTA_320x200, true, TAG_DONE);
	}
}

//==========================================================================
//
// D_AdvanceDemo
//
// Called after each demo or intro demosequence finishes
//
//==========================================================================

void D_AdvanceDemo (void)
{
	advancedemo = true;
}

//==========================================================================
//
// D_DoStrifeAdvanceDemo
//
//==========================================================================

void D_DoStrifeAdvanceDemo ()
{
	static const char *const fullVoices[6] =
	{
		"svox/pro1", "svox/pro2", "svox/pro3", "svox/pro4", "svox/pro5", "svox/pro6"
	};
	static const char *const teaserVoices[6] =
	{
		"svox/voc91", "svox/voc92", "svox/voc93", "svox/voc94", "svox/voc95", "svox/voc96"
	};
	const char *const *voices = gameinfo.flags & GI_SHAREWARE ? teaserVoices : fullVoices;
	const char *pagename = NULL;

	gamestate = GS_DEMOSCREEN;
	PageBlank = false;

	switch (demosequence)
	{
	default:
	case 0:
		pagetic = 6 * TICRATE;
		pagename = "TITLEPIC";
		if (Wads.CheckNumForName ("d_logo", ns_music) < 0)
		{ // strife0.wad does not have d_logo
			S_StartMusic ("");
		}
		else
		{
			S_StartMusic ("d_logo");
		}
		C_HideConsole ();
		break;

	case 1:
		// [RH] Strife fades to black and then to the Rogue logo, but
		// I think it looks better if it doesn't fade.
		pagetic = 10 * TICRATE/35;
		pagename = "";	// PANEL0, but strife0.wad doesn't have it, so don't use it.
		PageBlank = true;
		S_Sound (CHAN_VOICE | CHAN_UI, "bishop/active", 1, ATTN_NORM);
		break;

	case 2:
		pagetic = 4 * TICRATE;
		pagename = "RGELOGO";
		break;

	case 3:
		pagetic = 7 * TICRATE;
		pagename = "PANEL1";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[0], 1, ATTN_NORM);
		// The new Strife teaser has D_FMINTR.
		// The full retail Strife has D_INTRO.
		// And the old Strife teaser has both. (I do not know which one it actually uses, nor do I care.)
		S_StartMusic (gameinfo.flags & GI_TEASER2 ? "d_fmintr" : "d_intro");
		break;

	case 4:
		pagetic = 9 * TICRATE;
		pagename = "PANEL2";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[1], 1, ATTN_NORM);
		break;

	case 5:
		pagetic = 12 * TICRATE;
		pagename = "PANEL3";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[2], 1, ATTN_NORM);
		break;

	case 6:
		pagetic = 11 * TICRATE;
		pagename = "PANEL4";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[3], 1, ATTN_NORM);
		break;

	case 7:
		pagetic = 10 * TICRATE;
		pagename = "PANEL5";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[4], 1, ATTN_NORM);
		break;

	case 8:
		pagetic = 16 * TICRATE;
		pagename = "PANEL6";
		S_Sound (CHAN_VOICE | CHAN_UI, voices[5], 1, ATTN_NORM);
		break;

	case 9:
		pagetic = 6 * TICRATE;
		pagename = "vellogo";
		wipegamestate = GS_FORCEWIPEFADE;
		break;

	case 10:
		pagetic = 12 * TICRATE;
		pagename = "CREDIT";
		wipegamestate = GS_FORCEWIPEFADE;
		break;
	}
	if (demosequence++ > 10)
		demosequence = 0;
	if (demosequence == 9 && !(gameinfo.flags & GI_SHAREWARE))
		demosequence = 10;

	if (pagename)
	{
		if (Page != NULL)
		{
			Page->Unload ();
			Page = NULL;
		}
		if (pagename[0])
		{
			Page = TexMan[pagename];
		}
	}
}

//==========================================================================
//
// D_DoAdvanceDemo
//
//==========================================================================

void D_DoAdvanceDemo (void)
{
	static char demoname[8] = "DEMO1";
	static int democount = 0;
	static int pagecount;
	FString pagename;

	advancedemo = false;

	if (gameaction != ga_nothing)
	{
		return;
	}

	V_SetBlend (0,0,0,0);
	players[consoleplayer].playerstate = PST_LIVE;	// not reborn
	usergame = false;				// no save / end game here
	paused = 0;

	// [RH] If you want something more dynamic for your title, create a map
	// and name it TITLEMAP. That map will be loaded and used as the title.

	if (P_CheckMapData("TITLEMAP"))
	{
		G_InitNew ("TITLEMAP", true);
		return;
	}

	if (gameinfo.gametype == GAME_Strife)
	{
		D_DoStrifeAdvanceDemo ();
		return;
	}

	switch (demosequence)
	{
	case 3:
		if (gameinfo.advisoryTime)
		{
			Advisory = TexMan["ADVISOR"];
			demosequence = 1;
			pagetic = (int)(gameinfo.advisoryTime * TICRATE);
			break;
		}
		// fall through to case 1 if no advisory notice

	case 1:
		Advisory = NULL;
		if (!M_DemoNoPlay)
		{
			V_SetBorderNeedRefresh();
			democount++;
			mysnprintf (demoname + 4, countof(demoname) - 4, "%d", democount);
			if (Wads.CheckNumForName (demoname) < 0)
			{
				demosequence = 0;
				democount = 0;
				// falls through to case 0 below
			}
			else
			{
				G_DeferedPlayDemo (demoname);
				demosequence = 2;
				break;
			}
		}

	default:
	case 0:
		gamestate = GS_DEMOSCREEN;
		pagename = gameinfo.TitlePage;
		pagetic = (int)(gameinfo.titleTime * TICRATE);
		S_ChangeMusic (gameinfo.titleMusic, gameinfo.titleOrder, false);
		demosequence = 3;
		pagecount = 0;
		C_HideConsole ();
		break;

	case 2:
		pagetic = (int)(gameinfo.pageTime * TICRATE);
		gamestate = GS_DEMOSCREEN;
		if (gameinfo.creditPages.Size() > 0)
		{
			pagename = gameinfo.creditPages[pagecount];
			pagecount = (pagecount+1) % gameinfo.creditPages.Size();
		}
		demosequence = 1;
		break;
	}

	if (pagename.IsNotEmpty())
	{
		if (Page != NULL)
		{
			Page->Unload ();
		}
		Page = TexMan[pagename];
	}
}

//==========================================================================
//
// D_StartTitle
//
//==========================================================================

void D_StartTitle (void)
{
	gameaction = ga_nothing;
	demosequence = -1;
	D_AdvanceDemo ();
}

//==========================================================================
//
// Cmd_Endgame
//
// [RH] Quit the current game and go to fullscreen console
//
//==========================================================================

CCMD (endgame)
{
	if (!netgame)
	{
		gameaction = ga_fullconsole;
		demosequence = -1;
		G_CheckDemoStatus();
	}
}

//==========================================================================
//
// ParseCVarInfo
//
//==========================================================================

void ParseCVarInfo()
{
	int lump, lastlump = 0;
	bool addedcvars = false;

	while ((lump = Wads.FindLump("CVARINFO", &lastlump)) != -1)
	{
		FScanner sc(lump);
		sc.SetCMode(true);

		while (sc.GetToken())
		{
			FString cvarname;
			char *cvardefault = NULL;
			ECVarType cvartype = CVAR_Dummy;
			int cvarflags = CVAR_MOD|CVAR_ARCHIVE;
			FBaseCVar *cvar;

			// Check for flag tokens.
			while (sc.TokenType == TK_Identifier)
			{
				if (stricmp(sc.String, "server") == 0)
				{
					cvarflags |= CVAR_SERVERINFO;
				}
				else if (stricmp(sc.String, "user") == 0)
				{
					cvarflags |= CVAR_USERINFO;
				}
				else if (stricmp(sc.String, "noarchive") == 0)
				{
					cvarflags &= ~CVAR_ARCHIVE;
				}
				else if (stricmp(sc.String, "cheat") == 0)
				{
					cvarflags |= CVAR_CHEAT;
				}
				else if (stricmp(sc.String, "latch") == 0)
				{
					cvarflags |= CVAR_LATCH;
				}
				else
				{
					sc.ScriptError("Unknown cvar attribute '%s'", sc.String);
				}
				sc.MustGetAnyToken();
			}
			// Do some sanity checks.
			if ((cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO)) == 0 ||
				(cvarflags & (CVAR_SERVERINFO|CVAR_USERINFO)) == (CVAR_SERVERINFO|CVAR_USERINFO))
			{
				sc.ScriptError("One of 'server' or 'user' must be specified");
			}
			// The next token must be the cvar type.
			if (sc.TokenType == TK_Bool)
			{
				cvartype = CVAR_Bool;
			}
			else if (sc.TokenType == TK_Int)
			{
				cvartype = CVAR_Int;
			}
			else if (sc.TokenType == TK_Float)
			{
				cvartype = CVAR_Float;
			}
			else if (sc.TokenType == TK_Color)
			{
				cvartype = CVAR_Color;
			}
			else if (sc.TokenType == TK_String)
			{
				cvartype = CVAR_String;
			}
			else
			{
				sc.ScriptError("Bad cvar type '%s'", sc.String);
			}
			// The next token must be the cvar name.
			sc.MustGetToken(TK_Identifier);
			if (FindCVar(sc.String, NULL) != NULL)
			{
				sc.ScriptError("cvar '%s' already exists", sc.String);
			}
			cvarname = sc.String;
			// A default value is optional and signalled by a '=' token.
			if (sc.CheckToken('='))
			{
				switch (cvartype)
				{
				case CVAR_Bool:
					if (!sc.CheckToken(TK_True) && !sc.CheckToken(TK_False))
					{
						sc.ScriptError("Expected true or false");
					}
					cvardefault = sc.String;
					break;
				case CVAR_Int:
					sc.MustGetNumber();
					cvardefault = sc.String;
					break;
				case CVAR_Float:
					sc.MustGetFloat();
					cvardefault = sc.String;
					break;
				default:
					sc.MustGetString();
					cvardefault = sc.String;
					break;
				}
			}
			// Now create the cvar.
			cvar = C_CreateCVar(cvarname, cvartype, cvarflags);
			if (cvardefault != NULL)
			{
				UCVarValue val;
				val.String = cvardefault;
				cvar->SetGenericRepDefault(val, CVAR_String);
			}
			// To be like C and ACS, require a semicolon after everything.
			sc.MustGetToken(';');
			addedcvars = true;
		}
	}
	// Only load mod cvars from the config if we defined some, so we don't
	// clutter up the cvar space when not playing mods with custom cvars.
	if (addedcvars)
	{
		GameConfig->DoModSetup (gameinfo.ConfigName);
	}
}	

//==========================================================================
//
// D_AddFile
//
//==========================================================================

bool D_AddFile (TArray<FString> &wadfiles, const char *file, bool check, int position)
{
	if (file == NULL || *file == '\0')
	{
		return false;
	}

	if (check && !DirEntryExists (file))
	{
		const char *f = BaseFileSearch (file, ".wad");
		if (f == NULL)
		{
			Printf ("Can't find '%s'\n", file);
			return false;
		}
		file = f;
	}

	FString f = file;
	FixPathSeperator(f);
	if (position == -1) wadfiles.Push(f);
	else wadfiles.Insert(position, f);
	return true;
}

//==========================================================================
//
// D_AddWildFile
//
//==========================================================================

void D_AddWildFile (TArray<FString> &wadfiles, const char *value)
{
	if (value == NULL || *value == '\0')
	{
		return;
	}
	const char *wadfile = BaseFileSearch (value, ".wad");

	if (wadfile != NULL)
	{
		D_AddFile (wadfiles, wadfile);
	}
	else
	{ // Try pattern matching
		findstate_t findstate;
		char path[PATH_MAX];
		char *sep;
		void *handle = I_FindFirst (value, &findstate);

		strcpy (path, value);
		sep = strrchr (path, '/');
		if (sep == NULL)
		{
			sep = strrchr (path, '\\');
#ifdef _WIN32
			if (sep == NULL && path[1] == ':')
			{
				sep = path + 1;
			}
#endif
		}

		if (handle != ((void *)-1))
		{
			do
			{
				if (!(I_FindAttr(&findstate) & FA_DIREC))
				{
					if (sep == NULL)
					{
						D_AddFile (wadfiles, I_FindName (&findstate));
					}
					else
					{
						strcpy (sep+1, I_FindName (&findstate));
						D_AddFile (wadfiles, path);
					}
				}
			} while (I_FindNext (handle, &findstate) == 0);
		}
		I_FindClose (handle);
	}
}

//==========================================================================
//
// D_AddConfigWads
//
// Adds all files in the specified config file section.
//
//==========================================================================

void D_AddConfigWads (TArray<FString> &wadfiles, const char *section)
{
	if (GameConfig->SetSection (section))
	{
		const char *key;
		const char *value;
		FConfigFile::Position pos;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				// D_AddWildFile resets GameConfig's position, so remember it
				GameConfig->GetPosition (pos);
				D_AddWildFile (wadfiles, ExpandEnvVars(value));
				// Reset GameConfig's position to get next wad
				GameConfig->SetPosition (pos);
			}
		}
	}
}

//==========================================================================
//
// D_AddDirectory
//
// Add all .wad files in a directory. Does not descend into subdirectories.
//
//==========================================================================

static void D_AddDirectory (TArray<FString> &wadfiles, const char *dir)
{
	char curdir[PATH_MAX];

	if (getcwd (curdir, PATH_MAX))
	{
		char skindir[PATH_MAX];
		findstate_t findstate;
		void *handle;
		size_t stuffstart;

		stuffstart = strlen (dir);
		memcpy (skindir, dir, stuffstart*sizeof(*dir));
		skindir[stuffstart] = 0;

		if (skindir[stuffstart-1] == '/')
		{
			skindir[--stuffstart] = 0;
		}

		if (!chdir (skindir))
		{
			skindir[stuffstart++] = '/';
			if ((handle = I_FindFirst ("*.wad", &findstate)) != (void *)-1)
			{
				do
				{
					if (!(I_FindAttr (&findstate) & FA_DIREC))
					{
						strcpy (skindir + stuffstart, I_FindName (&findstate));
						D_AddFile (wadfiles, skindir);
					}
				} while (I_FindNext (handle, &findstate) == 0);
				I_FindClose (handle);
			}
		}
		chdir (curdir);
	}
}


//==========================================================================
//
// BaseFileSearch
//
// If a file does not exist at <file>, looks for it in the directories
// specified in the config file. Returns the path to the file, if found,
// or NULL if it could not be found.
//
//==========================================================================

const char *BaseFileSearch (const char *file, const char *ext, bool lookfirstinprogdir)
{
	static char wad[PATH_MAX];

	if (file == NULL || *file == '\0')
	{
		return NULL;
	}
	if (lookfirstinprogdir)
	{
		mysnprintf (wad, countof(wad), "%s%s%s", progdir.GetChars(), progdir.Back() == '/' ? "" : "/", file);
		if (DirEntryExists (wad))
		{
			return wad;
		}
	}

	if (DirEntryExists (file))
	{
		mysnprintf (wad, countof(wad), "%s", file);
		return wad;
	}

	if (GameConfig != NULL && GameConfig->SetSection ("FileSearch.Directories"))
	{
		const char *key;
		const char *value;

		while (GameConfig->NextInSection (key, value))
		{
			if (stricmp (key, "Path") == 0)
			{
				FString dir;

				dir = NicePath(value);
				if (dir.IsNotEmpty())
				{
					mysnprintf (wad, countof(wad), "%s%s%s", dir.GetChars(), dir.Back() == '/' ? "" : "/", file);
					if (DirEntryExists (wad))
					{
						return wad;
					}
				}
			}
		}
	}

	// Retry, this time with a default extension
	if (ext != NULL)
	{
		FString tmp = file;
		DefaultExtension (tmp, ext);
		return BaseFileSearch (tmp, NULL);
	}
	return NULL;
}

//==========================================================================
//
// ConsiderPatches
//
// Tries to add any deh/bex patches from the command line.
//
//==========================================================================

bool ConsiderPatches (const char *arg)
{
	int i, argc;
	FString *args;
	const char *f;

	argc = Args->CheckParmList(arg, &args);
	for (i = 0; i < argc; ++i)
	{
		if ( (f = BaseFileSearch(args[i], ".deh")) ||
			 (f = BaseFileSearch(args[i], ".bex")) )
		{
			D_LoadDehFile(f);
		}
	}
	return argc > 0;
}

//==========================================================================
//
// D_MultiExec
//
//==========================================================================

FExecList *D_MultiExec (FArgs *list, FExecList *exec)
{
	for (int i = 0; i < list->NumArgs(); ++i)
	{
		exec = C_ParseExecFile(list->GetArg(i), exec);
	}
	return exec;
}

static void GetCmdLineFiles(TArray<FString> &wadfiles)
{
	FString *args;
	int i, argc;

	argc = Args->CheckParmList("-file", &args);
	for (i = 0; i < argc; ++i)
	{
		D_AddWildFile(wadfiles, args[i]);
	}
}

static void CopyFiles(TArray<FString> &to, TArray<FString> &from)
{
	unsigned int ndx = to.Reserve(from.Size());
	for(unsigned i=0;i<from.Size(); i++)
	{
		to[ndx+i] = from[i];
	}
}

static FString ParseGameInfo(TArray<FString> &pwads, const char *fn, const char *data, int size)
{
	FScanner sc;
	FString iwad;
	int pos = 0;

	const char *lastSlash = strrchr (fn, '/');

	sc.OpenMem("GAMEINFO", data, size);
	while(sc.GetToken())
	{
		sc.TokenMustBe(TK_Identifier);
		FString nextKey = sc.String;
		sc.MustGetToken('=');
		if (!nextKey.CompareNoCase("IWAD"))
		{
			sc.MustGetString();
			iwad = sc.String;
		}
		else if (!nextKey.CompareNoCase("LOAD"))
		{
			do
			{
				sc.MustGetString();

				// Try looking for the wad in the same directory as the .wad
				// before looking for it in the current directory.

				FString checkpath;
				if (lastSlash != NULL)
				{
					checkpath = FString(fn, (lastSlash - fn) + 1);
					checkpath += sc.String;
				}
				else
				{
					checkpath = sc.String;
				}
				if (!FileExists(checkpath))
				{
					pos += D_AddFile(pwads, sc.String, true, pos);
				}
				else
				{
					pos += D_AddFile(pwads, checkpath, true, pos);
				}
			}
			while (sc.CheckToken(','));
		}
		else if (!nextKey.CompareNoCase("NOSPRITERENAME"))
		{
			sc.MustGetString();
			nospriterename = sc.Compare("true");
		}
		else if (!nextKey.CompareNoCase("STARTUPTITLE"))
		{
			sc.MustGetString();
			DoomStartupInfo.Name = sc.String;
		}
		else if (!nextKey.CompareNoCase("STARTUPCOLORS"))
		{
			sc.MustGetString();
			DoomStartupInfo.FgColor = V_GetColor(NULL, sc);
			sc.MustGetStringName(",");
			sc.MustGetString();
			DoomStartupInfo.BkColor = V_GetColor(NULL, sc);
		}
		else if (!nextKey.CompareNoCase("STARTUPTYPE"))
		{
			sc.MustGetString();
			FString sttype = sc.String;
			if (!sttype.CompareNoCase("DOOM"))
				DoomStartupInfo.Type = FStartupInfo::DoomStartup;
			else if (!sttype.CompareNoCase("HERETIC"))
				DoomStartupInfo.Type = FStartupInfo::HereticStartup;
			else if (!sttype.CompareNoCase("HEXEN"))
				DoomStartupInfo.Type = FStartupInfo::HexenStartup;
			else if (!sttype.CompareNoCase("STRIFE"))
				DoomStartupInfo.Type = FStartupInfo::StrifeStartup;
			else DoomStartupInfo.Type = FStartupInfo::DefaultStartup;
		}
		else if (!nextKey.CompareNoCase("STARTUPSONG"))
		{
			sc.MustGetString();
			DoomStartupInfo.Song = sc.String;
		}
		else
		{
			// Silently ignore unknown properties
			do
			{
				sc.MustGetAnyToken();
			}
			while(sc.CheckToken(','));
		}
	}
	return iwad;
}

static FString CheckGameInfo(TArray<FString> & pwads)
{
	// scan the list of WADs backwards to find the last one that contains a GAMEINFO lump
	for(int i=pwads.Size()-1; i>=0; i--)
	{
		bool isdir = false;
		FResourceFile *resfile;
		const char *filename = pwads[i];

		// Does this exist? If so, is it a directory?
		if (!DirEntryExists(pwads[i], &isdir))
		{
			Printf(TEXTCOLOR_RED "Could not stat %s\n", filename);
			continue;
		}

		if (!isdir)
		{
			FileReader fr;
			if (!fr.OpenFile(filename))
			{ 
				// Didn't find file
				continue;
			}
			resfile = FResourceFile::OpenResourceFile(filename, fr, true);
		}
		else
			resfile = FResourceFile::OpenDirectory(filename, true);

		if (resfile != NULL)
		{
			uint32_t cnt = resfile->LumpCount();
			for(int i=cnt-1; i>=0; i--)
			{
				FResourceLump *lmp = resfile->GetLump(i);

				if (lmp->Namespace == ns_global && !stricmp(lmp->Name, "GAMEINFO"))
				{
					// Found one!
					FString iwad = ParseGameInfo(pwads, resfile->Filename, (const char*)lmp->CacheLump(), lmp->LumpSize);
					delete resfile;
					return iwad;
				}
			}
			delete resfile;
		}
	}
	return "";
}

//==========================================================================
//
// Checks the IWAD for MAP01 and if found sets GI_MAPxx
//
//==========================================================================

static void SetMapxxFlag()
{
	int lump_name = Wads.CheckNumForName("MAP01", ns_global, Wads.GetIwadNum());
	int lump_wad = Wads.CheckNumForFullName("maps/map01.wad", Wads.GetIwadNum());
	int lump_map = Wads.CheckNumForFullName("maps/map01.map", Wads.GetIwadNum());

	if (lump_name >= 0 || lump_wad >= 0 || lump_map >= 0) gameinfo.flags |= GI_MAPxx;
}

//==========================================================================
//
// FinalGC
//
// If this doesn't free everything, the debug CRT will let us know.
//
//==========================================================================

static void FinalGC()
{
	delete Args;
	Args = nullptr;
	GC::FinalGC = true;
	GC::FullGC();
	GC::DelSoftRootHead();	// the soft root head will not be collected by a GC so we have to do it explicitly
}

//==========================================================================
//
// Initialize
//
//==========================================================================

static void D_DoomInit()
{
	// Set the FPU precision to 53 significant bits. This is the default
	// for Visual C++, but not for GCC, so some slight math variances
	// might crop up if we leave it alone.
#if defined(_FPU_GETCW) && defined(_FPU_EXTENDED) && defined(_FPU_DOUBLE)
	{
		int cw;
		_FPU_GETCW(cw);
		cw = (cw & ~_FPU_EXTENDED) | _FPU_DOUBLE;
		_FPU_SETCW(cw);
	}
#elif defined(_PC_53)
// On the x64 architecture, changing the floating point precision is not supported.
#ifndef _WIN64
	int cfp = _control87(_PC_53, _MCW_PC);
#endif
#endif

	// Check response files before coalescing file parameters.
	M_FindResponseFile ();

	atterm(FinalGC);

	// Combine different file parameters with their pre-switch bits.
	Args->CollectFiles("-deh", ".deh");
	Args->CollectFiles("-bex", ".bex");
	Args->CollectFiles("-exec", ".cfg");
	Args->CollectFiles("-playdemo", ".lmp");
	Args->CollectFiles("-file", NULL);	// anything left goes after -file

	gamestate = GS_STARTUP;

	SetLanguageIDs ();

	const char *v = Args->CheckValue("-rngseed");
	if (v)
	{
		rngseed = staticrngseed = atoi(v);
		use_staticrng = true;
		if (!batchrun) Printf("D_DoomInit: Static RNGseed %d set.\n", rngseed);
	}
	else
	{
		rngseed = I_MakeRNGSeed();
		use_staticrng = false;
	}
	srand(rngseed);
		
	FRandom::StaticClearRandom ();

	if (!batchrun) Printf ("M_LoadDefaults: Load system defaults.\n");
	M_LoadDefaults ();			// load before initing other systems
}

//==========================================================================
//
// AddAutoloadFiles
//
//==========================================================================

static void AddAutoloadFiles(const char *autoname)
{
	LumpFilterIWAD.Format("%s.", autoname);	// The '.' is appened to simplify parsing the string 

	// [SP] Dialog reaction - load lights.pk3 and brightmaps.pk3 based on user choices
	if (!(gameinfo.flags & GI_SHAREWARE))
	{
		if (autoloadlights)
		{
			const char *lightswad = BaseFileSearch ("lights.pk3", NULL);
			if (lightswad)
				D_AddFile (allwads, lightswad);
		}
		if (autoloadbrightmaps)
		{
			const char *bmwad = BaseFileSearch ("brightmaps.pk3", NULL);
			if (bmwad)
				D_AddFile (allwads, bmwad);
		}
	}

	if (!(gameinfo.flags & GI_SHAREWARE) && !Args->CheckParm("-noautoload") && !disableautoload)
	{
		FString file;

		// [RH] zvox.wad - A wad I had intended to be automatically generated
		// from Q2's pak0.pak so the female and cyborg player could have
		// voices. I never got around to writing the utility to do it, though.
		// And I probably never will now. But I know at least one person uses
		// it for something else, so this gets to stay here.
		const char *wad = BaseFileSearch ("zvox.wad", NULL);
		if (wad)
			D_AddFile (allwads, wad);
	
		// [RH] Add any .wad files in the skins directory
#ifdef __unix__
		file = SHARE_DIR;
#else
		file = progdir;
#endif
		file += "skins";
		D_AddDirectory (allwads, file);

#ifdef __unix__
		file = NicePath("$HOME/" GAME_DIR "/skins");
		D_AddDirectory (allwads, file);
#endif	

		// Add common (global) wads
		D_AddConfigWads (allwads, "Global.Autoload");

		long len;
		int lastpos = -1;

		while ((len = LumpFilterIWAD.IndexOf('.', lastpos+1)) > 0)
		{
			file = LumpFilterIWAD.Left(len) + ".Autoload";
			D_AddConfigWads(allwads, file);
			lastpos = len;
		}
	}
}

//==========================================================================
//
// CheckCmdLine
//
//==========================================================================

static void CheckCmdLine()
{
	int flags = dmflags;
	int p;
	const char *v;

	if (!batchrun) Printf ("Checking cmd-line parameters...\n");
	if (Args->CheckParm ("-nomonsters"))	flags |= DF_NO_MONSTERS;
	if (Args->CheckParm ("-respawn"))		flags |= DF_MONSTERS_RESPAWN;
	if (Args->CheckParm ("-fast"))			flags |= DF_FAST_MONSTERS;

	devparm = !!Args->CheckParm ("-devparm");

	if (Args->CheckParm ("-altdeath"))
	{
		deathmatch = 1;
		flags |= DF_ITEMS_RESPAWN;
	}
	else if (Args->CheckParm ("-deathmatch"))
	{
		deathmatch = 1;
		flags |= DF_WEAPONS_STAY | DF_ITEMS_RESPAWN;
	}

	dmflags = flags;

	// get skill / episode / map from parms
	if (gameinfo.gametype != GAME_Hexen)
	{
		startmap = (gameinfo.flags & GI_MAPxx) ? "MAP01" : "E1M1";
	}
	else
	{
		startmap = "&wt@01";
	}
	autostart = StoredWarp.IsNotEmpty();
				
	const char *val = Args->CheckValue ("-skill");
	if (val)
	{
		gameskill = val[0] - '1';
		autostart = true;
	}

	p = Args->CheckParm ("-warp");
	if (p && p < Args->NumArgs() - 1)
	{
		int ep, map;

		if (gameinfo.flags & GI_MAPxx)
		{
			ep = 1;
			map = atoi (Args->GetArg(p+1));
		}
		else 
		{
			ep = atoi (Args->GetArg(p+1));
			map = p < Args->NumArgs() - 2 ? atoi (Args->GetArg(p+2)) : 10;
			if (map < 1 || map > 9)
			{
				map = ep;
				ep = 1;
			}
		}

		startmap = CalcMapName (ep, map);
		autostart = true;
	}

	// [RH] Hack to handle +map. The standard console command line handler
	// won't be able to handle it, so we take it out of the command line and set
	// it up like -warp.
	FString mapvalue = Args->TakeValue("+map");
	if (mapvalue.IsNotEmpty())
	{
		if (!P_CheckMapData(mapvalue))
		{
			Printf ("Can't find map %s\n", mapvalue.GetChars());
		}
		else
		{
			startmap = mapvalue;
			autostart = true;
		}
	}

	if (devparm)
	{
		Printf ("%s", GStrings("D_DEVSTR"));
	}

	// turbo option  // [RH] (now a cvar)
	v = Args->CheckValue("-turbo");
	if (v != NULL)
	{
		double amt = atof(v);
		Printf ("turbo scale: %.0f%%\n", amt);
		turbo = (float)amt;
	}

	v = Args->CheckValue ("-timer");
	if (v)
	{
		double time = strtod (v, NULL);
		Printf ("Levels will end after %g minute%s.\n", time, time > 1 ? "s" : "");
		timelimit = (float)time;
	}

	v = Args->CheckValue ("-avg");
	if (v)
	{
		Printf ("Austin Virtual Gaming: Levels will end after 20 minutes\n");
		timelimit = 20.f;
	}

	//
	//  Build status bar line!
	//
	if (deathmatch)
		StartScreen->AppendStatusLine("DeathMatch...");
	if (dmflags & DF_NO_MONSTERS)
		StartScreen->AppendStatusLine("No Monsters...");
	if (dmflags & DF_MONSTERS_RESPAWN)
		StartScreen->AppendStatusLine("Respawning...");
	if (autostart)
	{
		FString temp;
		temp.Format ("Warp to map %s, Skill %d ", startmap.GetChars(), gameskill + 1);
		StartScreen->AppendStatusLine(temp);
	}
}

//==========================================================================
//
// D_DoomMain
//
//==========================================================================

void D_DoomMain (void)
{
	int p;
	const char *v;
	const char *wad;
	TArray<FString> pwads;
	FString *args;
	int argcount;	
	FIWadManager *iwad_man;
	const char *batchout = Args->CheckValue("-errorlog");

	// +logfile gets checked too late to catch the full startup log in the logfile so do some extra check for it here.
	FString logfile = Args->TakeValue("+logfile");
	if (logfile.IsNotEmpty())
	{
		execLogfile(logfile);
	}
	else if (batchout != NULL && *batchout != 0)
	{
		batchrun = true;
		execLogfile(batchout, true);
		Printf("Command line: ");
		for (int i = 0; i < Args->NumArgs(); i++)
		{
			Printf("%s ", Args->GetArg(i));
		}
		Printf("\n");
	}

	if (Args->CheckParm("-hashfiles"))
	{
		const char *filename = "fileinfo.txt";
		Printf("Hashing loaded content to: %s\n", filename);
		hashfile = fopen(filename, "w");
		if (hashfile)
		{
			fprintf(hashfile, "%s version %s (%s)\n", GAMENAME, GetVersionString(), GetGitHash());
#ifdef __VERSION__
			fprintf(hashfile, "Compiler version: %s\n", __VERSION__);
#endif
			fprintf(hashfile, "Command line:");
			for (int i = 0; i < Args->NumArgs(); ++i)
			{
				fprintf(hashfile, " %s", Args->GetArg(i));
			}
			fprintf(hashfile, "\n");
		}
	}

	if (!batchrun) Printf(PRINT_LOG, "%s version %s\n", GAMENAME, GetVersionString());

	D_DoomInit();

	extern void D_ConfirmSendStats();
	D_ConfirmSendStats();

	// [RH] Make sure zdoom.pk3 is always loaded,
	// as it contains magic stuff we need.
	wad = BaseFileSearch (BASEWAD, NULL, true);
	if (wad == NULL)
	{
		I_FatalError ("Cannot find " BASEWAD);
	}
	FString basewad = wad;

	FString optionalwad = BaseFileSearch(OPTIONALWAD, NULL, true);

	iwad_man = new FIWadManager(basewad);

	// Now that we have the IWADINFO, initialize the autoload ini sections.
	GameConfig->DoAutoloadSetup(iwad_man);

	// reinit from here

	do
	{
		PClass::StaticInit();
		PType::StaticInit();

		if (restart)
		{
			C_InitConsole(SCREENWIDTH, SCREENHEIGHT, false);
		}
		nospriterename = false;

		// Load zdoom.pk3 alone so that we can get access to the internal gameinfos before 
		// the IWAD is known.

		GetCmdLineFiles(pwads);
		FString iwad = CheckGameInfo(pwads);

		// The IWAD selection dialogue does not show in fullscreen so if the
		// restart is initiated without a defined IWAD assume for now that it's not going to change.
		if (iwad.IsEmpty()) iwad = lastIWAD;

		if (iwad_man == NULL)
		{
			iwad_man = new FIWadManager(basewad);
		}
		const FIWADInfo *iwad_info = iwad_man->FindIWAD(allwads, iwad, basewad, optionalwad);
		gameinfo.gametype = iwad_info->gametype;
		gameinfo.flags = iwad_info->flags;
		gameinfo.ConfigName = iwad_info->Configname;
		lastIWAD = iwad;

		if ((gameinfo.flags & GI_SHAREWARE) && pwads.Size() > 0)
		{
			I_FatalError ("You cannot -file with the shareware version. Register!");
		}

		FBaseCVar::DisableCallbacks();
		GameConfig->DoGameSetup (gameinfo.ConfigName);

		AddAutoloadFiles(iwad_info->Autoname);

		// Process automatically executed files
		FExecList *exec;
		FArgs *execFiles = new FArgs;
		GameConfig->AddAutoexec(execFiles, gameinfo.ConfigName);
		exec = D_MultiExec(execFiles, NULL);
		delete execFiles;

		// Process .cfg files at the start of the command line.
		execFiles = Args->GatherFiles ("-exec");
		exec = D_MultiExec(execFiles, exec);
		delete execFiles;

		// [RH] process all + commands on the command line
		exec = C_ParseCmdLineParams(exec);

		CopyFiles(allwads, pwads);
		if (exec != NULL)
		{
			exec->AddPullins(allwads);
		}

		// Since this function will never leave we must delete this array here manually.
		pwads.Clear();
		pwads.ShrinkToFit();

		if (hashfile)
		{
			Printf("Notice: File hashing is incredibly verbose. Expect loading files to take much longer than usual.\n");
		}

		if (!batchrun) Printf ("W_Init: Init WADfiles.\n");
		Wads.InitMultipleFiles (allwads);
		allwads.Clear();
		allwads.ShrinkToFit();
		SetMapxxFlag();

		GameConfig->DoKeySetup(gameinfo.ConfigName);

		// Now that wads are loaded, define mod-specific cvars.
		ParseCVarInfo();

		// Actually exec command line commands and exec files.
		if (exec != NULL)
		{
			exec->ExecCommands();
			delete exec;
			exec = NULL;
		}

		// [RH] Initialize localizable strings.
		GStrings.LoadStrings (false);

		V_InitFontColors ();

		// [RH] Moved these up here so that we can do most of our
		//		startup output in a fullscreen console.

		CT_Init ();

		if (!restart)
		{
			if (!batchrun) Printf ("I_Init: Setting up machine state.\n");
			I_Init ();
			I_CreateRenderer();
		}

		if (!batchrun) Printf ("V_Init: allocate screen.\n");
		V_Init (!!restart);

		// Base systems have been inited; enable cvar callbacks
		FBaseCVar::EnableCallbacks ();

		if (!batchrun) Printf ("S_Init: Setting up sound.\n");
		S_Init ();

		if (!batchrun) Printf ("ST_Init: Init startup screen.\n");
		if (!restart)
		{
			StartScreen = FStartupScreen::CreateInstance (TexMan.GuesstimateNumTextures() + 5);
		}
		else
		{
			StartScreen = new FStartupScreen(0);
		}

		ParseCompatibility();

		CheckCmdLine();

		// [RH] Load sound environments
		S_ParseReverbDef ();

		// [RH] Parse any SNDINFO lumps
		if (!batchrun) Printf ("S_InitData: Load sound definitions.\n");
		S_InitData ();

		// [RH] Parse through all loaded mapinfo lumps
		if (!batchrun) Printf ("G_ParseMapInfo: Load map definitions.\n");
		G_ParseMapInfo (iwad_info->MapInfo);
		ReadStatistics();

		// MUSINFO must be parsed after MAPINFO
		S_ParseMusInfo();

		if (!batchrun) Printf ("Texman.Init: Init texture manager.\n");
		TexMan.Init();
		C_InitConback();

		StartScreen->Progress();
		V_InitFonts();

		// [CW] Parse any TEAMINFO lumps.
		if (!batchrun) Printf ("ParseTeamInfo: Load team definitions.\n");
		TeamLibrary.ParseTeamInfo ();

		R_ParseTrnslate();
		PClassActor::StaticInit ();

		// [GRB] Initialize player class list
		SetupPlayerClasses ();

		// [RH] Load custom key and weapon settings from WADs
		D_LoadWadSettings ();

		// [GRB] Check if someone used clearplayerclasses but not addplayerclass
		if (PlayerClasses.Size () == 0)
		{
			I_FatalError ("No player classes defined");
		}

		StartScreen->Progress ();

		ParseGLDefs();

		if (!batchrun) Printf ("R_Init: Init %s refresh subsystem.\n", gameinfo.ConfigName.GetChars());
		StartScreen->LoadingStatus ("Loading graphics", 0x3f);
		R_Init ();

		if (!batchrun) Printf ("DecalLibrary: Load decals.\n");
		DecalLibrary.ReadAllDecals ();

		// Load embedded Dehacked patches
		D_LoadDehLumps(FromIWAD);

		// [RH] Add any .deh and .bex files on the command line.
		// If there are none, try adding any in the config file.
		// Note that the command line overrides defaults from the config.

		if ((ConsiderPatches("-deh") | ConsiderPatches("-bex")) == 0 &&
			gameinfo.gametype == GAME_Doom && GameConfig->SetSection ("Doom.DefaultDehacked"))
		{
			const char *key;
			const char *value;

			while (GameConfig->NextInSection (key, value))
			{
				if (stricmp (key, "Path") == 0 && FileExists (value))
				{
					if (!batchrun) Printf ("Applying patch %s\n", value);
					D_LoadDehFile(value);
				}
			}
		}

		// Load embedded Dehacked patches
		D_LoadDehLumps(FromPWADs);

		// Create replacements for dehacked pickups
		FinishDehPatch();

		if (!batchrun) Printf("M_Init: Init menus.\n");
		M_Init();

		// clean up the compiler symbols which are not needed any longer.
		RemoveUnusedSymbols();

		InitActorNumsFromMapinfo();
		InitSpawnablesFromMapinfo();
		PClassActor::StaticSetActorNums();

		//Added by MC:
		bglobal.getspawned.Clear();
		argcount = Args->CheckParmList("-bots", &args);
		for (p = 0; p < argcount; ++p)
		{
			bglobal.getspawned.Push(args[p]);
		}
		bglobal.spawn_tries = 0;
		bglobal.wanted_botnum = bglobal.getspawned.Size();

		if (!batchrun) Printf ("P_Init: Init Playloop state.\n");
		StartScreen->LoadingStatus ("Init game engine", 0x3f);
		AM_StaticInit();
		P_Init ();

		P_SetupWeapons_ntohton();

		//SBarInfo support. Note that the first SBARINFO lump contains the mugshot definition so it even needs to be read when a regular status bar is being used.
		SBarInfo::Load();
		HUD_InitHud();

		if (!batchrun)
		{
			// [RH] User-configurable startup strings. Because BOOM does.
			static const char *startupString[5] = {
				"STARTUP1", "STARTUP2", "STARTUP3", "STARTUP4", "STARTUP5"
			};
			for (p = 0; p < 5; ++p)
			{
				const char *str = GStrings[startupString[p]];
				if (str != NULL && str[0] != '\0')
				{
					Printf("%s\n", str);
				}
			}
		}

		if (!restart)
		{
			if (!batchrun) Printf ("D_CheckNetGame: Checking network game status.\n");
			StartScreen->LoadingStatus ("Checking network game status.", 0x3f);
			D_CheckNetGame ();
		}

		// [SP] Force vanilla transparency auto-detection to re-detect our game lumps now
		UpdateVanillaTransparency();

		// [RH] Lock any cvars that should be locked now that we're
		// about to begin the game.
		FBaseCVar::EnableNoSet ();

		delete iwad_man;	// now we won't need this anymore
		iwad_man = NULL;

		// [RH] Run any saved commands from the command line or autoexec.cfg now.
		gamestate = GS_FULLCONSOLE;
		Net_NewMakeTic ();
		DThinker::RunThinkers ();
		gamestate = GS_STARTUP;

		if (!restart)
		{
			// start the apropriate game based on parms
			v = Args->CheckValue ("-record");

			if (v)
			{
				G_RecordDemo (v);
				autostart = true;
			}

			delete StartScreen;
			StartScreen = NULL;
			S_Sound (CHAN_BODY, "misc/startupdone", 1, ATTN_NONE);

			if (Args->CheckParm("-norun") || batchrun)
			{
				throw CNoRunExit();
			}

			V_Init2();
			gl_PatchMenu();
			UpdateJoystickMenu(NULL);

			v = Args->CheckValue ("-loadgame");
			if (v)
			{
				FString file(v);
				FixPathSeperator (file);
				DefaultExtension (file, "." SAVEGAME_EXT);
				G_LoadGame (file);
			}

			v = Args->CheckValue("-playdemo");
			if (v != NULL)
			{
				singledemo = true;				// quit after one demo
				G_DeferedPlayDemo (v);
				D_DoomLoop ();	// never returns
			}
			else
			{
				v = Args->CheckValue("-timedemo");
				if (v)
				{
					G_TimeDemo(v);
					D_DoomLoop();	// never returns
				}
				else
				{
					if (gameaction != ga_loadgame && gameaction != ga_loadgamehidecon)
					{
						if (autostart || netgame)
						{
							// Do not do any screenwipes when autostarting a game.
							if (!Args->CheckParm("-warpwipe"))
							{
								NoWipe = TICRATE;
							}
							CheckWarpTransMap(startmap, true);
							if (demorecording)
								G_BeginRecording(startmap);
							G_InitNew(startmap, false);
							if (StoredWarp.IsNotEmpty())
							{
								AddCommandString(StoredWarp.LockBuffer());
								StoredWarp = NULL;
							}
						}
						else
						{
							D_StartTitle();				// start up intro loop
						}
					}
					else if (demorecording)
					{
						G_BeginRecording(NULL);
					}

					atterm(D_QuitNetGame);		// killough
				}
			}
		}
		else
		{
			// let the renderer reinitialize some stuff if needed
			screen->GameRestart();
			// These calls from inside V_Init2 are still necessary
			C_NewModeAdjust();
			M_InitVideoModesMenu();
			Renderer->RemapVoxels();
			D_StartTitle ();				// start up intro loop
			setmodeneeded = false;			// This may be set to true here, but isn't needed for a restart
		}

		D_DoAnonStats();

		if (I_FriendlyWindowTitle)
			I_SetWindowTitle(DoomStartupInfo.Name.GetChars());

		D_DoomLoop ();		// this only returns if a 'restart' CCMD is given.
		// 
		// Clean up after a restart
		//

		// Music and sound should be stopped first
		S_StopMusic(true);
		S_StopAllChannels ();

		M_ClearMenus();					// close menu if open
		F_EndFinale();					// If an intermission is active, end it now
		AM_ClearColorsets();

		// clean up game state
		ST_Clear();
		D_ErrorCleanup ();
		DThinker::DestroyThinkersInList(STAT_STATIC);
		P_FreeLevelData();
		P_FreeExtraLevelData();

		M_SaveDefaults(NULL);			// save config before the restart

		// delete all data that cannot be left until reinitialization
		V_ClearFonts();					// must clear global font pointers
		ColorSets.Clear();
		PainFlashes.Clear();
		R_DeinitTranslationTables();	// some tables are initialized from outside the translation code.
		gameinfo.~gameinfo_t();
		new (&gameinfo) gameinfo_t;		// Reset gameinfo
		S_Shutdown();					// free all channels and delete playlist
		C_ClearAliases();				// CCMDs won't be reinitialized so these need to be deleted here
		DestroyCVarsFlagged(CVAR_MOD);	// Delete any cvar left by mods
		FS_Close();						// destroy the global FraggleScript.
		DeinitMenus();

		// delete DoomStartupInfo data
		DoomStartupInfo.Name = (const char*)0;
		DoomStartupInfo.BkColor = DoomStartupInfo.FgColor = DoomStartupInfo.Type = 0;

		GC::FullGC();					// clean up before taking down the object list.

		// Delete the reference to the VM functions here which were deleted and will be recreated after the restart.
		FAutoSegIterator probe(ARegHead, ARegTail);
		while (*++probe != NULL)
		{
			AFuncDesc *afunc = (AFuncDesc *)*probe;
			*(afunc->VMPointer) = NULL;
		}

		GC::DelSoftRootHead();

		PClass::StaticShutdown();

		GC::FullGC();					// perform one final garbage collection after shutdown

		assert(GC::Root == nullptr);

		restart++;
		PClass::bShutdown = false;
		PClass::bVMOperational = false;
	}
	while (1);
}

//==========================================================================
//
// restart the game
//
//==========================================================================

UNSAFE_CCMD(restart)
{
	// remove command line args that would get in the way during restart
	Args->RemoveArgs("-iwad");
	Args->RemoveArgs("-deh");
	Args->RemoveArgs("-bex");
	Args->RemoveArgs("-playdemo");
	Args->RemoveArgs("-file");
	Args->RemoveArgs("-altdeath");
	Args->RemoveArgs("-deathmatch");
	Args->RemoveArgs("-skill");
	Args->RemoveArgs("-savedir");
	Args->RemoveArgs("-xlat");
	Args->RemoveArgs("-oldsprites");

	if (argv.argc() > 1)
	{
		for (int i = 1; i<argv.argc(); i++)
		{
			Args->AppendArg(argv[i]);
		}
	}

	wantToRestart = true;
}

//==========================================================================
//
// FStartupScreen Constructor
//
//==========================================================================

FStartupScreen::FStartupScreen(int max_progress)
{
	MaxPos = max_progress;
	CurPos = 0;
	NotchPos = 0;
}

//==========================================================================
//
// FStartupScreen Destructor
//
//==========================================================================

FStartupScreen::~FStartupScreen()
{
}

//==========================================================================
//
// FStartupScreen :: LoadingStatus
//
// Used by Heretic for the Loading Status "window."
//
//==========================================================================

void FStartupScreen::LoadingStatus(const char *message, int colors)
{
}

//==========================================================================
//
// FStartupScreen :: AppendStatusLine
//
// Used by Heretic for the "status line" at the bottom of the screen.
//
//==========================================================================

void FStartupScreen::AppendStatusLine(const char *status)
{
}


void FStartupScreen::Progress(void) {}
void FStartupScreen::NetInit(char const *,int) {}
void FStartupScreen::NetProgress(int) {}
void FStartupScreen::NetMessage(char const *,...) {}
void FStartupScreen::NetDone(void) {}
bool FStartupScreen::NetLoop(bool (*)(void *),void *) { return false; }

DEFINE_FIELD_X(InputEventData, event_t, type)
DEFINE_FIELD_X(InputEventData, event_t, subtype)
DEFINE_FIELD_X(InputEventData, event_t, data1)
DEFINE_FIELD_X(InputEventData, event_t, data2)
DEFINE_FIELD_X(InputEventData, event_t, data3)
DEFINE_FIELD_X(InputEventData, event_t, x)
DEFINE_FIELD_X(InputEventData, event_t, y)


CUSTOM_CVAR(Bool, I_FriendlyWindowTitle, true, CVAR_GLOBALCONFIG|CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self)
		I_SetWindowTitle(DoomStartupInfo.Name.GetChars());
	else
		I_SetWindowTitle(NULL);
}
