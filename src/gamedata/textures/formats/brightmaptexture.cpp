/*
** brightmaptexture.cpp
** The texture class for colormap based brightmaps.
**
**---------------------------------------------------------------------------
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

#include "filesystem.h"
#include "palettecontainer.h"
#include "bitmap.h"
#include "image.h"

class FBrightmapTexture : public FImageSource
{
public:
	FBrightmapTexture (FImageSource *source);

	int CopyPixels(FBitmap *bmp, int conversion) override;

protected:
	FImageSource *SourcePic;
};

//===========================================================================
//
// fake brightness maps
// These are generated for textures affected by a colormap with
// fullbright entries.
//
//===========================================================================

FBrightmapTexture::FBrightmapTexture (FImageSource *source)
{
	SourcePic = source;
	Width = source->GetWidth();
	Height = source->GetHeight();
	bMasked = false;
}

int FBrightmapTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	SourcePic->CopyTranslatedPixels(bmp, GPalette.GlobalBrightmap.Palette);
	return 0;
}

FTexture *CreateBrightmapTexture(FImageSource *tex)
{
	return CreateImageTexture(new FBrightmapTexture(tex));
}
