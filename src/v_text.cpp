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

#include "i_system.h"
#include "v_video.h"
#include "w_wad.h"

#include "gstrings.h"
#include "vm.h"
#include "serializer.h"

int ListGetInt(VMVa_List &tags);

//==========================================================================
//
// reads one character from the string.
// This can handle both ISO 8859-1 and UTF-8, as well as mixed strings
// between both encodings, which may happen if inconsistent encoding is 
// used between different files in a mod.
//
//==========================================================================

int GetCharFromString(const uint8_t *&string)
{
	int z, y, x;

	z = *string++;

	if (z < 192)
	{
		return z;
	}
	else if (z <= 223)
	{
		y = *string++;
		if (y < 128 || y >= 192)
		{
			// not an UTF-8 sequence so return the first byte unchanged
			string--;
		}
		else
		{
			z = (z - 192) * 64 + (y - 128);
		}
	}
	else if (z >= 224 && z <= 239)
	{
		y = *string++;
		if (y < 128 || y >= 192)
		{
			// not an UTF-8 sequence so return the first byte unchanged
			string--;
		}
		else
		{
			x = *string++;
			if (x < 128 || x >= 192)
			{
				// not an UTF-8 sequence so return the first byte unchanged
				string -= 2;
			}
			else
			{
				z = (z - 224) * 4096 + (y - 128) * 64 + (x - 128);
			}
		}
	}
	else if (z >= 240)
	{
		y = *string++;
		if (y < 128 || y >= 192)
		{
			// not an UTF-8 sequence so return the first byte unchanged
			string--;
		}
		else
		{
			// we do not support 4-Byte UTF-8 here
			string += 2;
			return '?';
		}
	}
	return z;
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

void DFrameBuffer::DrawTextCommon(FFont *font, int normalcolor, double x, double y, const char *string, DrawParms &parms)
{
	int 		w;
	const uint8_t *ch;
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

	ch = (const uint8_t *)string;
	cx = x;
	cy = y;


	auto currentcolor = normalcolor;
	while ((const char *)ch - string < parms.maxstrlen)
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
	DrawTextCommon(font, normalcolor, x, y, string, parms);
}

DEFINE_ACTION_FUNCTION(_Screen, DrawText)
{
	PARAM_PROLOGUE;
	PARAM_POINTER(font, FFont);
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


//==========================================================================
//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
//==========================================================================

static void breakit (FBrokenLines *line, FFont *font, const uint8_t *start, const uint8_t *stop, FString &linecolor)
{
	if (!linecolor.IsEmpty())
	{
		line->Text = TEXTCOLOR_ESCAPE;
		line->Text += linecolor;
	}
	line->Text.AppendCStrPart ((const char *)start, stop - start);
	line->Width = font->StringWidth (line->Text);
}

TArray<FBrokenLines> V_BreakLines (FFont *font, int maxwidth, const uint8_t *string, bool preservecolor)
{
	TArray<FBrokenLines> Lines(128);

	const uint8_t *space = NULL, *start = string;
	int c, w, nw;
	FString lastcolor, linecolor;
	bool lastWasSpace = false;
	int kerning = font->GetDefaultKerning ();

	w = 0;

	while ( (c = GetCharFromString(string)) )
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			if (*string)
			{
				if (*string == '[')
				{
					const uint8_t *start = string;
					while (*string != ']' && *string != '\0')
					{
						string++;
					}
					if (*string != '\0')
					{
						string++;
					}
					lastcolor = FString((const char *)start, string - start);
				}
				else
				{
					lastcolor = *string++;
				}
			}
			continue;
		}

		if (iswspace(c)) 
		{
			if (!lastWasSpace)
			{
				space = string - 1;
				lastWasSpace = true;
			}
		}
		else
		{
			lastWasSpace = false;
		}

		nw = font->GetCharWidth (c);

		if ((w > 0 && w + nw > maxwidth) || c == '\n')
		{ // Time to break the line
			if (!space)
				space = string - 1;

			auto index = Lines.Reserve(1);
			breakit (&Lines[index], font, start, space, linecolor);
			if (c == '\n' && !preservecolor)
			{
				lastcolor = "";		// Why, oh why, did I do it like this?
			}
			linecolor = lastcolor;

			w = 0;
			lastWasSpace = false;
			start = space;
			space = NULL;

			while (*start && iswspace (*start) && *start != '\n')
				start++;
			if (*start == '\n')
				start++;
			else
				while (*start && iswspace (*start))
					start++;
			string = start;
		}
		else
		{
			w += nw + kerning;
		}
	}

	// String here is pointing one character after the '\0'
	if (--string - start >= 1)
	{
		const uint8_t *s = start;

		while (s < string)
		{
			// If there is any non-white space in the remainder of the string, add it.
			if (!iswspace (*s++))
			{
				auto i = Lines.Reserve(1);
				breakit (&Lines[i], font, start, string, linecolor);
				break;
			}
		}
	}
	return Lines;
}

FSerializer &Serialize(FSerializer &arc, const char *key, FBrokenLines& g, FBrokenLines *def)
{
	if (arc.BeginObject(key))
	{
		arc("text", g.Text)
			("width", g.Width)
			.EndObject();
	}
	return arc;
}



class DBrokenLines : public DObject
{
	DECLARE_CLASS(DBrokenLines, DObject)

public:
	TArray<FBrokenLines> mBroken;

	DBrokenLines() = default;

	DBrokenLines(TArray<FBrokenLines> &broken)
	{
		mBroken = std::move(broken);
	}

	void Serialize(FSerializer &arc) override
	{
		arc("lines", mBroken);
	}
};

IMPLEMENT_CLASS(DBrokenLines, false, false);

DEFINE_ACTION_FUNCTION(DBrokenLines, Count)
{
	PARAM_SELF_PROLOGUE(DBrokenLines);
	ACTION_RETURN_INT(self->mBroken.Size());
}

DEFINE_ACTION_FUNCTION(DBrokenLines, StringWidth)
{
	PARAM_SELF_PROLOGUE(DBrokenLines);
	PARAM_INT(index);
	ACTION_RETURN_INT((unsigned)index >= self->mBroken.Size()? -1 : self->mBroken[index].Width);
}

DEFINE_ACTION_FUNCTION(DBrokenLines, StringAt)
{

	PARAM_SELF_PROLOGUE(DBrokenLines);
	PARAM_INT(index);
	ACTION_RETURN_STRING((unsigned)index >= self->mBroken.Size() ? -1 : self->mBroken[index].Text);
}

DEFINE_ACTION_FUNCTION(FFont, BreakLines)
{
	PARAM_SELF_STRUCT_PROLOGUE(FFont);
	PARAM_STRING(text);
	PARAM_INT(maxwidth);

	auto broken = V_BreakLines(self, maxwidth, text, true);
	ACTION_RETURN_OBJECT(Create<DBrokenLines>(broken));
}
