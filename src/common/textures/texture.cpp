/*
** texture.cpp
** The base texture class
**
**---------------------------------------------------------------------------
** Copyright 2004-2007 Randy Heit
** Copyright 2006-2018 Christoph Oelckers
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

#include "printf.h"
#include "files.h"
#include "filesystem.h"

#include "textures.h"
#include "bitmap.h"
#include "colormatcher.h"
#include "c_dispatch.h"
#include "m_fixed.h"
#include "imagehelpers.h"
#include "image.h"
#include "formats/multipatchtexture.h"
#include "texturemanager.h"
#include "c_cvars.h"
#include "imagehelpers.h"
#include "v_video.h"
#include "v_font.h"

// Wrappers to keep the definitions of these classes out of here.
IHardwareTexture* CreateHardwareTexture(int numchannels);

// Make sprite offset adjustment user-configurable per renderer.
int r_spriteadjustSW, r_spriteadjustHW;

//==========================================================================
//
// 
//
//==========================================================================

FTexture::FTexture (int lumpnum)
	:  SourceLump(lumpnum), bHasCanvas(false)
{
	bTranslucent = -1;
}

//===========================================================================
//
// FTexture::GetBgraBitmap
//
// Default returns just an empty bitmap. This needs to be overridden by
// any subclass that actually does return a software pixel buffer.
//
//===========================================================================

FBitmap FTexture::GetBgraBitmap(const PalEntry* remap, int* ptrans)
{
	FBitmap bmp;
	bmp.Create(Width, Height);
	return bmp;
}

//====================================================================
//
// CheckRealHeight
//
// Checks the posts in a texture and returns the lowest row (plus one)
// of the texture that is actually used.
//
//====================================================================

int FTexture::CheckRealHeight()
{
	auto pixels = Get8BitPixels(false);

	for(int h = GetHeight()-1; h>= 0; h--)
	{
		for(int w = 0; w < GetWidth(); w++)
		{
			if (pixels[h + w * GetHeight()] != 0)
			{
				return h;
			}
		}
	}
	return 0;
}

//===========================================================================
// 
//	Finds gaps in the texture which can be skipped by the renderer
//  This was mainly added to speed up one area in E4M6 of 007LTSD
//
//===========================================================================

bool FTexture::FindHoles(const unsigned char* buffer, int w, int h)
{
	const unsigned char* li;
	int y, x;
	int startdraw, lendraw;
	int gaps[5][2];
	int gapc = 0;


	// already done!
	if (areacount) return false;
	areacount = -1;	//whatever happens next, it shouldn't be done twice!

							// large textures and non-images are excluded for performance reasons
	if (h>512 || !GetImage()) return false;

	startdraw = -1;
	lendraw = 0;
	for (y = 0; y < h; y++)
	{
		li = buffer + w * y * 4 + 3;

		for (x = 0; x < w; x++, li += 4)
		{
			if (*li != 0) break;
		}

		if (x != w)
		{
			// non - transparent
			if (startdraw == -1)
			{
				startdraw = y;
				// merge transparent gaps of less than 16 pixels into the last drawing block
				if (gapc && y <= gaps[gapc - 1][0] + gaps[gapc - 1][1] + 16)
				{
					gapc--;
					startdraw = gaps[gapc][0];
					lendraw = y - startdraw;
				}
				if (gapc == 4) return false;	// too many splits - this isn't worth it
			}
			lendraw++;
		}
		else if (startdraw != -1)
		{
			if (lendraw == 1) lendraw = 2;
			gaps[gapc][0] = startdraw;
			gaps[gapc][1] = lendraw;
			gapc++;

			startdraw = -1;
			lendraw = 0;
		}
	}
	if (startdraw != -1)
	{
		gaps[gapc][0] = startdraw;
		gaps[gapc][1] = lendraw;
		gapc++;
	}
	if (startdraw == 0 && lendraw == h) return false;	// nothing saved so don't create a split list

	if (gapc > 0)
	{
		FloatRect* rcs = (FloatRect*)ImageArena.Alloc(gapc * sizeof(FloatRect));	// allocate this on the image arena

		for (x = 0; x < gapc; x++)
		{
			// gaps are stored as texture (u/v) coordinates
			rcs[x].width = rcs[x].left = -1.0f;
			rcs[x].top = (float)gaps[x][0] / (float)h;
			rcs[x].height = (float)gaps[x][1] / (float)h;
		}
		areas = rcs;
	}
	else areas = nullptr;
	areacount = gapc;

	return true;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FTexture::CheckTrans(unsigned char* buffer, int size, int trans)
{
	if (bTranslucent == -1)
	{
		bTranslucent = trans;
		if (trans == -1)
		{
			uint32_t* dwbuf = (uint32_t*)buffer;
			for (int i = 0; i < size; i++)
			{
				uint32_t alpha = dwbuf[i] >> 24;

				if (alpha != 0xff && alpha != 0)
				{
					bTranslucent = 1;
					return;
				}
			}
			bTranslucent = 0;
		}
	}
}


//===========================================================================
// 
// smooth the edges of transparent fields in the texture
//
//===========================================================================

#ifdef WORDS_BIGENDIAN
#define MSB 0
#define SOME_MASK 0xffffff00
#else
#define MSB 3
#define SOME_MASK 0x00ffffff
#endif

#define CHKPIX(ofs) (l1[(ofs)*4+MSB]==255 ? (( ((uint32_t*)l1)[0] = ((uint32_t*)l1)[ofs]&SOME_MASK), trans=true ) : false)

bool FTexture::SmoothEdges(unsigned char* buffer, int w, int h)
{
	int x, y;
	bool trans = buffer[MSB] == 0; // If I set this to false here the code won't detect textures 
								   // that only contain transparent pixels.
	bool semitrans = false;
	unsigned char* l1;

	if (h <= 1 || w <= 1) return false;  // makes (a) no sense and (b) doesn't work with this code!

	l1 = buffer;


	if (l1[MSB] == 0 && !CHKPIX(1)) CHKPIX(w);
	else if (l1[MSB] < 255) semitrans = true;
	l1 += 4;
	for (x = 1; x < w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-1) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB] < 255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-1)) CHKPIX(w);
	else if (l1[MSB] < 255) semitrans = true;
	l1 += 4;

	for (y = 1; y < h - 1; y++)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(1)) CHKPIX(w);
		else if (l1[MSB] < 255) semitrans = true;
		l1 += 4;
		for (x = 1; x < w - 1; x++, l1 += 4)
		{
			if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1) && !CHKPIX(1) && !CHKPIX(-w - 1) && !CHKPIX(-w + 1) && !CHKPIX(w - 1) && !CHKPIX(w + 1)) CHKPIX(w);
			else if (l1[MSB] < 255) semitrans = true;
		}
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(w);
		else if (l1[MSB] < 255) semitrans = true;
		l1 += 4;
	}

	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(1);
	else if (l1[MSB] < 255) semitrans = true;
	l1 += 4;
	for (x = 1; x < w - 1; x++, l1 += 4)
	{
		if (l1[MSB] == 0 && !CHKPIX(-w) && !CHKPIX(-1)) CHKPIX(1);
		else if (l1[MSB] < 255) semitrans = true;
	}
	if (l1[MSB] == 0 && !CHKPIX(-w)) CHKPIX(-1);
	else if (l1[MSB] < 255) semitrans = true;

	return trans || semitrans;
}

//===========================================================================
// 
// Post-process the texture data after the buffer has been created
//
//===========================================================================

bool FTexture::ProcessData(unsigned char* buffer, int w, int h, bool ispatch)
{
	if (Masked)
	{
		Masked = SmoothEdges(buffer, w, h);
		if (Masked && !ispatch) FindHoles(buffer, w, h);
	}
	return true;
}

//===========================================================================
// 
//	Initializes the buffer for the texture data
//
//===========================================================================

FTextureBuffer FTexture::CreateTexBuffer(int translation, int flags)
{
	FTextureBuffer result;
	if (flags & CTF_Indexed)
	{
		// Indexed textures will never be translated and never be scaled.
		int w = GetWidth(), h = GetHeight();

		auto store = Get8BitPixels(false);
		const uint8_t* p = store.Data();

		result.mBuffer = new uint8_t[w * h];
		result.mWidth = w;
		result.mHeight = h;
		result.mContentId = 0;
		ImageHelpers::FlipNonSquareBlock(result.mBuffer, p, h, w, h);
	}
	else
	{
		unsigned char* buffer = nullptr;
		int W, H;
		int isTransparent = -1;
		bool checkonly = !!(flags & CTF_CheckOnly);

		int exx = !!(flags & CTF_Expand);

		W = GetWidth() + 2 * exx;
		H = GetHeight() + 2 * exx;

		if (!checkonly)
		{
			auto remap = translation <= 0 || IsLuminosityTranslation(translation) ? nullptr : GPalette.TranslationToTable(translation);
			if (remap && remap->Inactive) remap = nullptr;
			if (remap) translation = remap->Index;

			int trans;
			auto Pixels = GetBgraBitmap(remap ? remap->Palette : nullptr, &trans);
			
			if(!exx && Pixels.ClipRect.x == 0 && Pixels.ClipRect.y == 0 && Pixels.ClipRect.width == Pixels.Width && Pixels.ClipRect.height == Pixels.Height && (Pixels.FreeBuffer || !IsLuminosityTranslation(translation)))
			{
				buffer = Pixels.data;
				result.mFreeBuffer = Pixels.FreeBuffer;
				Pixels.FreeBuffer = false;
			}
			else
			{
				buffer = new unsigned char[W * (H + 1) * 4];
				memset(buffer, 0, W * (H + 1) * 4);

				FBitmap bmp(buffer, W * 4, W, H);

				bmp.Blit(exx, exx, Pixels);
			}
			
			if (IsLuminosityTranslation(translation))
			{
				V_ApplyLuminosityTranslation(LuminosityTranslationDesc::fromInt(translation), buffer, W * H);
			}

			if (remap == nullptr)
			{
				CheckTrans(buffer, W * H, trans);
				isTransparent = bTranslucent;
			}
			else
			{
				isTransparent = 0;
				// A translated image is not conclusive for setting the texture's transparency info.
			}
		}

		if (GetImage())
		{
			FContentIdBuilder builder;
			builder.id = 0;
			builder.imageID = GetImage()->GetId();
			builder.translation = max(0, translation);
			builder.expand = exx;
			result.mContentId = builder.id;
		}
		else result.mContentId = 0;	// for non-image backed textures this has no meaning so leave it at 0.

		result.mBuffer = buffer;
		result.mWidth = W;
		result.mHeight = H;

		// Only do postprocessing for image-backed textures. (i.e. not for the burn texture which can also pass through here.)
		if (GetImage() && flags & CTF_ProcessData)
		{
			if (flags & CTF_Upscale) CreateUpsampledTextureBuffer(result, !!isTransparent, checkonly);

			if (!checkonly) ProcessData(result.mBuffer, result.mWidth, result.mHeight, false);
		}
	}
	return result;

}

//===========================================================================
// 
// Dummy texture for the 0-entry.
//
//===========================================================================

bool FTexture::DetermineTranslucency()
{
		// This will calculate all we need, so just discard the result.
		CreateTexBuffer(0);
	return !!bTranslucent;
}

//===========================================================================
// 
// the default just returns an empty texture.
//
//===========================================================================

TArray<uint8_t> FTexture::Get8BitPixels(bool alphatex)
{
	TArray<uint8_t> Pixels(Width * Height, true);
	memset(Pixels.Data(), 0, Width * Height);
	return Pixels;
}

//===========================================================================
// 
//  Finds empty space around the texture. 
//  Used for sprites that got placed into a huge empty frame.
//
//===========================================================================

bool FTexture::TrimBorders(uint16_t* rect)
{

	auto texbuffer = CreateTexBuffer(0);
	int w = texbuffer.mWidth;
	int h = texbuffer.mHeight;
	auto Buffer = texbuffer.mBuffer;

	if (texbuffer.mBuffer == nullptr)
	{
		return false;
	}
	if (w != Width || h != Height)
	{
		// external Hires replacements cannot be trimmed.
		return false;
	}

	int size = w * h;
	if (size == 1)
	{
		// nothing to be done here.
		rect[0] = 0;
		rect[1] = 0;
		rect[2] = 1;
		rect[3] = 1;
		return true;
	}
	int first, last;

	for (first = 0; first < size; first++)
	{
		if (Buffer[first * 4 + 3] != 0) break;
	}
	if (first >= size)
	{
		// completely empty
		rect[0] = 0;
		rect[1] = 0;
		rect[2] = 1;
		rect[3] = 1;
		return true;
	}

	for (last = size - 1; last >= first; last--)
	{
		if (Buffer[last * 4 + 3] != 0) break;
	}

	rect[1] = first / w;
	rect[3] = 1 + last / w - rect[1];

	rect[0] = 0;
	rect[2] = w;

	unsigned char* bufferoff = Buffer + (rect[1] * w * 4);
	h = rect[3];

	for (int x = 0; x < w; x++)
	{
		for (int y = 0; y < h; y++)
		{
			if (bufferoff[(x + y * w) * 4 + 3] != 0) goto outl;
		}
		rect[0]++;
	}
outl:
	rect[2] -= rect[0];

	for (int x = w - 1; rect[2] > 1; x--)
	{
		for (int y = 0; y < h; y++)
		{
			if (bufferoff[(x + y * w) * 4 + 3] != 0)
			{
				return true;
			}
		}
		rect[2]--;
	}
	return true;
}

//===========================================================================
//
// Create a hardware texture for this texture image.
//
//===========================================================================

IHardwareTexture* FTexture::GetHardwareTexture(int translation, int scaleflags)
{
	int indexed = scaleflags & CTF_Indexed;
	if (indexed) translation = -1;
	IHardwareTexture* hwtex = SystemTextures.GetHardwareTexture(translation, scaleflags);
	if (hwtex == nullptr)
	{
		hwtex = screen->CreateHardwareTexture(indexed? 1 : 4);
		SystemTextures.AddHardwareTexture(translation, scaleflags, hwtex);
	}
	return hwtex;
}


//==========================================================================
//
// this must be copied back to textures.cpp later.
//
//==========================================================================

FWrapperTexture::FWrapperTexture(int w, int h, int bits)
{
	Width = w;
	Height = h;
	Format = bits;
	//bNoCompress = true;
	auto hwtex = screen->CreateHardwareTexture(4);
	// todo: Initialize here.
	SystemTextures.AddHardwareTexture(0, false, hwtex);
}

