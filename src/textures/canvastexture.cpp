/*
** canvastexture.cpp
** Texture class for camera textures
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
#include "v_palette.h"
#include "v_video.h"
#include "textures/textures.h"

FCanvasTexture::FCanvasTexture (const char *name, int width, int height)
{
	Name = name;
	Width = width;
	Height = height;
	LeftOffset = TopOffset = 0;
	CalcBitSize ();

	bMasked = false;
	DummySpans[0].TopOffset = 0;
	DummySpans[0].Length = height;
	DummySpans[1].TopOffset = 0;
	DummySpans[1].Length = 0;
	UseType = ETextureType::Wall;
	bNeedsUpdate = true;
	bDidUpdate = false;
	bHasCanvas = true;
	bFirstUpdate = true;
	bPixelsAllocated = false;
}

FCanvasTexture::~FCanvasTexture ()
{
	Unload ();
}

const uint8_t *FCanvasTexture::GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out)
{
	bNeedsUpdate = true;
	if (Canvas == NULL)
	{
		MakeTexture (style);
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
		*spans_out = DummySpans;
	}
	return Pixels + column*Height;
}

const uint8_t *FCanvasTexture::GetPixels (FRenderStyle style)
{
	bNeedsUpdate = true;
	if (Canvas == NULL)
	{
		MakeTexture (style);
	}
	return Pixels;
}

const uint32_t *FCanvasTexture::GetPixelsBgra()
{
	bNeedsUpdate = true;
	if (CanvasBgra == NULL)
	{
		MakeTextureBgra();
	}
	return PixelsBgra;
}

void FCanvasTexture::MakeTexture (FRenderStyle)	// This ignores the render style because making it work as alpha texture is impractical.
{
	Canvas = new DSimpleCanvas (Width, Height, false);
	Canvas->Lock ();

	if (Width != Height || Width != Canvas->GetPitch())
	{
		Pixels = new uint8_t[Width*Height];
		bPixelsAllocated = true;
	}
	else
	{
		Pixels = (uint8_t*)Canvas->GetBuffer();
		bPixelsAllocated = false;
	}

	// Draw a special "unrendered" initial texture into the buffer.
	memset (Pixels, 0, Width*Height/2);
	memset (Pixels+Width*Height/2, 255, Width*Height/2);
}

void FCanvasTexture::MakeTextureBgra()
{
	CanvasBgra = new DSimpleCanvas(Width, Height, true);
	CanvasBgra->Lock();

	if (Width != Height || Width != CanvasBgra->GetPitch())
	{
		PixelsBgra = new uint32_t[Width*Height];
		bPixelsAllocatedBgra = true;
	}
	else
	{
		PixelsBgra = (uint32_t*)CanvasBgra->GetBuffer();
		bPixelsAllocatedBgra = false;
	}

	// Draw a special "unrendered" initial texture into the buffer.
	memset(PixelsBgra, 0, Width*Height / 2 * 4);
	memset(PixelsBgra + Width*Height / 2, 255, Width*Height / 2 * 4);
}

void FCanvasTexture::Unload ()
{
	if (bPixelsAllocated)
	{
		if (Pixels != NULL) delete[] Pixels;
		bPixelsAllocated = false;
		Pixels = NULL;
	}

	if (bPixelsAllocatedBgra)
	{
		if (PixelsBgra != NULL) delete[] PixelsBgra;
		bPixelsAllocatedBgra = false;
		PixelsBgra = NULL;
	}

	if (Canvas != NULL)
	{
		delete Canvas;
		Canvas = nullptr;
	}

	if (CanvasBgra != NULL)
	{
		delete CanvasBgra;
		CanvasBgra = nullptr;
	}

	FTexture::Unload();
}

bool FCanvasTexture::CheckModified (FRenderStyle)
{
	if (bDidUpdate)
	{
		bDidUpdate = false;
		return true;
	}
	return false;
}

