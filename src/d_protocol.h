/*
** d_protocol.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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
*/

#ifndef __D_PROTOCOL_H__
#define __D_PROTOCOL_H__

#include "doomtype.h"

// The IFF routines here all work with big-endian IDs, even if the host
// system is little-endian.
#define BIGE_ID(a,b,c,d)	((d)|((c)<<8)|((b)<<16)|((a)<<24))

#define FORM_ID		BIGE_ID('F','O','R','M')
#define ZDEM_ID		BIGE_ID('Z','D','E','M')
#define ZDHD_ID		BIGE_ID('Z','D','H','D')
#define VARS_ID		BIGE_ID('V','A','R','S')
#define UINF_ID		BIGE_ID('U','I','N','F')
#define COMP_ID		BIGE_ID('C','O','M','P')
#define BODY_ID		BIGE_ID('B','O','D','Y')
#define NETD_ID		BIGE_ID('N','E','T','D')
#define WEAP_ID		BIGE_ID('W','E','A','P')


struct zdemoheader_s {
	BYTE	demovermajor;
	BYTE	demoverminor;
	BYTE	minvermajor;
	BYTE	minverminor;
	BYTE	map[8];
	unsigned int rngseed;
	BYTE	consoleplayer;
};

struct usercmd_t
{
	DWORD	buttons;
	short	pitch;			// up/down
	short	yaw;			// left/right
	short	roll;			// "tilt"
	short	forwardmove;
	short	sidemove;
	short	upmove;
};

class FArchive;

FArchive &operator<< (FArchive &arc, usercmd_t &cmd);

// When transmitted, the above message is preceded by a byte
// indicating which fields are actually present in the message.
enum
{
	UCMDF_BUTTONS		= 0x01,
	UCMDF_PITCH			= 0x02,
	UCMDF_YAW			= 0x04,
	UCMDF_FORWARDMOVE	= 0x08,
	UCMDF_SIDEMOVE		= 0x10,
	UCMDF_UPMOVE		= 0x20,
	UCMDF_ROLL			= 0x40,
};

// When changing the following enum, be sure to update Net_SkipCommand()
// and Net_DoCommand() in d_net.cpp.
enum EDemoCommand
{
	DEM_BAD,			//  0 Bad command
	DEM_USERCMD,		//  1 Player movement
	DEM_EMPTYUSERCMD,	//  2 Equivalent to [DEM_USERCMD, 0]
	DEM_UNDONE2,		//  3
	DEM_MUSICCHANGE,	//  4 Followed by name of new music
	DEM_PRINT,			//  5 Print string to console
	DEM_CENTERPRINT,	//  6 Print string to middle of screen
	DEM_STOP,			//  7 End of demo
	DEM_UINFCHANGED,	//  8 User info changed
	DEM_SINFCHANGED,	//  9 Server/Host info changed
	DEM_GENERICCHEAT,	// 10 Next byte is cheat to apply (see next enum)
	DEM_GIVECHEAT,		// 11 String: item to give, Word: quantity
	DEM_SAY,			// 12 Byte: who to talk to, String: message to display
	DEM_DROPPLAYER,		// 13 Not implemented, takes a byte
	DEM_CHANGEMAP,		// 14 Name of map to change to
	DEM_SUICIDE,		// 15 Player wants to die
	DEM_ADDBOT,			// 16 Byte: botshift, String: userinfo for bot, 4 Bytes: skill (aiming, perfection, reaction, isp)
	DEM_KILLBOTS,		// 17 Remove all bots from the world
	DEM_INVUSEALL,		// 18 Use every item (panic!)
	DEM_INVUSE,			// 19 4 bytes: ID of item to use
	DEM_PAUSE,			// 20 Pause game
	DEM_SAVEGAME,		// 21 String: Filename, String: Description
	DEM_UNDONE3,		// 22 
	DEM_UNDONE4,		// 23
	DEM_UNDONE5,		// 24
	DEM_UNDONE6,		// 25
	DEM_SUMMON,			// 26 String: Thing to fabricate
	DEM_FOV,			// 27 Byte: New FOV for all players
	DEM_MYFOV,			// 28 Byte: New FOV for this player
	DEM_CHANGEMAP2,		// 29 Byte: Position in new map, String: name of new map
	DEM_UNDONE7,		// 30
	DEM_UNDONE8,		// 31
	DEM_RUNSCRIPT,		// 32 Word: Script#, Byte: # of args; each arg is a 4-byte int
	DEM_SINFCHANGEDXOR,	// 33 Like DEM_SINFCHANGED, but data is a byte indicating how to set a bit
	DEM_INVDROP,		// 34 4 bytes: ID of item to drop
	DEM_WARPCHEAT,		// 35 4 bytes: 2 for x, 2 for y
	DEM_CENTERVIEW,		// 36
	DEM_SUMMONFRIEND,	// 37 String: Thing to fabricate
	DEM_SPRAY,			// 38 String: The decal to spray
	DEM_CROUCH,			// 39
	DEM_RUNSCRIPT2,		// 40 Same as DEM_RUNSCRIPT, but always executes
	DEM_CHECKAUTOSAVE,	// 41 Check if the user has autosaves enabled. Ignored for demoplayback.
	DEM_DOAUTOSAVE,		// 42 An autosave should be made
	DEM_MORPHEX,		// 43 String: The class to morph to.
	DEM_SUMMONFOE,		// 44 String: Thing to fabricate
	DEM_UNDONE9,		// 45
	DEM_UNDONE10,		// 46
	DEM_TAKECHEAT,		// 47 String: item to take, Word: quantity
	DEM_ADDCONTROLLER,	// 48 Player to add to the controller list.
	DEM_DELCONTROLLER,	// 49 Player to remove from the controller list.
	DEM_KILLCLASSCHEAT,	// 50 String: Class to kill.
	DEM_UNDONE11,		// 51
	DEM_SUMMON2,		// 52 String: Thing to fabricate, WORD: angle offset
	DEM_SUMMONFRIEND2,	// 53
	DEM_SUMMONFOE2,		// 54
	DEM_ADDSLOTDEFAULT,	// 55
	DEM_ADDSLOT,		// 56
	DEM_SETSLOT,		// 57
	DEM_SUMMONMBF,		// 58
	DEM_CONVREPLY,		// 59 Word: Dialogue node, Byte: Reply number
	DEM_CONVCLOSE,		// 60
	DEM_CONVNULL,		// 61
	DEM_RUNSPECIAL,		// 62 Word: Special number, Byte: Arg count, Ints: Args
	DEM_SETPITCHLIMIT,	// 63 Byte: Up limit, Byte: Down limit (in degrees)
	DEM_ADVANCEINTER,	// 64 Advance intermission screen state
	DEM_RUNNAMEDSCRIPT,	// 65 String: Script name, Byte: Arg count + Always flag; each arg is a 4-byte int
	DEM_REVERTCAMERA,	// 66
	DEM_SETSLOTPNUM,	// 67 Byte: player number, the rest is the same as DEM_SETSLOT
	DEM_REMOVE,			// 68
	DEM_FINISHGAME,		// 69
};

