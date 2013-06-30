/*
** patchtexture.cpp
** Texture class for single Doom patches
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
**
*/

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "templates.h"
#include "v_palette.h"
#include "textures/textures.h"


// posts are runs of non masked source pixels
struct column_t
{
	BYTE		topdelta;		// -1 is the last post in a column
	BYTE		length; 		// length data bytes follows
};


//==========================================================================
//
// A texture that is just a single Doom format patch
//
//==========================================================================

class FPatchTexture : public FTexture
{
public:
	FPatchTexture (int lumpnum, patch_t *header);
	~FPatchTexture ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

protected:
	BYTE *Pixels;
	Span **Spans;
	bool hackflag;


	virtual void MakeTexture ();
	void HackHack (int newheight);
};

//==========================================================================
//
// Checks if the currently open lump can be a Doom patch
//
//==========================================================================

static bool CheckIfPatch(FileReader & file)
{
	if (file.GetLength() < 13) return false;	// minimum length of a valid Doom patch
	
	BYTE *data = new BYTE[file.GetLength()];
	file.Seek(0, SEEK_SET);
	file.Read(data, file.GetLength());
	
	const patch_t *foo = (const patch_t *)data;
	
	int height = LittleShort(foo->height);
	int width = LittleShort(foo->width);
	
	if (height > 0 && height <= 2048 && width > 0 && width <= 2048 && width < file.GetLength()/4)
	{
		// The dimensions seem like they might be valid for a patch, so
		// check the column directory for extra security. At least one
		// column must begin exactly at the end of the column directory,
		// and none of them must point past the end of the patch.
		bool gapAtStart = true;
		int x;
	
		for (x = 0; x < width; ++x)
		{
			DWORD ofs = LittleLong(foo->columnofs[x]);
			if (ofs == (DWORD)width * 4 + 8)
			{
				gapAtStart = false;
			}
			else if (ofs >= (DWORD)(file.GetLength()))	// Need one byte for an empty column (but there's patches that don't know that!)
			{
				delete [] data;
				return false;
			}
		}
		delete [] data;
		return !gapAtStart;
	}
	delete [] data;
	return false;
}

//==========================================================================
//
//
//
//==========================================================================

FTexture *PatchTexture_TryCreate(FileReader & file, int lumpnum)
{
	patch_t header;

	if (!CheckIfPatch(file)) return NULL;
	file.Seek(0, SEEK_SET);
	file >> header.width >> header.height >> header.leftoffset >> header.topoffset;
	return new FPatchTexture(lumpnum, &header);
}

//==========================================================================
//
//
//
//==========================================================================

FPatchTexture::FPatchTexture (int lumpnum, patch_t * header)
: FTexture(NULL, lumpnum), Pixels(0), Spans(0), hackflag(false)
{
	Width = header->width;
	Height = header->height;
	LeftOffset = header->leftoffset;
	TopOffset = header->topoffset;
	CalcBitSize ();
}

//==========================================================================
//
//
//
//==========================================================================

FPatchTexture::~FPatchTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FPatchTexture::Unload ()
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

const BYTE *FPatchTexture::GetPixels ()
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

const BYTE *FPatchTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	if ((unsigned)column >= (unsigned)Width)
	{
		if (WidthMask + 1 == Width)
		{
			column &= WidthMask;
		}
		else
		{
			column %= Width;
		}
	}
	if (spans_out != NULL)
	{
		if (Spans == NULL)
		{
			Spans = CreateSpans(Pixels);
		}
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}


//==========================================================================
//
//
//
//==========================================================================

