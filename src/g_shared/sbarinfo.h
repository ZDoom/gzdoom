#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"
#include "v_collection.h"

class FBarTexture;
class FScanner;

struct SBarInfoCommand; //we need to be able to use this before it is defined.

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
	FBarTexture *bar;
	SBarInfoBlock subBlock; //for type SBarInfo_CMD_GAMEMODE
};

struct SBarInfo
{
	TArray<FString> Images;
	SBarInfoBlock huds[6];
	bool automapbar;
	bool interpolateHealth;
	int interpolationSpeed;
	int height;
	int gameType;

	int GetGameType() { return gameType; }
	void ParseSBarInfo(int lump);
	void ParseSBarInfoBlock(FScanner &sc, SBarInfoBlock &block);
	void getCoordinates(FScanner &sc, SBarInfoCommand &cmd); //retrieves the next two arguments as x and y.
	int newImage(const char* patchname);
	void Init();
	EColorRange GetTranslation(FScanner &sc, char* translation);
	SBarInfo();
	SBarInfo(int lumpnum);
	~SBarInfo();
};

extern SBarInfo *SBarInfoScript;

void FreeSBarInfoScript();

#endif //__SBarInfo_SBAR_H__
