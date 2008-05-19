/*
** sbarinfo.h
**
** Header for custom status bar definitions.
**
**---------------------------------------------------------------------------
** Copyright 2008 Braden Obrzut
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

#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"
#include "v_collection.h"

#define NUMHUDS 9
#define NUMPOPUPS 3

class FBarTexture;
class FScanner;

struct SBarInfoCommand; //we need to be able to use this before it is defined.
struct MugShotState;

//Popups!
enum PopupTransition
{
	TRANSITION_NONE,
	TRANSITION_SLIDEINBOTTOM,
	TRANSITION_FADE,
};

struct Popup
{
	PopupTransition transition;
	bool opened;
	bool moving;
	int height;
	int width;
	int speed;
	int speed2;
	int alpha;
	int x;
	int y;

	Popup();
	void init();
	void tick();
	void open();
	void close();
	bool isDoneMoving();
	int getXOffset();
	int getYOffset();
	int getAlpha(int maxAlpha=FRACUNIT);
};

//SBarInfo
struct SBarInfoBlock
{
	TArray<SBarInfoCommand> commands;
	bool forceScaled;
	int alpha;

	SBarInfoBlock();
};

struct SBarInfoCommand
{
	SBarInfoCommand();
	~SBarInfoCommand();
	void setString(FScanner &sc, const char* source, int strnum, int maxlength=-1, bool exact=false);

	int type;
	int special;
	int special2;
	int special3;
	int special4;
	int flags;
	int x;
	int y;
	int value;
	int sprite;
	FString string[2];
	FFont *font;
	EColorRange translation;
	EColorRange translation2;
	EColorRange translation3;
	SBarInfoBlock subBlock; //for type SBarInfo_CMD_GAMEMODE
};

struct SBarInfo
{
	TArray<FString> Images;
	SBarInfoBlock huds[NUMHUDS];
	Popup popups[NUMPOPUPS];
	bool automapbar;
	bool interpolateHealth;
	bool interpolateArmor;
	bool completeBorder;
	bool lowerHealthCap;
	char spacingCharacter;
	int interpolationSpeed;
	int armorInterpolationSpeed;
	int height;
	int gameType;

	int GetGameType() { return gameType; }
	void ParseSBarInfo(int lump);
	void ParseSBarInfoBlock(FScanner &sc, SBarInfoBlock &block);
	void ParseMugShotBlock(FScanner &sc, MugShotState &state);
	void getCoordinates(FScanner &sc, SBarInfoCommand &cmd); //retrieves the next two arguments as x and y.
	int getSignedInteger(FScanner &sc); //returns a signed integer.
	int newImage(const char* patchname);
	void Init();
	EColorRange GetTranslation(FScanner &sc, char* translation);
	SBarInfo();
	SBarInfo(int lumpnum);
	~SBarInfo();
	static void Load();
};

extern SBarInfo *SBarInfoScript;

//Mug Shot scripting structs.
struct MugShotState;

struct MugShotFrame
{
	TArray<FString> graphic;
	int delay;

	MugShotFrame();
	~MugShotFrame();
	FTexture *getTexture(FString &defaultFace, FPlayerSkin *skn, int random, int level=0, int direction=0, bool usesLevels=false, bool health2=false, bool healthspecial=false, bool directional=false);
};


struct MugShotState
{
	bool usesLevels;
	bool health2; //health level is the 2nd character from the end.
	bool healthspecial; //like health2 only the 2nd frame gets the normal health type.
	bool directional; //faces direction of damage.

	unsigned int position;
	int time;
	int random;
	bool finished;
	FName state;
	TArray<MugShotFrame> frames;

	MugShotState();
	MugShotState(FString name);
	~MugShotState();
	void tick();
	void reset();
	MugShotFrame getCurrentFrame() { return frames[position]; }
	FTexture *getCurrentFrameTexture(FString &defaultFace, FPlayerSkin *skn, int level=0, int direction=0) { return getCurrentFrame().getTexture(defaultFace, skn, random, level, direction, usesLevels, health2, healthspecial, directional); }
};

extern TArray<MugShotState> MugShotStates;

const MugShotState *FindMugShotState(FString state);
int FindMugShotStateIndex(FName state);

// Enums used between the parser and the display
enum //statusbar flags
{
	STATUSBARFLAG_FORCESCALED = 1,
};

enum //gametype flags
{
	GAMETYPE_SINGLEPLAYER = 1,
	GAMETYPE_COOPERATIVE = 2,
	GAMETYPE_DEATHMATCH = 4,
	GAMETYPE_TEAMGAME = 8,
};

enum //drawimage flags
{
	DRAWIMAGE_PLAYERICON = 1,
	DRAWIMAGE_AMMO1 = 2,
	DRAWIMAGE_AMMO2 = 4,
	DRAWIMAGE_INVENTORYICON = 8,
	DRAWIMAGE_TRANSLATABLE = 16,
	DRAWIMAGE_WEAPONSLOT = 32,
	DRAWIMAGE_SWITCHABLE_AND = 64,
	DRAWIMAGE_INVULNERABILITY = 128,
	DRAWIMAGE_OFFSET_CENTER = 256,
	DRAWIMAGE_ARMOR = 512,
	DRAWIMAGE_WEAPONICON = 1024,
	DRAWIMAGE_SIGIL = 2048,
};

enum //drawnumber flags
{
	DRAWNUMBER_HEALTH = 0x1,
	DRAWNUMBER_ARMOR = 0x2,
	DRAWNUMBER_AMMO1 = 0x4,
	DRAWNUMBER_AMMO2 = 0x8,
	DRAWNUMBER_AMMO = 0x10,
	DRAWNUMBER_AMMOCAPACITY = 0x20,
	DRAWNUMBER_FRAGS = 0x40,
	DRAWNUMBER_INVENTORY = 0x80,
	DRAWNUMBER_KILLS = 0x100,
	DRAWNUMBER_MONSTERS = 0x200,
	DRAWNUMBER_ITEMS = 0x400,
	DRAWNUMBER_TOTALITEMS = 0x800,
	DRAWNUMBER_SECRETS = 0x1000,
	DRAWNUMBER_TOTALSECRETS = 0x2000,
	DRAWNUMBER_ARMORCLASS = 0x4000,
	DRAWNUMBER_GLOBALVAR = 0x8000,
	DRAWNUMBER_GLOBALARRAY = 0x10000,
	DRAWNUMBER_FILLZEROS = 0x20000,
	DRAWNUMBER_WHENNOTZERO = 0x40000,
};

enum //drawbar flags (will go into special2)
{
	DRAWBAR_HORIZONTAL = 1,
	DRAWBAR_REVERSE = 2,
	DRAWBAR_COMPAREDEFAULTS = 4,
};

enum //drawselectedinventory flags
{
	DRAWSELECTEDINVENTORY_ALTERNATEONEMPTY = 1,
	DRAWSELECTEDINVENTORY_ARTIFLASH = 2,
	DRAWSELECTEDINVENTORY_ALWAYSSHOWCOUNTER = 4,
};

enum //drawinventorybar flags
{
	DRAWINVENTORYBAR_ALWAYSSHOW = 1,
	DRAWINVENTORYBAR_NOARTIBOX = 2,
	DRAWINVENTORYBAR_NOARROWS = 4,
	DRAWINVENTORYBAR_ALWAYSSHOWCOUNTER = 8,
};

enum //drawgem flags
{
	DRAWGEM_WIGGLE = 1,
	DRAWGEM_TRANSLATABLE = 2,
	DRAWGEM_ARMOR = 4,
	DRAWGEM_REVERSE = 8,
};

enum //drawshader flags
{
	DRAWSHADER_VERTICAL = 1,
	DRAWSHADER_REVERSE = 2,
};

enum //drawmugshot flags
{
	DRAWMUGSHOT_XDEATHFACE = 1,
	DRAWMUGSHOT_ANIMATEDGODMODE = 2,
};

enum //drawkeybar flags
{
	DRAWKEYBAR_VERTICAL = 1,
};

enum //event flags
{
	SBARINFOEVENT_NOT = 1,
	SBARINFOEVENT_OR = 2,
	SBARINFOEVENT_AND = 4,
};

enum //aspect ratios
{
	ASPECTRATIO_4_3 = 0,
	ASPECTRATIO_16_9 = 1,
	ASPECTRATIO_16_10 = 2,
	ASPECTRATIO_5_4 = 3,
};

enum //Key words
{
	SBARINFO_BASE,
	SBARINFO_HEIGHT,
	SBARINFO_INTERPOLATEHEALTH,
	SBARINFO_INTERPOLATEARMOR,
	SBARINFO_COMPLETEBORDER,
	SBARINFO_MONOSPACEFONTS,
	SBARINFO_LOWERHEALTHCAP,
	SBARINFO_STATUSBAR,
	SBARINFO_MUGSHOT,
	SBARINFO_CREATEPOPUP,
};

enum //Bar types
{
	STBAR_NONE,
	STBAR_FULLSCREEN,
	STBAR_NORMAL,
	STBAR_AUTOMAP,
	STBAR_INVENTORY,
	STBAR_INVENTORYFULLSCREEN,
	STBAR_POPUPLOG,
	STBAR_POPUPKEYS,
	STBAR_POPUPSTATUS,
};

enum //Bar key words
{
	SBARINFO_DRAWIMAGE,
	SBARINFO_DRAWNUMBER,
	SBARINFO_DRAWSWITCHABLEIMAGE,
	SBARINFO_DRAWMUGSHOT,
	SBARINFO_DRAWSELECTEDINVENTORY,
	SBARINFO_DRAWINVENTORYBAR,
	SBARINFO_DRAWBAR,
	SBARINFO_DRAWGEM,
	SBARINFO_DRAWSHADER,
	SBARINFO_DRAWSTRING,
	SBARINFO_DRAWKEYBAR,
	SBARINFO_GAMEMODE,
	SBARINFO_PLAYERCLASS,
	SBARINFO_ASPECTRATIO,
	SBARINFO_ISSELECTED,
	SBARINFO_USESSECONDARYAMMO,
	SBARINFO_HASWEAPONPIECE,
	SBARINFO_WEAPONAMMO,
	SBARINFO_ININVENTORY,
};

//All this so I can change the mugshot state in ACS...
class FBarShader : public FTexture
{
public:
	FBarShader(bool vertical, bool reverse);
	const BYTE *GetColumn(unsigned int column, const Span **spans_out);
	const BYTE *GetPixels();
	void Unload();
private:
	BYTE Pixels[512];
	Span DummySpan[2];
};

class DSBarInfo : public DBaseStatusBar
{
	DECLARE_CLASS(DSBarInfo, DBaseStatusBar)
public:
	DSBarInfo();
	~DSBarInfo();
	void Draw(EHudState state);
	void NewGame();
	void AttachToPlayer(player_t *player);
	void Tick();
	void ReceivedWeapon (AWeapon *weapon);
	void FlashItem(const PClass *itemtype);
	void ShowPop(int popnum);
	void SetMugShotState(const char* stateName, bool waitTillDone=false);
private:
	void doCommands(SBarInfoBlock &block, int xOffset=0, int yOffset=0, int alpha=FRACUNIT);
	void DrawGraphic(FTexture* texture, int x, int y, int xOffset, int yOffset, int alpha, bool translate=false, bool dim=false, bool center=false);
	void DrawString(const char* str, int x, int y, int xOffset, int yOffset, int alpha, EColorRange translation, int spacing=0);
	void DrawNumber(int num, int len, int x, int y, int xOffset, int yOffset, int alpha, EColorRange translation, int spacing=0, bool fillzeros=false);
	void DrawFace(FString &defaultFace, int accuracy, bool xdth, bool animatedgodmode, int x, int y, int xOffset, int yOffset, int alpha);
	int updateState(bool xdth, bool animatedgodmode);
	void DrawInventoryBar(int type, int num, int x, int y, int xOffset, int yOffset, int alpha, bool alwaysshow,
		int counterx, int countery, EColorRange translation, bool drawArtiboxes, bool noArrows, bool alwaysshowcounter);
	void DrawGem(FTexture* chain, FTexture* gem, int value, int x, int y, int xOffset, int yOffset, int alpha, int padleft, int padright, int chainsize,
		bool wiggle, bool translate);
	FRemapTable* getTranslation();

	FImageCollection Images;
	FPlayerSkin *oldSkin;
	FFont *drawingFont;
	FString lastPrefix;
	MugShotState *currentState;
	bool weaponGrin;
	bool damageFaceActive;
	bool mugshotNormal;
	bool ouchActive;
	int lastDamageAngle;
	int rampageTimer;
	int oldHealth;
	int oldArmor;
	int mugshotHealth;
	int chainWiggle;
	int artiflash;
	int pendingPopup;
	int currentPopup;
	unsigned int invBarOffset;
	FBarShader shader_horz_normal;
	FBarShader shader_horz_reverse;
	FBarShader shader_vert_normal;
	FBarShader shader_vert_reverse;
};

#endif //__SBarInfo_SBAR_H__
