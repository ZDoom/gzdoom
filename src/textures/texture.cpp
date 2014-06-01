/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
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
#include "i_system.h"
#include "r_data/r_translate.h"
#include "bitmap.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "m_fixed.h"
#include "textures/textures.h"

typedef bool (*CheckFunc)(FileReader & file);
typedef FTexture * (*CreateFunc)(FileReader & file, int lumpnum);

struct TexCreateInfo
{
	CreateFunc TryCreate;
	int usetype;
};

BYTE FTexture::GrayMap[256];

void FTexture::InitGrayMap()
{
	for (int i = 0; i < 256; ++i)
	{
		GrayMap[i] = ColorMatcher.Pick (i, i, i);
	}
}

FTexture *IMGZTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PNGTexture_TryCreate(FileReader &, int lumpnum);
FTexture *JPEGTexture_TryCreate(FileReader &, int lumpnum);
FTexture *DDSTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PCXTexture_TryCreate(FileReader &, int lumpnum);
FTexture *TGATexture_TryCreate(FileReader &, int lumpnum);
FTexture *RawPageTexture_TryCreate(FileReader &, int lumpnum);
FTexture *FlatTexture_TryCreate(FileReader &, int lumpnum);
FTexture *PatchTexture_TryCreate(FileReader &, int lumpnum);
FTexture *EmptyTexture_TryCreate(FileReader &, int lumpnum);
FTexture *AutomapTexture_TryCreate(FileReader &, int lumpnum);


// Examines the lump contents to decide what type of texture to create,
// and creates the texture.
FTexture * FTexture::CreateTexture (int lumpnum, int usetype)
{
	static TexCreateInfo CreateInfo[]={
		{ IMGZTexture_TryCreate,		TEX_Any },
		{ PNGTexture_TryCreate,			TEX_Any },
		{ JPEGTexture_TryCreate,		TEX_Any },
		{ DDSTexture_TryCreate,			TEX_Any },
		{ PCXTexture_TryCreate,			TEX_Any },
		{ TGATexture_TryCreate,			TEX_Any },
		{ RawPageTexture_TryCreate,		TEX_MiscPatch },
		{ FlatTexture_TryCreate,		TEX_Flat },
		{ PatchTexture_TryCreate,		TEX_Any },
		{ EmptyTexture_TryCreate,		TEX_Any },
		{ AutomapTexture_TryCreate,		TEX_MiscPatch },
	};

	if (lumpnum == -1) return NULL;

	FWadLump data = Wads.OpenLumpNum (lumpnum);

	for(size_t i = 0; i < countof(CreateInfo); i++)
	{
		if ((CreateInfo[i].usetype == usetype || CreateInfo[i].usetype == TEX_Any))
		{
			FTexture * tex = CreateInfo[i].TryCreate(data, lumpnum);
			if (tex != NULL) 
			{
				tex->UseType = usetype;
				if (usetype == FTexture::TEX_Flat) 
				{
					int w = tex->GetWidth();
					int h = tex->GetHeight();

					// Auto-scale flats with dimensions 128x128 and 256x256.
					// In hindsight, a bad idea, but RandomLag made it sound better than it really is.
					// Now we're stuck with this stupid behaviour.
					if (w==128 && h==128) 
					{
						tex->xScale = tex->yScale = 2*FRACUNIT;
						tex->bWorldPanning = true;
					}
					else if (w==256 && h==256) 
					{
						tex->xScale = tex->yScale = 4*FRACUNIT;
						tex->bWorldPanning = true;
					}
				}
				return tex;
			}
		}
	}
	return NULL;
}

FTexture * FTexture::CreateTexture (const char *name, int lumpnum, int usetype)
{
	FTexture *tex = CreateTexture(lumpnum, usetype);
	if (tex != NULL && name != NULL) {
		tex->Name = name;
		tex->Name.ToUpper();
	}
	return tex;
}


FTexture::FTexture (const char *name, int lumpnum)
: LeftOffset(0), TopOffset(0),
  WidthBits(0), HeightBits(0), xScale(FRACUNIT), yScale(FRACUNIT), SourceLump(lumpnum),
  UseType(TEX_Any), bNoDecals(false), bNoRemap0(false), bWorldPanning(false),
  bMasked(true), bAlphaTexture(false), bHasCanvas(false), bWarped(0), bComplex(false), bMultiPatch(false), bKeepAround(false),
  Rotations(0xFFFF), SkyOffset(0), Width(0), Height(0), WidthMask(0), Native(NULL)
{
	id.SetInvalid();
	if (name != NULL)
	{
		Name = name;
		Name.ToUpper();
	}
	else if (lumpnum < 0)
	{
		Name = FString();
	}
	else
	{
		Wads.GetLumpName (Name, lumpnum);
	}
}

FTexture::~FTexture ()
{
	FTexture *link = Wads.GetLinkedTexture(SourceLump);
	if (link == this) Wads.SetLinkedTexture(SourceLump, NULL);
	KillNative();
}

bool FTexture::CheckModified ()
{
	return false;
}

