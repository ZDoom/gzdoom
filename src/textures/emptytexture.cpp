/*
** flattexture.cpp
** Texture class for empty placeholder textures
** (essentially patches with dimensions and offsets of (0,0) )
**
**---------------------------------------------------------------------------
** Copyright 2009 Christoph Oelckers
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

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "textures/textures.h"

//==========================================================================
//
// A texture defined between F_START and F_END markers
//
//==========================================================================

class FEmptyTexture : public FTexture
{
public:
	FEmptyTexture (int lumpnum);

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload() {}

protected:
	BYTE Pixels[1];
	Span DummySpans[1];
};



//==========================================================================
//
// Since there is no way to detect the validity of a flat
// they can't be used anywhere else but between F_START and F_END
//
//==========================================================================

FTexture *EmptyTexture_TryCreate(FileReader & file, int lumpnum)
{
	char check[8];
	if (file.GetLength() != 8) return NULL;
	file.Seek(0, SEEK_SET);
	if (file.Read(check, 8) != 8) return NULL;
	if (memcmp(check, "\0\0\0\0\0\0\0\0", 8)) return NULL;

	return new FEmptyTexture(lumpnum);
}

//==========================================================================
//
//
//
//==========================================================================

FEmptyTexture::FEmptyTexture (int lumpnum)
: FTexture(NULL, lumpnum)
{
	bMasked = true;
	WidthBits = HeightBits = 1;
	Width = Height = 1;
	WidthMask = 0;
	DummySpans[0].TopOffset = 0;
	DummySpans[0].Length = 0;
	Pixels[0] = 0;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FEmptyTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (spans_out != NULL)
	{
		*spans_out = DummySpans;
	}
	return Pixels;
}

//==========================================================================
//
//
//
//==========================================================================

const BYTE *FEmptyTexture::GetPixels ()
{
	return Pixels;
}

