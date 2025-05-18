/*
** buildtexture.cpp
** Handling Build textures (now as a usable editing feature!)
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2018 Christoph Oelckers
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

#include "files.h"
#include "filesystem.h"

#include "cmdlib.h"
#include "colormatcher.h"
#include "bitmap.h"
#include "textures.h"
#include "fs_filesystem.h"
#include "image.h"
#include "animations.h"
#include "texturemanager.h"
#include "r_translate.h"
#include "r_data/sprites.h"
#include "m_swap.h"

//===========================================================================
//
// CountTiles
//
// Returns the number of tiles provided by an artfile
//
//===========================================================================

static int CountTiles (const void *tiles)
{
	int version = LittleLong(*(uint32_t *)tiles);
	if (version != 1)
	{
		return 0;
	}

	int tilestart = LittleLong(((uint32_t *)tiles)[2]);
	int tileend = LittleLong(((uint32_t *)tiles)[3]);

	return tileend >= tilestart ? tileend - tilestart + 1 : 0;
}

//===========================================================================
//
// Create palette data and remap table for the tile set's palette
//
//===========================================================================

static int BuildPaletteTranslation(int lump)
{
	if (fileSystem.FileLength(lump) < 768)
	{
		return false;
	}

	auto data = fileSystem.ReadFile(lump);
	auto ipal = data.bytes();
	FRemapTable opal;

	bool blood = false;
	for (int c = 0; c < 765; c++)	// Build used VGA palettes (color values 0..63), Blood used full palettes (0..255) Let's hope this doesn't screw up...
	{
		if (ipal[c] >= 64)
		{
			blood = true;
			break;
		}
	}

	for (int c = 0; c < 255; c++)
	{
		int r, g, b;
		if (!blood)
		{
			r = (ipal[3*c    ] << 2) | (ipal[3 * c    ] >> 4);
			g = (ipal[3*c + 1] << 2) | (ipal[3 * c + 1] >> 4);
			b = (ipal[3*c + 2] << 2) | (ipal[3 * c + 2] >> 4);
		}
		else
		{
			r = ipal[3 * c] << 2;
			g = ipal[3 * c + 1] << 2;
			b = ipal[3 * c + 2] << 2;
		}
		opal.Palette[c] = PalEntry(255, r, g, b);
		opal.Remap[c] = ColorMatcher.Pick(r, g, b);
	}
	// The last entry is transparent.
	opal.Palette[255] = 0;
	opal.Remap[255] = 0;
	// Store the remap table in the translation manager so that we do not need to keep track of it ourselves. 
	// Slot 0 for internal translations is a convenient location because normally it only contains a small number of translations.
	return GetTranslationIndex(GPalette.StoreTranslation(TRANSLATION_Standard, &opal));
}


//===========================================================================
//
// AddTiles
//
// Adds all the tiles in an artfile to the texture manager.
//
//===========================================================================

void AddTiles(const FString& pathprefix, const void* tiles, FRemapTable *remap)
{

	//	int numtiles = LittleLong(((uint32_t *)tiles)[1]);	// This value is not reliable
	int tilestart = LittleLong(((uint32_t*)tiles)[2]);
	int tileend = LittleLong(((uint32_t*)tiles)[3]);
	const uint16_t* tilesizx = &((const uint16_t*)tiles)[8];
	const uint16_t* tilesizy = &tilesizx[tileend - tilestart + 1];
	const uint32_t* picanm = (const uint32_t*)&tilesizy[tileend - tilestart + 1];
	const uint8_t* tiledata = (const uint8_t*)&picanm[tileend - tilestart + 1];

	for (int i = tilestart; i <= tileend; ++i)
	{
		int pic = i - tilestart;
		int width = LittleShort(tilesizx[pic]);
		int height = LittleShort(tilesizy[pic]);
		uint32_t anm = LittleLong(picanm[pic]);
		int xoffs = (int8_t)((anm >> 8) & 255) + width / 2;
		int yoffs = (int8_t)((anm >> 16) & 255) + height / 2;
		int size = width * height;
		FTextureID texnum;

		if (width <= 0 || height <= 0) continue;

		FStringf name("%sBTIL%04d", pathprefix.GetChars(), i);
		auto tex = MakeGameTexture(new FImageTexture(new FBuildTexture(pathprefix, i, tiledata, remap, width, height, xoffs, yoffs)), name.GetChars(), ETextureType::Override);
		texnum = TexMan.AddGameTexture(tex);
		tiledata += size;


		// reactivate only if the texture counter works here.
		//StartScreen->Progress();

		if ((picanm[pic] & 63) && (picanm[pic] & 192))
		{
			int type, speed;

			switch (picanm[pic] & 192)
			{
			case 64:	type = 2;	break;
			case 128:	type = 0;	break;
			case 192:	type = 1;	break;
			default:    type = 0;   break;  // Won't happen, but GCC bugs me if I don't put this here.
			}

			speed = (anm >> 24) & 15;
			speed = max(1, (1 << speed) * 1000 / 120);	// Convert from 120 Hz to 1000 Hz.

			TexAnim.AddSimpleAnim(texnum, picanm[pic] & 63, type, speed);
		}

		// Blood's rotation types:
		// 0 - Single
		// 1 - 5 Full
		// 2 - 8 Full
		// 3 - Bounce (looks no different from Single; seems to signal bouncy sprites)
		// 4 - 5 Half (not used in game)
		// 5 - 3 Flat (not used in game)
		// 6 - Voxel
		// 7 - Spin Voxel

		int rotType = (anm >> 28) & 7;
		if (rotType == 1)
		{
			spriteframe_t rot;
			rot.Texture[0] =
				rot.Texture[1] = texnum;
			for (int j = 1; j < 4; ++j)
			{
				rot.Texture[j * 2].SetIndex(texnum.GetIndex() + j);
				rot.Texture[j * 2 + 1].SetIndex(texnum.GetIndex() + j);
				rot.Texture[16 - j * 2].SetIndex(texnum.GetIndex() + j);
				rot.Texture[17 - j * 2].SetIndex(texnum.GetIndex() + j);
			}
			rot.Texture[8].SetIndex(texnum.GetIndex());
			rot.Texture[9].SetIndex(texnum.GetIndex());
			rot.Flip = 0x00FC;
			rot.Voxel = NULL;
			tex->SetRotations(SpriteFrames.Push(rot));
		}
		else if (rotType == 2)
		{
			spriteframe_t rot;
			rot.Texture[0] =
				rot.Texture[1] = texnum;
			for (int j = 1; j < 8; ++j)
			{
				rot.Texture[16 - j * 2].SetIndex(texnum.GetIndex() + j);
				rot.Texture[17 - j * 2].SetIndex(texnum.GetIndex() + j);
			}
			rot.Flip = 0;
			rot.Voxel = NULL;
			tex->SetRotations(SpriteFrames.Push(rot));
		}
	}
}

//===========================================================================
//
// R_CountBuildTiles
//
// Returns the number of tiles found. Also loads all the data for
// R_InitBuildTiles() to process later.
//
//===========================================================================

void InitBuildTiles()
{
	int lumpnum;
	int numtiles;
	int totaltiles = 0;

	// The search rules are as follows: 
	// - scan the entire lump directory for palette.dat files.
	// - if one is found, process the directory for .ART files and add textures for them.
	// - once all have been found, process all directories that may contain Build data.
	// - the root is not excluded which allows to read this from .GRP files as well.
	// - Blood support has been removed because it is not useful for modding to have loose .ART files.
	//
	// Unfortunately neither the palettes nor the .ART files contain any usable identifying marker
	// so this can only go by the file names.

	int numlumps = fileSystem.GetNumEntries();
	for (int i = 0; i < numlumps; i++)
	{
		const char* name = fileSystem.GetFileFullName(i);
		if (fileSystem.CheckNumForFullName(name) != i) continue;	// This palette is hidden by a later one. Do not process
		FString base = ExtractFileBase(name, true);
		base.ToLower();
		if (base.Compare("palette.dat") == 0 && fileSystem.FileLength(i) >= 768)	// must be a valid palette, i.e. at least 256 colors.
		{
			FString path = ExtractFilePath(name);
			if (path.IsNotEmpty() && path.Back() != '/') path += '/';

			int translation = BuildPaletteTranslation(i);
			auto remap = GPalette.GetTranslation(TRANSLATION_Standard, translation);

			for (int numartfiles = 0; numartfiles < 1000; numartfiles++)
			{
				FStringf artpath("%stiles%03d.art", path.GetChars(), numartfiles);
				// only read from the same source as the palette.
				// The entire format here is just too volatile to allow liberal mixing.
				// An .ART set must be treated as one unit.
				lumpnum = fileSystem.CheckNumForFullName(artpath.GetChars(), fileSystem.GetFileContainer(i));
				if (lumpnum < 0)
				{
					break;
				}

				auto& artdata = TexMan.GetNewBuildTileData();

				ptrdiff_t len = fileSystem.FileLength(lumpnum);

				assert(len >= 0 && len < UINT_MAX);

				artdata.Resize((unsigned int)len);
				fileSystem.ReadFile(lumpnum, &artdata[0]);

				if ((numtiles = CountTiles(&artdata[0])) > 0)
				{
					AddTiles(path, &artdata[0], remap);
					totaltiles += numtiles;
				}
			}
		}
	}
}

