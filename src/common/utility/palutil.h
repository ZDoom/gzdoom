#pragma once

#include <stdint.h>
#include "zstring.h"
#include "palentry.h"

struct FScriptPosition;
class FScanner;

int BestColor(const uint32_t* pal, int r, int g, int b, int first = 1, int num = 255, const uint8_t* indexmap = nullptr);
int PTM_BestColor(const uint32_t* pal_in, int r, int g, int b, bool reverselookup, float powtable, int first = 1, int num = 255);
void DoBlending(const PalEntry* from, PalEntry* to, int count, int r, int g, int b, int a);

// Given an array of colors, fills in remap with values to remap the
// passed array of colors to BaseColors. Used for loading palette downconversions of PNGs.
void MakeRemap(uint32_t* BaseColors, const uint32_t* colors, uint8_t* remap, const uint8_t* useful, int numcolors);
void MakeGoodRemap(uint32_t* BaseColors, uint8_t* Remap, const uint8_t* cmapdata = nullptr);

// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

// Returns the closest color to the one desired. String
// should be of the form "rr gg bb".
int V_GetColorFromString(const char* colorstring, FScriptPosition* sc = nullptr);
// Scans through the X11R6RGB lump for a matching color
// and returns a color string suitable for V_GetColorFromString.
FString V_GetColorStringByName(const char* name, FScriptPosition* sc = nullptr);

// Tries to get color by name, then by string
int V_GetColor(const char* str, FScriptPosition* sc = nullptr);
int V_GetColor(FScanner& sc);
PalEntry averageColor(const uint32_t* data, int size, int maxout);

enum
{
	NOFIXEDCOLORMAP = -1,
	INVERSECOLORMAP,	// the inverse map is used explicitly in a few places.
	GOLDCOLORMAP,
	REDCOLORMAP,
	GREENCOLORMAP,
	BLUECOLORMAP,
	REALINVERSECOLORMAP,
};

struct FSpecialColormapParameters
{
	float Start[3], End[3];
};

struct FSpecialColormap
{
	float ColorizeStart[3];
	float ColorizeEnd[3];
	uint8_t Colormap[256];
	PalEntry GrayscaleToColor[256];
};

extern TArray<FSpecialColormap> SpecialColormaps;
extern uint8_t DesaturateColormap[31][256];

int AddSpecialColormap(PalEntry *pe, float r1, float g1, float b1, float r2, float g2, float b2);
void InitSpecialColormaps(PalEntry* pe);
void UpdateSpecialColormap(PalEntry* BaseColors, unsigned int index, float r1, float g1, float b1, float r2, float g2, float b2);
int ReadPalette(int lumpnum, uint8_t* buffer);

enum EColorManipulation
{
	CM_PLAIN2D = -2,			// regular 2D drawing.
	CM_INVALID = -1,
	CM_DEFAULT = 0,					// untranslated
	CM_FIRSTSPECIALCOLORMAP,		// first special fixed colormap
};

#define CM_MAXCOLORMAP int(CM_FIRSTSPECIALCOLORMAP + SpecialColormaps.Size())
