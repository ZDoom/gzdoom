// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// Revision 1.3  1997/01/29 20:10
// DESCRIPTION:
//		Preparation of data for rendering,
//		generation of lookups, caching, retrieval by name.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#include <malloc.h>
#include <stdio.h>

#include "i_system.h"
#include "m_alloc.h"

#include "m_swap.h"
#include "m_png.h"

#include "w_wad.h"

#include "doomdef.h"
#include "r_local.h"
#include "p_local.h"

#include "doomstat.h"
#include "r_sky.h"

#include "c_dispatch.h"
#include "c_console.h"
#include "r_data.h"
#include "sc_man.h"

#include "v_palette.h"
#include "v_video.h"
#include "v_text.h"
#include "gi.h"
#include "cmdlib.h"
#include "templates.h"
#include "st_start.h"

static void R_InitPatches ();
static int R_CountGroup (const char *start, const char *end);
static int R_CountTexturesX ();
static int R_CountLumpTextures (int lumpnum);

extern void R_DeinitBuildTiles();
extern int R_CountBuildTiles();



//
// Graphics.
// DOOM graphics for walls and sprites
// is stored in vertical runs of opaque pixels (posts).
// A column is composed of zero or more posts,
// a patch or sprite is composed of zero or more columns.
// 

// for global animation
bool*			flatwarp;
BYTE**			warpedflats;
int*			flatwarpedwhen;


static struct FakeCmap {
	char name[8];
	PalEntry blend;
} *fakecmaps;
size_t numfakecmaps;
int firstfakecmap;
BYTE *realcolormaps;
int lastusedcolormap;

