#ifndef __R_TRANSLATE_H
#define __R_TRANSLATE_H

#include "doomtype.h"
#include "tarray.h"
#include "palettecontainer.h"

class FSerializer;

enum
{
	TRANSLATION_Players = 1,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,
	TRANSLATION_Decorate,
	TRANSLATION_Blood,
	TRANSLATION_RainPillar,
	TRANSLATION_Custom,

	NUM_TRANSLATION_TABLES
};


enum EStandardTranslations
{
	STD_Ice = 7,
};

#define MAX_ACS_TRANSLATIONS		65535
#define MAX_DECORATE_TRANSLATIONS	65535

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables (void);

void R_BuildPlayerTranslation (int player);		// [RH] Actually create a player's translation table.
void R_GetPlayerTranslation (int color, const struct FPlayerColorSet *colorset, class FPlayerSkin *skin, struct FRemapTable *table);

FTranslationID CreateBloodTranslation(PalEntry color);

FTranslationID R_FindCustomTranslation(FName name);
void R_ParseTrnslate();
void StaticSerializeTranslations(FSerializer& arc);



#endif // __R_TRANSLATE_H
