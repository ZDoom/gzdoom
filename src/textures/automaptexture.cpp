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
#include "textures/textures.h"

//==========================================================================
//
// A raw 320x? graphic used by Heretic and Hexen for the automap parchment
//
//==========================================================================

class FAutomapTexture : public FTexture
{
public:
	~FAutomapTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	void MakeTexture ();

	FAutomapTexture (int lumpnum);

private:
	BYTE *Pixels;
	Span DummySpan[2];
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
: FTexture(NULL, lumpnum), Pixels(NULL)
{
	Width = 320;
	Height = WORD(Wads.LumpLength(lumpnum) / 320);
	CalcBitSize ();

	DummySpan[0].TopOffset = 0;
	DummySpan[0].Length = Height;
	DummySpan[1].TopOffset = 0;
	DummySpan[1].Length = 0;
}

//==========================================================================
//
//
//
//==========================================================================

FAutomapTexture::~FAutomapTexture ()
{
	Unload ();
}

//==========================================================================
//
//
//
//==========================================================================

void FAutomapTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FAutomapTexture::MakeTexture ()
{
	int x, y;
	FMemLump data = Wads.ReadLump (SourceLump);
	const BYTE *indata = (const BYTE *)data.GetMem();

	Pixels = new BYTE[Width * Height];

	for (x = 0; x < Width; ++x)
	{
		for (y = 0; y < Height; ++y)
		{
			Pixels[x*Height+y] = indata[x+320*y];
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FAutomapTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FAutomapTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	if ((unsigned)column >= (unsigned)Width)
	{
		column %= Width;
	}
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + column*Height;
}
