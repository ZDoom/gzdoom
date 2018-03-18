/*
** worldtexture.cpp
** Intermediate class for some common code for several classes
**
**---------------------------------------------------------------------------
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

#include "textures.h"


//==========================================================================
//
//
//
//==========================================================================

FWorldTexture::FWorldTexture(const char *name, int lumpnum)
 : FTexture(name, lumpnum)
{
}

//==========================================================================
//
//
//
//==========================================================================

FWorldTexture::~FWorldTexture()
{
	Unload();
	FreeAllSpans();
}

//==========================================================================
//
//
//
//==========================================================================

void FWorldTexture::FreeAllSpans()
{
	for(int i = 0; i < 2; i++)
	{
		if (Spandata[i] != nullptr)
		{
			FreeSpans (Spandata[i]);
			Spandata[i] = nullptr;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FWorldTexture::Unload ()
{
	for(int i = 0; i < 2; i++)
	{
		if (!(PixelsAreStatic & (1 << i)))
		{
			delete[] Pixeldata[i];
		}
		Pixeldata[i] = nullptr;
	}
	FTexture::Unload();
}

//==========================================================================
//
//
//
//==========================================================================

const uint8_t *FWorldTexture::GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out)
{
	int index = !!(style.Flags & STYLEF_RedIsAlpha);
	GetPixels(style);
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
	if (spans_out != nullptr)
	{
		if (Spandata[index] == nullptr)
		{
			Spandata[index] = CreateSpans (Pixeldata[index]);
		}
		*spans_out = Spandata[index][column];
	}
	return Pixeldata[index] + column*Height;
}

//==========================================================================
//
//
//
//==========================================================================

const uint8_t *FWorldTexture::GetPixels (FRenderStyle style)
{
	if (CheckModified(style))
	{
		Unload();
	}
	int index = !!(style.Flags & STYLEF_RedIsAlpha);
	if (Pixeldata[index] == nullptr)
	{
		Pixeldata[index] = MakeTexture (style);
	}
	return Pixeldata[index];
}

