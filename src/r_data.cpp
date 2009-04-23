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

#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "r_local.h"
#include "r_sky.h"
#include "c_dispatch.h"
#include "r_data.h"
#include "sc_man.h"
#include "v_text.h"
#include "st_start.h"
#include "doomstat.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "v_palette.h"


static int R_CountTexturesX ();
static int R_CountLumpTextures (int lumpnum);

extern void R_DeinitBuildTiles();
extern int R_CountBuildTiles();

struct FakeCmap 
{
	char name[8];
	PalEntry blend;
	int lump;
};

TArray<FakeCmap> fakecmaps;
BYTE *realcolormaps;
size_t numfakecmaps;

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
			Wads.GetLumpName (name, ns_colormaps);

			if (Wads.CheckNumForName (name, ns_colormaps) == i)
			{
				strncpy(cm.name, name, 8);
				cm.blend = 0;
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
	NormalLight.Maps = realcolormaps;
	numfakecmaps = fakecmaps.Size();
}

//==========================================================================
//
// R_DeinitColormaps
//
//==========================================================================

void R_DeinitColormaps ()
{
	if (realcolormaps != NULL)
	{
		delete[] realcolormaps;
		realcolormaps = NULL;
	}
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
		for(int i=fakecmaps.Size()-1; i > 0; i++)
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

//==========================================================================
//
// R_InitData
// Locates all the lumps that will be used by all views
// Must be called after W_Init.
//
//==========================================================================

void R_InitData ()
{
	StartScreen->Progress();

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
	int numtex = 0;
	
	for(int i = Wads.GetNumLumps()-1; i>=0; i--)
	{
		int space = Wads.GetLumpNamespace(i);
		switch(space)
		{
		case ns_flats:
		case ns_sprites:
		case ns_newtextures:
		case ns_hires:
		case ns_patches:
		case ns_graphics:
			numtex++;
			break;

		default:
			if (Wads.GetLumpFlags(i) & LUMPF_MAYBEFLAT) numtex++;

			break;
		}
	}

	numtex += R_CountBuildTiles ();
	numtex += R_CountTexturesX ();
	return numtex;
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
	int count = 0;
	int wadcount = Wads.GetNumWads();
	for (int wadnum = 0; wadnum < wadcount; wadnum++)
	{
		// Use the most recent PNAMES for this WAD.
		// Multiple PNAMES in a WAD will be ignored.
		int pnames = Wads.CheckNumForName("PNAMES", ns_global, wadnum, false);

		// should never happen except for zdoom.pk3
		if (pnames < 0) continue;

		// Only count the patches if the PNAMES come from the current file
		// Otherwise they have already been counted.
		if (Wads.GetLumpFile(pnames) == wadnum) 
		{
			count += R_CountLumpTextures (pnames);
		}

		int texlump1 = Wads.CheckNumForName ("TEXTURE1", ns_global, wadnum);
		int texlump2 = Wads.CheckNumForName ("TEXTURE2", ns_global, wadnum);

		count += R_CountLumpTextures (texlump1) - 1;
		count += R_CountLumpTextures (texlump2) - 1;
	}
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
		M_Free (openings);
		openings = NULL;
	}

	// Free drawsegs
	if (drawsegs != NULL)
	{
		M_Free (drawsegs);
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
					FTextureID pic = frame->Texture[k];
					if (pic.isValid())
					{
						hitlist[pic.GetIndex()] = 1;
					}
				}
			}
		}
	}

	delete[] spritelist;

	for (i = numsectors - 1; i >= 0; i--)
	{
		hitlist[sectors[i].GetTexture(sector_t::floor).GetIndex()] = 
			hitlist[sectors[i].GetTexture(sector_t::ceiling).GetIndex()] |= 2;
	}

	for (i = numsides - 1; i >= 0; i--)
	{
		hitlist[sides[i].GetTexture(side_t::top).GetIndex()] =
		hitlist[sides[i].GetTexture(side_t::mid).GetIndex()] =
		hitlist[sides[i].GetTexture(side_t::bottom).GetIndex()] |= 1;
	}

	// Sky texture is always present.
	// Note that F_SKY1 is the name used to
	//	indicate a sky floor/ceiling as a flat,
	//	while the sky texture is stored like
	//	a wall texture, with an episode dependant
	//	name.

	if (sky1texture.isValid())
	{
		hitlist[sky1texture.GetIndex()] |= 1;
	}
	if (sky2texture.isValid())
	{
		hitlist[sky2texture.GetIndex()] |= 1;
	}

	for (i = TexMan.NumTextures() - 1; i >= 0; i--)
	{
		screen->PrecacheTexture(TexMan.ByIndex(i), hitlist[i]);
	}

	delete[] hitlist;
}

//==========================================================================
//
// R_GetColumn
//
//==========================================================================

const BYTE *R_GetColumn (FTexture *tex, int col)
{
	return tex->GetColumn (col, NULL);
}


//==========================================================================
//
// Debug stuff
//
//==========================================================================

#ifdef _DEBUG
// Prints the spans generated for a texture. Only needed for debugging.
CCMD (printspans)
{
	if (argv.argc() != 2)
		return;

	FTextureID picnum = TexMan.CheckForTexture (argv[1], FTexture::TEX_Any);
	if (!picnum.Exists())
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
	//int picnum = TexMan.GetTexture (argv[1], FTexture::TEX_Any);
	//Printf ("%d: %s - %s\n", picnum, TexMan[picnum]->Name, TexMan(picnum)->Name);
}
#endif
