#pragma once

#include <stdint.h>
#include "palentry.h"

int BestColor(const uint32_t* pal, int r, int g, int b, int first = 1, int num = 255);
int PTM_BestColor(const uint32_t* pal_in, int r, int g, int b, bool reverselookup, float powtable, int first = 1, int num = 255);
void DoBlending(const PalEntry* from, PalEntry* to, int count, int r, int g, int b, int a);

// Given an array of colors, fills in remap with values to remap the
// passed array of colors to BaseColors. Used for loading palette downconversions of PNGs.
void MakeRemap(uint32_t* BaseColors, const uint32_t* colors, uint8_t* remap, const uint8_t* useful, int numcolors);
void MakeGoodRemap(uint32_t* BaseColors, uint8_t* Remap);

// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

