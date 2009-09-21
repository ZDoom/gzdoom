/*
** v_palette.cpp
** Automatic colormap generation for "colored lights", etc.
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

#include <stddef.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#define O_BINARY 0
#endif
#include <fcntl.h>

#include "templates.h"
#include "v_video.h"
#include "i_system.h"
#include "r_main.h"		// For lighting constants
#include "w_wad.h"
#include "i_video.h"
#include "c_dispatch.h"
#include "g_level.h"
#include "st_stuff.h"
#include "gi.h"
#include "x86.h"
#include "colormatcher.h"
#include "v_palette.h"

extern "C" {
FDynamicColormap NormalLight;
}
FPalette GPalette;
TArray<FSpecialColormap> SpecialColormaps;
BYTE DesaturateColormap[31][256];

struct FSpecialColormapParameters
{
	float Colorize[3];
	bool Inverted;
};

static FSpecialColormapParameters SpecialColormapParms[] =
{
	// Doom invulnerability is an inverted grayscale.
	// Strife uses it when firing the Sigil
	{ {    1,    1,   1 }, true },

	// Heretic invulnerability is a golden shade.
	{ {  1.5, 0.75,   0 }, false },

	// [BC] Build the Doomsphere colormap. It is red!
	{ {  1.5,    0,   0 }, false },

	// [BC] Build the Guardsphere colormap. It's a greenish-white kind of thing.
	{ { 1.25,  1.5,   1 }, false },

	// Build a blue colormap.
	{ {    0,    0, 1.5 }, false },
};

static void FreeSpecialLights();

FColorMatcher ColorMatcher;

/* Current color blending values */
int		BlendR, BlendG, BlendB, BlendA;

static int STACK_ARGS sortforremap (const void *a, const void *b);
static int STACK_ARGS sortforremap2 (const void *a, const void *b);

/**************************/
/* Gamma correction stuff */
/**************************/

BYTE newgamma[256];
CUSTOM_CVAR (Float, Gamma, 1.f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self == 0.f)
	{ // Gamma values of 0 are illegal.
		self = 1.f;
		return;
	}

	if (screen != NULL)
	{
		screen->SetGamma (self);
	}
}


/****************************/
/* Palette management stuff */
/****************************/

extern "C" BYTE BestColor_MMX (DWORD rgb, const DWORD *pal);

int BestColor (const uint32 *pal_in, int r, int g, int b, int first, int num)
{
#ifdef X86_ASM
	if (CPU.bMMX)
	{
		int pre = 256 - num - first;
		return BestColor_MMX (((first+pre)<<24)|(r<<16)|(g<<8)|b, pal_in-pre) - pre;
	}
#endif
	const PalEntry *pal = (const PalEntry *)pal_in;
	int bestcolor = first;
	int bestdist = 257*257+257*257+257*257;

	for (int color = first; color < num; color++)
	{
		int x = r - pal[color].r;
		int y = g - pal[color].g;
		int z = b - pal[color].b;
		int dist = x*x + y*y + z*z;
		if (dist < bestdist)
		{
			if (dist == 0)
				return color;

			bestdist = dist;
			bestcolor = color;
		}
	}
	return bestcolor;
}

FPalette::FPalette ()
{
}

FPalette::FPalette (const BYTE *colors)
{
	SetPalette (colors);
}

void FPalette::SetPalette (const BYTE *colors)
{
	for (int i = 0; i < 256; i++, colors += 3)
	{
		BaseColors[i] = PalEntry (colors[0], colors[1], colors[2]);
		Remap[i] = i;
	}

	// Find white and black from the original palette so that they can be
	// used to make an educated guess of the translucency % for a BOOM
	// translucency map.
	WhiteIndex = BestColor ((DWORD *)BaseColors, 255, 255, 255);
	BlackIndex = BestColor ((DWORD *)BaseColors, 0, 0, 0);
}

