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

#include "basictypes.h"

#define MAKERGB(r,g,b)		DWORD(((r)<<16)|((g)<<8)|(b))
#define MAKEARGB(a,r,g,b)	DWORD(((a)<<24)|((r)<<16)|((g)<<8)|(b))

#define APART(c)			(((c)>>24)&0xff)
#define RPART(c)			(((c)>>16)&0xff)
#define GPART(c)			(((c)>>8)&0xff)
#define BPART(c)			((c)&0xff)

struct FPalette
{
	FPalette ();
	FPalette (const BYTE *colors);

	void SetPalette (const BYTE *colors);

	void MakeGoodRemap ();

	PalEntry	BaseColors[256];	// non-gamma corrected palette
	BYTE		Remap[256];			// remap original palette indices to in-game indices

	BYTE		WhiteIndex;			// white in original palette index
	BYTE		BlackIndex;			// black in original palette index

	// Given an array of colors, fills in remap with values to remap the
	// passed array of colors to this palette.
	void MakeRemap (const DWORD *colors, BYTE *remap, const BYTE *useful, int numcolors) const;
};

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

extern BYTE InverseColormap[256];
extern BYTE GoldColormap[256];
// [BC] New Skulltag colormaps.
extern BYTE RedColormap[256];
extern BYTE GreenColormap[256];
extern BYTE DesaturateColormap[31][256];
extern FPalette GPalette;
extern "C" {
extern FDynamicColormap NormalLight;
}

// The color overlay to use for depleted items
#define DIM_OVERLAY MAKEARGB(170,0,0,0)

int BestColor (const uint32 *pal, int r, int g, int b, int first=1, int num=255);

void InitPalette ();

// V_SetBlend()
//	input: blendr: red component of blend
//		   blendg: green component of blend
//		   blendb: blue component of blend
//		   blenda: alpha component of blend
//
// Applies the blend to all palettes with PALETTEF_BLEND flag
void V_SetBlend (int blendr, int blendg, int blendb, int blenda);

// V_ForceBlend()
//
// Normally, V_SetBlend() does nothing if the new blend is the
// same as the old. This function will performing the blending
// even if the blend hasn't changed.
void V_ForceBlend (int blendr, int blendg, int blendb, int blenda);


// Colorspace conversion RGB <-> HSV
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v);
void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v);

FDynamicColormap *GetSpecialLights (PalEntry lightcolor, PalEntry fadecolor, int desaturate);

#endif //__V_PALETTE_H__
