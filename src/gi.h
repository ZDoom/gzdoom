/*
** gi.h
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

#ifndef __GI_H__
#define __GI_H__

#include "basictypes.h"

#define GI_MAPxx				0x00000001
#define GI_PAGESARERAW			0x00000002
#define GI_SHAREWARE			0x00000004
#define GI_NOLOOPFINALEMUSIC	0x00000008
#define GI_INFOINDEXED			0x00000010
#define GI_MENUHACK				0x00000060
#define GI_MENUHACK_RETAIL		0x00000020
#define GI_MENUHACK_EXTENDED	0x00000040	// (Heretic)
#define GI_MENUHACK_COMMERCIAL	0x00000060
#define GI_ALWAYSFALLINGDAMAGE	0x00000080
#define GI_TEASER2				0x00000100	// Alternate version of the Strife Teaser
#define GI_CHEX_QUEST			0x00000200

#ifndef EGAMETYPE
#define EGAMETYPE
enum EGameType
{
	GAME_Any	 = 0,
	GAME_Doom	 = 1,
	GAME_Heretic = 2,
	GAME_Hexen	 = 4,
	GAME_Strife	 = 8,
	GAME_Chex	 = 16, //Chex is basically Doom, but we need to have a different set of actors.

	GAME_Raven			= GAME_Heretic|GAME_Hexen,
	GAME_DoomStrife		= GAME_Doom|GAME_Strife,
	GAME_DoomChex		= GAME_Doom|GAME_Chex,
	GAME_DoomStrifeChex	= GAME_Doom|GAME_Strife|GAME_Chex
};
#endif

extern const char *GameNames[17];

typedef struct
{
	BYTE offset;
	BYTE size;
	char tl[8];
	char t[8];
	char tr[8];
	char l[8];
	char r[8];
	char bl[8];
	char b[8];
	char br[8];
} gameborder_t;

typedef struct
{
	int flags;
	char titlePage[9];
	char creditPage1[9];
	char creditPage2[9];
	char titleMusic[9];
	float titleTime;
	float advisoryTime;
	float pageTime;
	char chatSound[16];
	char finaleMusic[9];
	char finaleFlat[9];
	char finalePage1[9];
	char finalePage2[9];
	char finalePage3[9];
	union
	{
		char infoPage[2][9];
		struct
		{
			char basePage[9];
			char numPages;
		} indexed;
	} info;
	const char *quitSound;
	int maxSwitch;
	char borderFlat[9];
	gameborder_t *border;
	int telefogheight;
	EGameType gametype;
	int defKickback;
	char SkyFlatName[9];
	fixed_t StepHeight;
	const char *translator;
	const char *mapinfo[2];
	DWORD defaultbloodcolor;
	DWORD defaultbloodparticlecolor;
	const char *backpacktype;
	const char *statusbar;
} gameinfo_t;

extern gameinfo_t gameinfo;

#endif //__GI_H__
