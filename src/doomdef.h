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
// DESCRIPTION:
//	Internally used data structures for virtually everything,
//	 key definitions, lots of other stuff.
//
//-----------------------------------------------------------------------------

#ifndef __DOOMDEF_H__
#define __DOOMDEF_H__

#include <stdio.h>
#include <string.h>

//
// Global parameters/defines.
//

// Game mode handling - identify IWAD version
//	to handle IWAD dependend animations etc.
typedef enum
{
	shareware,		// DOOM 1 shareware, E1, M9
	registered,		// DOOM 1 registered, E3, M27
	commercial,		// DOOM 2 retail, E1 M34
	// DOOM 2 german edition not handled
	retail,			// DOOM 1 retail, E4, M36
	undetermined	// Well, no IWAD found.
  
} GameMode_t;


// If rangecheck is undefined, most parameter validation debugging code
// will not be compiled
#ifndef NORANGECHECKING
#ifndef RANGECHECK
#define RANGECHECK
#endif
#endif

// The maximum number of players, multiplayer/networking.
#define MAXPLAYERS		8

// State updates, number of tics / second.
#define TICRATE 		35

// Amount of damage done by a telefrag.
#define TELEFRAG_DAMAGE	1000000

// The current state of the game: whether we are
// playing, gazing at the intermission screen,
// the game final animation, or a demo. 
typedef enum
{
	GS_LEVEL,
	GS_INTERMISSION,
	GS_FINALE,
	GS_DEMOSCREEN,
	GS_FULLCONSOLE,		// [RH]	Fullscreen console
	GS_HIDECONSOLE,		// [RH] The menu just did something that should hide fs console
	GS_STARTUP,			// [RH] Console is fullscreen, and game is just starting
	GS_TITLELEVEL,		// [RH] A combination of GS_LEVEL and GS_DEMOSCREEN

	GS_FORCEWIPE = -1,
	GS_FORCEWIPEFADE = -2,
	GS_FORCEWIPEBURN = -3,
	GS_FORCEWIPEMELT = -4
} gamestate_t;

extern	gamestate_t 	gamestate;

// wipegamestate can be set to -1
//	to force a wipe on the next draw
extern gamestate_t wipegamestate;


typedef float skill_t;

/*
enum ESkillLevels
{
	sk_baby,
	sk_easy,
	sk_medium,
	sk_hard,
	sk_nightmare
};
*/



#define TELEFOGHEIGHT			(gameinfo.telefogheight)

//
// DOOM keyboard definition. Everything below 0x100 matches
// a mode 1 keyboard scan code.
//
#define KEY_PAUSE				0xc5	// DIK_PAUSE
#define KEY_RIGHTARROW			0xcd	// DIK_RIGHT
#define KEY_LEFTARROW			0xcb	// DIK_LEFT
#define KEY_UPARROW 			0xc8	// DIK_UP
#define KEY_DOWNARROW			0xd0	// DIK_DOWN
#define KEY_ESCAPE				0x01	// DIK_ESCAPE
#define KEY_ENTER				0x1c	// DIK_RETURN
#define KEY_SPACE				0x39	// DIK_SPACE
#define KEY_TAB 				0x0f	// DIK_TAB
#define KEY_F1					0x3b	// DIK_F1
#define KEY_F2					0x3c	// DIK_F2
#define KEY_F3					0x3d	// DIK_F3
#define KEY_F4					0x3e	// DIK_F4
#define KEY_F5					0x3f	// DIK_F5
#define KEY_F6					0x40	// DIK_F6
#define KEY_F7					0x41	// DIK_F7
#define KEY_F8					0x42	// DIK_F8
#define KEY_F9					0x43	// DIK_F9
#define KEY_F10 				0x44	// DIK_F10
#define KEY_F11 				0x57	// DIK_F11
#define KEY_F12 				0x58	// DIK_F12
#define KEY_GRAVE				0x29	// DIK_GRAVE

#define KEY_BACKSPACE			0x0e	// DIK_BACK

#define KEY_EQUALS				0x0d	// DIK_EQUALS
#define KEY_MINUS				0x0c	// DIK_MINUS

#define KEY_LSHIFT				0x2A	// DIK_LSHIFT
#define KEY_LCTRL				0x1d	// DIK_LCONTROL
#define KEY_LALT				0x38	// DIK_LMENU

