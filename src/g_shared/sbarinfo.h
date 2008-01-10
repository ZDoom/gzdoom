#ifndef __SBarInfo_SBAR_H__
#define __SBarInfo_SBAR_H__

#include "tarray.h"
#include "v_collection.h"

struct SBarInfoCommand; //we need to be able to use this before it is defined.

struct SBarInfoBlock
{
	TArray<SBarInfoCommand> commands;
	bool forceScaled;
	SBarInfoBlock();
};

struct SBarInfoCommand
{
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
	void setString(const char* source, int strnum, int maxlength=-1, bool exact=false);
	SBarInfoCommand();
};

struct SBarInfo
{
	TArray<FString> Images;
	SBarInfoBlock huds[6];
	bool automapbar;
	bool interpolateHealth;
	int interpolationSpeed;
	int height;
	int ParseSBarInfo(int lump);
	void ParseSBarInfoBlock(SBarInfoBlock &block);
	int newImage(const char* patchname);
	EColorRange GetTranslation(char* translation);
	SBarInfo();
};

extern SBarInfo *SBarInfoScript;

#endif //__SBarInfo_SBAR_H__
