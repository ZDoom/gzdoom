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

#include "gstrings.h"
#include "vm.h"
#include "serializer.h"

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

	// The real isspace is a bit too badly defined, so use our own one
	auto myisspace = [](int ch) { return ch == '\t' || ch == '\r' || ch == '\n' || ch == ' '; };

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

		if (myisspace(c)) 
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
			{
				for (space = string - 1; (*space & 0xc0) == 0x80 && space > start; space--);
			}

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

			while (*start && myisspace (*start) && *start != '\n')
				start++;
			if (*start == '\n')
				start++;
			else
				while (*start && myisspace (*start))
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
			if (!myisspace (*s++))
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
