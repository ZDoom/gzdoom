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

void DCanvas::CharWrapper (EWrapperCode drawer, int normalcolor, int x, int y, byte character) const
{
	if (Font == NULL)
		return;

	if (normalcolor >= NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;

	const byte *data;
	int w, h, xo, yo;
	const byte *range = Font->GetColorTranslation ((EColorRange)normalcolor);

	if ((data = Font->GetChar (character, &w, &h, &xo, &yo)))
	{
		x -= xo;
		y -= yo;
		switch (drawer)
		{
		default:
		case ETWrapper_Normal:
			DrawMaskedBlock (x, y, w, h, data, range);
			break;
		case ETWrapper_Translucent:
			DrawTranslucentMaskedBlock (x, y, w, h, data, range, V_TextTrans);
			break;
		case ETWrapper_Shadow:
			DrawShadowedMaskedBlock (x, y, w, h, data, range, TRANSLUC50);
			break;
		}
	}
}

void DCanvas::CharSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, byte character) const
{
	if (Font == NULL)
		return;

	if (normalcolor >= NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;

	const byte *data;
	int w, h, xo, yo;
	const byte *range = Font->GetColorTranslation ((EColorRange)normalcolor);

	if ((data = Font->GetChar (character, &w, &h, &xo, &yo)))
	{
		int sw = w * CleanXfac;
		int sh = h * CleanYfac;
		x -= xo * CleanXfac;
		y -= yo * CleanYfac;
		switch (drawer)
		{
		default:
			case ETWrapper_Normal:
				ScaleMaskedBlock (x, y, w, h, sw, sh, data, range);
				break;
			case ETWrapper_Translucent:
				ScaleTranslucentMaskedBlock (x, y, w, h, sw, sh, data, range, V_TextTrans);
				break;
			case ETWrapper_Shadow:
				ScaleShadowedMaskedBlock (x, y, w, h, sw, sh, data, range, TRANSLUC50);
				break;
		}
	}
}

//
// V_DrawText
//
// Write a string using the current font
//
void DCanvas::TextWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	int 		w, h, xo, yo;
	const byte *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;
	const byte *range;
	int			height;
	const byte *data;

	if (Font == NULL || string == NULL)
		return;

	if (normalcolor >= NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	range = Font->GetColorTranslation ((EColorRange)normalcolor);
	height = Font->GetHeight () + 1;

	ch = string;
	cx = x;
	cy = y;
		
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

		if ((data = Font->GetChar (c, &w, &h, &xo, &yo)))
		{
			if (cx + w > Width)
				break;

			if (w == 0 || h == 0)
			{
				printf ("char %c (%d) has dimensions %dx%d\n", c, c, w, h);
				continue;
			}

			switch (drawer)
			{
			default:
			case ETWrapper_Normal:
				DrawMaskedBlock (cx - xo, cy - yo, w, h, data, range);
				break;
			case ETWrapper_Translucent:
				DrawTranslucentMaskedBlock (cx - xo, cy - yo, w, h, data, range, V_TextTrans);
				break;
			case ETWrapper_Shadow:
				DrawShadowedMaskedBlock (cx - xo, cy - yo, w, h, data, range, TRANSLUC50);
				break;
			}
		}
		
		cx += w;
	}
}

void DCanvas::TextSWrapper (EWrapperCode drawer, int normalcolor, int x, int y, const byte *string) const
{
	int 		w, h, xo, yo;
	const byte *ch;
	int 		c;
	int 		cx;
	int 		cy;
	int			boldcolor;
	const byte *range;
	int			height;
	const byte *data;

	if (Font == NULL || string == NULL)
		return;

	if (normalcolor > NUM_TEXT_COLORS)
		normalcolor = CR_UNTRANSLATED;
	boldcolor = normalcolor ? normalcolor - 1 : NUM_TEXT_COLORS - 1;

	range = Font->GetColorTranslation ((EColorRange)normalcolor);
	height = (Font->GetHeight () + 1) * CleanYfac;

	ch = string;
	cx = x;
	cy = y;
		
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

		if ((data = Font->GetChar (c, &w, &h, &xo, &yo)))
		{
			int sw = w * CleanXfac;

//			if (cx + sw > Width)
//				break;

			switch (drawer)
			{
			default:
			case ETWrapper_Normal:
				ScaleMaskedBlock (cx - xo * CleanXfac, cy - yo * CleanYfac, w, h,
					sw, h * CleanYfac, data, range);
				break;
			case ETWrapper_Translucent:
				ScaleTranslucentMaskedBlock (cx - xo * CleanXfac, cy - yo * CleanYfac, w, h,
					sw, h * CleanYfac, data, range, V_TextTrans);
				break;
			case ETWrapper_Shadow:
				ScaleShadowedMaskedBlock (cx - xo * CleanXfac, cy - yo * CleanYfac, w, h,
					sw, h * CleanYfac, data, range, TRANSLUC50);
				break;
			}
			cx += sw;
		}
		else
		{
			cx += w * CleanXfac;
		}
	}
}

//
// Find string width using current font
//
int DCanvas::StringWidth (const BYTE *string) const
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
			w += Font->GetCharWidth (*string++);
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
	line->width = screen->StringWidth (line->string);
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
