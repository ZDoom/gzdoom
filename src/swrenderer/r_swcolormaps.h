#pragma once


#include "g_levellocals.h"

struct FSWColormap
{
	uint8_t *Maps = nullptr;
	PalEntry Color = 0xffffffff;
	PalEntry Fade = 0xff000000;
	int Desaturate = 0;
};

struct FDynamicColormap : FSWColormap
{
	void ChangeFade (PalEntry fadecolor);
	void ChangeColor (PalEntry lightcolor, int desaturate);
	void ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor);
	void ChangeFogDensity(int newdensity);
	void BuildLights ();
	static void RebuildAllLights();

	FDynamicColormap *Next;
};

extern FSWColormap realcolormaps;					// [RH] make the colormaps externally visible

extern FDynamicColormap NormalLight;
extern FDynamicColormap FullNormalLight;
extern bool NormalLightHasFixedLights;
extern TArray<FSWColormap> SpecialSWColormaps;

void InitSWColorMaps();
FDynamicColormap *GetSpecialLights (PalEntry lightcolor, PalEntry fadecolor, int desaturate);
void SetDefaultColormap (const char *name);


// Give the compiler a strong hint we want these functions inlined:
#ifndef FORCEINLINE
#if defined(_MSC_VER)
#define FORCEINLINE __forceinline
#elif defined(__GNUC__)
#define FORCEINLINE __attribute__((always_inline)) inline
#else
#define FORCEINLINE inline
#endif
#endif

// MSVC needs the forceinline here.
FORCEINLINE FDynamicColormap *GetColorTable(const FColormap &cm, PalEntry SpecialColor = 0xffffff)
{
	PalEntry c =  SpecialColor.Modulate(cm.LightColor);

	// First colormap is the default Doom colormap.
	// testcolor and testfade CCMDs modifies the first colormap, so we have to do the check without looking at the actual values stored in NormalLight.
	if (c == PalEntry(255, 255, 255) && cm.FadeColor == 0 && cm.Desaturation == 0)
		return &NormalLight;

	return GetSpecialLights(c, cm.FadeColor, cm.Desaturation);
}

FORCEINLINE FDynamicColormap *GetSpriteColorTable(const FColormap &cm, PalEntry SpecialColor, bool nocoloredspritelighting)
{
	PalEntry c;
	if (!nocoloredspritelighting) c =  SpecialColor.Modulate(cm.LightColor);
	else
	{
		c = cm.LightColor;
		c.Decolorize();
		c = SpecialColor.Modulate(c);
	}
	
	// First colormap is the default Doom colormap.
	// testcolor and testfade CCMDs modifies the first colormap, so we have to do the check without looking at the actual values stored in NormalLight.
	if (c == PalEntry(255, 255, 255) && cm.FadeColor == 0 && cm.Desaturation == 0)
		return &NormalLight;
	
	return GetSpecialLights(c, cm.FadeColor, cm.Desaturation);
}
