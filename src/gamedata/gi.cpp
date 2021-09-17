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
#include "sc_man.h"
#include "filesystem.h"
#include "v_video.h"
#include "g_level.h"
#include "vm.h"
#include "c_cvars.h"

gameinfo_t gameinfo;

EXTERN_CVAR(Float, turbo)


DEFINE_GLOBAL(gameinfo)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, backpacktype)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, Armor2Percent)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, ArmorIcon1)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, ArmorIcon2)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, gametype)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, norandomplayerclass)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, infoPages)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mBackButton)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mStatscreenMapNameFont)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mStatscreenEnteringFont)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mStatscreenFinishedFont)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mStatscreenContentFont)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mStatscreenAuthorFont)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, gibfactor)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, intermissioncounter)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, statusscreen_single)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, statusscreen_coop)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, statusscreen_dm)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mSliderColor)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mSliderBackColor)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, defaultbloodcolor)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, telefogheight)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, defKickback)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, healthpic)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, berserkpic)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, defaultdropstyle)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, normforwardmove)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, normsidemove)
DEFINE_FIELD_X(GameInfoStruct, gameinfo_t, mHideParTimes)

const char *GameNames[17] =
{
	NULL, "Doom", "Heretic", NULL, "Hexen", NULL, NULL, NULL, "Strife", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "Chex"
};


static staticgameborder_t DoomBorder =
{
	8, 8,
	"brdr_tl", "brdr_t", "brdr_tr",
	"brdr_l",			 "brdr_r",
	"brdr_bl", "brdr_b", "brdr_br"
};

static staticgameborder_t HereticBorder =
{
	4, 16,
	"bordtl", "bordt", "bordtr",
	"bordl",           "bordr",
	"bordbl", "bordb", "bordbr"
};

static staticgameborder_t StrifeBorder =
{
	8, 8,
	"brdr_tl", "brdr_t", "brdr_tr",
	"brdr_l",			 "brdr_r",
	"brdr_bl", "brdr_b", "brdr_br"
};

// Custom GAMEINFO ------------------------------------------------------------

const char* GameInfoBorders[] =
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

#define GAMEINFOKEY_STRINGARRAY(key, variable, length, clear) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		if (clear) gameinfo.key.Clear(); \
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
#define GAMEINFOKEY_SOUNDARRAY(key, variable, length, clear) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		if (clear) gameinfo.key.Clear(); \
		do \
		{ \
			sc.MustGetToken(TK_StringConst); \
			if(length > 0 && strlen(sc.String) > length) \
			{ \
				sc.ScriptError("Value for '%s' can not be longer than %d characters.", #key, length); \
			} \
			gameinfo.key[gameinfo.key.Reserve(1)] = FSoundID(sc.String); \
		} \
		while (sc.CheckToken(',')); \
	}

#define GAMEINFOKEY_STRING(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.key = sc.String; \
	}

#define GAMEINFOKEY_STRING_STAMPED(key, variable, stampvar) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.key = sc.String; \
		gameinfo.stampvar = fileSystem.GetFileContainer(sc.LumpNum); \
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

#define GAMEINFOKEY_DOUBLE(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetFloat(); \
		gameinfo.key = sc.Float; \
	}

#define GAMEINFOKEY_TWODOUBLES(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetValue(true); \
		gameinfo.key[0] = sc.Float; \
		sc.MustGetToken(','); \
		sc.MustGetValue(true); \
		gameinfo.key[1] = sc.Float; \
	}

#define GAMEINFOKEY_COLOR(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		FString color = sc.String; \
		FString colorName = V_GetColorStringByName(color); \
		if(!colorName.IsEmpty()) \
			color = colorName; \
		gameinfo.key = V_GetColorFromString(color); \
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

#define GAMEINFOKEY_FONT(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.key.fontname = sc.String; \
		if (sc.CheckToken(',')) { \
			sc.MustGetToken(TK_StringConst); \
			gameinfo.key.color = sc.String; \
		} else { \
			gameinfo.key.color = NAME_None; \
		} \
	}

