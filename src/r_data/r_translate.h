#ifndef __R_TRANSLATE_H
#define __R_TRANSLATE_H

#include "doomtype.h"
#include "tarray.h"
#include "palettecontainer.h"

class FSerializer;

enum
{
	TRANSLATION_Invalid,
	TRANSLATION_Players,
	TRANSLATION_PlayersExtra,
	TRANSLATION_Standard,
	TRANSLATION_LevelScripted,
	TRANSLATION_Decals,
	TRANSLATION_PlayerCorpses,
	TRANSLATION_Decorate,
	TRANSLATION_Blood,
	TRANSLATION_RainPillar,
	TRANSLATION_Custom,
	TRANSLATION_Font,

	NUM_TRANSLATION_TABLES
};


enum EStandardTranslations
{
	STD_Ice = 7,
	STD_Gray = 8,		// a 0-255 gray ramp
	STD_Grayscale = 9,	// desaturated version of the palette.
};

#define MAX_ACS_TRANSLATIONS		65535
#define MAX_DECORATE_TRANSLATIONS	65535

// Initialize color translation tables, for player rendering etc.
void R_InitTranslationTables (void);

void R_BuildPlayerTranslation (int player);		// [RH] Actually create a player's translation table.
void R_GetPlayerTranslation (int color, const struct FPlayerColorSet *colorset, class FPlayerSkin *skin, struct FRemapTable *table);

extern const uint8_t IcePalette[16][3];

int CreateBloodTranslation(PalEntry color);

int R_FindCustomTranslation(FName name);
void R_ParseTrnslate();
void StaticSerializeTranslations(FSerializer& arc);

struct TextureManipulation
{
	enum
	{
		BlendNone = 0,
		BlendAlpha = 1,
		BlendScreen = 2,
		BlendOverlay = 3,
		BlendHardLight = 4,
		BlendMask = 7,
		InvertBit = 8,
		ActiveBit = 16,	// Must be set for the shader to do something
	};
	PalEntry AddColor;		// Alpha contains the blend flags
	PalEntry ModulateColor;	// Alpha may contain a multiplier to get higher values than 1.0 without promoting this to 4 full floats.
	PalEntry BlendColor;
	float DesaturationFactor;

	bool CheckIfEnabled()	// check if this manipulation is doing something. NoOps do not need to be preserved, unless they override older setttings.
	{
		if (AddColor != 0 ||	// this includes a check for the blend mode without which BlendColor is not active
			ModulateColor != 0x01ffffff ||	// 1 in alpha must be the default for a no-op.
			DesaturationFactor != 0)
		{
			AddColor.a |= ActiveBit; // mark as active for the shader's benefit.
			return true;
		}
		return false;
	}

	void SetTextureModulateColor(int slot, PalEntry rgb)
	{
		rgb.a = ModulateColor.a;
		ModulateColor = rgb;
	}

	void SetTextureModulateScaleFactor(int slot, int fac)
	{
		ModulateColor.a = (uint8_t)fac;
	}

	void SetTextureAdditiveColor(int slot, PalEntry rgb)
	{
		rgb.a = AddColor.a;
		AddColor = rgb;
	}

	void SetTextureBlendColor(int slot, PalEntry rgb)
	{
		BlendColor = rgb;
	}

	void SetTextureDesaturationFactor(int slot, double fac)
	{
		DesaturationFactor = (float)fac;
	}

	void SetTextureBlendMode(int slot, int mode)
	{
		AddColor.a = (AddColor.a & ~TextureManipulation::BlendMask) | (mode & TextureManipulation::BlendMask);
	}

	void SetTextureInvert(bool on)
	{
		AddColor.a |= TextureManipulation::InvertBit;
		AddColor.a &= ~TextureManipulation::InvertBit;
	}

};




#endif // __R_TRANSLATE_H
