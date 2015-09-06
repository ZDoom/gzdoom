#ifndef __RES_CMAP_H
#define __RES_CMAP_H

void R_InitColormaps ();
void R_DeinitColormaps ();

DWORD R_ColormapNumForName(const char *name);	// killough 4/4/98
void R_SetDefaultColormap (const char *name);	// [RH] change normal fadetable
DWORD R_BlendForColormap (DWORD map);			// [RH] return calculated blend for a colormap
extern BYTE *realcolormaps;						// [RH] make the colormaps externally visible
extern size_t numfakecmaps;



struct FDynamicColormap
{
	void ChangeFade (PalEntry fadecolor);
	void ChangeColor (PalEntry lightcolor, int desaturate);
	void ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor);
	void BuildLights ();
	static void RebuildAllLights();

	BYTE *Maps;
	PalEntry Color;
	PalEntry Fade;
	int Desaturate;
	FDynamicColormap *Next;
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
	BYTE Colormap[256];
	PalEntry GrayscaleToColor[256];
};

extern TArray<FSpecialColormap> SpecialColormaps;

// some utility functions to store special colormaps in powerup blends
#define SPECIALCOLORMAP_MASK 0x00b60000

inline uint32 MakeSpecialColormap(int index)
{
	assert(index >= 0 && index < 65536);
	return index | SPECIALCOLORMAP_MASK;
}

inline bool IsSpecialColormap(uint32 map)
{
	return (map & 0xFFFF0000) == SPECIALCOLORMAP_MASK;
}

inline int GetSpecialColormap(int blend)
{
	return IsSpecialColormap(blend) ? blend & 0xFFFF : NOFIXEDCOLORMAP;
}

int AddSpecialColormap(float r1, float g1, float b1, float r2, float g2, float b2);



extern BYTE DesaturateColormap[31][256];
extern "C" 
{
extern FDynamicColormap NormalLight;
}
extern bool NormalLightHasFixedLights;

FDynamicColormap *GetSpecialLights (PalEntry lightcolor, PalEntry fadecolor, int desaturate);


#endif