#define GAMEINFOKEY_PATCH(key, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.key.fontname = sc.String; \
		gameinfo.key.color = NAME_Null; \
	}

#define GAMEINFOKEY_MUSIC(key, order, variable) \
	else if(nextKey.CompareNoCase(variable) == 0) \
	{ \
		sc.MustGetToken(TK_StringConst); \
		gameinfo.order = 0; \
		char *colon = strchr (sc.String, ':'); \
		if (colon) \
		{ \
			gameinfo.order = atoi(colon+1); \
			*colon = 0; \
		} \
		gameinfo.key = sc.String; \
	}


void FMapInfoParser::ParseGameInfo()
{
	sc.MustGetToken('{');
	while(sc.GetToken())
	{
		if (sc.TokenType == '}') break;

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
			staticgameborder_t *b;
			if (sc.CheckToken(TK_Identifier))
			{
				switch(sc.MustMatchString(GameInfoBorders))
				{
					default:
						b = &DoomBorder;
						break;
					case 1:
						b = &HereticBorder;
						break;
					case 2:
						b = &StrifeBorder;
						break;
				}
				gameinfo.Border = *b;
			}
			else
			{
				// border = {size, offset, tr, t, tl, r, l ,br, b, bl};
				FString *graphics[8] = { &gameinfo.Border.tr, &gameinfo.Border.t, &gameinfo.Border.tl, &gameinfo.Border.r, &gameinfo.Border.l, &gameinfo.Border.br, &gameinfo.Border.b, &gameinfo.Border.bl };
				sc.MustGetToken(TK_IntConst);
				gameinfo.Border.offset = sc.Number;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_IntConst);
				gameinfo.Border.size = sc.Number;
				for(int i = 0;i < 8;i++)
				{
					sc.MustGetToken(',');
					sc.MustGetToken(TK_StringConst);
					(*graphics[i]) = sc.String;
				}
			}
		}
		else if(nextKey.CompareNoCase("armoricons") == 0)
		{
			sc.MustGetToken(TK_StringConst);
			gameinfo.ArmorIcon1 = sc.String;
			if (sc.CheckToken(','))
			{
				sc.MustGetToken(TK_FloatConst);
				gameinfo.Armor2Percent = sc.Float;
				sc.MustGetToken(',');
				sc.MustGetToken(TK_StringConst);
				gameinfo.ArmorIcon2 = sc.String;
			}
		}
		else if(nextKey.CompareNoCase("maparrow") == 0)
		{
			sc.MustGetToken(TK_StringConst);
			gameinfo.mMapArrow = sc.String;
			if (sc.CheckToken(','))
			{
				sc.MustGetToken(TK_StringConst);
				gameinfo.mCheatMapArrow = sc.String;
			}
			else gameinfo.mCheatMapArrow = "";
		}
		else if (nextKey.CompareNoCase("dialogue") == 0)
		{
			sc.MustGetToken(TK_StringConst);
			gameinfo.Dialogue = sc.String;
			gameinfo.AddDialogues.Clear();
		}
		// Insert valid keys here.
		GAMEINFOKEY_STRING(mCheatKey, "cheatKey")
			GAMEINFOKEY_STRING(mEasyKey, "easyKey")
			GAMEINFOKEY_STRING(TitlePage, "titlePage")
			GAMEINFOKEY_STRINGARRAY(creditPages, "addcreditPage", 8, false)
			GAMEINFOKEY_STRINGARRAY(creditPages, "CreditPage", 8, true)
			GAMEINFOKEY_STRINGARRAY(PlayerClasses, "addplayerclasses", 0, false)
			GAMEINFOKEY_STRINGARRAY(PlayerClasses, "playerclasses", 0, true)
			GAMEINFOKEY_MUSIC(titleMusic, titleOrder, "titleMusic")
			GAMEINFOKEY_FLOAT(titleTime, "titleTime")
			GAMEINFOKEY_FLOAT(advisoryTime, "advisoryTime")
			GAMEINFOKEY_FLOAT(pageTime, "pageTime")
			GAMEINFOKEY_STRING(chatSound, "chatSound")
			GAMEINFOKEY_MUSIC(finaleMusic, finaleOrder, "finaleMusic")
			GAMEINFOKEY_STRING(FinaleFlat, "finaleFlat")
			GAMEINFOKEY_STRINGARRAY(finalePages, "finalePage", 8, true)
			GAMEINFOKEY_STRINGARRAY(infoPages, "addinfoPage", 8, false)
			GAMEINFOKEY_STRINGARRAY(infoPages, "infoPage", 8, true)
			GAMEINFOKEY_STRINGARRAY(PrecachedClasses, "precacheclasses", 0, false)
			GAMEINFOKEY_STRINGARRAY(PrecachedTextures, "precachetextures", 0, false)
			GAMEINFOKEY_SOUNDARRAY(PrecachedSounds, "precachesounds", 0, false)
			GAMEINFOKEY_STRINGARRAY(EventHandlers, "addeventhandlers", 0, false)
			GAMEINFOKEY_STRINGARRAY(EventHandlers, "eventhandlers", 0, true)
			GAMEINFOKEY_STRING(PauseSign, "pausesign")
			GAMEINFOKEY_STRING(quitSound, "quitSound")
			GAMEINFOKEY_STRING(BorderFlat, "borderFlat")
			GAMEINFOKEY_DOUBLE(telefogheight, "telefogheight")
			GAMEINFOKEY_DOUBLE(gibfactor, "gibfactor")
			GAMEINFOKEY_INT(defKickback, "defKickback")
			GAMEINFOKEY_INT(fullscreenautoaspect, "fullscreenautoaspect")
			GAMEINFOKEY_STRING(SkyFlatName, "SkyFlatName")
			GAMEINFOKEY_STRING(translator, "translator")
			GAMEINFOKEY_COLOR(pickupcolor, "pickupcolor")
			GAMEINFOKEY_COLOR(defaultbloodcolor, "defaultbloodcolor")
			GAMEINFOKEY_COLOR(defaultbloodparticlecolor, "defaultbloodparticlecolor")
			GAMEINFOKEY_STRING(backpacktype, "backpacktype")
			GAMEINFOKEY_STRING_STAMPED(statusbar, "statusbar", statusbarfile)
			GAMEINFOKEY_STRING_STAMPED(statusbarclass, "statusbarclass", statusbarclassfile)
			GAMEINFOKEY_STRING(althudclass, "althudclass")
			GAMEINFOKEY_MUSIC(intermissionMusic, intermissionOrder, "intermissionMusic")
			GAMEINFOKEY_STRING(CursorPic, "CursorPic")
			GAMEINFOKEY_STRING(MessageBoxClass, "MessageBoxClass")
			GAMEINFOKEY_BOOL(noloopfinalemusic, "noloopfinalemusic")
			GAMEINFOKEY_BOOL(drawreadthis, "drawreadthis")
			GAMEINFOKEY_BOOL(swapmenu, "swapmenu")
			GAMEINFOKEY_BOOL(dontcrunchcorpses, "dontcrunchcorpses")
			GAMEINFOKEY_BOOL(correctprintbold, "correctprintbold")
			GAMEINFOKEY_BOOL(forcetextinmenus, "forcetextinmenus")
			GAMEINFOKEY_BOOL(forcenogfxsubstitution, "forcenogfxsubstitution")
			GAMEINFOKEY_BOOL(intermissioncounter, "intermissioncounter")
			GAMEINFOKEY_BOOL(nightmarefast, "nightmarefast")
			GAMEINFOKEY_COLOR(dimcolor, "dimcolor")
			GAMEINFOKEY_FLOAT(dimamount, "dimamount")
			GAMEINFOKEY_FLOAT(bluramount, "bluramount")
			GAMEINFOKEY_STRING(mSliderColor, "menuslidercolor")
			GAMEINFOKEY_STRING(mSliderBackColor, "menusliderbackcolor")
			GAMEINFOKEY_INT(definventorymaxamount, "definventorymaxamount")
			GAMEINFOKEY_INT(defaultrespawntime, "defaultrespawntime")
			GAMEINFOKEY_INT(defaultdropstyle, "defaultdropstyle")
			GAMEINFOKEY_STRING(Endoom, "endoom")
			GAMEINFOKEY_STRINGARRAY(quitmessages, "addquitmessages", 0, false)
			GAMEINFOKEY_STRINGARRAY(quitmessages, "quitmessages", 0, true)
			GAMEINFOKEY_STRING(mTitleColor, "menufontcolor_title")
			GAMEINFOKEY_STRING(mFontColor, "menufontcolor_label")
			GAMEINFOKEY_STRING(mFontColorValue, "menufontcolor_value")
			GAMEINFOKEY_STRING(mFontColorMore, "menufontcolor_action")
			GAMEINFOKEY_STRING(mFontColorHeader, "menufontcolor_header")
			GAMEINFOKEY_STRING(mFontColorHighlight, "menufontcolor_highlight")
			GAMEINFOKEY_STRING(mFontColorSelection, "menufontcolor_selection")
			GAMEINFOKEY_STRING(mBackButton, "menubackbutton")
			GAMEINFOKEY_INT(TextScreenX, "textscreenx")
			GAMEINFOKEY_INT(TextScreenY, "textscreeny")
			GAMEINFOKEY_STRING(DefaultEndSequence, "defaultendsequence")
			GAMEINFOKEY_STRING(DefaultConversationMenuClass, "defaultconversationmenuclass")
			GAMEINFOKEY_FONT(mStatscreenMapNameFont, "statscreen_mapnamefont")
			GAMEINFOKEY_FONT(mStatscreenFinishedFont, "statscreen_finishedfont")
			GAMEINFOKEY_FONT(mStatscreenEnteringFont, "statscreen_enteringfont")
			GAMEINFOKEY_FONT(mStatscreenContentFont, "statscreen_contentfont")
			GAMEINFOKEY_FONT(mStatscreenAuthorFont, "statscreen_authorfont")
			GAMEINFOKEY_BOOL(norandomplayerclass, "norandomplayerclass")
			GAMEINFOKEY_BOOL(forcekillscripts, "forcekillscripts") // [JM] Force kill scripts on thing death. (MF7_NOKILLSCRIPTS overrides.)
			GAMEINFOKEY_STRINGARRAY(AddDialogues, "adddialogues", 0, false)
			GAMEINFOKEY_STRING(statusscreen_single, "statscreen_single")
			GAMEINFOKEY_STRING(statusscreen_coop, "statscreen_coop")
			GAMEINFOKEY_STRING(statusscreen_dm, "statscreen_dm")
			GAMEINFOKEY_TWODOUBLES(normforwardmove, "normforwardmove")
			GAMEINFOKEY_TWODOUBLES(normsidemove, "normsidemove")
			GAMEINFOKEY_BOOL(nomergepickupmsg, "nomergepickupmsg")
			GAMEINFOKEY_BOOL(mHideParTimes, "hidepartimes")

		else
		{
			DPrintf(DMSG_ERROR, "Unknown GAMEINFO key \"%s\" found in %s:%i\n", nextKey.GetChars(), sc.ScriptName.GetChars(), sc.Line);

			// ignore unkown keys.
			sc.UnGet();
			SkipToNext();
		}
	}
	turbo.Callback();
}

const char *gameinfo_t::GetFinalePage(unsigned int num) const
{
	if (finalePages.Size() == 0) return "-NOFLAT-";
	else if (num < 1 || num > finalePages.Size()) return finalePages[0].GetChars();
	else return finalePages[num-1].GetChars();
}

bool CheckGame(const char* string, bool chexisdoom)
{
	int test = gameinfo.gametype;
	if (test == GAME_Chex && chexisdoom) test = GAME_Doom;
	return !stricmp(string, GameNames[test]);
}