void R_SetDefaultColormap (const char *name)
{
	if (strnicmp (fakecmaps[0].name, name, 8) != 0)
	{
		int lump, i, j;
		BYTE map[256];
		BYTE unremap[256];
		BYTE remap[256];

		// [RH] If using BUILD's palette, generate the colormap
		if (Wads.CheckNumForFullName("palette.dat") >= 0 || Wads.CheckNumForFullName("blood.pal") >= 0)
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
			lump = Wads.CheckNumForName (name, ns_colormaps);
			if (lump == -1)
				lump = Wads.CheckNumForName (name, ns_global);
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

//
// R_InitColormaps
//
void R_InitColormaps ()
{
	// [RH] Try and convert BOOM colormaps into blending values.
	//		This is a really rough hack, but it's better than
	//		not doing anything with them at all (right?)
	int lastfakecmap = Wads.CheckNumForName ("C_END");
	firstfakecmap = Wads.CheckNumForName ("C_START");

	if (firstfakecmap == -1 || lastfakecmap == -1)
		numfakecmaps = 1;
	else
		numfakecmaps = lastfakecmap - firstfakecmap;
	realcolormaps = new BYTE[256*NUMCOLORMAPS*numfakecmaps];
	fakecmaps = new FakeCmap[numfakecmaps];

	fakecmaps[0].name[0] = 0;
	R_SetDefaultColormap ("COLORMAP");

	if (numfakecmaps > 1)
	{
		BYTE unremap[256], remap[256], mapin[256];
		int i;
		size_t j;

		memcpy (remap, GPalette.Remap, 256);
		memset (unremap, 0, 256);
		for (i = 0; i < 256; ++i)
		{
			unremap[remap[i]] = i;
		}
		remap[0] = 0;
		for (i = ++firstfakecmap, j = 1; j < numfakecmaps; i++, j++)
		{
			if (Wads.LumpLength (i) >= (NUMCOLORMAPS+1)*256)
			{
				int k, r, g, b;
				FWadLump lump = Wads.OpenLumpNum (i);
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
				Wads.GetLumpName (fakecmaps[j].name, i);
				fakecmaps[j].blend = PalEntry (255, r/256, g/256, b/256);
			}
		}
	}
	NormalLight.Maps = realcolormaps;
}

void R_DeinitColormaps ()
{
	if (fakecmaps != NULL)
	{
		delete[] fakecmaps;
		fakecmaps = NULL;
	}
	if (realcolormaps != NULL)
	{
		delete[] realcolormaps;
		realcolormaps = NULL;
	}
}

// [RH] Returns an index into realcolormaps. Multiply it by
//		256*NUMCOLORMAPS to find the start of the colormap to use.
//		WATERMAP is an exception and returns a blending value instead.
DWORD R_ColormapNumForName (const char *name)
{
	int lump;
	DWORD blend = 0;

	if (strnicmp (name, "COLORMAP", 8))
	{	// COLORMAP always returns 0
		if (-1 != (lump = Wads.CheckNumForName (name, ns_colormaps)) )
			blend = lump - firstfakecmap + 1;
		else if (!strnicmp (name, "WATERMAP", 8))
			blend = MAKEARGB (128,0,0x4f,0xa5);
	}

	return blend;
}

DWORD R_BlendForColormap (DWORD map)
{
	return APART(map) ? map : 
		   map < numfakecmaps ? DWORD(fakecmaps[map].blend) : 0;
}

//
// R_InitData
// Locates all the lumps that will be used by all views
// Must be called after W_Init.
//
void R_InitData ()
{
	FTexture::InitGrayMap();
	StartScreen->Progress();
	TexMan.Init();

	V_InitFonts();
	StartScreen->Progress();
	R_InitColormaps ();
	StartScreen->Progress();
}

//===========================================================================
//
// R_GuesstimateNumTextures
//
// Returns an estimate of the number of textures R_InitData will have to
// process. Used by D_DoomMain() when it calls ST_Init().
//
//===========================================================================

int R_GuesstimateNumTextures ()
{
	int numtex;

	numtex  = R_CountGroup ("S_START", "S_END");
	numtex += R_CountGroup ("F_START", "F_END");
	numtex += R_CountGroup ("TX_START", "TX_END");
	numtex += R_CountGroup ("HI_START", "HI_END");
	numtex += R_CountBuildTiles ();
	numtex += R_CountTexturesX ();
	return numtex;
}

//===========================================================================
//
// R_CountGroup
//
//===========================================================================

static int R_CountGroup (const char *start, const char *end)
{
	int startl = Wads.CheckNumForName (start);
	int endl = Wads.CheckNumForName (end);

	if (startl < 0 || endl < 0)
	{
		return 0;
	}
	else
	{
		return endl - startl - 1;
	}
}

//===========================================================================
//
// R_CountTexturesX
//
// See R_InitTextures() for the logic in deciding what lumps to check.
//
//===========================================================================

static int R_CountTexturesX ()
{
	int lastlump = 0, lump;
	int texlump1 = -1, texlump2 = -1, texlump1a, texlump2a;
	int count = 0;
	int pfile = -1;

	while ((lump = Wads.FindLump ("PNAMES", &lastlump)) != -1)
	{
		pfile = Wads.GetLumpFile (lump);
		count += R_CountLumpTextures (lump);
		texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, pfile);
		texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, pfile);
		count += R_CountLumpTextures (texlump1) - 1;
		count += R_CountLumpTextures (texlump2) - 1;
	}
	texlump1a = Wads.CheckNumForName ("TEXTURE1");
	texlump2a = Wads.CheckNumForName ("TEXTURE2");
	if (texlump1a != -1 && (texlump1a == texlump1 || Wads.GetLumpFile (texlump1a) <= pfile))
	{
		texlump1a = -1;
	}
	if (texlump2a != -1 && (texlump2a == texlump2 || Wads.GetLumpFile (texlump2a) <= pfile))
	{
		texlump2a = -1;
	}
	count += R_CountLumpTextures (texlump1a) - 1;
	count += R_CountLumpTextures (texlump2a) - 1;

	return count;
}

//===========================================================================
//
// R_CountLumpTextures
//
// Returns the number of patches in a PNAMES/TEXTURE1/TEXTURE2 lump.
//
//===========================================================================

