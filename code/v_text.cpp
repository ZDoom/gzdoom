#include <ctype.h>

#include "v_text.h"

#include "i_system.h"
#include "v_video.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_swap.h"

#include "doomstat.h"

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

	if (Font == NULL)
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

		if (c == 0x81)
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

	if (Font == NULL)
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

		if (c == 0x81)
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

			if (cx + sw > Width)
				break;

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
int DCanvas::StringWidth (const char *string) const
{
	int w = 0;
		
	while (*string)
	{
		if (*string == '\x81')
		{
			if (*(++string))
				string++;
			continue;
		}
		else
		{
			w += Font->GetCharWidth (*string++);
		}
	}
				
	return w;
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
		if (c == 0x81)
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

		if (w + nw > maxwidth || c == '\n')
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