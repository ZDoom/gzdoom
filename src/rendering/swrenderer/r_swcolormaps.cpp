/*
** r_swcolormaps.cpp
**  Colormap handling for the software renderer
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
#include "filesystem.h"
#include "doomdef.h"
#include "r_sky.h"
#include "c_dispatch.h"
#include "sc_man.h"
#include "v_text.h"
#include "doomstat.h"
#include "v_palette.h"
#include "colormatcher.h"
#include "r_data/colormaps.h"
#include "r_swcolormaps.h"
#include "v_video.h"

#include "r_utility.h"
#include "swrenderer/r_renderer.h"
#include <atomic>

FDynamicColormap NormalLight;
FDynamicColormap FullNormalLight; //[SP] Emulate GZDoom brightness
bool NormalLightHasFixedLights;

FSWColormap realcolormaps;
FSWColormap realfbcolormaps; //[SP] For fullbright use
TArray<FSWColormap> SpecialSWColormaps;

//==========================================================================
//
// Colored Lighting Stuffs
//
//==========================================================================
static std::mutex buildmapmutex;

static FDynamicColormap *CreateSpecialLights (PalEntry color, PalEntry fade, int desaturate)
{
	// GetSpecialLights is called by the scene worker threads.
	// If we didn't find the colormap, search again, but this time one thread at a time
	std::unique_lock<std::mutex> lock(buildmapmutex);

	// If this colormap has already been created, just return it
	// This may happen if another thread beat us to it
	for (FDynamicColormap *colormap = &NormalLight; colormap != NULL; colormap = colormap->Next)
	{
		if (color == colormap->Color &&
			fade == colormap->Fade &&
			desaturate == colormap->Desaturate)
		{
			return colormap;
		}
	}

	// Not found. Create it.
	FDynamicColormap *colormap = new FDynamicColormap;
	colormap->Next = NormalLight.Next;
	colormap->Color = color;
	colormap->Fade = fade;
	colormap->Desaturate = desaturate;
	colormap->Maps = new uint8_t[NUMCOLORMAPS*256];
	colormap->BuildLights ();

	// Make sure colormap is fully built before making it publicly visible
	std::atomic_thread_fence(std::memory_order_release);
	NormalLight.Next = colormap;

	return colormap;
}

FDynamicColormap *GetSpecialLights (PalEntry color, PalEntry fade, int desaturate)
{
	// If this colormap has already been created, just return it
	for (FDynamicColormap *colormap = &NormalLight; colormap != NULL; colormap = colormap->Next)
	{
		if (color == colormap->Color &&
			fade == colormap->Fade &&
			desaturate == colormap->Desaturate)
		{
			return colormap;
		}
	}

	return CreateSpecialLights(color, fade, desaturate);
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
	uint8_t *shade;

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
		if ((uint32_t)Color == MAKERGB(255,255,255))
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
	FDynamicColormap *cm;

	for (cm = &NormalLight; cm != NULL; cm = cm->Next)
	{
		if (cm->Maps == NULL)
		{
			cm->Maps = new uint8_t[NUMCOLORMAPS*256];
			cm->BuildLights ();
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

static bool R_CheckForFixedLights(const uint8_t *colormaps)
{
	const uint8_t *lastcolormap = colormaps + (NUMCOLORMAPS - 1) * 256;
	uint8_t freq[256];
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
		uint8_t color = lastcolormap[i];
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
// R_SetDefaultColormap
//
//==========================================================================

void SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8) != 0)
	{
		int lump, i, j;
		uint8_t map[256];
		uint8_t unremap[256];
		uint8_t remap[256];

		lump = fileSystem.CheckNumForAnyName (name, ns_colormaps);
		if (lump == -1)
			lump = fileSystem.CheckNumForName (name, ns_global);

		// [RH] If using BUILD's palette, generate the colormap
		if (lump == -1 || fileSystem.FindFile("palette.dat") >= 0 || fileSystem.FindFile("blood.pal") >= 0)
		{
			DPrintf (DMSG_NOTIFY, "Make colormap\n");
			FDynamicColormap foo;

			foo.Color = 0xFFFFFF;
			foo.Fade = 0;
			foo.Maps = realcolormaps.Maps;
			foo.Desaturate = 0;
			foo.Next = NULL;
			foo.BuildLights ();
		}
		else
		{
			auto lumpr = fileSystem.OpenFileReader (lump);

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
				uint8_t *map2 = &realcolormaps.Maps[i*256];
				lumpr.Read (map, 256);
				for (j = 0; j < 256; ++j)
				{
					map2[j] = remap[map[unremap[j]]];
				}
			}
		}
	}
}

//==========================================================================
//
// R_InitColormaps
//
//==========================================================================

static void InitBoomColormaps ()
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)

	uint32_t NumLumps = fileSystem.GetFileCount();

	realcolormaps.Maps = new uint8_t[256*NUMCOLORMAPS*fakecmaps.Size()];
	SetDefaultColormap ("COLORMAP");

	if (fakecmaps.Size() > 1)
	{
		uint8_t unremap[256], remap[256], mapin[256];
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
			if (fileSystem.FileLength (fakecmaps[j].lump) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r;
				auto lump = fileSystem.OpenFileReader (fakecmaps[j].lump);
				uint8_t *const map = realcolormaps.Maps + NUMCOLORMAPS*256*j;

				for (k = 0; k < NUMCOLORMAPS; ++k)
				{
					uint8_t *map2 = &map[k*256];
					lump.Read (mapin, 256);
					map2[0] = 0;
					for (r = 1; r < 256; ++r)
					{
						map2[r] = remap[mapin[unremap[r]]];
					}
				}
			}
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void DeinitSWColorMaps()
{
	FreeSpecialLights();
	if (realcolormaps.Maps != nullptr)
	{
		delete[] realcolormaps.Maps;
		realcolormaps.Maps = nullptr;
	}
	if (realfbcolormaps.Maps)
	{
		delete[] realfbcolormaps.Maps;
		realfbcolormaps.Maps = nullptr;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void InitSWColorMaps()
{
	InitBoomColormaps();
	NormalLight.Color = PalEntry (255, 255, 255);
	NormalLight.Fade = 0;
	NormalLight.Maps = realcolormaps.Maps;
	NormalLightHasFixedLights = R_CheckForFixedLights(realcolormaps.Maps);

	// [SP] Create a copy of the colormap
	if (!realfbcolormaps.Maps)
	{
		realfbcolormaps.Maps = new uint8_t[256*NUMCOLORMAPS*fakecmaps.Size()];
		memcpy(realfbcolormaps.Maps, realcolormaps.Maps, 256*NUMCOLORMAPS*fakecmaps.Size());
	}
	FullNormalLight.Color = PalEntry(255, 255, 255);
	FullNormalLight.Fade = 0;
	FullNormalLight.Maps = realfbcolormaps.Maps;

	SpecialSWColormaps.Resize(SpecialColormaps.Size());
	for(unsigned i = 0; i < SpecialColormaps.Size(); i++)
	{
		SpecialSWColormaps[i].Maps = SpecialColormaps[i].Colormap;
	}
}

//==========================================================================
//
//
//
//==========================================================================

CCMD (testfade)
{
	FString colorstring;
	uint32_t color;

	if (argv.argc() < 2)
	{
		Printf ("testfade <color>\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (colorstring.GetChars());
		}
		else
		{
			color = V_GetColorFromString (argv[1]);
		}
		for (auto Level : AllLevels())
		{
			Level->fadeto = color;
		}
		NormalLight.ChangeFade (color);
	}
}

//==========================================================================
//
//
//
//==========================================================================

CCMD (testcolor)
{
	FString colorstring;
	uint32_t color;
	int desaturate;

	if (argv.argc() < 2)
	{
		Printf ("testcolor <color> [desaturation]\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (colorstring.GetChars());
		}
		else
		{
			color = V_GetColorFromString (argv[1]);
		}
		if (argv.argc() > 2)
		{
			desaturate = atoi (argv[2]);
		}
		else
		{
			desaturate = NormalLight.Desaturate;
		}
		NormalLight.ChangeColor (color, desaturate);
	}
}
