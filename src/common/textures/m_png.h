#ifndef __M_PNG_H
#define __M_PNG_H
/*
** m_png.h
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Randy Heit
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
*/

#include <utility>
#include <variant>
#include <stdio.h>
#include "zstring.h"
#include "files.h"
#include "palentry.h"
#include "tarray.h"

// PNG Reading --------------------------------------------------------------

class FBitmap;

bool M_IsPNG(FileSys::FileReader &fr);

bool M_GetPNGSize(FileSys::FileReader &fr, uint32_t &width, uint32_t &height, int32_t *ofs_x = nullptr, int32_t *ofs_y = nullptr, bool *isMasked = nullptr);

bool M_ReadPNG(FileSys::FileReader &&fr, FBitmap * out);

class FGameTexture;

FGameTexture *PNGTexture_CreateFromFile(FileSys::FileReader &&fr, const FString &filename);

// PNG Writing --------------------------------------------------------------

enum ESSType
{
	SS_PAL,
	SS_RGB,
	SS_BGRA
};

//palette = null, buffer must be rgb or bgra, 24/32 bpp
//palette = non-null, buffer must be indices, 8 bpp
bool M_WritePNG(const uint8_t * buffer, uint32_t width, uint32_t height, const PalEntry (*palette)[256], double gamma, bool bgra, bool upside_down, const TArray<std::pair<FString, FString>> &text, FileWriter &out);
bool M_CreateDummyPNG (FileWriter *file);

#endif
