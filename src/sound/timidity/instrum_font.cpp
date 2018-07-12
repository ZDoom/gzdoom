#include <stdio.h>
#include <string.h>
#include <memory>

#include "timidity.h"
#include "sf2.h"
#include "i_soundfont.h"

namespace Timidity
{

FontFile *Fonts;
extern std::unique_ptr<FSoundFontReader> gus_sfreader;

FontFile *ReadDLS(const char *filename, FileReader &f)
{
	return NULL;
}

void font_freeall()
{
	FontFile *font, *next;

	for (font = Fonts; font != NULL; font = next)
	{
		next = font->Next;
		delete font;
	}
	Fonts = NULL;
}

FontFile *font_find(const char *filename)
{
	for (FontFile *font = Fonts; font != NULL; font = font->Next)
	{
		if (stricmp(filename, font->Filename) == 0)
		{
			return font;
		}
	}
	return NULL;
}

void font_add(const char *filename, int load_order)
{
	FontFile *font;

	font = font_find(filename);
	if (font != NULL)
	{
		font->SetAllOrders(load_order);
	}
	else
	{
		FileReader fp = gus_sfreader->LookupFile(filename).first;

		if (fp.isOpen())
		{
			if ((font = ReadSF2(filename, fp)) || (font = ReadDLS(filename, fp)))
			{
				font->SetAllOrders(load_order);
			}
		}
	}
}

void font_remove(const char *filename)
{
	FontFile *font;

	font = font_find(filename);
	if (font != NULL)
	{
		// Don't actually remove the font from the list, because instruments
		// from it might be loaded using the %font extension.
		font->SetAllOrders(255);
	}
}

void font_order(int order, int bank, int preset, int keynote)
{
	for (FontFile *font = Fonts; font != NULL; font = font->Next)
	{
		font->SetOrder(order, bank, preset, keynote);
	}
}

Instrument *load_instrument_font(struct Renderer *song, const char *font, int drum, int bank, int instr)
{
	FontFile *fontfile = font_find(font);
	if (fontfile != NULL)
	{
		return fontfile->LoadInstrument(song, drum, bank, instr);
	}
	return NULL;
}

Instrument *load_instrument_font_order(struct Renderer *song, int order, int drum, int bank, int instr)
{
	for (FontFile *font = Fonts; font != NULL; font = font->Next)
	{
		Instrument *ip = font->LoadInstrument(song, drum, bank, instr);
		if (ip != NULL)
		{
			return ip;
		}
	}
	return NULL;
}

FontFile::FontFile(FString filename)
: Filename(filename)
{
	Next = Fonts;
	Fonts = this;
}

FontFile::~FontFile()
{
	for (FontFile **probe = &Fonts; *probe != NULL; probe = &(*probe)->Next)
	{
		if (*probe == this)
		{
			*probe = Next;
			break;
		}
	}
}

}
