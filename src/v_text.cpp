/*
** v_text.cpp
** Draws text to a canvas. Also has a text line-breaker thingy.
**
**---------------------------------------------------------------------------
** Copyright 1998-2001 Randy Heit
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

#include "v_text.h"

#include "i_system.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "m_swap.h"

#include "doomstat.h"
#include "templates.h"

fixed_t V_TextTrans;

//
// SetFont
//
// Set the canvas's font
//
void DCanvas::SetFont (FFont *font)
{
	Font = font;
}

void DCanvas::DrawChar (int normalcolor, int x, int y, byte character, DWORD tags, ...)
{
	if (Font == NULL)
		return;

	if (normalcolor >= NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;

	FTexture *pic;
	int dummy;

	if (NULL != (pic = Font->GetChar (character, &dummy)))
	{
		const BYTE *range = Font->GetColorTranslation ((EColorRange)normalcolor);
		DrawTexture (pic, x, y, DTA_Translation, range, TAG_MORE, &tags);
	}
}

//
// DrawText
//
// Write a string using the current font
//
void DCanvas::DrawText (int normalcolor, int x, int y, const char *string, DWORD tags_first, ...)
{
	va_list tags;
	DWORD tag;
	BOOL boolval;

	int 		w, maxwidth;
	const byte *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;
	const byte *range;
	int			height;
	int			scalex, scaley;
	FTexture *pic;

	if (Font == NULL || string == NULL)
		return;

	if (normalcolor >= NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	range = Font->GetColorTranslation ((EColorRange)normalcolor);
	height = Font->GetHeight () + 1;

	ch = (const byte *)string;
	cx = x;
	cy = y;

	// Parse the tag list to see if we need to adjust for scaling.
	bool scaled = false;
	maxwidth = Width;
	scalex = scaley = 1;

	va_start (tags, tags_first);
	tag = tags_first;

	while (tag != TAG_DONE)
	{
		DWORD *more_p;
		DWORD data;
		void *ptrval;

		switch (tag)
		{
		case TAG_IGNORE:
		default:
			data = va_arg (tags, DWORD);
			break;

		case TAG_MORE:
			more_p = va_arg (tags, DWORD *);
			va_end (tags);
			va_start (tags, *more_p);
			tag = *more_p;
			continue;

		case DTA_DestWidth:
		case DTA_DestHeight:
			*(DWORD *)tags = TAG_IGNORE;
			data = va_arg (tags, DWORD);
			break;

		case DTA_Translation:
			*(DWORD *)tags = TAG_IGNORE;
			ptrval = va_arg (tags, void*);
			break;

		case DTA_CleanNoMove:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				scalex = CleanXfac;
				scaley = CleanYfac;
				maxwidth = Width - (Width % CleanYfac);
			}
			break;

		case DTA_Clean:
		case DTA_320x200:
			boolval = va_arg (tags, BOOL);
			if (boolval)
			{
				scalex = scaley = 1;
				maxwidth = 320;
			}
			break;

		case DTA_VirtualWidth:
			maxwidth = va_arg (tags, int);
			scalex = scaley = 1;
			break;
		}
		tag = va_arg (tags, DWORD);
	}

	height *= scaley;
		
	for (;;)
	{
		c = *ch++;
		if (!c)
			break;

		if (c == TEXTCOLOR_ESCAPE)
		{
			int newcolor = toupper(*ch++);

			if (newcolor == 0)
			{
				return;
			}
			else if (newcolor == '-')
			{
				newcolor = normalcolor;
			}
			else if (newcolor >= 'A' && newcolor < 'A' + NUM_TEXT_COLORS)
			{
				newcolor -= 'A';
			}
			else if (newcolor == '+')
			{
				newcolor = boldcolor;
			}
			else
			{
				continue;
			}
			range = Font->GetColorTranslation ((EColorRange)newcolor);
			continue;
		}
		
		if (c == '\n')
		{
			cx = x;
			cy += height;
			continue;
		}

		if (NULL != (pic = Font->GetChar (c, &w)))
		{
			DrawTexture (pic, cx, cy, DTA_Translation, range, TAG_MORE, &tags_first);
		}
		cx += w * scalex;
	}
}

//
// Find string width using this font
//
int FFont::StringWidth (const BYTE *string) const
{
	int w = 0;
	int maxw = 0;
		
	while (*string)
	{
		if (*string == TEXTCOLOR_ESCAPE)
		{
			if (*(++string))
				++string;
			continue;
		}
		else if (*string == '\n')
		{
			if (w > maxw)
				maxw = w;
			w = 0;
			++string;
		}
		else
		{
			w += GetCharWidth (*string++);
		}
	}
				
	return MAX (maxw, w);
}

//
// Break long lines of text into multiple lines no longer than maxwidth pixels
//
static void breakit (brokenlines_t *line, const byte *start, const byte *string, bool keepspace)
{
	// Leave out trailing white space
	if (!keepspace)
	{
		while (string > start && isspace (*(string - 1)))
			string--;
	}

	line->string = new char[string - start + 1];
	strncpy (line->string, (char *)start, string - start);
	line->string[string - start] = 0;
	line->width = screen->Font->StringWidth (line->string);
}

brokenlines_t *V_BreakLines (int maxwidth, const byte *string, bool keepspace)
{
	brokenlines_t lines[128];	// Support up to 128 lines (should be plenty)

	const byte *space = NULL, *start = string;
	int i, c, w, nw;
	BOOL lastWasSpace = false;

	i = w = 0;

	while ( (c = *string++) )
	{
		if (c == TEXTCOLOR_ESCAPE)
		{
			if (*string)
				string++;
			continue;
		}

		if (isspace(c)) 
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

		nw = screen->Font->GetCharWidth (c);

		if ((w > 0 && w + nw > maxwidth) || c == '\n')
		{ // Time to break the line
			if (!space)
				space = string - 1;

			lines[i].nlterminated = (c == '\n');
			breakit (&lines[i], start, space, keepspace);

			i++;
			w = 0;
			lastWasSpace = false;
			start = space;
			space = NULL;

			while (*start && isspace (*start) && *start != '\n')
				start++;
			if (*start == '\n')
				start++;
			else
				while (*start && isspace (*start))
					start++;
			string = start;
		}
		else
		{
			w += nw;
		}
	}

	if (string - start > 1)
	{
		const byte *s = start;

		while (s < string)
		{
			if (keepspace || !isspace (*s))
			{
				lines[i].nlterminated = (*s == '\n');
				s++;
				breakit (&lines[i++], start, string, keepspace);
				break;
			}
			s++;
		}
	}

	{
		// Make a copy of the broken lines and return them
		brokenlines_t *broken = new brokenlines_t[i+1];

		memcpy (broken, lines, sizeof(brokenlines_t) * i);
		broken[i].string = NULL;
		broken[i].width = -1;

		return broken;
	}
}

void V_FreeBrokenLines (brokenlines_t *lines)
{
	if (lines)
	{
		int i = 0;

		while (lines[i].width != -1)
		{
			delete[] lines[i].string;
			lines[i].string = NULL;
			i++;
		}
		delete[] lines;
	}
}