FTextureFormat FTexture::GetFormat()
{
	return TEX_Pal;
}

void FTexture::SetFrontSkyLayer ()
{
	bNoRemap0 = true;
}

void FTexture::CalcBitSize ()
{
	// WidthBits is rounded down, and HeightBits is rounded up
	int i;

	for (i = 0; (1 << i) < Width; ++i)
	{ }

	WidthBits = i;

	// Having WidthBits that would allow for columns past the end of the
	// texture is not allowed, even if it means the entire texture is
	// not drawn.
	if (Width < (1 << WidthBits))
	{
		WidthBits--;
	}
	WidthMask = (1 << WidthBits) - 1;

	// The minimum height is 2, because we cannot shift right 32 bits.
	for (i = 1; (1 << i) < Height; ++i)
	{ }

	HeightBits = i;
}

void FTexture::HackHack (int newheight)
{
}

FTexture::Span **FTexture::CreateSpans (const BYTE *pixels) const
{
	Span **spans, *span;

	if (!bMasked)
	{ // Texture does not have holes, so it can use a simpler span structure
		spans = (Span **)M_Malloc (sizeof(Span*)*Width + sizeof(Span)*2);
		span = (Span *)&spans[Width];
		for (int x = 0; x < Width; ++x)
		{
			spans[x] = span;
		}
		span[0].Length = Height;
		span[0].TopOffset = 0;
		span[1].Length = 0;
		span[1].TopOffset = 0;
	}
	else
	{ // Texture might have holes, so build a complete span structure
		int numcols = Width;
		int numrows = Height;
		int numspans = numcols;	// One span to terminate each column
		const BYTE *data_p;
		bool newspan;
		int x, y;

		data_p = pixels;

		// Count the number of spans in this texture
		for (x = numcols; x > 0; --x)
		{
			newspan = true;
			for (y = numrows; y > 0; --y)
			{
				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
					}
				}
				else if (newspan)
				{
					newspan = false;
					numspans++;
				}
			}
		}

		// Allocate space for the spans
		spans = (Span **)M_Malloc (sizeof(Span*)*numcols + sizeof(Span)*numspans);

		// Fill in the spans
		for (x = 0, span = (Span *)&spans[numcols], data_p = pixels; x < numcols; ++x)
		{
			newspan = true;
			spans[x] = span;
			for (y = 0; y < numrows; ++y)
			{
				if (*data_p++ == 0)
				{
					if (!newspan)
					{
						newspan = true;
						span++;
					}
				}
				else
				{
					if (newspan)
					{
						newspan = false;
						span->TopOffset = y;
						span->Length = 1;
					}
					else
					{
						span->Length++;
					}
				}
			}
			if (!newspan)
			{
				span++;
			}
			span->TopOffset = 0;
			span->Length = 0;
			span++;
		}
	}
	return spans;
}

void FTexture::FreeSpans (Span **spans) const
{
	M_Free (spans);
}

void FTexture::CopyToBlock (BYTE *dest, int dwidth, int dheight, int xpos, int ypos, int rotate, const BYTE *translation)
{
	const BYTE *pixels = GetPixels();
	int srcwidth = Width;
	int srcheight = Height;
	int step_x = Height;
	int step_y = 1;
	FClipRect cr = {0, 0, dwidth, dheight};

	if (ClipCopyPixelRect(&cr, xpos, ypos, pixels, srcwidth, srcheight, step_x, step_y, rotate))
	{
		dest += ypos + dheight * xpos;
		if (translation == NULL)
		{
			for (int x = 0; x < srcwidth; x++)
			{
				int pos = x * dheight;
				for (int y = 0; y < srcheight; y++, pos++)
				{
					// the optimizer is doing a good enough job here so there's no need to optimize this by hand
					BYTE v = pixels[y * step_y + x * step_x]; 
					if (v != 0) dest[pos] = v;
				}
			}
		}
		else
		{
			for (int x = 0; x < srcwidth; x++)
			{
				int pos = x * dheight;
				for (int y = 0; y < srcheight; y++, pos++)
				{
					BYTE v = pixels[y * step_y + x * step_x]; 
					if (v != 0) dest[pos] = translation[v];
				}
			}
		}
	}
}

// Converts a texture between row-major and column-major format
// by flipping it about the X=Y axis.

void FTexture::FlipSquareBlock (BYTE *block, int x, int y)
{
	int i, j;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		BYTE *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			swapvalues<BYTE> (corner[count], corner[count*x]);
		}
		for (j = 0; j < count; j += 2)
		{
			swapvalues<BYTE> (corner[j], corner[j*x]);
			swapvalues<BYTE> (corner[j+1], corner[(j+1)*x]);
		}
	}
}

