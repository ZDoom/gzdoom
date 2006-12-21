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
#include "r_data.h"
#include "w_wad.h"
#include "templates.h"


bool FPatchTexture::Check(FileReader & file)
{
	if (file.GetLength() < 13) return false;	// minimum length of a valid Doom patch
	
	BYTE *data = new BYTE[file.GetLength()];
	file.Seek(0, SEEK_SET);
	file.Read(data, file.GetLength());
	
	const patch_t * foo = (const patch_t *)data;
	
	int height = LittleShort(foo->height);
	int width = LittleShort(foo->width);
	
	if (height > 0 && height < 2048 && width > 0 && width <= 2048 && width < file.GetLength()/4)
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

FTexture *FPatchTexture::Create(FileReader & file, int lumpnum)
{
	patch_t header;

	file.Seek(0, SEEK_SET);
	file >> header.width >> header.height >> header.leftoffset >> header.topoffset;
	return new FPatchTexture(lumpnum, &header);
}

FPatchTexture::FPatchTexture (int lumpnum, patch_t * header)
: SourceLump(lumpnum), Pixels(0), Spans(0)
{
	Wads.GetLumpName (Name, lumpnum);
	Name[8] = 0;
	bIsPatch = true;
	Width = header->width;
	Height = header->height;
	LeftOffset = header->leftoffset;
	TopOffset = header->topoffset;
	CalcBitSize ();
}

FPatchTexture::~FPatchTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
}

void FPatchTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

const BYTE *FPatchTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

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
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}


void FPatchTexture::MakeTexture ()
{
	BYTE *remap, remaptable[256];
	Span *spanstuffer, *spanstarter;
	const column_t *maxcol;
	bool warned;
	int numspans;
	int x;

	FMemLump lump = Wads.ReadLump (SourceLump);
	const patch_t *patch = (const patch_t *)lump.GetMem();

	maxcol = (const column_t *)((const BYTE *)patch + Wads.LumpLength (SourceLump) - 3);

	// Check for badly-sized patches
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

	// Add a little extra space at the end if the texture's height is not
	// a power of 2, in case somebody accidentally makes it repeat vertically.
	int numpix = Width * Height + (1 << HeightBits) - Height;

	numspans = Width;

	Pixels = new BYTE[numpix];
	memset (Pixels, 0, numpix);

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

	// Create the spans
	if (Spans != NULL)
	{
		return;
	}

	Spans = (Span **)M_Malloc (sizeof(Span*)*Width + sizeof(Span)*numspans);
	spanstuffer = (Span *)((BYTE *)Spans + sizeof(Span*)*Width);
	warned = false;

	for (x = 0; x < Width; ++x)
	{
		const column_t *column = (const column_t *)((const BYTE *)patch + LittleLong(patch->columnofs[x]));
		int top = -1;

		Spans[x] = spanstuffer;
		spanstarter = spanstuffer;

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

			if (len != 0)
			{
				if (top + len > Height)	// Clip posts that extend past the bottom
				{
					len = Height - top;
				}
				if (len > 0)
				{
					// There is something of this post to draw. If it starts at the same
					// place where the previous span ends, add it to that one. If it starts
					// before the other one ends, that's bad, but deal with it. If it starts
					// after the previous one ends, create another span.

					// Assume we need to create another span.
					spanstuffer->TopOffset = top;
					spanstuffer->Length = len;

					// Now check if that's really the case.
					if (spanstuffer > spanstarter)
					{
						if ((spanstuffer - 1)->TopOffset + (spanstuffer - 1)->Length == top)
						{
							(--spanstuffer)->Length += len;
						}
						else
						{
							int prevbot;

							while (spanstuffer > spanstarter &&
								spanstuffer->TopOffset < (prevbot = 
								(spanstuffer - 1)->TopOffset + (spanstuffer - 1)->Length))
							{
								if (spanstuffer->TopOffset < (spanstuffer - 1)->TopOffset)
								{
									(spanstuffer - 1)->TopOffset = spanstuffer->TopOffset;
								}
								(spanstuffer - 1)->Length = MAX(prevbot,
									spanstuffer->TopOffset + spanstuffer->Length)
									- (spanstuffer - 1)->TopOffset;
								spanstuffer--;
								if (!warned)
								{
									warned = true;
									Printf (PRINT_BOLD, "Patch %s is malformed.\n", Name);
								}
							}
						}
					}
					spanstuffer++;
				}
			}
			column = (const column_t *)((const BYTE *)column + column->length + 4);
		}

		spanstuffer->Length = spanstuffer->TopOffset = 0;
		spanstuffer++;
	}
}

// Fix for certain special patches on single-patch textures.
void FPatchTexture::HackHack (int newheight)
{
	BYTE *out;
	int x;

	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
	}

	{
		FMemLump lump = Wads.ReadLump (SourceLump);
		const patch_t *patch = (const patch_t *)lump.GetMem();

		Width = LittleShort(patch->width);
		Height = newheight;
		LeftOffset = 0;
		TopOffset = 0;

		Pixels = new BYTE[Width * Height];

		// Draw the image to the buffer
		for (x = 0, out = Pixels; x < Width; ++x)
		{
			const BYTE *in = (const BYTE *)patch + LittleLong(patch->columnofs[x]) + 3;

			for (int y = newheight; y > 0; --y)
			{
				*out = *in != 255 ? *in : Near255;
				out++, in++;
			}
			out += newheight;
		}
	}

	// Create the spans
	Spans = (Span **)M_Malloc (sizeof(Span *)*Width + sizeof(Span)*Width*2);

	Span *span = (Span *)&Spans[Width];

	for (x = 0; x < Width; ++x)
	{
		Spans[x] = span;
		span[0].Length = newheight;
		span[0].TopOffset = 0;
		span[1].Length = 0;
		span[1].TopOffset = 0;
		span += 2;
	}
}
