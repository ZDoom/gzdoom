/*
** startscreentexture.cpp
** Texture class to create a texture from the start screen's imagé
**
**---------------------------------------------------------------------------
** Copyright 2004-2006 Randy Heit
** Copyright 2019 Christoph Oelckers
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
#include "gi.h"
#include "bitmap.h"
#include "textures.h"
#include "imagehelpers.h"
#include "image.h"
#include "startscreen.h"


//==========================================================================
//
//
//
//==========================================================================

class FStartScreenTexture : public FImageSource
{
	FBitmap& info; // This must remain constant for the lifetime of this texture

public:
	FStartScreenTexture(FBitmap& srcdata)
		: FImageSource(-1), info(srcdata)
	{
		Width = srcdata.GetWidth();
		Height = srcdata.GetHeight();
		bUseGamePalette = false;
	}
	int CopyPixels(FBitmap* bmp, int conversion)
	{
		bmp->Blit(0, 0, info);
		return 0;
	}
};

//==========================================================================
//
//
//
//==========================================================================

FImageSource *CreateStartScreenTexture(FBitmap& srcdata)
{
	return new FStartScreenTexture(srcdata);
}


