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
#include "sc_man.h"
#include "w_wad.h"
#include "i_system.h"
#include "v_video.h"
#include "g_level.h"

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

// Custom GAMEINFO ------------------------------------------------------------

const char* GameInfoBoarders[] =
{
	"DoomBorder",
	"HereticBorder",
	"StrifeBorder",
	NULL
};

#define GAMEINFOKEY_CSTRING(key, variable, length) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		if(strlen(sc.String) > length) \
		{ \
			sc.ScriptError("Value for '%s' can not be longer than %d characters.", #key, length); \
		} \
		strcpy(gameinfo.key, sc.String); \
	}

#define GAMEINFOKEY_STRINGARRAY(key, variable, length) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		gameinfo.key.Clear(); \
		do \
		{ \
			sc.MustGetToken(TK_StringConst); \
			if(length > 0 && strlen(sc.String) > length) \
			{ \
				sc.ScriptError("Value for '%s' can not be longer than %d characters.", #key, length); \
			} \
			gameinfo.key[gameinfo.key.Reserve(1)] = sc.String; \
		} \
		while (sc.CheckToken(',')); \
	}

#define GAMEINFOKEY_STRING(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.key = sc.String; \
	}

#define GAMEINFOKEY_INT(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetNumber(); \
 		gameinfo.key = sc.Number; \
	}

#define GAMEINFOKEY_FLOAT(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetFloat(); \
		gameinfo.key = static_cast<float> (sc.Float); \
	}

#define GAMEINFOKEY_FIXED(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetFloat(); \
		gameinfo.key = static_cast<int> (sc.Float*FRACUNIT); \
	}

#define GAMEINFOKEY_COLOR(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		FString color = sc.String; \
		FString colorName = V_GetColorStringByName(color); \
		if(!colorName.IsEmpty()) \
			color = colorName; \
		gameinfo.key = V_GetColorFromString(NULL, color); \
	}

#define GAMEINFOKEY_BOOL(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		if(sc.CheckToken(TK_False)) \
			gameinfo.key = false; \
		else \
		{ \
			sc.MustGetToken(TK_True); \
			gameinfo.key = true; \
		} \
	}