// The following are implemented by cht_DoCheat in m_cheat.cpp
enum ECheatCommand
{
	CHT_GOD,
	CHT_NOCLIP,
	CHT_NOTARGET,
	CHT_CHAINSAW,
	CHT_IDKFA,
	CHT_IDFA,
	CHT_BEHOLDV,
	CHT_BEHOLDS,
	CHT_BEHOLDI,
	CHT_BEHOLDR,
	CHT_BEHOLDA,
	CHT_BEHOLDL,
	CHT_PUMPUPI,
	CHT_PUMPUPM,
	CHT_PUMPUPT,
	CHT_PUMPUPH,
	CHT_PUMPUPP,
	CHT_PUMPUPS,
	CHT_IDDQD,			// Same as CHT_GOD, but sets health
	CHT_MASSACRE,
	CHT_CHASECAM,
	CHT_FLY,
	CHT_MORPH,
	CHT_POWER,
	CHT_HEALTH,
	CHT_KEYS,
	CHT_TAKEWEAPS,
	CHT_NOWUDIE,
	CHT_ALLARTI,
	CHT_PUZZLE,
	CHT_MDK,			// Kill actor player is aiming at
	CHT_ANUBIS,
	CHT_NOVELOCITY,
	CHT_DONNYTRUMP,
	CHT_LEGO,
	CHT_RESSURECT,		// [GRB]
	CHT_CLEARFROZENPROPS,
	CHT_FREEZE,
	CHT_GIMMIEA,
	CHT_GIMMIEB,
	CHT_GIMMIEC,
	CHT_GIMMIED,
	CHT_GIMMIEE,
	CHT_GIMMIEF,
	CHT_GIMMIEG,
	CHT_GIMMIEH,
	CHT_GIMMIEI,
	CHT_GIMMIEJ,
	CHT_GIMMIEZ,
	CHT_BUDDHA,
	CHT_NOCLIP2,
	CHT_BUDDHA2,
	CHT_GOD2
};

void StartChunk (int id, BYTE **stream);
void FinishChunk (BYTE **stream);
void SkipChunk (BYTE **stream);

int UnpackUserCmd (usercmd_t *ucmd, const usercmd_t *basis, BYTE **stream);
int PackUserCmd (const usercmd_t *ucmd, const usercmd_t *basis, BYTE **stream);
int WriteUserCmdMessage (usercmd_t *ucmd, const usercmd_t *basis, BYTE **stream);

struct ticcmd_t;

int SkipTicCmd (BYTE **stream, int count);
void ReadTicCmd (BYTE **stream, int player, int tic);
void RunNetSpecs (int player, int buf);

int ReadByte (BYTE **stream);
int ReadWord (BYTE **stream);
int ReadLong (BYTE **stream);
float ReadFloat (BYTE **stream);
char *ReadString (BYTE **stream);
const char *ReadStringConst(BYTE **stream);
void WriteByte (BYTE val, BYTE **stream);
void WriteWord (short val, BYTE **stream);
void WriteLong (int val, BYTE **stream);
void WriteFloat (float val, BYTE **stream);
void WriteString (const char *string, BYTE **stream);

#endif //__D_PROTOCOL_H__
