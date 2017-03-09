// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2007-2016 Christoph Oelckers
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
/*
** gl_translate.cpp
** GL-related translation stuff
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
	pd.crc32 = CalcCRC32((uint8_t*)pd.pe, sizeof(pd.pe));
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
