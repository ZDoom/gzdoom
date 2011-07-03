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
#include "resources/colormaps.h"


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
// R_DeinitData
//
//===========================================================================

void R_DeinitData ()
{
	R_DeinitColormaps ();
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

	if (demoplayback)
		return;

	hitlist = new BYTE[TexMan.NumTextures()];
	memset (hitlist, 0, TexMan.NumTextures());

	screen->GetHitlist(hitlist);
	for (int i = TexMan.NumTextures() - 1; i >= 0; i--)
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
// GetVoxelRemap
//
// Calculates a remap table for the voxel's palette. Results are cached so
// passing the same palette repeatedly will not require repeated
// recalculations.
//
//==========================================================================

static BYTE *GetVoxelRemap(const BYTE *pal)
{
	static BYTE remap[256];
	static BYTE oldpal[768];
	static bool firsttime = true;

	if (firsttime || memcmp(oldpal, pal, 768) != 0)
	{ // Not the same palette as last time, so recalculate.
		firsttime = false;
		memcpy(oldpal, pal, 768);
		for (int i = 0; i < 256; ++i)
		{
			// The voxel palette uses VGA colors, so we have to expand it
			// from 6 to 8 bits per component.
			remap[i] = BestColor((uint32 *)GPalette.BaseColors,
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
		src = (kvxslab_t *)((BYTE *)src + slabzleng);
		dest = (kvxslab_t *)((BYTE *)dest + slabzleng);
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

static void RemapVoxelSlabs(kvxslab_t *dest, int size, const BYTE *remap)
{
	while (size >= 3)
	{
		int slabzleng = dest->zleng;

		for (int j = 0; j < slabzleng; ++j)
		{
			dest->col[j] = remap[dest->col[j]];
		}
		slabzleng += 3;
		dest = (kvxslab_t *)((BYTE *)dest + slabzleng);
		size -= slabzleng;
	}
}

//==========================================================================
//
// R_LoadKVX
//
//==========================================================================

FVoxel *R_LoadKVX(int lumpnum)
{
	const kvxslab_t *slabs[MAXVOXMIPS];
	FVoxel *voxel = new FVoxel;
	const BYTE *rawmip;
	int mip, maxmipsize;
	int i, j, n;

	FMemLump lump = Wads.ReadLump(lumpnum);	// FMemLump adds an extra 0 byte to the end.
	BYTE *rawvoxel = (BYTE *)lump.GetMem();
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
		mipl->PivotX = GetInt(rawmip + 12);
		mipl->PivotY = GetInt(rawmip + 16);
		mipl->PivotZ = GetInt(rawmip + 20);

		// How much space do we have for voxdata?
		int offsetsize = (mipl->SizeX + 1) * 4 + mipl->SizeX * (mipl->SizeY + 1) * 2;
		int voxdatasize = numbytes - 24 - offsetsize;
		if (voxdatasize < 0)
		{ // Clearly, not enough.
			break;
		}
		if (voxdatasize == 0)
		{ // This mip level is empty.
			goto nextmip;
		}

		// Allocate slab data space.
		mipl->OffsetX = new int[(numbytes - 24 + 3) / 4];
		mipl->OffsetXY = (short *)(mipl->OffsetX + mipl->SizeX + 1);
		mipl->SlabData = (BYTE *)(mipl->OffsetXY + mipl->SizeX * (mipl->SizeY + 1));

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
					goto bad;
				}
			}
		}

		// Record slab location for the end.
		slabs[mip] = (kvxslab_t *)(rawmip + 24 + offsetsize);

		// Time for the next mip Level.
nextmip:
		rawmip += numbytes;
		maxmipsize -= numbytes + 4;
	}
	// Did we get any mip levels, and if so, does the last one leave just
	// enough room for the palette after it?
	if (mip == 0 || rawmip != rawvoxel + voxelsize - 768)
	{
bad:	delete voxel;
		return NULL;
	}

	// Do not count empty mips at the end.
	for (; mip > 0; --mip)
	{
		if (voxel->Mips[mip - 1].SlabData != NULL)
			break;
	}
	voxel->NumMips = mip;

	for (i = 0; i < mip; ++i)
	{
		if (!CopyVoxelSlabs((kvxslab_t *)voxel->Mips[i].SlabData, slabs[i], voxel->Mips[i].OffsetX[voxel->Mips[i].SizeX]))
		{ // Invalid slabs encountered. Reject this voxel.
			delete voxel;
			return NULL;
		}
	}

	voxel->LumpNum = lumpnum;
	voxel->Palette = new BYTE[768];
	memcpy(voxel->Palette, rawvoxel + voxelsize - 768, 768);

	return voxel;
}

//==========================================================================
//
// FVoxelMipLevel Constructor
//
//==========================================================================

FVoxelMipLevel::FVoxelMipLevel()
{
	SizeZ = SizeY = SizeX = 0;
	PivotZ = PivotY = PivotX = 0;
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
// Remap the voxel to the game palette
//
//==========================================================================

void FVoxel::Remap()
{
	if (Palette != NULL)
	{
		BYTE *remap = GetVoxelRemap(Palette);
		for (int i = 0; i < NumMips; ++i)
		{
			RemapVoxelSlabs((kvxslab_t *)Mips[i].SlabData, Mips[i].OffsetX[Mips[i].SizeX], remap);
		}
		delete [] Palette;
		Palette = NULL;
	}
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
#endif
