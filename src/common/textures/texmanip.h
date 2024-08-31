#pragma once

#include "palentry.h"

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


