#ifndef __RES_CMAP_H
#define __RES_CMAP_H

#include "doomtype.h"

struct lightlist_t;

void R_InitColormaps ();
void R_DeinitColormaps ();

uint32_t R_ColormapNumForName(const char *name);	// killough 4/4/98
void R_SetDefaultColormap (const char *name);	// [RH] change normal fadetable
uint32_t R_BlendForColormap (uint32_t map);			// [RH] return calculated blend for a colormap

struct FakeCmap 
{
	char name[8];
	PalEntry blend;
	int lump;
};

extern TArray<FakeCmap> fakecmaps;

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

	void CopyFrom3DLight(lightlist_t *light);

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



// For hardware-accelerated weapon sprites in colored sectors
struct FColormapStyle
{
	PalEntry Color;
	PalEntry Fade;
	int Desaturate;
	float FadeLevel;
};

enum
{
	NOFIXEDCOLORMAP = -1,
	INVERSECOLORMAP,	// the inverse map is used explicitly in a few places.
};


struct FSpecialColormap
{
	float ColorizeStart[3];
	float ColorizeEnd[3];
	uint8_t Colormap[256];
	PalEntry GrayscaleToColor[256];
};

extern TArray<FSpecialColormap> SpecialColormaps;

// some utility functions to store special colormaps in powerup blends
#define SPECIALCOLORMAP_MASK 0x00b60000

inline uint32_t MakeSpecialColormap(int index)
{
	assert(index >= 0 && index < 65536);
	return index | SPECIALCOLORMAP_MASK;
}

int AddSpecialColormap(float r1, float g1, float b1, float r2, float g2, float b2);



extern uint8_t DesaturateColormap[31][256];


enum EColorManipulation
{
	CM_PLAIN2D = -2,			// regular 2D drawing.
	CM_INVALID = -1,
	CM_DEFAULT = 0,					// untranslated
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap
	CM_FIRSTSPECIALCOLORMAPFORCED = 0x08000000,	// first special fixed colormap, application forced (for 2D overlays)
};

#define CM_MAXCOLORMAP int(CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
#define CM_MAXCOLORMAPFORCED int(CM_FIRSTSPECIALCOLORMAPFORCED + SpecialColormaps.Size())


#endif
