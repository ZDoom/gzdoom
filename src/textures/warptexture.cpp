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
	SetupMultipliers(128, 128); // [mxd]
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
	int xmul = WidthOffsetMultipiler;  // [mxd]
	int ymul = HeightOffsetMultipiler; // [mxd]
	int xmask = WidthMask;
	int ymask = Height - 1;
	int ybits = HeightBits;
	int x, y;

	if ((1 << ybits) > Height)
	{
		ybits--;
	}

	DWORD timebase = DWORD(time * Speed * 32 / 28);
	// [mxd] Rewrote to fix animation for NPo2 textures
	for (y = ysize-1; y >= 0; y--)
	{
		int xf = (finesine[(timebase+y*ymul)&FINEMASK]>>13) % xsize;
		if(xf < 0) xf += xsize; 
		int xt = xf;
		const BYTE *source = otherpix + y;
		BYTE *dest = Pixels + y;
		for (xt = xsize; xt; xt--, xf = (xf+1)%xsize, dest += ysize)
			*dest = source[xf + ymask * xf];
	}
	timebase = DWORD(time * Speed * 23 / 28);
	for (x = xsize-1; x >= 0; x--)
	{
		int yf = (finesine[(time+(x+17)*xmul)&FINEMASK]>>13) % ysize;
		if(yf < 0) yf += ysize;
		int yt = yf;
		const BYTE *source = Pixels + (x + ymask * x);
		BYTE *dest = buffer;
		for (yt = ysize; yt; yt--, yf = (yf+1)%ysize)
			*dest++ = source[yf];
		memcpy (Pixels+(x+ymask*x), buffer, ysize);
	}
}

// [mxd] Non power of 2 textures need different offset multipliers, otherwise warp animation won't sync across texture
void FWarpTexture::SetupMultipliers (int width, int height)
{
	WidthOffsetMultipiler = width;
	HeightOffsetMultipiler = height;
	int widthpo2 = NextPo2(Width);
	int heightpo2 = NextPo2(Height);
	if(widthpo2 != Width) WidthOffsetMultipiler = (int)(WidthOffsetMultipiler * ((float)widthpo2 / Width));
	if(heightpo2 != Height) HeightOffsetMultipiler = (int)(HeightOffsetMultipiler * ((float)heightpo2 / Height));
}

int FWarpTexture::NextPo2 (int v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return ++v;
}

// [GRB] Eternity-like warping
FWarp2Texture::FWarp2Texture (FTexture *source)
: FWarpTexture (source)
{
	SetupMultipliers(256, 128); // [mxd]
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
	int xmul = WidthOffsetMultipiler;  // [mxd]
	int ymul = HeightOffsetMultipiler; // [mxd]
	int xmask = WidthMask;
	int ymask = Height - 1;
	int ybits = HeightBits;
	int x, y;

	if ((1 << ybits) > Height)
	{
		ybits--;
	}

	DWORD timebase = DWORD(time * Speed * 40 / 28);
	// [mxd] Rewrote to fix animation for NPo2 textures
	for (x = 0; x < xsize; x++)
	{
		BYTE *dest = Pixels + (x + ymask * x);
		for (y = 0; y < ysize; y++)
		{
			int xt = (x + 128
				+ ((finesine[(y*ymul + timebase*5 + 900) & FINEMASK]*2)>>FRACBITS)
				+ ((finesine[(x*xmul + timebase*4 + 300) & FINEMASK]*2)>>FRACBITS)) % xsize;

			int yt = (y + 128
				+ ((finesine[(y*ymul + timebase*3 + 700) & FINEMASK]*2)>>FRACBITS)
				+ ((finesine[(x*xmul + timebase*4 + 1200) & FINEMASK]*2)>>FRACBITS)) % ysize;

			*dest++ = otherpix[(xt + ymask * xt) + yt];
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