void FPatchTexture::MakeTexture ()
{
	BYTE *remap, remaptable[256];
	int numspans;
	const column_t *maxcol;
	int x;

	FMemLump lump = Wads.ReadLump (SourceLump);
	const patch_t *patch = (const patch_t *)lump.GetMem();

	maxcol = (const column_t *)((const BYTE *)patch + Wads.LumpLength (SourceLump) - 3);

	// Check for badly-sized patches
#if 0	// Such textures won't be created so there's no need to check here
	if (LittleShort(patch->width) <= 0 || LittleShort(patch->height) <= 0)
	{
		lump = Wads.ReadLump ("-BADPATC");
		patch = (const patch_t *)lump.GetMem();
		Printf (PRINT_BOLD, "Patch %s has a non-positive size.\n", Name);
	}
	else if (LittleShort(patch->width) > 2048 || LittleShort(patch->height) > 2048)
	{
		lump = Wads.ReadLump ("-BADPATC");
		patch = (const patch_t *)lump.GetMem();
		Printf (PRINT_BOLD, "Patch %s is too big.\n", Name);
	}
#endif

	if (bNoRemap0)
	{
		memcpy (remaptable, GPalette.Remap, 256);
		remaptable[0] = 0;
		remap = remaptable;
	}
	else
	{
		remap = GPalette.Remap;
	}


	if (hackflag)
	{
		Pixels = new BYTE[Width * Height];
		BYTE *out;

		// Draw the image to the buffer
		for (x = 0, out = Pixels; x < Width; ++x)
		{
			const BYTE *in = (const BYTE *)patch + LittleLong(patch->columnofs[x]) + 3;

			for (int y = Height; y > 0; --y)
			{
				*out = remap[*in];
				out++, in++;
			}
		}
		return;
	}

	// Add a little extra space at the end if the texture's height is not
	// a power of 2, in case somebody accidentally makes it repeat vertically.
	int numpix = Width * Height + (1 << HeightBits) - Height;

	numspans = Width;

	Pixels = new BYTE[numpix];
	memset (Pixels, 0, numpix);

	// Draw the image to the buffer
	for (x = 0; x < Width; ++x)
	{
		BYTE *outtop = Pixels + x*Height;
		const column_t *column = (const column_t *)((const BYTE *)patch + LittleLong(patch->columnofs[x]));
		int top = -1;

		while (column < maxcol && column->topdelta != 0xFF)
		{
			if (column->topdelta <= top)
			{
				top += column->topdelta;
			}
			else
			{
				top = column->topdelta;
			}

			int len = column->length;
			BYTE *out = outtop + top;

			if (len != 0)
			{
				if (top + len > Height)	// Clip posts that extend past the bottom
				{
					len = Height - top;
				}
				if (len > 0)
				{
					numspans++;

					const BYTE *in = (const BYTE *)column + 3;
					for (int i = 0; i < len; ++i)
					{
						out[i] = remap[in[i]];
					}
				}
			}
			column = (const column_t *)((const BYTE *)column + column->length + 4);
		}
	}
}


//==========================================================================
//
// Fix for certain special patches on single-patch textures.
//
//==========================================================================

void FPatchTexture::HackHack (int newheight)
{
	// Check if this patch is likely to be a problem.
	// It must be 256 pixels tall, and all its columns must have exactly
	// one post, where each post has a supposed length of 0.
	FMemLump lump = Wads.ReadLump (SourceLump);
	const patch_t *realpatch = (patch_t *)lump.GetMem();
	const DWORD *cofs = realpatch->columnofs;
	int x, x2 = LittleShort(realpatch->width);

	if (LittleShort(realpatch->height) == 256)
	{
		for (x = 0; x < x2; ++x)
		{
			const column_t *col = (column_t*)((BYTE*)realpatch+LittleLong(cofs[x]));
			if (col->topdelta != 0 || col->length != 0)
			{
				break;	// It's not bad!
			}
			col = (column_t *)((BYTE *)col + 256 + 4);
			if (col->topdelta != 0xFF)
			{
				break;	// More than one post in a column!
			}
		}
		if (x == x2)
		{ 
			// If all the columns were checked, it needs fixing.
			Unload ();
			if (Spans != NULL)
			{
				FreeSpans (Spans);
			}

			Height = newheight;
			LeftOffset = 0;
			TopOffset = 0;

			hackflag = true;
			bMasked = false;	// Hacked textures don't have transparent parts.
		}
	}
}
