/*
** voxels.cpp
**
**---------------------------------------------------------------------------
** Copyright 2010-2011 Randy Heit
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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>

#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "m_argv.h"
#include "i_system.h"
#include "w_wad.h"
#include "c_console.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "doomstat.h"
#include "v_video.h"
#include "sc_man.h"
#include "s_sound.h"
#include "sbar.h"
#include "gi.h"
#include "r_sky.h"
#include "cmdlib.h"
#include "g_level.h"
#include "d_net.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "v_palette.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"
#include "r_data/sprites.h"
#include "voxels.h"
#include "info.h"

void VOX_AddVoxel(int sprnum, int frame, FVoxelDef *def);

TDeletingArray<FVoxel *> Voxels;	// used only to auto-delete voxels on exit.
TDeletingArray<FVoxelDef *> VoxelDefs;

struct VoxelOptions
{
	VoxelOptions()
	: DroppedSpin(0), PlacedSpin(0), Scale(1.), AngleOffset(90.), OverridePalette(false)
	{}

	int			DroppedSpin;
	int			PlacedSpin;
	double		Scale;
	DAngle		AngleOffset;
	bool		OverridePalette;
};

//==========================================================================
//
// GetVoxelRemap
//
// Calculates a remap table for the voxel's palette. Results are cached so
// passing the same palette repeatedly will not require repeated
// recalculations.
//
//==========================================================================

static uint8_t *GetVoxelRemap(const uint8_t *pal)
{
	static uint8_t remap[256];
	static uint8_t oldpal[768];
	static bool firsttime = true;

	if (firsttime || memcmp(oldpal, pal, 768) != 0)
	{ // Not the same palette as last time, so recalculate.
		firsttime = false;
		memcpy(oldpal, pal, 768);
		for (int i = 0; i < 256; ++i)
		{
			// The voxel palette uses VGA colors, so we have to expand it
			// from 6 to 8 bits per component.
			remap[i] = BestColor((uint32_t *)GPalette.BaseColors,
				(oldpal[i*3 + 0] << 2) | (oldpal[i*3 + 0] >> 4),
				(oldpal[i*3 + 1] << 2) | (oldpal[i*3 + 1] >> 4),
				(oldpal[i*3 + 2] << 2) | (oldpal[i*3 + 2] >> 4));
		}
	}
	return remap;
}

//==========================================================================
//
// CopyVoxelSlabs
//
// Copy all the slabs in a block of slabs.
//
//==========================================================================

static bool CopyVoxelSlabs(kvxslab_t *dest, const kvxslab_t *src, int size)
{
	while (size >= 3)
	{
		int slabzleng = src->zleng;

		if (3 + slabzleng > size)
		{ // slab is too tall
			return false;
		}

		dest->ztop = src->ztop;
		dest->zleng = src->zleng;
		dest->backfacecull = src->backfacecull;

		for (int j = 0; j < slabzleng; ++j)
		{
			dest->col[j] = src->col[j];
		}
		slabzleng += 3;
		src = (kvxslab_t *)((uint8_t *)src + slabzleng);
		dest = (kvxslab_t *)((uint8_t *)dest + slabzleng);
		size -= slabzleng;
	}
	return true;
}

//==========================================================================
//
// RemapVoxelSlabs
//
// Remaps all the slabs in a block of slabs.
//
//==========================================================================

static void RemapVoxelSlabs(kvxslab_t *dest, int size, const uint8_t *remap)
{
	while (size >= 3)
	{
		int slabzleng = dest->zleng;

		for (int j = 0; j < slabzleng; ++j)
		{
			dest->col[j] = remap[dest->col[j]];
		}
		slabzleng += 3;
		dest = (kvxslab_t *)((uint8_t *)dest + slabzleng);
		size -= slabzleng;
	}
}

//==========================================================================
//
// R_LoadKVX
//
//==========================================================================

#if defined __GNUC__ && !defined __clang__
#pragma GCC push_options
#pragma GCC optimize ("-fno-tree-loop-vectorize")
#endif // __GNUC__ && !__clang__

FVoxel *R_LoadKVX(int lumpnum)
{
	const kvxslab_t *slabs[MAXVOXMIPS];
	FVoxel *voxel = new FVoxel;
	const uint8_t *rawmip;
	int mip, maxmipsize;
	int i, j, n;

	FMemLump lump = Wads.ReadLump(lumpnum);	// FMemLump adds an extra 0 byte to the end.
	uint8_t *rawvoxel = (uint8_t *)lump.GetMem();
	int voxelsize = (int)(lump.GetSize()-1);

	// Oh, KVX, why couldn't you have a proper header? We'll just go through
	// and collect each MIP level, doing lots of range checking, and if the
	// last one doesn't end exactly 768 bytes before the end of the file,
	// we'll reject it.

	for (mip = 0, rawmip = rawvoxel, maxmipsize = voxelsize - 768 - 4;
		 mip < MAXVOXMIPS;
		 mip++)
	{
		int numbytes = GetInt(rawmip);
		if (numbytes > maxmipsize || numbytes < 24)
		{
			break;
		}
		rawmip += 4;

		FVoxelMipLevel *mipl = &voxel->Mips[mip];

		// Load header data.
		mipl->SizeX = GetInt(rawmip + 0);
		mipl->SizeY = GetInt(rawmip + 4);
		mipl->SizeZ = GetInt(rawmip + 8);
		mipl->Pivot.X = GetInt(rawmip + 12) / 256.;
		mipl->Pivot.Y = GetInt(rawmip + 16) / 256.;
		mipl->Pivot.Z = GetInt(rawmip + 20) / 256.;

		// How much space do we have for voxdata?
		int offsetsize = (mipl->SizeX + 1) * 4 + mipl->SizeX * (mipl->SizeY + 1) * 2;
		int voxdatasize = numbytes - 24 - offsetsize;
		if (voxdatasize < 0)
		{ // Clearly, not enough.
			break;
		}
		if (voxdatasize != 0)
		{	// This mip level is not empty.
			// Allocate slab data space.
			mipl->OffsetX = new int[(numbytes - 24 + 3) / 4];
			mipl->OffsetXY = (short *)(mipl->OffsetX + mipl->SizeX + 1);
			mipl->SlabData = (uint8_t *)(mipl->OffsetXY + mipl->SizeX * (mipl->SizeY + 1));

			// Load x offsets.
			for (i = 0, n = mipl->SizeX; i <= n; ++i)
			{
				// The X offsets stored in the KVX file are relative to the start of the
				// X offsets array. Make them relative to voxdata instead.
				mipl->OffsetX[i] = GetInt(rawmip + 24 + i * 4) - offsetsize;
			}

			// The first X offset must be 0 (since we subtracted offsetsize), according to the spec:
			//		NOTE: xoffset[0] = (xsiz+1)*4 + xsiz*(ysiz+1)*2 (ALWAYS)
			if (mipl->OffsetX[0] != 0)
			{
				break;
			}
			// And the final X offset must point just past the end of the voxdata.
			if (mipl->OffsetX[mipl->SizeX] != voxdatasize)
			{
				break;
			}

			// Load xy offsets.
			i = 24 + i * 4;
			for (j = 0, n *= mipl->SizeY + 1; j < n; ++j)
			{
				mipl->OffsetXY[j] = GetShort(rawmip + i + j * 2);
			}

			// Ensure all offsets are within bounds.
			for (i = 0; i < mipl->SizeX; ++i)
			{
				int xoff = mipl->OffsetX[i];
				for (j = 0; j < mipl->SizeY; ++j)
				{
					int yoff = mipl->OffsetXY[(mipl->SizeY + 1) * i + j];
					if (unsigned(xoff + yoff) > unsigned(voxdatasize))
					{
						delete voxel;
						return NULL;
					}
				}
			}

			// Record slab location for the end.
			slabs[mip] = (kvxslab_t *)(rawmip + 24 + offsetsize);
		}

		// Time for the next mip Level.
		rawmip += numbytes;
		maxmipsize -= numbytes + 4;
	}
	// Did we get any mip levels, and if so, does the last one leave just
	// enough room for the palette after it?
	if (mip == 0 || rawmip != rawvoxel + voxelsize - 768)
	{
		delete voxel;
		return NULL;
	}

	// Do not count empty mips at the end.
	for (; mip > 0; --mip)
	{
		if (voxel->Mips[mip - 1].SlabData != NULL)
			break;
	}
	voxel->NumMips = mip;

	// Fix pivot data for submips, since some tools seem to like to just center these.
	for (i = 1; i < mip; ++i)
	{
		voxel->Mips[i].Pivot = voxel->Mips[i - 1].Pivot / 2;
	}

	for (i = 0; i < mip; ++i)
	{
		if (!CopyVoxelSlabs((kvxslab_t *)voxel->Mips[i].SlabData, slabs[i], voxel->Mips[i].OffsetX[voxel->Mips[i].SizeX]))
		{ // Invalid slabs encountered. Reject this voxel.
			delete voxel;
			return NULL;
		}
	}

	voxel->LumpNum = lumpnum;
	voxel->Palette = new uint8_t[768];
	memcpy(voxel->Palette, rawvoxel + voxelsize - 768, 768);

	return voxel;
}

#if defined __GNUC__ && !defined __clang__
#pragma GCC pop_options
#endif // __GNUC__ && !__clang__

//==========================================================================
//
//
//
//==========================================================================

FVoxelDef *R_LoadVoxelDef(int lumpnum, int spin)
{
	FVoxel *vox = R_LoadKVX(lumpnum);
	if (vox == NULL)
	{
		Printf("%s is not a valid voxel file\n", Wads.GetLumpFullName(lumpnum));
		return NULL;
	}
	else
	{
		FVoxelDef *voxdef = new FVoxelDef;
		voxdef->Voxel = vox;
		voxdef->Scale = 1.;
		voxdef->DroppedSpin = voxdef->PlacedSpin = spin;
		voxdef->AngleOffset = 90.;

		Voxels.Push(vox);
		VoxelDefs.Push(voxdef);
		return voxdef;
	}
}

//==========================================================================
//
// FVoxelMipLevel Constructor
//
//==========================================================================

FVoxelMipLevel::FVoxelMipLevel()
{
	SizeZ = SizeY = SizeX = 0;
	Pivot.Zero();
	OffsetX = NULL;
	OffsetXY = NULL;
	SlabData = NULL;
}

//==========================================================================
//
// FVoxelMipLevel Destructor
//
//==========================================================================

FVoxelMipLevel::~FVoxelMipLevel()
{
	if (OffsetX != NULL)
	{
		delete[] OffsetX;
	}
}

//==========================================================================
//
// FVoxel Constructor
//
//==========================================================================

FVoxel::FVoxel()
{
	Palette = NULL;
}

FVoxel::~FVoxel()
{
	if (Palette != NULL) delete [] Palette;
}

//==========================================================================
//
// Create true color version of the slab data
//
//==========================================================================

void FVoxel::CreateBgraSlabData()
{
	for (int i = 0; i < NumMips; ++i)
	{
		int size = Mips[i].OffsetX[Mips[i].SizeX];
		if (size <= 0) continue;

		Mips[i].SlabDataBgra.Resize(size);

		kvxslab_t *src = (kvxslab_t*)Mips[i].SlabData;
		kvxslab_bgra_t *dest = (kvxslab_bgra_t*)&Mips[i].SlabDataBgra[0];

		while (size >= 3)
		{
			dest->backfacecull = src->backfacecull;
			dest->ztop = src->ztop;
			dest->zleng = src->zleng;

			int slabzleng = src->zleng;
			for (int j = 0; j < slabzleng; ++j)
			{
				int colorIndex = src->col[j];

				uint32_t red, green, blue;
				if (Palette)
				{
					red = (Palette[colorIndex * 3 + 0] << 2) | (Palette[colorIndex * 3 + 0] >> 4);
					green = (Palette[colorIndex * 3 + 1] << 2) | (Palette[colorIndex * 3 + 1] >> 4);
					blue = (Palette[colorIndex * 3 + 2] << 2) | (Palette[colorIndex * 3 + 2] >> 4);
				}
				else
				{
					red = GPalette.BaseColors[colorIndex].r;
					green = GPalette.BaseColors[colorIndex].g;
					blue = GPalette.BaseColors[colorIndex].b;
				}

				dest->col[j] = 0xff000000 | (red << 16) | (green << 8) | blue;
			}
			slabzleng += 3;

			dest = (kvxslab_bgra_t *)((uint32_t *)dest + slabzleng);
			src = (kvxslab_t *)((uint8_t *)src + slabzleng);
			size -= slabzleng;
		}
	}
}

//==========================================================================
//
// Remap the voxel to the game palette
//
//==========================================================================

void FVoxel::Remap()
{
	if (Palette != NULL)
	{
		uint8_t *remap = GetVoxelRemap(Palette);
		for (int i = 0; i < NumMips; ++i)
		{
			RemapVoxelSlabs((kvxslab_t *)Mips[i].SlabData, Mips[i].OffsetX[Mips[i].SizeX], remap);
		}
		RemovePalette();
	}
}

//==========================================================================
//
// Delete the voxel's built-in palette
//
//==========================================================================

void FVoxel::RemovePalette()
{
	if (Palette != NULL)
	{
		delete [] Palette;
		Palette = NULL;
	}
}



//==========================================================================
//
// VOX_ReadSpriteNames
//
// Reads a list of sprite names from a VOXELDEF lump.
//
//==========================================================================

static bool VOX_ReadSpriteNames(FScanner &sc, TArray<uint32_t> &vsprites)
{
	vsprites.Clear();
	while (sc.GetString())
	{
		// A sprite name list is terminated by an '=' character.
		if (sc.String[0] == '=')
		{
			if (vsprites.Size() == 0)
			{
				sc.ScriptMessage("No sprites specified for voxel.\n");
			}
			return true;
		}
		if (sc.StringLen != 4 && sc.StringLen != 5)
		{
			sc.ScriptMessage("Sprite name \"%s\" is wrong size.\n", sc.String);
		}
		else if (sc.StringLen == 5 && (sc.String[4] = toupper(sc.String[4]), sc.String[4] < 'A' || sc.String[4] >= 'A' + MAX_SPRITE_FRAMES))
		{
			sc.ScriptMessage("Sprite frame %c is invalid.\n", sc.String[4]);
		}
		else
		{
			int frame = (sc.StringLen == 4) ? 255 : sc.String[4] - 'A';
			int i = GetSpriteIndex(sc.String, false);
			if (i != -1)
			{
				vsprites.Push((frame << 24) | i);
			}
		}
	}
	if (vsprites.Size() != 0)
	{
		sc.ScriptMessage("Unexpected end of file\n");
	}
	return false;
}

//==========================================================================
//
// VOX_ReadOptions
//
// Reads a list of options from a VOXELDEF lump, terminated with a '}'
// character. The leading '{' must already be consumed
//
//==========================================================================

static void VOX_ReadOptions(FScanner &sc, VoxelOptions &opts)
{
	while (sc.GetToken())
	{
		if (sc.TokenType == '}')
		{
			return;
		}
		sc.TokenMustBe(TK_Identifier);
		if (sc.Compare("scale"))
		{
			sc.MustGetToken('=');
			sc.MustGetToken(TK_FloatConst);
			opts.Scale = sc.Float;
		}
		else if (sc.Compare("spin"))
		{
			int mul = 1;
			sc.MustGetToken('=');
			if (sc.CheckToken('-')) mul = -1;
			sc.MustGetToken(TK_IntConst);
			opts.DroppedSpin = opts.PlacedSpin = sc.Number*mul;
		}
		else if (sc.Compare("placedspin"))
		{
			int mul = 1;
			sc.MustGetToken('=');
			if (sc.CheckToken('-')) mul = -1;
			sc.MustGetToken(TK_IntConst);
			opts.PlacedSpin = sc.Number*mul;
		}
		else if (sc.Compare("droppedspin"))
		{
			int mul = 1;
			sc.MustGetToken('=');
			if (sc.CheckToken('-')) mul = -1;
			sc.MustGetToken(TK_IntConst);
			opts.DroppedSpin = sc.Number*mul;
		}
		else if (sc.Compare("angleoffset"))
		{
			int mul = 1;
			sc.MustGetToken('=');
			if (sc.CheckToken('-')) mul = -1;
			sc.MustGetAnyToken();
			if (sc.TokenType == TK_IntConst)
			{
				sc.Float = sc.Number;
			}
			else
			{
				sc.TokenMustBe(TK_FloatConst);
			}
			opts.AngleOffset = mul * sc.Float + 90.;
		}
		else if (sc.Compare("overridepalette"))
		{
			opts.OverridePalette = true;
		}
		else
		{
			sc.ScriptMessage("Unknown voxel option '%s'\n", sc.String);
			if (sc.CheckToken('='))
			{
				sc.MustGetAnyToken();
			}
		}
	}
	sc.ScriptMessage("Unterminated voxel option block\n");
}

//==========================================================================
//
// VOX_GetVoxel
//
// Returns a voxel object for the given lump or NULL if it is not a valid
// voxel. If the voxel has already been loaded, it will be reused.
//
//==========================================================================

static FVoxel *VOX_GetVoxel(int lumpnum)
{
	// Is this voxel already loaded? If so, return it.
	for (unsigned i = 0; i < Voxels.Size(); ++i)
	{
		if (Voxels[i]->LumpNum == lumpnum)
		{
			return Voxels[i];
		}
	}
	FVoxel *vox = R_LoadKVX(lumpnum);
	if (vox != NULL)
	{
		Voxels.Push(vox);
	}
	return vox;
}

//==========================================================================
//
// R_InitVoxels
//
// Process VOXELDEF lumps for defining voxel options that cannot be
// condensed neatly into a sprite name format.
//
//==========================================================================

void R_InitVoxels()
{
	int lump, lastlump = 0;
	
	while ((lump = Wads.FindLump("VOXELDEF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		TArray<uint32_t> vsprites;

		while (VOX_ReadSpriteNames(sc, vsprites))
		{
			FVoxel *voxeldata = NULL;
			int voxelfile;
			VoxelOptions opts;

			sc.SetCMode(true);
			sc.MustGetToken(TK_StringConst);
			voxelfile = Wads.CheckNumForFullName(sc.String, true, ns_voxels);
			if (voxelfile < 0)
			{
				sc.ScriptMessage("Voxel \"%s\" not found.\n", sc.String);
			}
			else
			{
				voxeldata = VOX_GetVoxel(voxelfile);
				if (voxeldata == NULL)
				{
					sc.ScriptMessage("\"%s\" is not a valid voxel file.\n", sc.String);
				}
			}
			if (sc.CheckToken('{'))
			{
				VOX_ReadOptions(sc, opts);
			}
			sc.SetCMode(false);
			if (voxeldata != NULL && vsprites.Size() != 0)
			{
				if (opts.OverridePalette)
				{
					voxeldata->RemovePalette();
				}
				FVoxelDef *def = new FVoxelDef;

				def->Voxel = voxeldata;
				def->Scale = opts.Scale;
				def->DroppedSpin = opts.DroppedSpin;
				def->PlacedSpin = opts.PlacedSpin;
				def->AngleOffset = opts.AngleOffset;
				VoxelDefs.Push(def);

				for (unsigned i = 0; i < vsprites.Size(); ++i)
				{
					int sprnum = int(vsprites[i] & 0xFFFFFF);
					int frame = int(vsprites[i] >> 24);
					if (frame == 255)
					{ // Apply voxel to all frames.
						for (int j = MAX_SPRITE_FRAMES - 1; j >= 0; --j)
						{
							VOX_AddVoxel(sprnum, j, def);
						}
					}
					else
					{ // Apply voxel to only one frame.
						VOX_AddVoxel(sprnum, frame, def);
					}
				}
			}
		}
	}
}

