#pragma once

#include <stdint.h>
#include "palentry.h"

// for internal use
struct FColormap
{
	PalEntry		LightColor;		// a is saturation (0 full, 31=b/w, other=custom colormap)
	PalEntry		FadeColor;		// a is fadedensity>>1
	uint8_t			Desaturation;
	uint8_t			BlendFactor;	// This is for handling Legacy-style colormaps which use a different formula to calculate how the color affects lighting.
	uint16_t		FogDensity;

	void Clear()
	{
		LightColor = 0xffffff;
		FadeColor = 0;
		Desaturation = 0;
		BlendFactor = 0;
		FogDensity = 0;
	}

	void MakeWhite()
	{
		LightColor = 0xffffff;
	}

	void ClearColor()
	{
		LightColor = 0xffffff;
		BlendFactor = 0;
		Desaturation = 0;
	}

	void CopyLight(FColormap &from)
	{
		LightColor = from.LightColor;
		Desaturation = from.Desaturation;
		BlendFactor = from.BlendFactor;
	}

	void CopyFog(FColormap &from)
	{
		FadeColor = from.FadeColor;
		FogDensity = from.FogDensity;
	}

	void Decolorize()
	{
		LightColor.Decolorize();
	}

	bool operator == (const FColormap &other)
	{
		return LightColor == other.LightColor && FadeColor == other.FadeColor && Desaturation == other.Desaturation &&
			BlendFactor == other.BlendFactor && FogDensity == other.FogDensity;
	}

	bool operator != (const FColormap &other)
	{
		return !operator==(other);
	}

};