#define	KEY_RSHIFT				KEY_LSHIFT
#define KEY_RCTRL				KEY_LCTRL
#define KEY_RALT				KEY_LALT

#define KEY_INS 				0xd2	// DIK_INSERT
#define KEY_DEL 				0xd3	// DIK_DELETE
#define KEY_END 				0xcf	// DIK_END
#define KEY_HOME				0xc7	// DIK_HOME
#define KEY_PGUP				0xc9	// DIK_PRIOR
#define KEY_PGDN				0xd1	// DIK_NEXT

#define KEY_MOUSE1				0x100
#define KEY_MOUSE2				0x101
#define KEY_MOUSE3				0x102
#define KEY_MOUSE4				0x103
#define KEY_MOUSE5				0x104
#define KEY_MOUSE6				0x105
#define KEY_MOUSE7				0x106
#define KEY_MOUSE8				0x107

#define KEY_FIRSTJOYBUTTON		0x108
#define KEY_JOY1				(KEY_FIRSTJOYBUTTON+0)
#define KEY_JOY2				(KEY_FIRSTJOYBUTTON+1)
#define KEY_JOY3				(KEY_FIRSTJOYBUTTON+2)
#define KEY_JOY4				(KEY_FIRSTJOYBUTTON+3)
#define KEY_JOY5				(KEY_FIRSTJOYBUTTON+4)
#define KEY_JOY6				(KEY_FIRSTJOYBUTTON+5)
#define KEY_JOY7				(KEY_FIRSTJOYBUTTON+6)
#define KEY_JOY8				(KEY_FIRSTJOYBUTTON+7)
#define KEY_LASTJOYBUTTON		0x187
#define KEY_JOYPOV1_UP			0x188
#define KEY_JOYPOV1_RIGHT		0x189
#define KEY_JOYPOV1_DOWN		0x18a
#define KEY_JOYPOV1_LEFT		0x18b
#define KEY_JOYPOV2_UP			0x18c
#define KEY_JOYPOV3_UP			0x190
#define KEY_JOYPOV4_UP			0x194

#define KEY_MWHEELUP			0x198
#define KEY_MWHEELDOWN			0x199
#define KEY_MWHEELRIGHT			0x19A
#define KEY_MWHEELLEFT			0x19B

#define KEY_JOYAXIS1PLUS		0x19C
#define KEY_JOYAXIS1MINUS		0x19D
#define KEY_JOYAXIS2PLUS		0x19E
#define KEY_JOYAXIS2MINUS		0x19F
#define KEY_JOYAXIS3PLUS		0x1A0
#define KEY_JOYAXIS3MINUS		0x1A1
#define KEY_JOYAXIS4PLUS		0x1A2
#define KEY_JOYAXIS4MINUS		0x1A3
#define KEY_JOYAXIS5PLUS		0x1A4
#define KEY_JOYAXIS5MINUS		0x1A5
#define KEY_JOYAXIS6PLUS		0x1A6
#define KEY_JOYAXIS6MINUS		0x1A7
#define KEY_JOYAXIS7PLUS		0x1A8
#define KEY_JOYAXIS7MINUS		0x1A9
#define KEY_JOYAXIS8PLUS		0x1AA
#define KEY_JOYAXIS8MINUS		0x1AB
#define NUM_JOYAXISBUTTONS		8

#define KEY_PAD_LTHUMB_RIGHT	0x1AC
#define KEY_PAD_LTHUMB_LEFT		0x1AD
#define KEY_PAD_LTHUMB_DOWN		0x1AE
#define KEY_PAD_LTHUMB_UP		0x1AF

#define KEY_PAD_RTHUMB_RIGHT	0x1B0
#define KEY_PAD_RTHUMB_LEFT		0x1B1
#define KEY_PAD_RTHUMB_DOWN		0x1B2
#define KEY_PAD_RTHUMB_UP		0x1B3

