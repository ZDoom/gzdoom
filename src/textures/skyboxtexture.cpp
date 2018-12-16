// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "doomtype.h"
#include "w_wad.h"
#include "textures.h"
#include "skyboxtexture.h"
#include "bitmap.h"



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyBox::FSkyBox(const char *name)
: FTexture(name)
{
	FTextureID texid = TexMan.CheckForTexture(name, ETextureType::Wall);
	previous = nullptr;
	if (texid.isValid())
	{
		previous = TexMan.GetTexture(texid);
		CopySize(previous);
	}
	faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5] = nullptr;
	UseType = ETextureType::Override;
	bSkybox = true;
	fliptop = false;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

TArray<uint8_t> FSkyBox::Get8BitPixels(bool alphatex)
{
	return previous->Get8BitPixels(alphatex);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FBitmap FSkyBox::GetBgraBitmap(PalEntry *p, int *trans)
{
	return previous->GetBgraBitmap(p, trans);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FImageSource *FSkyBox::GetImage() const
{
	return previous->GetImage();
}
