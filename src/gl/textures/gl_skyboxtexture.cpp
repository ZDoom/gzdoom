/*
** gl_skyboxtexture.cpp
**
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
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
*/

#include "doomtype.h"
#include "sc_man.h"
#include "w_wad.h"
#include "textures/textures.h"
#include "gl/textures/gl_skyboxtexture.h"



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyBox::FSkyBox() 
{ 
	faces[0]=faces[1]=faces[2]=faces[3]=faces[4]=faces[5]=NULL; 
	UseType=TEX_Override;
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

const BYTE *FSkyBox::GetColumn (unsigned int column, const Span **spans_out)
{
	if (faces[0]) return faces[0]->GetColumn(column, spans_out);
	return NULL;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

const BYTE *FSkyBox::GetPixels ()
{
	if (faces[0]) return faces[0]->GetPixels();
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

void FSkyBox::PrecacheGL()
{
	//for(int i=0;i<6;i++) if (faces[i]) faces[i]->PrecacheGL();
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void gl_ParseSkybox(FScanner &sc)
{
	int facecount=0;

	sc.MustGetString();

	FSkyBox * sb = new FSkyBox;
	uppercopy(sb->Name, sc.String);
	sb->Name[8]=0;
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
			sb->faces[facecount] = TexMan[TexMan.GetTexture(sc.String, FTexture::TEX_Wall, FTextureManager::TEXMAN_TryAny|FTextureManager::TEXMAN_Overridable)];
		}
		facecount++;
	}
	if (facecount != 3 && facecount != 6)
	{
		sc.ScriptError("%s: Skybox definition requires either 3 or 6 faces", sb->Name);
	}
	sb->SetSize();
	TexMan.AddTexture(sb);
}

//-----------------------------------------------------------------------------
//
// gl_ParseVavoomSkybox
//
//-----------------------------------------------------------------------------

void gl_ParseVavoomSkybox()
{
	int lump = Wads.CheckNumForName("SKYBOXES");

	if (lump < 0) return;

	FScanner sc(lump);
	while (sc.GetString())
	{
		int facecount=0;
		int maplump = -1;
		FSkyBox * sb = new FSkyBox;
		uppercopy(sb->Name, sc.String);
		sb->Name[8]=0;
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
				if (maplump==-1) 
					Printf("Texture '%s' not found in Vavoom skybox '%s'\n", sc.String, sb->Name);

				FTextureID tex = TexMan.FindTextureByLumpNum(maplump);
				if (!tex.isValid())
				{
					tex = TexMan.CreateTexture(maplump, FTexture::TEX_MiscPatch);
				}
				sb->faces[facecount] = TexMan[tex];
				sc.MustGetStringName("}");
			}
			facecount++;
		}
		if (facecount != 6)
		{
			sc.ScriptError("%s: Skybox definition requires 6 faces", sb->Name);
		}
		sb->SetSize();
		TexMan.AddTexture(sb);
	}
}