// In ZDoom's new texture system, color 0 is used as the transparent color.
// But color 0 is also a valid color for Doom engine graphics. What to do?
// Simple. The default palette for every game has at least one duplicate
// color, so find a duplicate pair of palette entries, make one of them a
// duplicate of color 0, and remap every graphic so that it uses that entry
// instead of entry 0.
void FPalette::MakeGoodRemap ()
{
	PalEntry color0 = BaseColors[0];
	int i;

	// First try for an exact match of color 0. Only Hexen does not have one.
	for (i = 1; i < 256; ++i)
	{
		if (BaseColors[i] == color0)
		{
			Remap[0] = i;
			break;
		}
	}

	// If there is no duplicate of color 0, find the first set of duplicate
	// colors and make one of them a duplicate of color 0. In Hexen's PLAYPAL
	// colors 209 and 229 are the only duplicates, but we cannot assume
	// anything because the player might be using a custom PLAYPAL where those
	// entries are not duplicates.
	if (Remap[0] == 0)
	{
		PalEntry sortcopy[256];

		for (i = 0; i < 256; ++i)
		{
			sortcopy[i] = BaseColors[i] | (i << 24);
		}
		qsort (sortcopy, 256, 4, sortforremap);
		for (i = 255; i > 0; --i)
		{
			if ((sortcopy[i] & 0xFFFFFF) == (sortcopy[i-1] & 0xFFFFFF))
			{
				int new0 = sortcopy[i].a;
				int dup = sortcopy[i-1].a;
				if (new0 > dup)
				{
					// Make the lower-numbered entry a copy of color 0. (Just because.)
					swap (new0, dup);
				}
				Remap[0] = new0;
				Remap[new0] = dup;
				BaseColors[new0] = color0;
				break;
			}
		}
	}

	// If there were no duplicates, InitPalette() will remap color 0 to the
	// closest matching color. Hopefully nobody will use a palette where all
	// 256 entries are different. :-)
}

static int STACK_ARGS sortforremap (const void *a, const void *b)
{
	return (*(const DWORD *)a & 0xFFFFFF) - (*(const DWORD *)b & 0xFFFFFF);
}

struct RemappingWork
{
	DWORD Color;
	BYTE Foreign;	// 0 = local palette, 1 = foreign palette
	BYTE PalEntry;	// Entry # in the palette
	BYTE Pad[2];
};

void FPalette::MakeRemap (const DWORD *colors, BYTE *remap, const BYTE *useful, int numcolors) const
{
	RemappingWork workspace[255+256];
	int i, j, k;

	// Fill in workspace with the colors from the passed palette and this palette.
	// By sorting this array, we can quickly find exact matches so that we can
	// minimize the time spent calling BestColor for near matches.

	for (i = 1; i < 256; ++i)
	{
		workspace[i-1].Color = DWORD(BaseColors[i]) & 0xFFFFFF;
		workspace[i-1].Foreign = 0;
		workspace[i-1].PalEntry = i;
	}
	for (i = k = 0, j = 255; i < numcolors; ++i)
	{
		if (useful == NULL || useful[i] != 0)
		{
			workspace[j].Color = colors[i] & 0xFFFFFF;
			workspace[j].Foreign = 1;
			workspace[j].PalEntry = i;
			++j;
			++k;
		}
		else
		{
			remap[i] = 0;
		}
	}
	qsort (workspace, j, sizeof(RemappingWork), sortforremap2);

	// Find exact matches
	--j;
	for (i = 0; i < j; ++i)
	{
		if (workspace[i].Foreign)
		{
			if (!workspace[i+1].Foreign && workspace[i].Color == workspace[i+1].Color)
			{
				remap[workspace[i].PalEntry] = workspace[i+1].PalEntry;
				workspace[i].Foreign = 2;
				++i;
				--k;
			}
		}
	}

	// Find near matches
	if (k > 0)
	{
		for (i = 0; i <= j; ++i)
		{
			if (workspace[i].Foreign == 1)
			{
				remap[workspace[i].PalEntry] = BestColor ((DWORD *)BaseColors,
					RPART(workspace[i].Color), GPART(workspace[i].Color), BPART(workspace[i].Color),
					1, 255);
			}
		}
	}
}

static int STACK_ARGS sortforremap2 (const void *a, const void *b)
{
	const RemappingWork *ap = (const RemappingWork *)a;
	const RemappingWork *bp = (const RemappingWork *)b;

	if (ap->Color == bp->Color)
	{
		return bp->Foreign - ap->Foreign;
	}
	else
	{
		return ap->Color - bp->Color;
	}
}

