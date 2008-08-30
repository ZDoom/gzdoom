/*
** gi.cpp
** Holds same game-dependant info
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

#include <stdlib.h>
#include "info.h"
#include "gi.h"
#include "m_fixed.h"
#include "v_palette.h"

gameinfo_t gameinfo;

const char *GameNames[17] =
{
	NULL, "Doom", "Heretic", NULL, "Hexen", NULL, NULL, NULL, "Strife", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "Chex"
};


static gameborder_t DoomBorder =
{
	8, 8,
	"brdr_tl", "brdr_t", "brdr_tr",
	"brdr_l",			 "brdr_r",
	"brdr_bl", "brdr_b", "brdr_br"
};

static gameborder_t HereticBorder =
{
	4, 16,
	"bordtl", "bordt", "bordtr",
	"bordl",           "bordr",
	"bordbl", "bordb", "bordbr"
};

static gameborder_t StrifeBorder =
{
	8, 8,
	"brdr_tl", "brdr_t", "brdr_tr",
	"brdr_l",			 "brdr_r",
	"brdr_bl", "brdr_b", "brdr_br"
};

gameinfo_t HexenGameInfo =
{
	GI_PAGESARERAW | GI_MAPxx | GI_NOLOOPFINALEMUSIC | GI_INFOINDEXED | GI_ALWAYSFALLINGDAMAGE,
	"TITLE",
	"CREDIT",
	"CREDIT",
	"HEXEN",
	280/35,
	210/35,
	200/35,
	"Chat",
	"hub",
	"-NOFLAT",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", {4} } },
	NULL,
	33,
	"F_022",
	&HereticBorder,
	32*FRACUNIT,
	GAME_Hexen,
	150,
	"F_SKY",
	24*FRACUNIT,
	"xlat/heretic.txt",	// not really correct but this was used before.
	{ "mapinfo/hexen.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t HexenDKGameInfo =
{
	GI_PAGESARERAW | GI_MAPxx | GI_NOLOOPFINALEMUSIC | GI_INFOINDEXED | GI_ALWAYSFALLINGDAMAGE,
	"TITLE",
	"CREDIT",
	"CREDIT",
	"HEXEN",
	280/35,
	210/35,
	200/35,
	"Chat",
	"hub",
	"-NOFLAT-",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", {4} } },
	NULL,
	33,
	"F_022",
	&HereticBorder,
	32*FRACUNIT,
	GAME_Hexen,
	150,
	"F_SKY",
	24*FRACUNIT,
	"xlat/heretic.txt",	// not really correct but this was used before.
	{ "mapinfo/hexen.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t HereticGameInfo =
{
	GI_PAGESARERAW | GI_INFOINDEXED,
	"TITLE",
	"CREDIT",
	"CREDIT",
	"MUS_TITL",
	280/35,
	210/35,
	200/35,
	"misc/chat",
	"MUS_CPTD",
	"FLOOR25",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", {4} } },
	NULL,
	17,
	"FLAT513",
	&HereticBorder,
	32*FRACUNIT,
	GAME_Heretic,
	150,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/heretic.txt",
	{ "mapinfo/heretic.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t HereticSWGameInfo =
{
	GI_PAGESARERAW | GI_SHAREWARE | GI_INFOINDEXED,
	"TITLE",
	"CREDIT",
	"ORDER",
	"MUS_TITL",
	280/35,
	210/35,
	200/35,
	"misc/chat",
	"MUS_CPTD",
	"FLOOR25",
	"ORDER",
	"CREDIT",
	"CREDIT",
	{ { "TITLE", {5} } },
	NULL,
	17,
	"FLOOR04",
	&HereticBorder,
	32*FRACUNIT,
	GAME_Heretic,
	150,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/heretic.txt",
	{ "mapinfo/heretic.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t SharewareGameInfo =
{
	GI_SHAREWARE,
	"TITLEPIC",
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"HELP2",
	"VICTORY2",
	"HELP2",
	{ { "HELP1", "HELP2" } },
	"menu/quit1",
	1,
	"FLOOR7_2",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/doom1.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t RegisteredGameInfo =
{
	0,
	"TITLEPIC",
	"CREDIT",
	"HELP2",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"HELP2",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "HELP2" } },
	"menu/quit1",
	2,
	"FLOOR7_2",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/doom1.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t ChexGameInfo =
{
	GI_CHEX_QUEST,
	"TITLEPIC",
	"CREDIT",
	"HELP1",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"HELP2",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "CREDIT" } },
	"menu/quit1",
	2,
	"FLOOR7_2",
	&DoomBorder,
	0,
	GAME_Chex,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/chex.txt", NULL },
	MAKERGB(95,175,87),
};

gameinfo_t RetailGameInfo =
{
	GI_MENUHACK_RETAIL,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_INTRO",
	5,
	0,
	200/35,
	"misc/chat2",
	"D_VICTOR",
	"FLOOR4_8",
	"CREDIT",
	"VICTORY2",
	"ENDPIC",
	{ { "HELP1", "CREDIT" } },
	"menu/quit1",
	2,
	"FLOOR7_2",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/doom1.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t CommercialGameInfo =
{
	GI_MAPxx | GI_MENUHACK_COMMERCIAL,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_DM2TTL",
	11,
	0,
	200/35,
	"misc/chat",
	"D_READ_M",
	"SLIME16",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "HELP", "CREDIT" } },
	"menu/quit2",
	3,
	"GRNROCK",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/doom2.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t PlutoniaGameInfo =
{
	GI_MAPxx | GI_MENUHACK_COMMERCIAL,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_DM2TTL",
	11,
	0,
	200/35,
	"misc/chat",
	"D_READ_M",
	"SLIME16",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "HELP", "CREDIT" } },
	"menu/quit2",
	3,
	"GRNROCK",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/plutonia.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t TNTGameInfo =
{
	GI_MAPxx | GI_MENUHACK_COMMERCIAL,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_DM2TTL",
	11,
	0,
	200/35,
	"misc/chat",
	"D_READ_M",
	"SLIME16",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "HELP", "CREDIT" } },
	"menu/quit2",
	3,
	"GRNROCK",
	&DoomBorder,
	0,
	GAME_Doom,
	100,
	"F_SKY1",
	24*FRACUNIT,
	"xlat/doom.txt",
	{ "mapinfo/doomcommon.txt", "mapinfo/tnt.txt" },
	MAKERGB(100,0,0),
};

gameinfo_t StrifeGameInfo =
{
	GI_MAPxx | GI_INFOINDEXED | GI_ALWAYSFALLINGDAMAGE,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_LOGO",
	280/35,
	0,
	200/35,
	"misc/chat",
	"d_intro",
	"-NOFLAT",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "CREDIT", {4} } },
	NULL,
	49,
	"F_PAVE01",
	&StrifeBorder,
	0,
	GAME_Strife,
	150,
	"F_SKY001",
	16*FRACUNIT,
	"xlat/strife.txt",
	{ "mapinfo/strife.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t StrifeTeaserGameInfo =
{
	GI_MAPxx | GI_INFOINDEXED | GI_ALWAYSFALLINGDAMAGE | GI_SHAREWARE,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_LOGO",
	280/35,
	0,
	200/35,
	"misc/chat",
	"d_intro",
	"-NOFLAT",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "CREDIT", {4} } },
	NULL,
	49,
	"F_PAVE01",
	&StrifeBorder,
	0,
	GAME_Strife,
	150,
	"F_SKY001",
	16*FRACUNIT,
	"xlat/strife.txt",
	{ "mapinfo/strife.txt", NULL },
	MAKERGB(100,0,0),
};

gameinfo_t StrifeTeaser2GameInfo =
{
	GI_MAPxx | GI_INFOINDEXED | GI_ALWAYSFALLINGDAMAGE | GI_SHAREWARE | GI_TEASER2,
	"TITLEPIC",
	"CREDIT",
	"CREDIT",
	"D_LOGO",
	280/35,
	0,
	200/35,
	"misc/chat",
	"d_intro",
	"-NOFLAT",
	"CREDIT",
	"CREDIT",
	"CREDIT",
	{ { "CREDIT", {4} } },
	NULL,
	49,
	"F_PAVE01",
	&StrifeBorder,
	0,
	GAME_Strife,
	150,
	"F_SKY001",
	16*FRACUNIT,
	"xlat/strife.txt",
	{ "mapinfo/strife.txt", NULL },
	MAKERGB(100,0,0),
};
