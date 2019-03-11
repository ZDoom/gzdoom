/*
** v_text.cpp
** Draws text to a canvas. Also has a text line-breaker thingy.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <wctype.h>

#include "v_text.h"
#include "utf8.h"


#include "v_video.h"
#include "w_wad.h"
#include "image.h"
#include "textures/formats/multipatchtexture.h"

#include "gstrings.h"
#include "vm.h"
#include "serializer.h"


int ListGetInt(VMVa_List &tags);


//==========================================================================
//
// Create a texture from a text in a given font.
//
//==========================================================================

FTexture * BuildTextTexture(FFont *font, const char *string, int textcolor)
{
	int 		w;
	const uint8_t *ch;
	int 		cx;
	int 		cy;
	FRemapTable *range;
	int			kerning;
	FTexture *pic;

	kerning = font->GetDefaultKerning();

	ch = (const uint8_t *)string;
	cx = 0;
	cy = 0;


	IntRect box;

	while (auto c = GetCharFromString(ch))
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			// Here we only want to measure the texture so just parse over the color.
			V_ParseFontColor(ch, 0, 0);
			continue;
		}

		if (c == '\n')
		{
			cx = 0;
			cy += font->GetHeight();
			continue;
		}

		if (nullptr != (pic = font->GetChar(c, CR_UNTRANSLATED, &w, nullptr)))
		{
			auto img = pic->GetImage();
			auto offsets = img->GetOffsets();
			int x = cx - offsets.first;
			int y = cy - offsets.second;
			int ww = img->GetWidth();
			int h = img->GetHeight();

			box.AddToRect(x, y);
			box.AddToRect(x + ww, y + h);
		}
		cx += (w + kerning);
	}

	cx = -box.left;
	cy = -box.top;

	TArray<TexPart> part(strlen(string));

	while (auto c = GetCharFromString(ch))
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			EColorRange newcolor = V_ParseFontColor(ch, textcolor, textcolor);
			if (newcolor != CR_UNDEFINED)
			{
				range = font->GetColorTranslation(newcolor);
				textcolor = newcolor;
			}
			continue;
		}

		if (c == '\n')
		{
			cx = 0;
			cy += font->GetHeight();
			continue;
		}

		if (nullptr != (pic = font->GetChar(c, textcolor, &w, nullptr)))
		{
			auto img = pic->GetImage();
			auto offsets = img->GetOffsets();
			int x = cx - offsets.first;
			int y = cy - offsets.second;

			auto &tp = part[part.Reserve(1)];

			tp.OriginX = x;
			tp.OriginY = y;
			tp.Image = img;
			tp.Translation = range;
		}
		cx += (w + kerning);
	}
	FMultiPatchTexture *image = new FMultiPatchTexture(box.width, box.height, part, false, false);
	image->SetOffsets(-box.left, -box.top);
	FImageTexture *tex = new FImageTexture(image, "");
	tex->SetUseType(ETextureType::MiscPatch);
	TexMan.AddTexture(tex);
	return tex;
}



//==========================================================================
//
// DrawChar
//
// Write a single character using the given font
//
//==========================================================================

void DFrameBuffer::DrawChar (FFont *font, int normalcolor, double x, double y, int character, int tag_first, ...)
{
	if (font == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;

	FTexture *pic;
	int dummy;
	bool redirected;

	if (NULL != (pic = font->GetChar (character, normalcolor, &dummy, &redirected)))
	{
		DrawParms parms;
		Va_List tags;
		va_start(tags.list, tag_first);
		bool res = ParseDrawTextureTags(pic, x, y, tag_first, tags, &parms, false);
		va_end(tags.list);
		if (!res)
		{
			return;
		}
		PalEntry color = 0xffffffff;
		parms.remap = redirected? nullptr : font->GetColorTranslation((EColorRange)normalcolor, &color);
		parms.color = PalEntry((color.a * parms.color.a) / 255, (color.r * parms.color.r) / 255, (color.g * parms.color.g) / 255, (color.b * parms.color.b) / 255);
		DrawTextureParms(pic, parms);
	}
}

void DFrameBuffer::DrawChar(FFont *font, int normalcolor, double x, double y, int character, VMVa_List &args)
{
	if (font == NULL)
		return;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;

	FTexture *pic;
	int dummy;
	bool redirected;

	if (NULL != (pic = font->GetChar(character, normalcolor, &dummy, &redirected)))
	{
		DrawParms parms;
		uint32_t tag = ListGetInt(args);
		bool res = ParseDrawTextureTags(pic, x, y, tag, args, &parms, false);
		if (!res) return;
		PalEntry color = 0xffffffff;
		parms.remap = redirected ? nullptr : font->GetColorTranslation((EColorRange)normalcolor, &color);
		parms.color = PalEntry((color.a * parms.color.a) / 255, (color.r * parms.color.r) / 255, (color.g * parms.color.g) / 255, (color.b * parms.color.b) / 255);
		DrawTextureParms(pic, parms);
	}
}

DEFINE_ACTION_FUNCTION(_Screen, DrawChar)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(font, FFont);
	PARAM_INT(cr);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_INT(chr);

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	VMVa_List args = { param + 5, 0, numparam - 6, va_reginfo + 5 };
	screen->DrawChar(font, cr, x, y, chr, args);
	return 0;
}

//==========================================================================
//
// DrawText
//
// Write a string using the given font
//
//==========================================================================

// This is only needed as a dummy. The code using wide strings does not need color control.
EColorRange V_ParseFontColor(const char32_t *&color_value, int normalcolor, int boldcolor) { return CR_UNTRANSLATED; } 

template<class chartype>
void DFrameBuffer::DrawTextCommon(FFont *font, int normalcolor, double x, double y, const chartype *string, DrawParms &parms)
{
	int 		w;
	const chartype *ch;
	int 		c;
	double 		cx;
	double 		cy;
	int			boldcolor;
	FRemapTable *range;
	int			kerning;
	FTexture *pic;

	if (parms.celly == 0) parms.celly = font->GetHeight() + 1;
	parms.celly *= parms.scaley;

	if (normalcolor >= NumTextColors)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NumTextColors - 1;

	PalEntry colorparm = parms.color;
	PalEntry color = 0xffffffff;
	range = font->GetColorTranslation((EColorRange)normalcolor, &color);
	parms.color = PalEntry(colorparm.a, (color.r * colorparm.r) / 255, (color.g * colorparm.g) / 255, (color.b * colorparm.b) / 255);

	kerning = font->GetDefaultKerning();

	ch = string;
	cx = x;
	cy = y;


	auto currentcolor = normalcolor;
	while (ch - string < parms.maxstrlen)
	{
		c = GetCharFromString(ch);
		if (!c)
			break;

		if (c == TEXTCOLOR_ESCAPE)
		{
			EColorRange newcolor = V_ParseFontColor(ch, normalcolor, boldcolor);
			if (newcolor != CR_UNDEFINED)
			{
				range = font->GetColorTranslation(newcolor, &color);
				parms.color = PalEntry(colorparm.a, (color.r * colorparm.r) / 255, (color.g * colorparm.g) / 255, (color.b * colorparm.b) / 255);
				currentcolor = newcolor;
			}
			continue;
		}

		if (c == '\n')
		{
			cx = x;
			cy += parms.celly;
			continue;
		}

		bool redirected = false;
		if (NULL != (pic = font->GetChar(c, currentcolor, &w, &redirected)))
		{
			parms.remap = redirected? nullptr : range;
			SetTextureParms(&parms, pic, cx, cy);
			if (parms.cellx)
			{
				w = parms.cellx;
				parms.destwidth = parms.cellx;
				parms.destheight = parms.celly;
			}
			DrawTextureParms(pic, parms);
		}
		cx += (w + kerning) * parms.scalex;
	}
}

void DFrameBuffer::DrawText(FFont *font, int normalcolor, double x, double y, const char *string, int tag_first, ...)
{
	Va_List tags;
	DrawParms parms;

	if (font == NULL || string == NULL)
		return;

	va_start(tags.list, tag_first);
	bool res = ParseDrawTextureTags(nullptr, 0, 0, tag_first, tags, &parms, true);
	va_end(tags.list);
	if (!res)
	{
		return;
	}
	DrawTextCommon(font, normalcolor, x, y, (const uint8_t*)string, parms);
}

void DFrameBuffer::DrawText(FFont *font, int normalcolor, double x, double y, const char32_t *string, int tag_first, ...)
{
	Va_List tags;
	DrawParms parms;

	if (font == NULL || string == NULL)
		return;

	va_start(tags.list, tag_first);
	bool res = ParseDrawTextureTags(nullptr, 0, 0, tag_first, tags, &parms, true);
	va_end(tags.list);
	if (!res)
	{
		return;
	}
	DrawTextCommon(font, normalcolor, x, y, string, parms);
}

void DFrameBuffer::DrawText(FFont *font, int normalcolor, double x, double y, const char *string, VMVa_List &args)
{
	DrawParms parms;

	if (font == NULL || string == NULL)
		return;

	uint32_t tag = ListGetInt(args);
	bool res = ParseDrawTextureTags(nullptr, 0, 0, tag, args, &parms, true);
	if (!res)
	{
		return;
	}
	DrawTextCommon(font, normalcolor, x, y, (const uint8_t*)string, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawText)
{
	PARAM_PROLOGUE;
	PARAM_POINTER_NOT_NULL(font, FFont);
	PARAM_INT(cr);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_STRING(chr);

	PARAM_VA_POINTER(va_reginfo)	// Get the hidden type information array

	if (!screen->HasBegun2D()) ThrowAbortException(X_OTHER, "Attempt to draw to screen outside a draw function");
	VMVa_List args = { param + 5, 0, numparam - 6, va_reginfo + 5 };
	const char *txt = chr[0] == '$' ? GStrings(&chr[1]) : chr.GetChars();
	screen->DrawText(font, cr, x, y, txt, args);
	return 0;
}