static bool FixBuildPalette (BYTE *opal, int lump, bool blood)
{
	if (Wads.LumpLength (lump) < 768)
	{
		return false;
	}

	FMemLump data = Wads.ReadLump (lump);
	const BYTE *ipal = (const BYTE *)data.GetMem();
	
	// Reverse the palette because BUILD used entry 255 as
	// transparent, but we use 0 as transparent.

	for (int c = 0; c < 768; c += 3)
	{
		if (!blood)
		{
			opal[c] =	(ipal[765-c] << 2) | (ipal[765-c] >> 4);
			opal[c+1] = (ipal[766-c] << 2) | (ipal[766-c] >> 4);
			opal[c+2] = (ipal[767-c] << 2) | (ipal[767-c] >> 4);
		}
		else
		{
			opal[c] = ipal[765-c];
			opal[c+1] = ipal[766-c];
			opal[c+2] = ipal[767-c];
		}
	}
	return true;
}

int AddSpecialColormap(double r, double g, double b, bool inv)
{
	for(unsigned i=0; i<SpecialColormaps.Size(); i++)
	{
		if (SpecialColormaps[i].Colorize[0] == r &&
			SpecialColormaps[i].Colorize[1] == g &&
			SpecialColormaps[i].Colorize[2] == b &&
			SpecialColormaps[i].Inverted == inv)
		{
			return i;	// The map already exists
		}
	}

	FSpecialColormap *cm = &SpecialColormaps[SpecialColormaps.Reserve(1)];

	cm->Colorize[0] = float(r);
	cm->Colorize[1] = float(g);
	cm->Colorize[2] = float(b);
	cm->Inverted = inv;

	for (int c = 0; c < 256; c++)
	{
		double intensity = (GPalette.BaseColors[c].r * 77 +
							GPalette.BaseColors[c].g * 143 +
							GPalette.BaseColors[c].b * 37) / 256.0;
		if (inv)
		{
			intensity = 255 - intensity;
		}

		PalEntry pe = PalEntry(	MIN(255, int(intensity*r)), 
								MIN(255, int(intensity*g)), 
								MIN(255, int(intensity*b)));

		cm->Colormap[c] = ColorMatcher.Pick(pe);

		// This table is used by the texture composition code
		for(int i = 0;i < 256; i++)
		{
			intensity = inv? 255-i : i;
			cm->GrayscaleToColor[i] = PalEntry(	MIN(255, int(intensity*r)), 
												MIN(255, int(intensity*g)), 
												MIN(255, int(intensity*b)));
		}
	}
	return SpecialColormaps.Size() - 1;
}

void InitPalette ()
{
	BYTE pal[768];
	int c;
	bool usingBuild = false;
	int lump;

	atterm (FreeSpecialLights);

	if ((lump = Wads.CheckNumForFullName ("palette.dat")) >= 0 && Wads.LumpLength (lump) >= 768)
	{
		usingBuild = FixBuildPalette (pal, lump, false);
	}
	else if ((lump = Wads.CheckNumForFullName ("blood.pal")) >= 0 && Wads.LumpLength (lump) >= 768)
	{
		usingBuild = FixBuildPalette (pal, lump, true);
	}

	if (!usingBuild)
	{
		FWadLump palump = Wads.OpenLumpName ("PLAYPAL");
		palump.Read (pal, 768);
	}

	GPalette.SetPalette (pal);
	GPalette.MakeGoodRemap ();
	ColorMatcher.SetPalette ((DWORD *)GPalette.BaseColors);

	// The BUILD engine already has a transparent color, so it doesn't need any remapping.
	if (!usingBuild)
	{
		if (GPalette.Remap[0] == 0)
		{ // No duplicates, so settle for something close to color 0
			GPalette.Remap[0] = BestColor ((DWORD *)GPalette.BaseColors,
				GPalette.BaseColors[0].r, GPalette.BaseColors[0].g, GPalette.BaseColors[0].b, 1, 255);
		}
	}

	NormalLight.Color = PalEntry (255, 255, 255);
	NormalLight.Fade = 0;
	// NormalLight.Maps is set by R_InitColormaps()

	// build default special maps (e.g. invulnerability)
	SpecialColormaps.Clear();

	for (int i = 0; i < countof(SpecialColormapParms); ++i)
	{
		AddSpecialColormap(SpecialColormapParms[i].Colorize[0], SpecialColormapParms[i].Colorize[1],
			SpecialColormapParms[i].Colorize[2], SpecialColormapParms[i].Inverted);
	}
	// desaturated colormaps
	for(int m = 0; m < 31; m++)
	{
		BYTE *shade = DesaturateColormap[m];
		for (c = 0; c < 256; c++)
		{
			int intensity = (GPalette.BaseColors[c].r * 77 +
						GPalette.BaseColors[c].g * 143 +
						GPalette.BaseColors[c].b * 37) / 255;

			int r = (GPalette.BaseColors[c].r * (31-m) + intensity *m) / 31;
			int g = (GPalette.BaseColors[c].g * (31-m) + intensity *m) / 31;
			int b = (GPalette.BaseColors[c].b * (31-m) + intensity *m) / 31;
			shade[c] = ColorMatcher.Pick (r, g, b);
		}
	}

}