void FMapInfoParser::ParseGameInfo()
{
	sc.MustGetToken('{');
	while(sc.GetToken())
	{
		if (sc.TokenType == '}') return;

		sc.TokenMustBe(TK_Identifier);
		FString nextKey = sc.String;
		sc.MustGetToken('=');

		if (nextKey.CompareNoCase("weaponslot") == 0)
		{
			sc.MustGetToken(TK_IntConst);
			if (sc.Number < 0 || sc.Number >= 10)
			{
				sc.ScriptError("Weapon slot index must be in range [0..9].\n");
			}
			int i = sc.Number;
			gameinfo.DefaultWeaponSlots[i].Clear();
			sc.MustGetToken(',');
			do
			{
				sc.MustGetString();
				FName val = sc.String;
				gameinfo.DefaultWeaponSlots[i].Push(val);

			}
			while (sc.CheckToken(','));
		}
		else if(nextKey.CompareNoCase("border") == 0)
		{
			if(sc.CheckToken(TK_Identifier))
			{
				switch(sc.MustMatchString(GameInfoBoarders))
				{
					default:
						gameinfo.border = &DoomBorder;
						break;
					case 1:
						gameinfo.border = &HereticBorder;
						break;
					case 2:
						gameinfo.border = &StrifeBorder;
						break;
				}
			}
			else
			{
				// border = {size, offset, tr, t, tl, r, l ,br, b, bl};
				char *graphics[8] = {DoomBorder.tr, DoomBorder.t, DoomBorder.tl, DoomBorder.r, DoomBorder.l, DoomBorder.br, DoomBorder.b, DoomBorder.bl};
				sc.MustGetToken(TK_IntConst);
				DoomBorder.offset = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				DoomBorder.size = sc.Number;
				for(int i = 0;i < 8;i++)
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					int len = int(strlen(sc.String));
					if(len > 8)
						sc.ScriptError("Border graphic can not be more than 8 characters long.\n");
					memcpy(graphics[i], sc.String, len);
					if(len < 8) // end with a null byte if the string is less than 8 chars.
						graphics[i][len] = 0;
				}
			}
		}
		else if(nextKey.CompareNoCase("armoricons") == 0)
		{
			sc.MustGetToken(TK_StringConst);
			strncpy(gameinfo.ArmorIcon1, sc.String, 8);
			gameinfo.ArmorIcon1[8] = 0;
			if (sc.CheckToken(','))
			{
				sc.MustGetToken(TK_FloatConst);
				gameinfo.Armor2Percent = FLOAT2FIXED(sc.Float);
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				strncpy(gameinfo.ArmorIcon2, sc.String, 8);
				gameinfo.ArmorIcon2[8] = 0;
			}
		}
		// Insert valid keys here.
		GAMEINFOKEY_CSTRING(titlePage, "titlePage", 8)
		GAMEINFOKEY_STRINGARRAY(creditPages, "creditPage", 8)
		GAMEINFOKEY_STRING(titleMusic, "titleMusic")
		GAMEINFOKEY_FLOAT(titleTime, "titleTime")
		GAMEINFOKEY_FLOAT(advisoryTime, "advisoryTime")
		GAMEINFOKEY_FLOAT(pageTime, "pageTime")
		GAMEINFOKEY_STRING(chatSound, "chatSound")
		GAMEINFOKEY_STRING(finaleMusic, "finaleMusic")
		GAMEINFOKEY_CSTRING(finaleFlat, "finaleFlat", 8)
		GAMEINFOKEY_STRINGARRAY(finalePages, "finalePage", 8)
		GAMEINFOKEY_STRINGARRAY(infoPages, "infoPage", 8)
		GAMEINFOKEY_STRING(quitSound, "quitSound")
		GAMEINFOKEY_CSTRING(borderFlat, "borderFlat", 8)
		GAMEINFOKEY_FIXED(telefogheight, "telefogheight")
		GAMEINFOKEY_INT(defKickback, "defKickback")
		GAMEINFOKEY_CSTRING(SkyFlatName, "SkyFlatName", 8)
		GAMEINFOKEY_STRING(translator, "translator")
		GAMEINFOKEY_COLOR(pickupcolor, "pickupcolor")
		GAMEINFOKEY_COLOR(defaultbloodcolor, "defaultbloodcolor")
		GAMEINFOKEY_COLOR(defaultbloodparticlecolor, "defaultbloodparticlecolor")
		GAMEINFOKEY_STRING(backpacktype, "backpacktype")
		GAMEINFOKEY_STRING(statusbar, "statusbar")
		GAMEINFOKEY_STRING(intermissionMusic, "intermissionMusic")
		GAMEINFOKEY_BOOL(noloopfinalemusic, "noloopfinalemusic")
		GAMEINFOKEY_BOOL(drawreadthis, "drawreadthis")
		GAMEINFOKEY_BOOL(intermissioncounter, "intermissioncounter")
		GAMEINFOKEY_COLOR(dimcolor, "dimcolor")
		GAMEINFOKEY_FLOAT(dimamount, "dimamount")
		GAMEINFOKEY_INT(definventorymaxamount, "definventorymaxamount")
		GAMEINFOKEY_INT(defaultrespawntime, "defaultrespawntime")
		GAMEINFOKEY_INT(defaultrespawntime, "defaultrespawntime")
		GAMEINFOKEY_INT(defaultdropstyle, "defaultdropstyle")
		GAMEINFOKEY_CSTRING(Endoom, "endoom", 8)
		GAMEINFOKEY_INT(player5start, "player5start")
		GAMEINFOKEY_STRINGARRAY(quitmessages, "quitmessages", 0)
		GAMEINFOKEY_STRING(mTitleColor, "menufontcolor_title")
		GAMEINFOKEY_STRING(mFontColor, "menufontcolor_label")
		GAMEINFOKEY_STRING(mFontColorValue, "menufontcolor_value")
		GAMEINFOKEY_STRING(mFontColorMore, "menufontcolor_action")
		GAMEINFOKEY_STRING(mFontColorHeader, "menufontcolor_header")
		GAMEINFOKEY_STRING(mFontColorHighlight, "menufontcolor_highlight")
		GAMEINFOKEY_STRING(mFontColorSelection, "menufontcolor_selection")
		GAMEINFOKEY_CSTRING(mBackButton, "menubackbutton", 8)

		else
		{
			// ignore unkown keys.
			sc.UnGet();
			SkipToNext();
		}
	}
}

const char *gameinfo_t::GetFinalePage(unsigned int num) const
{
	if (finalePages.Size() == 0) return "-NOFLAT-";
	else if (num < 1 || num > finalePages.Size()) return finalePages[0];
	else return finalePages[num-1];
}
