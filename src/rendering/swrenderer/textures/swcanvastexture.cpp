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

#include "r_swtexture.h"
#include "bitmap.h"
#include "m_alloc.h"
#include "imagehelpers.h"

static TMap<FCanvasTexture*, FSWCanvasTexture*> canvasMap;

FSWCanvasTexture* GetSWCamTex(FCanvasTexture* camtex)
{
	auto p = canvasMap.CheckKey(camtex);
	return p ? *p : nullptr;
}

FSWCanvasTexture::FSWCanvasTexture(FGameTexture* source) : FSoftwareTexture(source) 
{
	// The SW renderer needs to link the canvas textures, but let's do that outside the texture manager.
	auto camtex = static_cast<FCanvasTexture*>(source->GetTexture());
	canvasMap.Insert(camtex, this);
}


FSWCanvasTexture::~FSWCanvasTexture()
{
	if (Canvas != nullptr)
	{
		delete Canvas;
		Canvas = nullptr;
	}

	if (CanvasBgra != nullptr)
	{
		delete CanvasBgra;
		CanvasBgra = nullptr;
	}
}


//==========================================================================
//
// 
//
//==========================================================================

const uint8_t *FSWCanvasTexture::GetPixels(int style)
{
	static_cast<FCanvasTexture*>(mSource)->NeedUpdate();
	if (Canvas == nullptr)
	{
		MakeTexture();
	}
	return Pixels.Data();

}

//==========================================================================
//
//
//
//==========================================================================

const uint32_t *FSWCanvasTexture::GetPixelsBgra()
{
	static_cast<FCanvasTexture*>(mSource)->NeedUpdate();
	if (CanvasBgra == nullptr)
	{
		MakeTextureBgra();
	}
	return PixelsBgra.Data();
}

//==========================================================================
//
//
//
//==========================================================================

void FSWCanvasTexture::MakeTexture ()
{
	Canvas = new DCanvas (GetWidth(), GetHeight(), false);
	Pixels.Resize(GetWidth() * GetHeight());

	// Draw a special "unrendered" initial texture into the buffer.
	memset (Pixels.Data(), 0, GetWidth() * GetHeight() / 2);
	memset (Pixels.Data() + GetWidth() * GetHeight() / 2, 255, GetWidth() * GetHeight() / 2);
}

//==========================================================================
//
//
//
//==========================================================================

void FSWCanvasTexture::MakeTextureBgra()
{
	CanvasBgra =  new DCanvas(GetWidth(), GetHeight(), true);
	PixelsBgra.Resize(GetWidth() * GetHeight());

	// Draw a special "unrendered" initial texture into the buffer.
	memset(PixelsBgra.Data(), 0, 4* GetWidth() * GetHeight() / 2);
	memset(PixelsBgra.Data() + GetWidth() * GetHeight() / 2, 255, 4* GetWidth() * GetHeight() / 2);
}

//==========================================================================
//
//
//
//==========================================================================

void FSWCanvasTexture::Unload ()
{
	if (Canvas != nullptr)
	{
		delete Canvas;
		Canvas = nullptr;
	}

	if (CanvasBgra != nullptr)
	{
		delete CanvasBgra;
		CanvasBgra = nullptr;
	}

	FSoftwareTexture::Unload();
}

//==========================================================================
//
//
//
//==========================================================================

void FSWCanvasTexture::UpdatePixels(bool truecolor)
{

	if (truecolor)
	{
		ImageHelpers::FlipNonSquareBlock(PixelsBgra.Data(), (const uint32_t*)CanvasBgra->GetPixels(), GetWidth(), GetHeight(), CanvasBgra->GetPitch());
		// True color render still sometimes uses palette textures (for sprites, mostly).
		// We need to make sure that both pixel buffers contain data:
		int width = GetWidth();
		int height = GetHeight();
		uint8_t* palbuffer = const_cast<uint8_t*>(GetPixels(0));
		const uint32_t* bgrabuffer = GetPixelsBgra();
		for (int x = 0; x < width; x++)
		{
			for (int y = 0; y < height; y++)
			{
				uint32_t color = bgrabuffer[y];
				int r = RPART(color);
				int g = GPART(color);
				int b = BPART(color);
				palbuffer[y] = RGB32k.RGB[r >> 3][g >> 3][b >> 3];
			}
			palbuffer += height;
			bgrabuffer += height;
		}
	}
	else
	{
		ImageHelpers::FlipNonSquareBlockRemap(Pixels.Data(), Canvas->GetPixels(), GetWidth(), GetHeight(), Canvas->GetPitch(), GPalette.Remap);
	}

	static_cast<FCanvasTexture*>(mSource)->SetUpdated(false);
}