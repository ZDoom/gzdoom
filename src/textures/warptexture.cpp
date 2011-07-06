/*
** warptexture.cpp
** Texture class for warped textures
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
#include "templates.h"
#include "r_utility.h"
#include "textures/textures.h"


FWarpTexture::FWarpTexture (FTexture *source)
: GenTime (0), SourcePic (source), Pixels (0), Spans (0), Speed (1.f)
{
	CopyInfo(source);
	bWarped = 1;
}

FWarpTexture::~FWarpTexture ()
{
	Unload ();
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
	delete SourcePic;
}

void FWarpTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}
	SourcePic->Unload ();
}

bool FWarpTexture::CheckModified ()
{
	return r_FrameTime != GenTime;
}

const BYTE *FWarpTexture::GetPixels ()
{
	DWORD time = r_FrameTime;

	if (Pixels == NULL || time != GenTime)
	{
		MakeTexture (time);
	}
	return Pixels;
}

const BYTE *FWarpTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	DWORD time = r_FrameTime;

	if (Pixels == NULL || time != GenTime)
	{
		MakeTexture (time);
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
			Spans = CreateSpans (Pixels);
		}
		*spans_out = Spans[column];
	}
	return Pixels + column*Height;
}

void FWarpTexture::MakeTexture (DWORD time)
{
	const BYTE *otherpix = SourcePic->GetPixels ();

	if (Pixels == NULL)
	{
		Pixels = new BYTE[Width * Height];
	}
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}

	GenTime = time;

	BYTE *buffer = (BYTE *)alloca (MAX (Width, Height));
	int xsize = Width;
	int ysize = Height;
	int xmask = WidthMask;
	int ymask = Height - 1;
	int ybits = HeightBits;
	int x, y;

	if ((1 << ybits) > Height)
	{
		ybits--;
	}

	DWORD timebase = DWORD(time * Speed * 32 / 28);
	for (y = ysize-1; y >= 0; y--)
	{
		int xt, xf = (finesine[(timebase+y*128)&FINEMASK]>>13) & xmask;
		const BYTE *source = otherpix + y;
		BYTE *dest = Pixels + y;
		for (xt = xsize; xt; xt--, xf = (xf+1)&xmask, dest += ysize)
			*dest = source[xf << ybits];
	}
	timebase = DWORD(time * Speed * 23 / 28);
	for (x = xsize-1; x >= 0; x--)
	{
		int yt, yf = (finesine[(time+(x+17)*128)&FINEMASK]>>13) & ymask;
		const BYTE *source = Pixels + (x << ybits);
		BYTE *dest = buffer;
		for (yt = ysize; yt; yt--, yf = (yf+1)&ymask)
			*dest++ = source[yf];
		memcpy (Pixels+(x<<ybits), buffer, ysize);
	}
}

// [GRB] Eternity-like warping
FWarp2Texture::FWarp2Texture (FTexture *source)
: FWarpTexture (source)
{
	bWarped = 2;
}

void FWarp2Texture::MakeTexture (DWORD time)
{
	const BYTE *otherpix = SourcePic->GetPixels ();

	if (Pixels == NULL)
	{
		Pixels = new BYTE[Width * Height];
	}
	if (Spans != NULL)
	{
		FreeSpans (Spans);
		Spans = NULL;
	}

	GenTime = time;

	int xsize = Width;
	int ysize = Height;
	int xmask = WidthMask;
	int ymask = Height - 1;
	int ybits = HeightBits;
	int x, y;

	if ((1 << ybits) > Height)
	{
		ybits--;
	}

	DWORD timebase = DWORD(time * Speed * 40 / 28);
	for (x = 0; x < xsize; ++x)
	{
		BYTE *dest = Pixels + (x << ybits);
		for (y = 0; y < ysize; ++y)
		{
			int xt = (x + 128
				+ ((finesine[(y*128 + timebase*5 + 900) & FINEMASK]*2)>>FRACBITS)
				+ ((finesine[(x*256 + timebase*4 + 300) & FINEMASK]*2)>>FRACBITS)) & xmask;
			int yt = (y + 128
				+ ((finesine[(y*128 + timebase*3 + 700) & FINEMASK]*2)>>FRACBITS)
				+ ((finesine[(x*256 + timebase*4 + 1200) & FINEMASK]*2)>>FRACBITS)) & ymask;
			*dest++ = otherpix[(xt << ybits) + yt];
		}
	}
}

//==========================================================================
//
// FMultiPatchTexture :: TexPart :: TexPart
//
//==========================================================================

FTexture *FWarpTexture::GetRedirect(bool wantwarped)
{
	if (!wantwarped) return SourcePic->GetRedirect(false);
	else return this;
}

//==========================================================================
//
// FMultiPatchTexture :: CopyTrueColorPixels
//
// This doesn't warp. It's just here in case someone tries to use a warp
// texture for compositing a multipatch texture
//
//==========================================================================

int FWarpTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	return SourcePic->CopyTrueColorPixels(bmp, x, y, rotate, inf);
}