void FTexture::FlipSquareBlockRemap (BYTE *block, int x, int y, const BYTE *remap)
{
	int i, j;
	BYTE t;

	if (x != y) return;

	for (i = 0; i < x; ++i)
	{
		BYTE *corner = block + x*i + i;
		int count = x - i;
		if (count & 1)
		{
			count--;
			t = remap[corner[count]];
			corner[count] = remap[corner[count*x]];
			corner[count*x] = t;
		}
		for (j = 0; j < count; j += 2)
		{
			t = remap[corner[j]];
			corner[j] = remap[corner[j*x]];
			corner[j*x] = t;
			t = remap[corner[j+1]];
			corner[j+1] = remap[corner[(j+1)*x]];
			corner[(j+1)*x] = t;
		}
	}
}

void FTexture::FlipNonSquareBlock (BYTE *dst, const BYTE *src, int x, int y, int srcpitch)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = src[i+j*srcpitch];
		}
	}
}

void FTexture::FlipNonSquareBlockRemap (BYTE *dst, const BYTE *src, int x, int y, int srcpitch, const BYTE *remap)
{
	int i, j;

	for (i = 0; i < x; ++i)
	{
		for (j = 0; j < y; ++j)
		{
			dst[i*y+j] = remap[src[i+j*srcpitch]];
		}
	}
}

FNativeTexture *FTexture::GetNative(bool wrapping)
{
	if (Native != NULL)
	{
		if (!Native->CheckWrapping(wrapping))
		{ // Texture's wrapping mode is not compatible.
		  // Destroy it and get a new one.
			delete Native;
		}
		else
		{
			if (CheckModified())
			{
				Native->Update();
			}
			return Native;
		}
	}
	Native = screen->CreateTexture(this, wrapping);
	return Native;
}

void FTexture::KillNative()
{
	if (Native != NULL)
	{
		delete Native;
		Native = NULL;
	}
}

// For this generic implementation, we just call GetPixels and copy that data
// to the buffer. Texture formats that can do better than paletted images
// should provide their own implementation that may preserve the original
// color data. Note that the buffer expects row-major data, since that's
// generally more convenient for any non-Doom image formats, and it doesn't
// need to be used by any of Doom's column drawing routines.
void FTexture::FillBuffer(BYTE *buff, int pitch, int height, FTextureFormat fmt)
{
	const BYTE *pix;
	int x, y, w, h, stride;

	w = GetWidth();
	h = GetHeight();

	switch (fmt)
	{
	case TEX_Pal:
	case TEX_Gray:
		pix = GetPixels();
		stride = pitch - w;
		for (y = 0; y < h; ++y)
		{
			const BYTE *pix2 = pix;
			for (x = 0; x < w; ++x)
			{
				*buff++ = *pix2;
				pix2 += h;
			}
			pix++;
			buff += stride;
		}
		break;

	case TEX_RGB:
	{
		FCopyInfo inf = {OP_OVERWRITE, BLEND_NONE, {0}, 0, 0};
		FBitmap bmp(buff, pitch, pitch/4, height);
		CopyTrueColorPixels(&bmp, 0, 0, 0, &inf); 
		break;
	}

	default:
		I_Error("FTexture::FillBuffer: Unsupported format %d", fmt);
	}
}

//===========================================================================
//
// FTexture::CopyTrueColorPixels 
//
// this is the generic case that can handle
// any properly implemented texture for software rendering.
// Its drawback is that it is limited to the base palette which is
// why all classes that handle different palettes should subclass this
// method
//
//===========================================================================

int FTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	PalEntry *palette = screen->GetPalette();
	for(int i=1;i<256;i++) palette[i].a = 255;	// set proper alpha values
	bmp->CopyPixelData(x, y, GetPixels(), Width, Height, Height, 1, rotate, palette, inf);
	for(int i=1;i<256;i++) palette[i].a = 0;
	return 0;
}

int FTexture::CopyTrueColorTranslated(FBitmap *bmp, int x, int y, int rotate, FRemapTable *remap, FCopyInfo *inf)
{
	PalEntry *palette = remap->Palette;
	bmp->CopyPixelData(x, y, GetPixels(), Width, Height, Height, 1, rotate, palette, inf);
	return 0;
}

bool FTexture::UseBasePalette() 
{ 
	return true; 
}

FTexture *FTexture::GetRedirect(bool wantwarped)
{
	return this;
}

FTexture *FTexture::GetRawTexture()
{
	return this;
}

void FTexture::SetScaledSize(int fitwidth, int fitheight)
{
	xScale = FLOAT2FIXED(float(Width) / fitwidth);
	yScale = FLOAT2FIXED(float(Height) / fitheight);
	// compensate for roundoff errors
	if (MulScale16(xScale, fitwidth) != Width) xScale++;
	if (MulScale16(yScale, fitheight) != Height) yScale++;
}


FDummyTexture::FDummyTexture ()
{
	Width = 64;
	Height = 64;
	HeightBits = 6;
	WidthBits = 6;
	WidthMask = 63;
	UseType = TEX_Null;
}

void FDummyTexture::Unload ()
{
}

void FDummyTexture::SetSize (int width, int height)
{
	Width = width;
	Height = height;
	CalcBitSize ();
}

// This must never be called
const BYTE *FDummyTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	return NULL;
}

// And this also must never be called
const BYTE *FDummyTexture::GetPixels ()
{
	return NULL;
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
