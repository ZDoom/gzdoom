#pragma once

#include <stdint.h>
#include "tarray.h"

// This structure is used by BuildTranslations() to hold color information.
struct TranslationParm
{
	short RangeStart;	// First level for this range
	short RangeEnd;		// Last level for this range
	uint8_t Start[3];		// Start color for this range
	uint8_t End[3];		// End color for this range
};

struct TempParmInfo
{
	unsigned int StartParm[2];
	unsigned int ParmLen[2];
	int Index;
};
struct TempColorInfo
{
	FName Name;
	unsigned int ParmInfo;
	PalEntry LogColor;
};

struct TranslationMap
{
	FName Name;
	int Number;
};

extern TArray<TranslationParm> TranslationParms[2];
extern TArray<TranslationMap> TranslationLookup;
extern TArray<PalEntry> TranslationColors;
extern uint16_t lowerforupper[65536];
extern uint16_t upperforlower[65536];

class FImageSource;

void RecordTextureColors (FImageSource *pic, uint32_t *usedcolors);
bool myislower(int code);
int stripaccent(int code);
