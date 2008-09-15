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
#include "r_local.h"
#include "v_palette.h"

extern float LastFOV;

FCanvasTexture::FCanvasTexture (const char *name, int width, int height)
{
	strncpy (Name, name, 8);
	Name[8] = 0;
	Width = width;
	Height = height;
	LeftOffset = TopOffset = 0;
	CalcBitSize ();

	bMasked = false;
	DummySpans[0].TopOffset = 0;
	DummySpans[0].Length = height;
	DummySpans[1].TopOffset = 0;
	DummySpans[1].Length = 0;
	UseType = TEX_Wall;
	Canvas = NULL;
	bNeedsUpdate = true;
	bDidUpdate = false;
	bHasCanvas = true;
	bFirstUpdate = true;
}

FCanvasTexture::~FCanvasTexture ()
{
	Unload ();
}

const BYTE *FCanvasTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	bNeedsUpdate = true;
	if (Canvas == NULL)
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
		*spans_out = DummySpans;
	}
	return Pixels + column*Height;
}

const BYTE *FCanvasTexture::GetPixels ()
{
	bNeedsUpdate = true;
	if (Canvas == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FCanvasTexture::MakeTexture ()
{
	Canvas = new DSimpleCanvas (Width, Height);
	Canvas->Lock ();
	GC::AddSoftRoot(Canvas);
	if (Width != Height || Width != Canvas->GetPitch())
	{
		Pixels = new BYTE[Width*Height];
	}
	else
	{
		Pixels = Canvas->GetBuffer();
	}
	// Draw a special "unrendered" initial texture into the buffer.
	memset (Pixels, 0, Width*Height/2);
	memset (Pixels+Width*Height/2, 255, Width*Height/2);
}

void FCanvasTexture::Unload ()
{
	if (Canvas != NULL)
	{
		if (Pixels != NULL && Pixels != Canvas->GetBuffer())
		{
			delete[] Pixels;
		}
		Pixels = NULL;
		GC::DelSoftRoot(Canvas);
		Canvas->Destroy();
		Canvas = NULL;
	}
}

bool FCanvasTexture::CheckModified ()
{
	if (bDidUpdate)
	{
		bDidUpdate = false;
		return true;
	}
	return false;
}

void FCanvasTexture::RenderView (AActor *viewpoint, int fov)
{
	if (Canvas == NULL)
	{
		MakeTexture ();
	}
	float savedfov = LastFOV;
	R_SetFOV ((float)fov);
	R_RenderViewToCanvas (viewpoint, Canvas, 0, 0, Width, Height, bFirstUpdate);
	R_SetFOV (savedfov);
	if (Pixels == Canvas->GetBuffer())
	{
		FlipSquareBlockRemap (Pixels, Width, Height, GPalette.Remap);
	}
	else
	{
		FlipNonSquareBlockRemap (Pixels, Canvas->GetBuffer(), Width, Height, Canvas->GetPitch(), GPalette.Remap);
	}
	bNeedsUpdate = false;
	bDidUpdate = true;
	bFirstUpdate = false;
}

