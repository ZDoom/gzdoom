#pragma once

#include <stdint.h>
struct PalEntry;

#define MAKERGB(r,g,b)		uint32_t(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB(a,r,g,b)	uint32_t(((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define APART(c)			(((c)>>24)&0xff)
#define RPART(c)			(((c)>>16)&0xff)
#define GPART(c)			(((c)>>8)&0xff)
#define BPART(c)			((c)&0xff)

int BestColor(const uint32_t* pal, int r, int g, int b, int first = 1, int num = 255);
int PTM_BestColor(const uint32_t* pal_in, int r, int g, int b, bool reverselookup, float powtable, int first = 1, int num = 255);
void DoBlending(const PalEntry* from, PalEntry* to, int count, int r, int g, int b, int a);
// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);
