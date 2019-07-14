/*
** colormaps.cpp
** common Colormap handling 
**
**---------------------------------------------------------------------------
** Copyright 1998-2008 Randy Heit
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
**
*/

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>


#include "w_wad.h"
#include "r_sky.h"
#include "colormaps.h"
#include "templates.h"

TArray<FakeCmap> fakecmaps;

TArray<FSpecialColormap> SpecialColormaps;
uint8_t DesaturateColormap[31][256];

struct FSpecialColormapParameters
{
	float Start[3], End[3];
};

static FSpecialColormapParameters SpecialColormapParms[] =
{
	// Doom invulnerability is an inverted grayscale.
	// Strife uses it when firing the Sigil
	{ { 1, 1, 1 }, {    0,    0,   0 } },

	// Heretic invulnerability is a golden shade.
	{ { 0, 0, 0 }, {  1.5, 0.75,   0 }, },

	// [BC] Build the Doomsphere colormap. It is red!
	{ { 0, 0, 0 }, {  1.5,    0,   0 } },

	// [BC] Build the Guardsphere colormap. It's a greenish-white kind of thing.
	{ { 0, 0, 0 }, { 1.25,  1.5,   1 } },

	// Build a blue colormap.
	{ { 0, 0, 0 }, {    0,    0, 1.5 } },
};

static void FreeSpecialLights();



//==========================================================================
//
//
//
//==========================================================================

int AddSpecialColormap(float r1, float g1, float b1, float r2, float g2, float b2)
{
	// Clamp these in range for the hardware shader.
	r1 = clamp(r1, 0.0f, 2.0f);
	g1 = clamp(g1, 0.0f, 2.0f);
	b1 = clamp(b1, 0.0f, 2.0f);
	r2 = clamp(r2, 0.0f, 2.0f);
	g2 = clamp(g2, 0.0f, 2.0f);
	b2 = clamp(b2, 0.0f, 2.0f);

	for(unsigned i=0; i<SpecialColormaps.Size(); i++)
	{
		// Avoid precision issues here when trying to find a proper match.
		if (fabs(SpecialColormaps[i].ColorizeStart[0]- r1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeStart[1]- g1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeStart[2]- b1) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[0]- r2) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[1]- g2) < FLT_EPSILON &&
			fabs(SpecialColormaps[i].ColorizeEnd[2]- b2) < FLT_EPSILON)
		{
			return i;	// The map already exists
		}
	}

	FSpecialColormap *cm = &SpecialColormaps[SpecialColormaps.Reserve(1)];

	cm->ColorizeStart[0] = float(r1);
	cm->ColorizeStart[1] = float(g1);
	cm->ColorizeStart[2] = float(b1);
	cm->ColorizeEnd[0] = float(r2);
	cm->ColorizeEnd[1] = float(g2);
	cm->ColorizeEnd[2] = float(b2);

	r2 -= r1;
	g2 -= g1;
	b2 -= b1;
	r1 *= 255;
	g1 *= 255;
	b1 *= 255;

	for (int c = 0; c < 256; c++)
	{
		double intensity = (GPalette.BaseColors[c].r * 77 +
							GPalette.BaseColors[c].g * 143 +
							GPalette.BaseColors[c].b * 37) / 256.0;

		PalEntry pe = PalEntry(	MIN(255, int(r1 + intensity*r2)), 
								MIN(255, int(g1 + intensity*g2)), 
								MIN(255, int(b1 + intensity*b2)));

		cm->Colormap[c] = ColorMatcher.Pick(pe);
	}

	// This table is used by the texture composition code
	for(int i = 0;i < 256; i++)
	{
		cm->GrayscaleToColor[i] = PalEntry(	MIN(255, int(r1 + i*r2)), 
											MIN(255, int(g1 + i*g2)), 
											MIN(255, int(b1 + i*b2)));
	}
	return SpecialColormaps.Size() - 1;
}

//==========================================================================
//
// R_DeinitColormaps
//
//==========================================================================

void R_DeinitColormaps ()
{
	SpecialColormaps.Clear();
	fakecmaps.Clear();
}

//==========================================================================
//
// R_InitColormaps
//
//==========================================================================