static int R_CountLumpTextures (int lumpnum)
{
	if (lumpnum >= 0)
	{
		FWadLump file = Wads.OpenLumpNum (lumpnum); 
		DWORD numtex;

		file >> numtex;
		return numtex >= 0 ? numtex : 0;
	}
	return 0;
}

//===========================================================================
//
// R_DeinitData
//
//===========================================================================

void R_DeinitData ()
{
	R_DeinitColormaps ();
	R_DeinitBuildTiles();
	FCanvasTextureInfo::EmptyList();

	// Free openings
	if (openings != NULL)
	{
		free (openings);
		openings = NULL;
	}

	// Free drawsegs
	if (drawsegs != NULL)
	{
		free (drawsegs);
		drawsegs = NULL;
	}
}

//===========================================================================
//
// R_PrecacheLevel
//
// Preloads all relevant graphics for the level.
//
//===========================================================================

void R_PrecacheLevel (void)
{
	BYTE *hitlist;
	BYTE *spritelist;
	int i;

	if (demoplayback)
		return;

	hitlist = new BYTE[TexMan.NumTextures()];
	spritelist = new BYTE[sprites.Size()];
	
	// Precache textures (and sprites).
	memset (hitlist, 0, TexMan.NumTextures());
	memset (spritelist, 0, sprites.Size());

	{
		AActor *actor;
		TThinkerIterator<AActor> iterator;

		while ( (actor = iterator.Next ()) )
			spritelist[actor->sprite] = 1;
	}

	for (i = (int)(sprites.Size () - 1); i >= 0; i--)
	{
		if (spritelist[i])
		{
			int j, k;
			for (j = 0; j < sprites[i].numframes; j++)
			{
				const spriteframe_t *frame = &SpriteFrames[sprites[i].spriteframes + j];

				for (k = 0; k < 16; k++)
				{
					int pic = frame->Texture[k];
					if (pic != 0xFFFF)
					{
						hitlist[pic] = 1;
					}
				}
			}
		}
	}

	delete[] spritelist;

	for (i = numsectors - 1; i >= 0; i--)
	{
		hitlist[sectors[i].floorpic] = hitlist[sectors[i].ceilingpic] = 1;
	}

	for (i = numsides - 1; i >= 0; i--)
	{
		hitlist[sides[i].toptexture] =
			hitlist[sides[i].midtexture] =
			hitlist[sides[i].bottomtexture] = 1;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.

	if (sky1texture >= 0)
	{
		hitlist[sky1texture] = 1;
	}
	if (sky2texture >= 0)
	{
		hitlist[sky2texture] = 1;
	}

	for (i = TexMan.NumTextures() - 1; i >= 0; i--)
	{
		FTexture *tex = TexMan[i];
		if (tex != NULL)
		{
			if (hitlist[i])
			{
				tex->GetPixels ();
			}
			else
			{
				tex->Unload ();
			}
		}
	}

	delete[] hitlist;
}

const BYTE *R_GetColumn (FTexture *tex, int col)
{
	return tex->GetColumn (col, NULL);
}


#ifdef _DEBUG
// Prints the spans generated for a texture. Only needed for debugging.
CCMD (printspans)
{
	if (argv.argc() != 2)
		return;

	int picnum = TexMan.CheckForTexture (argv[1], FTexture::TEX_Any);
	if (picnum < 0)
	{
		Printf ("Unknown texture %s\n", argv[1]);
		return;
	}
	FTexture *tex = TexMan[picnum];
	for (int x = 0; x < tex->GetWidth(); ++x)
	{
		const FTexture::Span *spans;
		Printf ("%4d:", x);
		tex->GetColumn (x, &spans);
		while (spans->Length != 0)
		{
			Printf (" (%4d,%4d)", spans->TopOffset, spans->TopOffset+spans->Length-1);
			spans++;
		}
		Printf ("\n");
	}
}

CCMD (picnum)
{
	int picnum = TexMan.GetTexture (argv[1], FTexture::TEX_Any);
	Printf ("%d: %s - %s\n", picnum, TexMan[picnum]->Name, TexMan(picnum)->Name);
}
#endif
