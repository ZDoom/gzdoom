/*
** gl_translate.cpp
** GL-related translation stuff
**
**---------------------------------------------------------------------------
** Copyright 2007 Christoph Oelckers
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
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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
#include "r_data/r_translate.h"
#include "gl/textures/gl_translate.h"
#include "m_crc32.h"

TArray<GLTranslationPalette::PalData> GLTranslationPalette::AllPalettes;


GLTranslationPalette *GLTranslationPalette::CreatePalette(FRemapTable *remap)
{
	GLTranslationPalette *p = new GLTranslationPalette(remap);
	p->Update();
	return p;
}

bool GLTranslationPalette::Update()
{
	PalData pd;

	memset(pd.pe, 0, sizeof(pd.pe));
	memcpy(pd.pe, remap->Palette, remap->NumEntries * sizeof(*remap->Palette));
	pd.crc32 = CalcCRC32((BYTE*)pd.pe, sizeof(pd.pe));
	for(unsigned int i=0;i< AllPalettes.Size(); i++)
	{
		if (pd.crc32 == AllPalettes[i].crc32)
		{
			if (!memcmp(pd.pe, AllPalettes[i].pe, sizeof(pd.pe))) 
			{
				Index = 1+i;
				return true;
			}
		}
	}
	Index = 1+AllPalettes.Push(pd);
	return true;
}

int GLTranslationPalette::GetInternalTranslation(int trans)
{
	if (trans <= 0) return 0;

	FRemapTable *remap = TranslationToTable(trans);
	if (remap == NULL || remap->Inactive) return 0;

	GLTranslationPalette *tpal = static_cast<GLTranslationPalette*>(remap->GetNative());
	if (tpal == NULL) return 0;
	return tpal->GetIndex();
}
