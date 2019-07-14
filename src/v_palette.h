/*
** v_palette.h
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __V_PALETTE_H__
#define __V_PALETTE_H__

#include "doomtype.h"
#include "c_cvars.h"

#define MAKERGB(r,g,b)		uint32_t(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB(a,r,g,b)	uint32_t(((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define APART(c)			(((c)>>24)&0xff)
#define RPART(c)			(((c)>>16)&0xff)
#define GPART(c)			(((c)>>8)&0xff)
#define BPART(c)			((c)&0xff)

struct FPalette
{
	FPalette ();
	FPalette (const uint8_t *colors);

	void SetPalette (const uint8_t *colors);

	void MakeGoodRemap ();

	PalEntry	BaseColors[256];	// non-gamma corrected palette
	uint8_t		Remap[256];			// remap original palette indices to in-game indices

	uint8_t		WhiteIndex;			// white in original palette index
	uint8_t		BlackIndex;			// black in original palette index

	// Given an array of colors, fills in remap with values to remap the
	// passed array of colors to this palette.
	void MakeRemap (const uint32_t *colors, uint8_t *remap, const uint8_t *useful, int numcolors) const;
};

extern FPalette GPalette;

// The color overlay to use for depleted items
#define DIM_OVERLAY MAKEARGB(170,0,0,0)

int BestColor (const uint32_t *pal, int r, int g, int b, int first=1, int num=255);
int PTM_BestColor (const uint32_t *pal_in, int r, int g, int b, bool reverselookup, float powtable, int first=1, int num=255);
void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

void ReadPalette(int lumpnum, uint8_t *buffer);
void InitPalette ();

EXTERN_CVAR (Int, paletteflash)
enum PaletteFlashFlags
{
	PF_HEXENWEAPONS		= 1,
	PF_POISON			= 2,
	PF_ICE				= 4,
	PF_HAZARD			= 8,
};

class player_t;

void V_AddBlend (float r, float g, float b, float a, float v_blend[4]);
void V_AddPlayerBlend (player_t *CPlayer, float blend[4], float maxinvalpha, int maxpainblend);


// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

#endif //__V_PALETTE_H__
