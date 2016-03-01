/*
** r_data.cpp
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

#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "r_sky.h"
#include "c_dispatch.h"
#include "sc_man.h"
#include "v_text.h"
#include "st_start.h"
#include "doomstat.h"
#include "v_palette.h"
#include "colormatcher.h"
#include "colormaps.h"
#include "v_video.h"
#include "templates.h"
#include "r_utility.h"
#include "r_renderer.h"

static bool R_CheckForFixedLights(const BYTE *colormaps);


extern "C" {
FDynamicColormap NormalLight;
}
bool NormalLightHasFixedLights;


struct FakeCmap 
{
	char name[8];
	PalEntry blend;
	int lump;
};

TArray<FakeCmap> fakecmaps;
BYTE *realcolormaps;
size_t numfakecmaps;



TArray<FSpecialColormap> SpecialColormaps;
BYTE DesaturateColormap[31][256];

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
// Colored Lighting Stuffs
//
//==========================================================================

FDynamicColormap *GetSpecialLights (PalEntry color, PalEntry fade, int desaturate)
{
	FDynamicColormap *colormap;

	// If this colormap has already been created, just return it
	for (colormap = &NormalLight; colormap != NULL; colormap = colormap->Next)
	{
		if (color == colormap->Color &&
			fade == colormap->Fade &&
			desaturate == colormap->Desaturate)
		{
			return colormap;
		}
	}

	// Not found. Create it.
	colormap = new FDynamicColormap;
	colormap->Next = NormalLight.Next;
	colormap->Color = color;
	colormap->Fade = fade;
	colormap->Desaturate = desaturate;
	NormalLight.Next = colormap;

	if (Renderer->UsesColormap())
	{
		colormap->Maps = new BYTE[NUMCOLORMAPS*256];
		colormap->BuildLights ();
	}
	else colormap->Maps = NULL;

	return colormap;
}

//==========================================================================
//
// Free all lights created with GetSpecialLights
//
//==========================================================================

static void FreeSpecialLights()
{
	FDynamicColormap *colormap, *next;

	for (colormap = NormalLight.Next; colormap != NULL; colormap = next)
	{
		next = colormap->Next;
		delete[] colormap->Maps;
		delete colormap;
	}
	NormalLight.Next = NULL;
}

//==========================================================================
//
// Builds NUMCOLORMAPS colormaps lit with the specified color
//
//==========================================================================

void FDynamicColormap::BuildLights ()
{
	int l, c;
	int lr, lg, lb, ld, ild;
	PalEntry colors[256], basecolors[256];
	BYTE *shade;

	if (Maps == NULL)
		return;

	// Scale light to the range 0-256, so we can avoid
	// dividing by 255 in the bottom loop.
	lr = Color.r*256/255;
	lg = Color.g*256/255;
	lb = Color.b*256/255;
	ld = Desaturate*256/255;
	if (ld < 0)	// No negative desaturations, please.
	{
		ld = -ld;
	}
	ild = 256-ld;

	if (ld == 0)
	{
		memcpy (basecolors, GPalette.BaseColors, sizeof(basecolors));
	}
	else
	{
		// Desaturate the palette before lighting it.
		for (c = 0; c < 256; c++)
		{
			int r = GPalette.BaseColors[c].r;
			int g = GPalette.BaseColors[c].g;
			int b = GPalette.BaseColors[c].b;
			int intensity = ((r * 77 + g * 143 + b * 37) >> 8) * ld;
			basecolors[c].r = (r*ild + intensity) >> 8;
			basecolors[c].g = (g*ild + intensity) >> 8;
			basecolors[c].b = (b*ild + intensity) >> 8;
			basecolors[c].a = 0;
		}
	}

	// build normal (but colored) light mappings
	for (l = 0; l < NUMCOLORMAPS; l++)
	{
		DoBlending (basecolors, colors, 256,
			Fade.r, Fade.g, Fade.b, l * (256 / NUMCOLORMAPS));

		shade = Maps + 256*l;
		if ((DWORD)Color == MAKERGB(255,255,255))
		{ // White light, so we can just pick the colors directly
			for (c = 0; c < 256; c++)
			{
				*shade++ = ColorMatcher.Pick (colors[c].r, colors[c].g, colors[c].b);
			}
		}
		else
		{ // Colored light, so do the (slightly) slower thing
			for (c = 0; c < 256; c++)
			{
				*shade++ = ColorMatcher.Pick (
					(colors[c].r*lr)>>8,
					(colors[c].g*lg)>>8,
					(colors[c].b*lb)>>8);
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDynamicColormap::ChangeColor (PalEntry lightcolor, int desaturate)
{
	if (lightcolor != Color || desaturate != Desaturate)
	{
		Color = lightcolor;
		// [BB] desaturate must be in [0,255]
		Desaturate = clamp(desaturate, 0, 255);
		if (Maps) BuildLights ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDynamicColormap::ChangeFade (PalEntry fadecolor)
{
	if (fadecolor != Fade)
	{
		Fade = fadecolor;
		if (Maps) BuildLights ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDynamicColormap::ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor)
{
	if (lightcolor != Color || fadecolor != Fade)
	{
		Color = lightcolor;
		Fade = fadecolor;
		if (Maps) BuildLights ();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FDynamicColormap::RebuildAllLights()
{
	if (Renderer->UsesColormap())
	{
		FDynamicColormap *cm;

		for (cm = &NormalLight; cm != NULL; cm = cm->Next)
		{
			if (cm->Maps == NULL)
			{
				cm->Maps = new BYTE[NUMCOLORMAPS*256];
				cm->BuildLights ();
			}
		}
	}
}

//==========================================================================
//
// R_SetDefaultColormap
//
//==========================================================================

void R_SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8) != 0)
	{
		int lump, i, j;
		BYTE map[256];
		BYTE unremap[256];
		BYTE remap[256];

		lump = Wads.CheckNumForFullName (name, true, ns_colormaps);
		if (lump == -1)
			lump = Wads.CheckNumForName (name, ns_global);

		// [RH] If using BUILD's palette, generate the colormap
		if (lump == -1 || Wads.CheckNumForFullName("palette.dat") >= 0 || Wads.CheckNumForFullName("blood.pal") >= 0)
		{
			Printf ("Make colormap\n");
			FDynamicColormap foo;

			foo.Color = 0xFFFFFF;
			foo.Fade = 0;
			foo.Maps = realcolormaps;
			foo.Desaturate = 0;
			foo.Next = NULL;
			foo.BuildLights ();
		}
		else
		{
			FWadLump lumpr = Wads.OpenLumpNum (lump);

			// [RH] The colormap may not have been designed for the specific
			// palette we are using, so remap it to match the current palette.
			memcpy (remap, GPalette.Remap, 256);
			memset (unremap, 0, 256);
			for (i = 0; i < 256; ++i)
			{
				unremap[remap[i]] = i;
			}
			// Mapping to color 0 is okay, because the colormap won't be used to
			// produce a masked texture.
			remap[0] = 0;
			for (i = 0; i < NUMCOLORMAPS; ++i)
			{
				BYTE *map2 = &realcolormaps[i*256];
				lumpr.Read (map, 256);
				for (j = 0; j < 256; ++j)
				{
					map2[j] = remap[map[unremap[j]]];
				}
			}
		}

		uppercopy (fakecmaps[0].name, name);
		fakecmaps[0].blend = 0;
	}
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
	if (realcolormaps != NULL)
	{
		delete[] realcolormaps;
		realcolormaps = NULL;
	}
	FreeSpecialLights();
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

	DWORD NumLumps = Wads.GetNumLumps();

	for (DWORD i = 0; i < NumLumps; i++)
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
	realcolormaps = new BYTE[256*NUMCOLORMAPS*fakecmaps.Size()];
	R_SetDefaultColormap ("COLORMAP");

	if (fakecmaps.Size() > 1)
	{
		BYTE unremap[256], remap[256], mapin[256];
		int i;
		unsigned j;

		memcpy (remap, GPalette.Remap, 256);
		memset (unremap, 0, 256);
		for (i = 0; i < 256; ++i)
		{
			unremap[remap[i]] = i;
		}
		remap[0] = 0;
		for (j = 1; j < fakecmaps.Size(); j++)
		{
			if (Wads.LumpLength (fakecmaps[j].lump) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r, g, b;
				FWadLump lump = Wads.OpenLumpNum (fakecmaps[j].lump);
				BYTE *const map = realcolormaps + NUMCOLORMAPS*256*j;

				for (k = 0; k < NUMCOLORMAPS; ++k)
				{
					BYTE *map2 = &map[k*256];
					lump.Read (mapin, 256);
					map2[0] = 0;
					for (r = 1; r < 256; ++r)
					{
						map2[r] = remap[mapin[unremap[r]]];
					}
				}

				r = g = b = 0;

				for (k = 0; k < 256; k++)
				{
					r += GPalette.BaseColors[map[k]].r;
					g += GPalette.BaseColors[map[k]].g;
					b += GPalette.BaseColors[map[k]].b;
				}
				fakecmaps[j].blend = PalEntry (255, r/256, g/256, b/256);
			}
		}
	}
	NormalLight.Color = PalEntry (255, 255, 255);
	NormalLight.Fade = 0;
	NormalLight.Maps = realcolormaps;
	NormalLightHasFixedLights = R_CheckForFixedLights(realcolormaps);
	numfakecmaps = fakecmaps.Size();

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
		BYTE *shade = DesaturateColormap[m];
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
// R_CheckForFixedLights
//
// Returns true if there are any entries in the colormaps that are the
// same for every colormap and not the fade color.
//
//==========================================================================

static bool R_CheckForFixedLights(const BYTE *colormaps)
{
	const BYTE *lastcolormap = colormaps + (NUMCOLORMAPS - 1) * 256;
	BYTE freq[256];
	int i, j;

	// Count the frequencies of different colors in the final colormap.
	// If they occur more than X amount of times, we ignore them as a
	// potential fixed light.

	memset(freq, 0, sizeof(freq));
	for (i = 0; i < 256; ++i)
	{
		freq[lastcolormap[i]]++;
	}

	// Now check the colormaps for fixed lights that are uncommon in the
	// final coloramp.
	for (i = 255; i >= 0; --i)
	{
		BYTE color = lastcolormap[i];
		if (freq[color] > 10)		// arbitrary number to decide "common" colors
		{
			continue;
		}
		// It's rare in the final colormap. See if it's the same for all colormaps.
		for (j = 0; j < NUMCOLORMAPS - 1; ++j)
		{
			if (colormaps[j * 256 + i] != color)
				break;
		}
		if (j == NUMCOLORMAPS - 1)
		{ // It was the same all the way across.
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// [RH] Returns an index into realcolormaps. Multiply it by
//		256*NUMCOLORMAPS to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
//
//==========================================================================

DWORD R_ColormapNumForName (const char *name)
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

DWORD R_BlendForColormap (DWORD map)
{
	return APART(map) ? map : 
		map < fakecmaps.Size() ? DWORD(fakecmaps[map].blend) : 0;
}