#define KEY_PAD_DPAD_UP			0x1B4
#define KEY_PAD_DPAD_DOWN		0x1B5
#define KEY_PAD_DPAD_LEFT		0x1B6
#define KEY_PAD_DPAD_RIGHT		0x1B7
#define KEY_PAD_START			0x1B8
#define KEY_PAD_BACK			0x1B9
#define KEY_PAD_LTHUMB			0x1BA
#define KEY_PAD_RTHUMB			0x1BB
#define KEY_PAD_LSHOULDER		0x1BC
#define KEY_PAD_RSHOULDER		0x1BD
#define KEY_PAD_LTRIGGER		0x1BE
#define KEY_PAD_RTRIGGER		0x1BF
#define KEY_PAD_A				0x1C0
#define KEY_PAD_B				0x1C1
#define KEY_PAD_X				0x1C2
#define KEY_PAD_Y				0x1C3

#define NUM_KEYS				0x1C4

// [RH] dmflags bits (based on Q2's)
enum
{
	DF_NO_HEALTH			= 1 << 0,	// Do not spawn health items (DM)
	DF_NO_ITEMS				= 1 << 1,	// Do not spawn powerups (DM)
	DF_WEAPONS_STAY			= 1 << 2,	// Leave weapons around after pickup (DM)
	DF_FORCE_FALLINGZD		= 1 << 3,	// Falling too far hurts (old ZDoom style)
	DF_FORCE_FALLINGHX		= 2 << 3,	// Falling too far hurts (Hexen style)
	DF_FORCE_FALLINGST		= 3 << 3,	// Falling too far hurts (Strife style)
//							  1 << 5	-- this space left blank --
	DF_SAME_LEVEL			= 1 << 6,	// Stay on the same map when someone exits (DM)
	DF_SPAWN_FARTHEST		= 1 << 7,	// Spawn players as far as possible from other players (DM)
	DF_FORCE_RESPAWN		= 1 << 8,	// Automatically respawn dead players after respawn_time is up (DM)
	DF_NO_ARMOR				= 1 << 9,	// Do not spawn armor (DM)
	DF_NO_EXIT				= 1 << 10,	// Kill anyone who tries to exit the level (DM)
	DF_INFINITE_AMMO		= 1 << 11,	// Don't use up ammo when firing
	DF_NO_MONSTERS			= 1 << 12,	// Don't spawn monsters (replaces -nomonsters parm)
	DF_MONSTERS_RESPAWN		= 1 << 13,	// Monsters respawn sometime after their death (replaces -respawn parm)
	DF_ITEMS_RESPAWN		= 1 << 14,	// Items other than invuln. and invis. respawn
	DF_FAST_MONSTERS		= 1 << 15,	// Monsters are fast (replaces -fast parm)
	DF_NO_JUMP				= 1 << 16,	// Don't allow jumping
	DF_YES_JUMP				= 2 << 16,
	DF_NO_FREELOOK			= 1 << 18,	// Don't allow freelook
	DF_RESPAWN_SUPER		= 1 << 19,	// Respawn invulnerability and invisibility
	DF_NO_FOV				= 1 << 20,	// Only let the arbitrator set FOV (for all players)
	DF_NO_COOP_WEAPON_SPAWN	= 1 << 21,	// Don't spawn multiplayer weapons in coop games
	DF_NO_CROUCH			= 1 << 22,	// Don't allow crouching
	DF_YES_CROUCH			= 2 << 22,	//
	DF_COOP_LOSE_INVENTORY	= 1 << 24,	// Lose all your old inventory when respawning in coop
	DF_COOP_LOSE_KEYS		= 1 << 25,	// Lose keys when respawning in coop
	DF_COOP_LOSE_WEAPONS	= 1 << 26,	// Lose weapons when respawning in coop
	DF_COOP_LOSE_ARMOR		= 1 << 27,	// Lose armor when respawning in coop
	DF_COOP_LOSE_POWERUPS	= 1 << 28,	// Lose powerups when respawning in coop
	DF_COOP_LOSE_AMMO		= 1 << 29,	// Lose ammo when respawning in coop
	DF_COOP_HALVE_AMMO		= 1 << 30,	// Lose half your ammo when respawning in coop (but not less than the normal starting amount)
};

