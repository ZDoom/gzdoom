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

#include "basics.h"
#include "zstring.h"
#include "name.h"
#include "screenjob.h"

// Flags are not user configurable and only depend on the standard IWADs
enum
{
	GI_MAPxx				= 0x00000001,
	GI_SHAREWARE			= 0x00000002,
	GI_MENUHACK_EXTENDED	= 0x00000004,	// (Heretic)
	GI_TEASER2				= 0x00000008,	// Alternate version of the Strife Teaser
	GI_COMPATSHORTTEX		= 0x00000010,	// always force COMPAT_SHORTTEX for IWAD maps.
	GI_COMPATSTAIRS			= 0x00000020,	// same for stairbuilding
	GI_COMPATPOLY1			= 0x00000040,	// Hexen's MAP36 needs old polyobject drawing
	GI_COMPATPOLY2			= 0x00000080,	// so does HEXDD's MAP47
	GI_IGNORETITLEPATCHES	= 0x00000200,	// Ignore the map name graphics when not runnning in English language
	GI_NOSECTIONMERGE		= 0x00000400,	// For the original id IWADs: avoid merging sections due to how idbsp created its sectors.
};

#include "gametype.h"

extern const char *GameNames[17];

struct staticgameborder_t
{
	uint8_t offset;
	uint8_t size;
	char tl[8];
	char t[8];
	char tr[8];
	char l[8];
	char r[8];
	char bl[8];
	char b[8];
	char br[8];
};

struct gameborder_t
{
	uint8_t offset;
	uint8_t size;
	FString tl;
	FString t;
	FString tr;
	FString l;
	FString r;
	FString bl;
	FString b;
	FString br;

	gameborder_t &operator=(staticgameborder_t &other)
	{
		offset = other.offset;
		size = other.size;
		tl = other.tl;
		t = other.t;
		tr = other.tr;
		l = other.l;
		r = other.r;
		bl = other.bl;
		b = other.b;
		br = other.br;
		return *this;
	}
};

struct FGIFont
{
	FName fontname;
	FName color;
};

struct gameinfo_t
{
	int flags;
	EGameType gametype;
	FString ConfigName;

	FString TitlePage;
	bool nokeyboardcheats;
	bool drawreadthis;
	bool noloopfinalemusic;
	bool intermissioncounter;
	bool nightmarefast;
	bool swapmenu;
	bool dontcrunchcorpses;
	bool correctprintbold;
	bool forcetextinmenus;
	bool forcenogfxsubstitution;
	TArray<FName> creditPages;
	TArray<FName> finalePages;
	TArray<FName> infoPages;
	TArray<FName> DefaultWeaponSlots[10];
	TArray<FName> PlayerClasses;

	TArray<FName> PrecachedClasses;
	TArray<FString> PrecachedTextures;
	TArray<FSoundID> PrecachedSounds;
	TArray<FString> EventHandlers;

	FString titleMusic;
	int titleOrder;
	float titleTime;
	float advisoryTime;
	float pageTime;
	FString chatSound;
	FString finaleMusic;
	int finaleOrder;
	FString FinaleFlat;
	FString BorderFlat;
	FString SkyFlatName;
	FString ArmorIcon1;
	FString ArmorIcon2;
	FName BasicArmorClass;
	FName HexenArmorClass;
	FString PauseSign;
	FString Endoom;
	double Armor2Percent;
	FString quitSound;
	gameborder_t Border;
	double telefogheight;
	int defKickback;
	FString translator;
	uint32_t defaultbloodcolor;
	uint32_t defaultbloodparticlecolor;
	FString statusbar;
	int statusbarfile = -1;
	FName statusbarclass;
	FName althudclass;
	int statusbarclassfile = -1;
	FName MessageBoxClass;
	FName HelpMenuClass;
	FName MenuDelegateClass;
	FName backpacktype;
	FString intermissionMusic;
	int intermissionOrder;
	FString CursorPic;
	uint32_t dimcolor;
	float dimamount;
	float bluramount;
	int definventorymaxamount;
	int defaultrespawntime;
	int defaultdropstyle;
	uint32_t pickupcolor;
	TArray<FString> quitmessages;
	FName mTitleColor;
	FName mFontColor;
	FName mFontColorValue;
	FName mFontColorMore;
	FName mFontColorHeader;
	FName mFontColorHighlight;
	FName mFontColorSelection;
	FName mSliderColor;
	FName mSliderBackColor;
	FString mBackButton;
	double gibfactor;
	int TextScreenX;
	int TextScreenY;
	FName DefaultConversationMenuClass;
	FName DefaultEndSequence;
	FString mMapArrow, mCheatMapArrow;
	FString mEasyKey, mCheatKey;
	FString Dialogue;
	TArray<FString> AddDialogues;
	FGIFont mStatscreenMapNameFont;
	FGIFont mStatscreenFinishedFont;
	FGIFont mStatscreenEnteringFont;
	FGIFont mStatscreenContentFont;
	FGIFont mStatscreenAuthorFont;
	bool norandomplayerclass;
	bool forcekillscripts;
	FName statusscreen_single;
	FName statusscreen_coop;
	FName statusscreen_dm;
	int healthpic;	// These get filled in from ALTHUDCF
	int berserkpic;
	double normforwardmove[2];
	double normsidemove[2];
	int fullscreenautoaspect = 3;
	bool nomergepickupmsg;
	bool mHideParTimes;
	CutsceneDef IntroScene;
	double BloodSplatDecalDistance;

	const char *GetFinalePage(unsigned int num) const;
};


extern gameinfo_t gameinfo;

inline const char *GameTypeName()
{
	return GameNames[gameinfo.gametype];
}

bool CheckGame(const char *string, bool chexisdoom);

#endif //__GI_H__
