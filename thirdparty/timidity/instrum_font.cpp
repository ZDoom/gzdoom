#include <stdio.h>
#include <string.h>
#include <memory>

#include "timidity.h"
#include "common.h"
#include "instrum.h"
#include "sf2.h"

#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

namespace Timidity
{


FontFile *Instruments::ReadDLS(const char *filename, timidity_file *f)
{
	return NULL;
}

void Instruments::font_freeall()
{
	FontFile *font, *next;

	for (font = Fonts; font != NULL; font = next)
	{
		next = font->Next;
		delete font;
	}
	Fonts = NULL;
}

FontFile * Instruments::font_find(const char *filename)
{
	for (FontFile *font = Fonts; font != NULL; font = font->Next)
	{
		if (stricmp(filename, font->Filename.c_str()) == 0)
		{
			return font;
		}
	}
	return NULL;
}

void Instruments::font_add(const char *filename, int load_order)
{
	FontFile *font;

	font = font_find(filename);
	if (font != NULL)
	{
		font->SetAllOrders(load_order);
	}
	else
	{
		auto fp = sfreader->open_file(filename);

		if (fp)
		{
			if ((font = ReadSF2(filename, fp)) || (font = ReadDLS(filename, fp)))
			{
				font->Next = Fonts;
				Fonts = font;

				font->SetAllOrders(load_order);
			}
			fp->close();
		}
	}
}

void Instruments::font_remove(const char *filename)
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

void Instruments::font_order(int order, int bank, int preset, int keynote)
{
	for (FontFile *font = Fonts; font != NULL; font = font->Next)
	{
		font->SetOrder(order, bank, preset, keynote);
	}
}

Instrument *Renderer::load_instrument_font(const char *font, int drum, int bank, int instr)
{
	FontFile *fontfile = instruments->font_find(font);
	if (fontfile != NULL)
	{
		return fontfile->LoadInstrument(this, drum, bank, instr);
	}
	return NULL;
}

Instrument *Renderer::load_instrument_font_order(int order, int drum, int bank, int instr)
{
	for (FontFile *font = instruments->Fonts; font != NULL; font = font->Next)
	{
		Instrument *ip = font->LoadInstrument(this, drum, bank, instr);
		if (ip != NULL)
		{
			return ip;
		}
	}
	return NULL;
}

FontFile::FontFile(const char *filename)
: Filename(filename)
{
}

FontFile::~FontFile()
{
}

}