// [BC] More dmflags. w00p!
enum
{
//	DF2_YES_IMPALING		= 1 << 0,	// Player gets implaed on MF2_IMPALE items
	DF2_YES_WEAPONDROP		= 1 << 1,	// Drop current weapon upon death
//	DF2_NO_RUNES			= 1 << 2,	// Don't spawn runes
//	DF2_INSTANT_RETURN		= 1 << 3,	// Instantly return flags and skulls when player carrying it dies (ST/CTF)
	DF2_NO_TEAM_SWITCH		= 1 << 4,	// Do not allow players to switch teams in teamgames
//	DF2_NO_TEAM_SELECT		= 1 << 5,	// Player is automatically placed on a team.
	DF2_YES_DOUBLEAMMO		= 1 << 6,	// Double amount of ammo that items give you like skill 1 and 5 do
	DF2_YES_DEGENERATION	= 1 << 7,	// Player slowly loses health when over 100% (Quake-style)
	DF2_NO_FREEAIMBFG		= 1 << 8,	// Disallow BFG freeaiming. Prevents cheap BFG frags by aiming at floor or ceiling
	DF2_BARRELS_RESPAWN		= 1 << 9,	// Barrels respawn (duh)
	DF2_YES_RESPAWN_INVUL	= 1 << 10,	// Player is temporarily invulnerable when respawned
//	DF2_COOP_SHOTGUNSTART	= 1 << 11,	// All playres start with a shotgun when they respawn
	DF2_SAME_SPAWN_SPOT		= 1 << 12,	// Players respawn in the same place they died (co-op)
	DF2_YES_KEEPFRAGS		= 1 << 13,	// Don't clear frags after each level
	DF2_NO_RESPAWN			= 1 << 14,	// Player cannot respawn
	DF2_YES_LOSEFRAG		= 1 << 15,	// Lose a frag when killed. More incentive to try to not get yerself killed
	DF2_INFINITE_INVENTORY	= 1 << 16,	// Infinite inventory.
	DF2_KILL_MONSTERS		= 1 << 17,	// All monsters must be killed before the level exits.
	DF2_NO_AUTOMAP			= 1 << 18,	// Players are allowed to see the automap.
	DF2_NO_AUTOMAP_ALLIES	= 1 << 19,	// Allies can been seen on the automap.
	DF2_DISALLOW_SPYING		= 1 << 20,	// You can spy on your allies.
	DF2_CHASECAM			= 1 << 21,	// Players can use the chasecam cheat.
	DF2_NOSUICIDE			= 1 << 22,	// Players are not allowed to suicide.
	DF2_NOAUTOAIM			= 1 << 23,	// Players cannot use autoaim.
	DF2_DONTCHECKAMMO		= 1 << 24,	// Don't Check ammo when switching weapons.
	DF2_KILLBOSSMONST		= 1 << 25,	// Kills all monsters spawned by a boss cube when the boss dies
	DF2_NOCOUNTENDMONST		= 1 << 26,	// Do not count monsters in 'end level when dying' sectors towards kill count
};

