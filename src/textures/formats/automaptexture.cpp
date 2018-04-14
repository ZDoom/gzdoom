/*
** automaptexture.cpp
** Texture class for Raven's automap parchment
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
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
** This texture type is only used as a last resort when everything else has failed for creating 
** the AUTOPAGE texture. That's because Raven used a raw lump of non-standard proportions to define it.
**
*/

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "v_palette.h"
#include "textures/textures.h"

//==========================================================================
//
// A raw 320x? graphic used by Heretic and Hexen for the automap parchment
//
//==========================================================================

class FAutomapTexture : public FWorldTexture
{
public:
	FAutomapTexture(int lumpnum);
	uint8_t *MakeTexture (FRenderStyle style);
};



//==========================================================================
//
// This texture type will only be used for the AUTOPAGE lump if no other
// format matches.
//
//==========================================================================

FTexture *AutomapTexture_TryCreate(FileReader &data, int lumpnum)
{
	if (data.GetLength() < 320) return NULL;
	if (!Wads.CheckLumpName(lumpnum, "AUTOPAGE")) return NULL;
	return new FAutomapTexture(lumpnum);
}

//==========================================================================
//
//
//
//==========================================================================

FAutomapTexture::FAutomapTexture (int lumpnum)
: FWorldTexture(NULL, lumpnum)
{
	Width = 320;
	Height = uint16_t(Wads.LumpLength(lumpnum) / 320);
	CalcBitSize ();
}

//==========================================================================
//
//
//
//==========================================================================

uint8_t *FAutomapTexture::MakeTexture (FRenderStyle style)
{
	int x, y;
	FMemLump data = Wads.ReadLump (SourceLump);
	const uint8_t *indata = (const uint8_t *)data.GetMem();

	auto Pixels = new uint8_t[Width * Height];

	const uint8_t *remap = GetRemap(style);
	for (x = 0; x < Width; ++x)
	{
		for (y = 0; y < Height; ++y)
		{
			auto p = indata[x + 320 * y];
			Pixels[x*Height + y] = remap[p];
		}
	}
	return Pixels;
}