extern "C" void STACK_ARGS DoBlending_MMX (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);
extern void DoBlending_SSE2 (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a);

void DoBlending (const PalEntry *from, PalEntry *to, int count, int r, int g, int b, int a)
{
	if (a == 0)
	{
		if (from != to)
		{
			memcpy (to, from, count * sizeof(DWORD));
		}
	}
	else if (a == 256)
	{
		DWORD t = MAKERGB(r,g,b);
		int i;

		for (i = 0; i < count; i++)
		{
			to[i] = t;
		}
	}
#if defined(_M_X64) || defined(_M_IX86) || defined(__i386__) || defined(__amd64__)
	else if (CPU.bSSE2)
	{
		if (count >= 4)
		{
			int not3count = count & ~3;
			DoBlending_SSE2 (from, to, not3count, r, g, b, a);
			count &= 3;
			if (count <= 0)
			{
				return;
			}
			from += not3count;
			to += not3count;
		}
	}
#endif
#ifdef X86_ASM
	else if (CPU.bMMX)
	{
		if (count >= 4)
		{
			int not3count = count & ~3;
			DoBlending_MMX (from, to, not3count, r, g, b, a);
			count &= 3;
			if (count <= 0)
			{
				return;
			}
			from += not3count;
			to += not3count;
		}
	}
#endif
	int i, ia;

	ia = 256 - a;
	r *= a;
	g *= a;
	b *= a;

	for (i = count; i > 0; i--, to++, from++)
	{
		to->r = (r + from->r * ia) >> 8;
		to->g = (g + from->g * ia) >> 8;
		to->b = (b + from->b * ia) >> 8;
	}
}

void V_SetBlend (int blendr, int blendg, int blendb, int blenda)
{
	// Don't do anything if the new blend is the same as the old
	if (((blenda|BlendA) == 0) ||
		(blendr == BlendR &&
		 blendg == BlendG &&
		 blendb == BlendB &&
		 blenda == BlendA))
		return;

	V_ForceBlend (blendr, blendg, blendb, blenda);
}

void V_ForceBlend (int blendr, int blendg, int blendb, int blenda)
{
	BlendR = blendr;
	BlendG = blendg;
	BlendB = blendb;
	BlendA = blenda;

	screen->SetFlash (PalEntry (BlendR, BlendG, BlendB), BlendA);
}