// [RH] Compatibility flags.
enum
{
	COMPATF_SHORTTEX		= 1 << 0,	// Use Doom's shortest texture around behavior?
	COMPATF_STAIRINDEX		= 1 << 1,	// Don't fix loop index for stair building?
	COMPATF_LIMITPAIN		= 1 << 2,	// Pain elemental is limited to 20 lost souls?
	COMPATF_SILENTPICKUP	= 1 << 3,	// Pickups are only heard locally?
	COMPATF_NO_PASSMOBJ		= 1 << 4,	// Pretend every actor is infinitely tall?
	COMPATF_MAGICSILENCE	= 1 << 5,	// Limit actors to one sound at a time?
	COMPATF_WALLRUN			= 1 << 6,	// Enable buggier wall clipping so players can wallrun?
	COMPATF_NOTOSSDROPS		= 1 << 7,	// Spawn dropped items directly on the floor?
	COMPATF_USEBLOCKING		= 1 << 8,	// Any special line can block a use line
	COMPATF_NODOORLIGHT		= 1 << 9,	// Don't do the BOOM local door light effect
	COMPATF_RAVENSCROLL		= 1 << 10,	// Raven's scrollers use their original carrying speed
	COMPATF_SOUNDTARGET		= 1 << 11,	// Use sector based sound target code.
	COMPATF_DEHHEALTH		= 1 << 12,	// Limit deh.MaxHealth to the health bonus (as in Doom2.exe)
	COMPATF_TRACE			= 1 << 13,	// Trace ignores lines with the same sector on both sides
	COMPATF_DROPOFF			= 1 << 14,	// Monsters cannot move when hanging over a dropoff
	COMPATF_BOOMSCROLL		= 1 << 15,	// Scrolling sectors are additive like in Boom
	COMPATF_INVISIBILITY	= 1 << 16,	// Monsters can see semi-invisible players
	COMPATF_SILENT_INSTANT_FLOORS = 1<<17,	// Instantly moving floors are not silent
	COMPATF_SECTORSOUNDS	= 1 << 18,	// Sector sounds use original method for sound origin.
	COMPATF_MISSILECLIP		= 1 << 19,	// Use original Doom heights for clipping against projectiles
	COMPATF_CROSSDROPOFF	= 1 << 20,	// monsters can't be pushed over dropoffs
	COMPATF_ANYBOSSDEATH	= 1 << 21,	// [GZ] Any monster which calls BOSSDEATH counts for level specials
	COMPATF_MINOTAUR		= 1 << 22,	// Minotaur's floor flame is exploded immediately when feet are clipped
	COMPATF_MUSHROOM		= 1 << 23,	// Force original velocity calculations for A_Mushroom in Dehacked mods.
	COMPATF_MBFMONSTERMOVE	= 1 << 24,	// Monsters are affected by friction and pushers/pullers.
	COMPATF_CORPSEGIBS		= 1 << 25,	// Crushed monsters are turned into gibs, rather than replaced by gibs.
	COMPATF_NOBLOCKFRIENDS	= 1 << 26,	// Friendly monsters aren't blocked by monster-blocking lines.
	COMPATF_SPRITESORT		= 1 << 27,	// Invert sprite sorting order for sprites of equal distance
	COMPATF_HITSCAN			= 1 << 28,	// Hitscans use original blockmap anf hit check code.
	COMPATF_LIGHT			= 1 << 29,	// Find neighboring light level like Doom
	COMPATF_POLYOBJ			= 1 << 30,	// Draw polyobjects the old fashioned way
	COMPATF_MASKEDMIDTEX	= 1u << 31,	// Ignore compositing when drawing masked midtextures

	COMPATF2_BADANGLES		= 1 << 0,	// It is impossible to face directly NSEW.
	COMPATF2_FLOORMOVE		= 1 << 1,	// Use the same floor motion behavior as Doom.
};

// Emulate old bugs for select maps. These are not exposed by a cvar
// or mapinfo because we do not want new maps to use these bugs.
enum
{
	BCOMPATF_SETSLOPEOVERFLOW	= 1 << 0,	// SetSlope things can overflow
	BCOMPATF_RESETPLAYERSPEED	= 1 << 1,	// Set player speed to 1.0 when changing maps
	BCOMPATF_VILEGHOSTS			= 1 << 2,	// Monsters' radius and height aren't restored properly when resurrected.
	BCOMPATF_BADTELEPORTERS		= 1 << 3,	// Ignore tags on Teleport specials
	BCOMPATF_BADPORTALS			= 1 << 4,	// Restores the old unstable portal behavior
	BCOMPATF_REBUILDNODES		= 1 << 5,	// Force node rebuild
	BCOMPATF_LINKFROZENPROPS	= 1 << 6,	// Clearing PROP_TOTALLYFROZEN or PROP_FROZEN also clears the other
	BCOMPATF_NOWINDOWCHECK		= 1 << 7,	// Disable the window check in CheckForPushSpecial()
};

// phares 3/20/98:
//
// Player friction is variable, based on controlling
// linedefs. More friction can create mud, sludge,
// magnetized floors, etc. Less friction can create ice.

#define MORE_FRICTION_VELOCITY	15000	// mud factor based on velocity
#define ORIG_FRICTION			0xE800	// original value
#define ORIG_FRICTION_FACTOR	2048	// original value
#define FRICTION_LOW			0xf900
#define FRICTION_FLY			0xeb00


#define BLINKTHRESHOLD (4*32)

#ifndef __BIG_ENDIAN__
#define MAKE_ID(a,b,c,d)	((DWORD)((a)|((b)<<8)|((c)<<16)|((d)<<24)))
#else
#define MAKE_ID(a,b,c,d)	((DWORD)((d)|((c)<<8)|((b)<<16)|((a)<<24)))
#endif

#endif	// __DOOMDEF_H__