void R_InitColormaps ()
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)

	FakeCmap cm;

	R_DeinitColormaps();

	cm.name[0] = 0;
	cm.blend = 0;
	fakecmaps.Push(cm);

	uint32_t NumLumps = Wads.GetNumLumps();

	for (uint32_t i = 0; i < NumLumps; i++)
	{
		if (Wads.GetLumpNamespace(i) == ns_colormaps)
		{
			char name[9];
			name[8] = 0;
			Wads.GetLumpName (name, i);

			if (Wads.CheckNumForName (name, ns_colormaps) == (int)i)
			{
				strncpy(cm.name, name, 8);
				cm.blend = 0;
				cm.lump = i;
				fakecmaps.Push(cm);
			}
		}
	}

	int rr = 0, gg = 0, bb = 0;
	for(int x=0;x<256;x++)
	{
		rr += GPalette.BaseColors[x].r;
		gg += GPalette.BaseColors[x].g;
		bb += GPalette.BaseColors[x].b;
	}
	rr >>= 8;
	gg >>= 8;
	bb >>= 8;

	int palette_brightness = (rr*77 + gg*143 + bb*35) / 255;

	// To calculate the blend it will just average the colors of the first map
	if (fakecmaps.Size() > 1)
	{
		uint8_t map[256];

		for (unsigned j = 1; j < fakecmaps.Size(); j++)
		{
			if (Wads.LumpLength (fakecmaps[j].lump) >= 256)
			{
				int k, r, g, b;
				auto lump = Wads.OpenLumpReader (fakecmaps[j].lump);
				lump.Read(map, 256);
				r = g = b = 0;

				for (k = 0; k < 256; k++)
				{
					r += GPalette.BaseColors[map[k]].r;
					g += GPalette.BaseColors[map[k]].g;
					b += GPalette.BaseColors[map[k]].b;
				}
				r /= 256;
				g /= 256;
				b /= 256;
				// The calculated average is too dark so brighten it according to the palettes's overall brightness
				int maxcol = MAX<int>(MAX<int>(palette_brightness, r), MAX<int>(g, b));
				
				fakecmaps[j].blend = PalEntry (255, r * 255 / maxcol, g * 255 / maxcol, b * 255 / maxcol);
			}
		}
	}

	// build default special maps (e.g. invulnerability)

	for (unsigned i = 0; i < countof(SpecialColormapParms); ++i)
	{
		AddSpecialColormap(SpecialColormapParms[i].Start[0], SpecialColormapParms[i].Start[1],
			SpecialColormapParms[i].Start[2], SpecialColormapParms[i].End[0],
			SpecialColormapParms[i].End[1], SpecialColormapParms[i].End[2]);
	}
	// desaturated colormaps. These are used for texture composition
	for(int m = 0; m < 31; m++)
	{
		uint8_t *shade = DesaturateColormap[m];
		for (int c = 0; c < 256; c++)
		{
			int intensity = (GPalette.BaseColors[c].r * 77 +
						GPalette.BaseColors[c].g * 143 +
						GPalette.BaseColors[c].b * 37) / 256;

			int r = (GPalette.BaseColors[c].r * (31-m) + intensity *m) / 31;
			int g = (GPalette.BaseColors[c].g * (31-m) + intensity *m) / 31;
			int b = (GPalette.BaseColors[c].b * (31-m) + intensity *m) / 31;
			shade[c] = ColorMatcher.Pick(r, g, b);
		}
	}
}

//==========================================================================
//
// [RH] Returns an index into realcolormaps. Multiply it by
//		256*NUMCOLORMAPS to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
//
//==========================================================================

uint32_t R_ColormapNumForName (const char *name)
{
	if (strnicmp (name, "COLORMAP", 8))
	{	// COLORMAP always returns 0
		for(int i=fakecmaps.Size()-1; i > 0; i--)
		{
			if (!strnicmp(name, fakecmaps[i].name, 8))
			{
				return i;
			}
		}
				
		if (!strnicmp (name, "WATERMAP", 8))
			return MAKEARGB (128,0,0x4f,0xa5);
	}
	return 0;
}

//==========================================================================
//
// R_BlendForColormap
//
//==========================================================================

uint32_t R_BlendForColormap (uint32_t map)
{
	return APART(map) ? map : 
		map < fakecmaps.Size() ? uint32_t(fakecmaps[map].blend) : 0;
}

