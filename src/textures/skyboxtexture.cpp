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



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyBox::FSkyBox() 
{ 
	faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5]=NULL; 
	UseType = ETextureType::Override;
	bSkybox = true;
	fliptop = false;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyBox::~FSkyBox()
{
	// The faces are only referenced but not owned so don't delete them.
}

//-----------------------------------------------------------------------------
//
// If something attempts to use this as a texture just pass the information of the first face.
//
//-----------------------------------------------------------------------------

const uint8_t *FSkyBox::GetColumn(FRenderStyle style, unsigned int column, const Span **spans_out)
{
	if (faces[0]) return faces[0]->GetColumn(style, column, spans_out);
	return NULL;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

const uint8_t *FSkyBox::GetPixels (FRenderStyle style)
{
	if (faces[0]) return faces[0]->GetPixels(style);
	return NULL;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

int FSkyBox::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	if (faces[0]) return faces[0]->CopyTrueColorPixels(bmp, x, y, rotate, inf);
	return 0;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

bool FSkyBox::UseBasePalette() 
{ 
	return false; // not really but here it's not important.
}	

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyBox::Unload () 
{
}