CCMD (testblend)
{
	FString colorstring;
	int color;
	float amt;

	if (argv.argc() < 3)
	{
		Printf ("testblend <color> <amount>\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (NULL, colorstring);
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		amt = (float)atof (argv[2]);
		if (amt > 1.0f)
			amt = 1.0f;
		else if (amt < 0.0f)
			amt = 0.0f;
		BaseBlendR = RPART(color);
		BaseBlendG = GPART(color);
		BaseBlendB = BPART(color);
		BaseBlendA = amt;
	}
}

CCMD (testfade)
{
	FString colorstring;
	DWORD color;

	if (argv.argc() < 2)
	{
		Printf ("testfade <color>\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (NULL, colorstring);
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
		}
		level.fadeto = color;
		NormalLight.ChangeFade (color);
	}
}

/****** Colorspace Conversion Functions ******/

// Code from http://www.cs.rit.edu/~yxv4997/t_convert.html

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//				if s == 0, then h = -1 (undefined)

// Green Doom guy colors:
// RGB - 0: {    .46  1 .429 } 7: {    .254 .571 .206 } 15: {    .0317 .0794 .0159 }
// HSV - 0: { 116.743 .571 1 } 7: { 112.110 .639 .571 } 15: { 105.071  .800 .0794 }
void RGBtoHSV (float r, float g, float b, float *h, float *s, float *v)
{
	float min, max, delta, foo;

	if (r == g && g == b)
	{
		*h = 0;
		*s = 0;
		*v = r;
		return;
	}

	foo = r < g ? r : g;
	min = (foo < b) ? foo : b;
	foo = r > g ? r : g;
	max = (foo > b) ? foo : b;

	*v = max;									// v

	delta = max - min;

	*s = delta / max;							// s

	if (r == max)
		*h = (g - b) / delta;					// between yellow & magenta
	else if (g == max)
		*h = 2 + (b - r) / delta;				// between cyan & yellow
	else
		*h = 4 + (r - g) / delta;				// between magenta & cyan

	*h *= 60;									// degrees
	if (*h < 0)
		*h += 360;
}

void HSVtoRGB (float *r, float *g, float *b, float h, float s, float v)
{
	int i;
	float f, p, q, t;

	if (s == 0)
	{ // achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;									// sector 0 to 5
	i = (int)floor (h);
	f = h - i;									// factorial part of h
	p = v * (1 - s);
	q = v * (1 - s * f);
	t = v * (1 - s * (1 - f));

	switch (i)
	{
	case 0:		*r = v; *g = t; *b = p; break;
	case 1:		*r = q; *g = v; *b = p; break;
	case 2:		*r = p; *g = v; *b = t; break;
	case 3:		*r = p; *g = q; *b = v; break;
	case 4:		*r = t; *g = p; *b = v; break;
	default:	*r = v; *g = p; *b = q; break;
	}
}

/****** Colored Lighting Stuffs ******/

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

	if (screen->UsesColormap())
	{
		colormap->Maps = new BYTE[NUMCOLORMAPS*256];
		colormap->BuildLights ();
	}
	else colormap->Maps = NULL;

	return colormap;
}

// Free all lights created with GetSpecialLights
static void FreeSpecialLights()
{
	FDynamicColormap *colormap, *next;

	for (colormap = NormalLight.Next; colormap != NULL; colormap = next)
	{
		next = colormap->Next;
		delete[] colormap->Maps;
		delete colormap;
	}
}

// Builds NUMCOLORMAPS colormaps lit with the specified color
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

void FDynamicColormap::ChangeColor (PalEntry lightcolor, int desaturate)
{
	if (lightcolor != Color || desaturate != Desaturate)
	{
		Color = lightcolor;
		// [BB] desaturate must be in [0,255]
		if( desaturate > 255 )
			desaturate = 255;
		else if ( desaturate < 0 )
			desaturate = 0;
		Desaturate = desaturate;
		if (Maps) BuildLights ();
	}
}

void FDynamicColormap::ChangeFade (PalEntry fadecolor)
{
	if (fadecolor != Fade)
	{
		Fade = fadecolor;
		if (Maps) BuildLights ();
	}
}

void FDynamicColormap::ChangeColorFade (PalEntry lightcolor, PalEntry fadecolor)
{
	if (lightcolor != Color || fadecolor != Fade)
	{
		Color = lightcolor;
		Fade = fadecolor;
		if (Maps) BuildLights ();
	}
}

void FDynamicColormap::RebuildAllLights()
{
	if (screen->UsesColormap())
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


CCMD (testcolor)
{
	FString colorstring;
	DWORD color;
	int desaturate;

	if (argv.argc() < 2)
	{
		Printf ("testcolor <color> [desaturation]\n");
	}
	else
	{
		if ( !(colorstring = V_GetColorStringByName (argv[1])).IsEmpty() )
		{
			color = V_GetColorFromString (NULL, colorstring);
		}
		else
		{
			color = V_GetColorFromString (NULL, argv[1]);
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
