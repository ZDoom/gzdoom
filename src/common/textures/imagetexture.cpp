/*
** imagetexture.cpp
** Texture class based on FImageSource
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

#include "files.h"
#include "filesystem.h"

#include "bitmap.h"
#include "image.h"
#include "textures.h"


//==========================================================================
//
//
//
//==========================================================================

FImageTexture::FImageTexture(FImageSource *img) noexcept
: FTexture(img? img->LumpNum() : 0)
{
	mImage = img;
	if (img != nullptr)
	{
		SetFromImage();
	}
}

FImageTexture::~FImageTexture()
{
	delete mImage;
}

void FImageTexture::SetFromImage()
{
	auto img = mImage;
	Width = img->GetWidth();
	Height = img->GetHeight();

	Masked = img->bMasked;
	bTranslucent = img->bTranslucent;
}
//===========================================================================
//
// 
//
//===========================================================================

FBitmap FImageTexture::GetBgraBitmap(const PalEntry *p, int *trans)
{
	return mImage->GetCachedBitmap(p, bNoRemap0? FImageSource::noremap0 : FImageSource::normal, trans);
}	

//===========================================================================
//
// 
//
//===========================================================================

TArray<uint8_t> FImageTexture::Get8BitPixels(bool alpha)
{
	return mImage->GetPalettedPixels(alpha? FImageSource::luminance : bNoRemap0 ? FImageSource::noremap0 : FImageSource::normal);
}	

//===========================================================================
//
// use the already known state of the underlying image to save time.
//
//===========================================================================

bool FImageTexture::DetermineTranslucency()
{
	if (mImage->bTranslucent != -1)
	{
		bTranslucent = mImage->bTranslucent;
		return !!bTranslucent;
	}
	else
	{
		return FTexture::DetermineTranslucency();
	}
}


FTexture* CreateImageTexture(FImageSource* img) noexcept
{
	return new FImageTexture(img);
}

