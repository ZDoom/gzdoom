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

#include "doomtype.h"
#include "files.h"
#include "w_wad.h"
#include "templates.h"
#include "bitmap.h"
#include "v_video.h"
#include "image.h"


//==========================================================================
//
//
//
//==========================================================================

FImageTexture::FImageTexture(FImageSource *img, const char *name)
: FTexture(name, img? img->LumpNum() : 0)
{
	mImage = img;
	if (img != nullptr)
	{
		if (name == nullptr) Wads.GetLumpName(Name, img->LumpNum());
		Width = img->GetWidth();
		Height = img->GetHeight();

		auto offsets = img->GetOffsets();
		_LeftOffset[1] = _LeftOffset[0] = offsets.first;
		_TopOffset[1] = _TopOffset[0] = offsets.second;

		bMasked = img->bMasked;
		bTranslucent = img->bTranslucent;
	}
}

//===========================================================================
//
// 
//
//===========================================================================

FBitmap FImageTexture::GetBgraBitmap(PalEntry *p, int *trans)
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
	return mImage->GetPalettedPixels(alpha? alpha : bNoRemap0 ? FImageSource::noremap0 : FImageSource::normal);
}	

//==========================================================================
//
// FMultiPatchTexture :: GetRawTexture
//
// Doom ignored all compositing of mid-sided textures on two-sided lines.
// Since these textures had to be single-patch in Doom, that essentially
// means it ignores their Y offsets.
//
// If this texture is composed of only one patch, return that patch.
// Otherwise, return this texture, since Doom wouldn't have been able to
// draw it anyway.
//
//==========================================================================

/* todo: this needs to be reimplemented without assuming that the underlying patch will be usable as-is.
FTexture *FMultiPatchTexture::GetRawTexture()
{
	return NumParts == 1 && UseType == ETextureType::Wall && bMultiPatch == 1 && Scale == Parts->Texture->Scale ? Parts->Texture : this;
}
*/

