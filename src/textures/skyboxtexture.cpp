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
#include "sc_man.h"
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
	gl_info.bSkybox = true;
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
	//for(int i=0;i<6;i++) if (faces[i]) faces[i]->Unload();
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void ParseGldefSkybox(FScanner &sc)
{
	int facecount=0;

	sc.MustGetString();

	FSkyBox * sb = new FSkyBox;
	sb->Name = sc.String;
	sb->Name.ToUpper();
	if (sc.CheckString("fliptop"))
	{
		sb->fliptop = true;
	}
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		sc.MustGetString();
		if (facecount<6) 
		{
			sb->faces[facecount] = TexMan[TexMan.GetTexture(sc.String, ETextureType::Wall, FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_Overridable)];
		}
		facecount++;
	}
	if (facecount != 3 && facecount != 6)
	{
		sc.ScriptError("%s: Skybox definition requires either 3 or 6 faces", sb->Name.GetChars());
	}
	sb->SetSize();
	TexMan.AddTexture(sb);
}

//-----------------------------------------------------------------------------
//
// gl_ParseVavoomSkybox
//
//-----------------------------------------------------------------------------

void ParseVavoomSkybox()
{
	int lump = Wads.CheckNumForName("SKYBOXES");

	if (lump < 0) return;

	FScanner sc(lump);
	while (sc.GetString())
	{
		int facecount=0;
		int maplump = -1;
		bool error = false;
		FSkyBox * sb = new FSkyBox;
		sb->Name = sc.String;
		sb->Name.ToUpper();
		sb->fliptop = true;
		sc.MustGetStringName("{");
		while (!sc.CheckString("}"))
		{
			if (facecount<6) 
			{
				sc.MustGetStringName("{");
				sc.MustGetStringName("map");
				sc.MustGetString();

				maplump = Wads.CheckNumForFullName(sc.String, true);

				FTexture *tex = TexMan.FindTexture(sc.String, ETextureType::Wall, FTextureManager::TEXMAN_TryAny);
				if (tex == NULL)
				{
					sc.ScriptMessage("Texture '%s' not found in Vavoom skybox '%s'\n", sc.String, sb->Name.GetChars());
					error = true;
				}
				sb->faces[facecount] = tex;
				sc.MustGetStringName("}");
			}
			facecount++;
		}
		if (facecount != 6)
		{
			sc.ScriptError("%s: Skybox definition requires 6 faces", sb->Name.GetChars());
		}
		sb->SetSize();
		if (!error) TexMan.AddTexture(sb);
	}
}

