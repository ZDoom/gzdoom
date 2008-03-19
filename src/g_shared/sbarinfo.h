#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"
#include "v_collection.h"

class FBarTexture;
class FScanner;

struct SBarInfoCommand; //we need to be able to use this before it is defined.
struct MugShotState;

struct SBarInfoBlock
{
	TArray<SBarInfoCommand> commands;
	bool forceScaled;
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
	SBarInfoBlock huds[6];
	bool automapbar;
	bool interpolateHealth;
	bool interpolateArmor;
	bool completeBorder;
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

void FreeSBarInfoScript();

//Mug Shot scripting structs.  All functions are defined in sbarinfo_parser.cpp
struct MugShotState;

struct MugShotFrame
{
	TArray<FString> graphic;
	int delay;

	MugShotFrame();
	~MugShotFrame();
	FTexture *getTexture(FPlayerSkin *skn, int random, int level=0, int direction=0, bool usesLevels=false, bool health2=false, bool healthspecial=false, bool directional=false);
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
	FTexture *getCurrentFrameTexture(FPlayerSkin *skn, int level=0, int direction=0) { return getCurrentFrame().getTexture(skn, random, level, direction, usesLevels, health2, healthspecial, directional); }
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
};

enum //drawnumber flags
{
	DRAWNUMBER_HEALTH = 1,
	DRAWNUMBER_ARMOR = 2,
	DRAWNUMBER_AMMO1 = 4,
	DRAWNUMBER_AMMO2 = 8,
	DRAWNUMBER_AMMO = 16,
	DRAWNUMBER_AMMOCAPACITY = 32,
	DRAWNUMBER_FRAGS = 64,
	DRAWNUMBER_INVENTORY = 128,
	DRAWNUMBER_KILLS = 256,
	DRAWNUMBER_MONSTERS = 512,
	DRAWNUMBER_ITEMS = 1024,
	DRAWNUMBER_TOTALITEMS = 2048,
	DRAWNUMBER_SECRETS = 4096,
	DRAWNUMBER_TOTALSECRETS = 8192,
	DRAWNUMBER_ARMORCLASS = 16384,
};

enum //drawbar flags (will go into special2)
{
	DRAWBAR_HORIZONTAL = 1,
	DRAWBAR_REVERSE = 2,
	DRAWBAR_COMPAREDEFAULTS = 4,
	DRAWBAR_KEEPOFFSETS = 8,
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

enum //Key words
{
	SBARINFO_BASE,
	SBARINFO_HEIGHT,
	SBARINFO_INTERPOLATEHEALTH,
	SBARINFO_INTERPOLATEARMOR,
	SBARINFO_COMPLETEBORDER,
	SBARINFO_STATUSBAR,
	SBARINFO_MUGSHOT,
};

enum //Bar types
{
	STBAR_NONE,
	STBAR_FULLSCREEN,
	STBAR_NORMAL,
	STBAR_AUTOMAP,
	STBAR_INVENTORY,
	STBAR_INVENTORYFULLSCREEN,
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
	SBARINFO_WEAPONAMMO,
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
	void SetMugShotState(const char* stateName, bool waitTillDone=false);
private:
	void doCommands(SBarInfoBlock &block);
	void DrawGraphic(FTexture* texture, int x, int y, int flags);
	void DrawString(const char* str, int x, int y, EColorRange translation, int spacing=0);
	void DrawNumber(int num, int len, int x, int y, EColorRange translation, int spacing=0);
	void DrawFace(int accuracy, bool xdth, bool animatedgodmode, int x, int y);
	int updateState(bool xdth, bool animatedgodmode);
	void DrawInventoryBar(int type, int num, int x, int y, bool alwaysshow, 
		int counterx, int countery, EColorRange translation, bool drawArtiboxes, bool noArrows, bool alwaysshowcounter);
	void DrawGem(FTexture* chain, FTexture* gem, int value, int x, int y, int padleft, int padright, int chainsize, 
		bool wiggle, bool translate);
	FRemapTable* getTranslation();

	FImageCollection Images;
	FPlayerSkin *oldSkin;
	FFont *drawingFont;
	FString lastPrefix;
	MugShotState *currentState;
	bool weaponGrin;
	bool damageFaceActive;
	int lastDamageAngle;
	int rampageTimer;
	int oldHealth;
	int oldArmor;
	int mugshotHealth;
	int chainWiggle;
	int artiflash;
	unsigned int invBarOffset;
	FBarShader shader_horz_normal;
	FBarShader shader_horz_reverse;
	FBarShader shader_vert_normal;
	FBarShader shader_vert_reverse;
};

#endif //__SBarInfo_SBAR_H__
