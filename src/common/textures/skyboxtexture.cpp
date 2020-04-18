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

#include "filesystem.h"
#include "textures.h"
#include "skyboxtexture.h"
#include "bitmap.h"
#include "texturemanager.h"



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyBox::FSkyBox(const char *name)
	: FImageTexture(nullptr)
{
	FTextureID texid = TexMan.CheckForTexture(name, ETextureType::Wall);
	if (texid.isValid())
	{
		previous = TexMan.GetGameTexture(texid);
	}
	else previous = nullptr;
	faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5] = nullptr;
	fliptop = false;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyBox::SetSize()
{
	if (!previous && faces[0]) previous = faces[0];
	if (previous && previous->GetTexture()->GetImage())
	{
		SetImage(previous->GetTexture()->GetImage());
	}
}
