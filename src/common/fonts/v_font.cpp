/*
** v_font.cpp
** Font management
**
**---------------------------------------------------------------------------
** Copyright 1998-2016 Randy Heit
** Copyright 2005-2019 Christoph Oelckers
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

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "templates.h"
#include "m_swap.h"
#include "v_font.h"
#include "filesystem.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "gstrings.h"
#include "image.h"
#include "utf8.h"
#include "fontchars.h"
#include "textures.h"
#include "texturemanager.h"
#include "printf.h"
#include "palentry.h"

#include "fontinternals.h"

// MACROS ------------------------------------------------------------------

#define DEFAULT_LOG_COLOR	PalEntry(223,223,223)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static int TranslationMapCompare (const void *a, const void *b);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int PrintColors[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------
FFont* SmallFont, * SmallFont2, * BigFont, * BigUpper, * ConFont, * IntermissionFont, * NewConsoleFont, * NewSmallFont, 
	* CurrentConsoleFont, * OriginalSmallFont, * AlternativeSmallFont, * OriginalBigFont, *AlternativeBigFont;

FFont *FFont::FirstFont = nullptr;
int NumTextColors;
static bool translationsLoaded;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

TArray<TranslationParm> TranslationParms[2];
TArray<TranslationMap> TranslationLookup;
TArray<PalEntry> TranslationColors;

// CODE --------------------------------------------------------------------

FFont *V_GetFont(const char *name, const char *fontlumpname)
{
	if (!stricmp(name, "DBIGFONT")) name = "BigFont";
	else if (!stricmp(name, "CONFONT")) name = "ConsoleFont";	// several mods have used the name CONFONT directly and effectively duplicated the font.
	else if (!stricmp(name, "INDEXFON")) name = "IndexFont";	// Same here - for whatever reason some people had to use its 8 character name...
	FFont *font = FFont::FindFont (name);
	if (font == nullptr)
	{
		if (!stricmp(name, "BIGUPPER"))
		{
			font = FFont::FindFont("BIGFONT");
			if (font) return font;
		}

		int lump = -1;
		int folderfile = -1;
		
		TArray<FolderEntry> folderdata;
		FStringf path("fonts/%s/", name);
		
		// Use a folder-based font only if it comes from a later file than the single lump version.
		if (fileSystem.GetFilesInFolder(path, folderdata, true))
		{
			// This assumes that any custom font comes in one piece and not distributed across multiple resource files.
			folderfile = fileSystem.GetFileContainer(folderdata[0].lumpnum);
		}


		lump = fileSystem.CheckNumForFullName(fontlumpname? fontlumpname : name, true);
		
		if (lump != -1 && fileSystem.GetFileContainer(lump) >= folderfile)
		{
			uint32_t head;
			{
				auto lumpy = fileSystem.OpenFileReader (lump);
				lumpy.Read (&head, 4);
			}
			if ((head & MAKE_ID(255,255,255,0)) == MAKE_ID('F','O','N',0) ||
				head == MAKE_ID(0xE1,0xE6,0xD5,0x1A))
			{
				FFont *CreateSingleLumpFont (const char *fontname, int lump);
				font = CreateSingleLumpFont (name, lump);
				if (translationsLoaded) font->LoadTranslations();
				return font;
			}
		}
		FTextureID picnum = TexMan.CheckForTexture (name, ETextureType::Any);
		if (picnum.isValid())
		{
			auto tex = TexMan.GetGameTexture(picnum);
			if (tex && tex->GetSourceLump() >= folderfile)
			{
				FFont *CreateSinglePicFont(const char *name);
				font =  CreateSinglePicFont (name);
				return font;
			}
		}
		if (folderdata.Size() > 0)
		{
			font = new FFont(name, nullptr, name, 0, 0, 1, -1);
			if (translationsLoaded) font->LoadTranslations();
			return font;
		}
	}
	return font;
}

//==========================================================================
//
// V_InitCustomFonts
//
// Initialize a list of custom multipatch fonts
//
//==========================================================================

void V_InitCustomFonts()
{
	FScanner sc;
	FGameTexture *lumplist[256];
	bool notranslate[256];
	bool donttranslate;
	FString namebuffer, templatebuf;
	int i;
	int llump,lastlump=0;
	int format;
	int start;
	int first;
	int count;
	int spacewidth = -1;
	int kerning;
	char cursor = '_';
	bool ignoreoffsets = false;
	int MinLum = -1, MaxLum = -1;

	while ((llump = fileSystem.FindLump ("FONTDEFS", &lastlump)) != -1)
	{
		sc.OpenLumpNum(llump);
		while (sc.GetString())
		{
			memset (lumplist, 0, sizeof(lumplist));
			memset (notranslate, 0, sizeof(notranslate));
			donttranslate = false;
			namebuffer = sc.String;
			format = 0;
			start = 33;
			first = 33;
			count = 223;
			spacewidth = -1;
			kerning = 0;

			sc.MustGetStringName ("{");
			while (!sc.CheckString ("}"))
			{
				sc.MustGetString();
				if (sc.Compare ("TEMPLATE"))
				{
					if (format == 2) goto wrong;
					sc.MustGetString();
					templatebuf = sc.String;
					format = 1;
				}
				else if (sc.Compare ("BASE"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					start = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("FIRST"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					first = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("COUNT"))
				{
					if (format == 2) goto wrong;
					sc.MustGetNumber();
					count = sc.Number;
					format = 1;
				}
				else if (sc.Compare ("CURSOR"))
				{
					sc.MustGetString();
					cursor = sc.String[0];
				}
				else if (sc.Compare ("SPACEWIDTH"))
				{
					sc.MustGetNumber();
					spacewidth = sc.Number;
				}
				else if (sc.Compare("DONTTRANSLATE"))
				{
					donttranslate = true;
				}
				else if (sc.Compare ("NOTRANSLATION"))
				{
					if (format == 1) goto wrong;
					while (sc.CheckNumber() && !sc.Crossed)
					{
						if (sc.Number >= 0 && sc.Number < 256)
							notranslate[sc.Number] = true;
					}
					format = 2;
				}
				else if (sc.Compare("KERNING"))
				{
					sc.MustGetNumber();
					kerning = sc.Number;
				}
				else if (sc.Compare("ignoreoffsets"))
				{
					ignoreoffsets = true;
				}
				else if (sc.Compare("minluminosity"))
				{
					sc.MustGetValue(false);
					MinLum = (int16_t)clamp(sc.Number, 0, 255);
				}
				else if (sc.Compare("maxluminosity"))
				{
					sc.MustGetValue(false);
					MaxLum = (int16_t)clamp(sc.Number, 0, 255);
				}
				else
				{
					if (format == 1) goto wrong;
					// The braces must be filtered so because they'd be treated as block terminators otherwise.
					if (!strcmp(sc.String, "-{")) strcpy(sc.String, "{");
					if (!strcmp(sc.String, "-}")) strcpy(sc.String, "}");
					FGameTexture **p = &lumplist[*(unsigned char*)sc.String];
					sc.MustGetString();
					FTextureID texid = TexMan.CheckForTexture(sc.String, ETextureType::MiscPatch);
					if (texid.Exists())
					{
						*p = TexMan.GetGameTexture(texid);
					}
					else if (fileSystem.GetFileContainer(sc.LumpNum) >= fileSystem.GetIwadNum())
					{
						// Print a message only if this isn't in zdoom.pk3
						sc.ScriptMessage("%s: Unable to find texture in font definition for %s", sc.String, namebuffer.GetChars());
					}
					format = 2;
				}
			}
			if (format == 1)
			{
				FFont *fnt = new FFont (namebuffer, templatebuf, nullptr, first, count, start, llump, spacewidth, donttranslate);
				fnt->SetCursor(cursor);
				fnt->SetKerning(kerning);
				if (ignoreoffsets) fnt->ClearOffsets();
			}
			else if (format == 2)
			{
				for (i = 0; i < 256; i++)
				{
					if (lumplist[i] != nullptr)
					{
						first = i;
						break;
					}
				}
				for (i = 255; i >= 0; i--)
				{
					if (lumplist[i] != nullptr)
					{
						count = i - first + 1;
						break;
					}
				}
				if (count > 0)
				{
					FFont *CreateSpecialFont (const char *name, int first, int count, FGameTexture **lumplist, const bool *notranslate, int lump, bool donttranslate);
					FFont *fnt = CreateSpecialFont (namebuffer, first, count, &lumplist[first], notranslate, llump, donttranslate);
					fnt->SetCursor(cursor);
					fnt->SetKerning(kerning);
					if (spacewidth >= 0) fnt->SpaceWidth = spacewidth;
					fnt->MinLum = MinLum;
					fnt->MaxLum = MaxLum;
					if (ignoreoffsets) fnt->ClearOffsets();
				}
			}
			else goto wrong;
		}
		sc.Close();
	}
	return;

wrong:
	sc.ScriptError ("Invalid combination of properties in font '%s', %s not allowed", namebuffer.GetChars(), sc.String);
}

//==========================================================================
//
// V_InitFontColors
//
// Reads the list of color translation definitions into memory.
//
//==========================================================================

void V_InitFontColors ()
{
	TArray<FName> names;
	int lump, lastlump = 0;
	TranslationParm tparm = { 0, 0, {0}, {0} };	// Silence GCC (for real with -Wextra )
	TArray<TranslationParm> parms;
	TArray<TempParmInfo> parminfo;
	TArray<TempColorInfo> colorinfo;
	int c, parmchoice;
	TempParmInfo info;
	TempColorInfo cinfo;
	PalEntry logcolor;
	unsigned int i, j;
	int k, index;

	info.Index = -1;

	TranslationParms[0].Clear();
	TranslationParms[1].Clear();
	TranslationLookup.Clear();
	TranslationColors.Clear();

	while ((lump = fileSystem.FindLump ("TEXTCOLO", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			names.Clear();

			logcolor = DEFAULT_LOG_COLOR;

			// Everything until the '{' is considered a valid name for the
			// color range.
			names.Push (sc.String);
			while (sc.MustGetString(), !sc.Compare ("{"))
			{
				if (names[0] == NAME_Untranslated)
				{
					sc.ScriptError ("The \"untranslated\" color may not have any other names");
				}
				names.Push (sc.String);
			}

			parmchoice = 0;
			info.StartParm[0] = parms.Size();
			info.StartParm[1] = 0;
			info.ParmLen[1] = info.ParmLen[0] = 0;
			tparm.RangeEnd = tparm.RangeStart = -1;

			while (sc.MustGetString(), !sc.Compare ("}"))
			{
				if (sc.Compare ("Console:"))
				{
					if (parmchoice == 1)
					{
						sc.ScriptError ("Each color may only have one set of console ranges");
					}
					parmchoice = 1;
					info.StartParm[1] = parms.Size();
					info.ParmLen[0] = info.StartParm[1] - info.StartParm[0];
					tparm.RangeEnd = tparm.RangeStart = -1;
				}
				else if (sc.Compare ("Flat:"))
				{
					sc.MustGetString();
					logcolor = V_GetColor (sc);
				}
				else
				{
					// Get first color
					c = V_GetColor (sc);
					tparm.Start[0] = RPART(c);
					tparm.Start[1] = GPART(c);
					tparm.Start[2] = BPART(c);

					// Get second color
					sc.MustGetString();
					c = V_GetColor (sc);
					tparm.End[0] = RPART(c);
					tparm.End[1] = GPART(c);
					tparm.End[2] = BPART(c);

					// Check for range specifier
					if (sc.CheckNumber())
					{
						if (tparm.RangeStart == -1 && sc.Number != 0)
						{
							sc.ScriptError ("The first color range must start at position 0");
						}
						if (sc.Number < 0 || sc.Number > 256)
						{
							sc.ScriptError ("The color range must be within positions [0,256]");
						}
						if (sc.Number <= tparm.RangeEnd)
						{
							sc.ScriptError ("The color range must not start before the previous one ends");
						}
						tparm.RangeStart = sc.Number;

						sc.MustGetNumber();
						if (sc.Number < 0 || sc.Number > 256)
						{
							sc.ScriptError ("The color range must be within positions [0,256]");
						}
						if (sc.Number <= tparm.RangeStart)
						{
							sc.ScriptError ("The color range end position must be larger than the start position");
						}
						tparm.RangeEnd = sc.Number;
					}
					else
					{
						tparm.RangeStart = tparm.RangeEnd + 1;
						tparm.RangeEnd = 256;
						if (tparm.RangeStart >= tparm.RangeEnd)
						{
							sc.ScriptError ("The color has too many ranges");
						}
					}
					parms.Push (tparm);
				}
			}
			info.ParmLen[parmchoice] = parms.Size() - info.StartParm[parmchoice];
			if (info.ParmLen[0] == 0)
			{
				if (names[0] != NAME_Untranslated)
				{
					sc.ScriptError ("There must be at least one normal range for a color");
				}
			}
			else
			{
				if (names[0] == NAME_Untranslated)
				{
					sc.ScriptError ("The \"untranslated\" color must be left undefined");
				}
			}
			if (info.ParmLen[1] == 0 && names[0] != NAME_Untranslated)
			{ // If a console translation is unspecified, make it white, since the console
			  // font has no color information stored with it.
				tparm.RangeStart = 0;
				tparm.RangeEnd = 256;
				tparm.Start[2] = tparm.Start[1] = tparm.Start[0] = 0;
				tparm.End[2] = tparm.End[1] = tparm.End[0] = 255;
				info.StartParm[1] = parms.Push (tparm);
				info.ParmLen[1] = 1;
			}
			cinfo.ParmInfo = parminfo.Push (info);
			// Record this color information for each name it goes by
			for (i = 0; i < names.Size(); ++i)
			{
				// Redefine duplicates in-place
				for (j = 0; j < colorinfo.Size(); ++j)
				{
					if (colorinfo[j].Name == names[i])
					{
						colorinfo[j].ParmInfo = cinfo.ParmInfo;
						colorinfo[j].LogColor = logcolor;
						break;
					}
				}
				if (j == colorinfo.Size())
				{
					cinfo.Name = names[i];
					cinfo.LogColor = logcolor;
					colorinfo.Push (cinfo);
				}
			}
		}
	}
	// Make permananent copies of all the color information we found.
	for (i = 0, index = 0; i < colorinfo.Size(); ++i)
	{
		TranslationMap tmap;
		TempParmInfo *pinfo;

		tmap.Name = colorinfo[i].Name;
		pinfo = &parminfo[colorinfo[i].ParmInfo];
		if (pinfo->Index < 0)
		{
			// Write out the set of remappings for this color.
			for (k = 0; k < 2; ++k)
			{
				for (j = 0; j < pinfo->ParmLen[k]; ++j)
				{
					TranslationParms[k].Push (parms[pinfo->StartParm[k] + j]);
				}
			}
			TranslationColors.Push (colorinfo[i].LogColor);
			pinfo->Index = index++;
		}
		tmap.Number = pinfo->Index;
		TranslationLookup.Push (tmap);
	}
	// Leave a terminating marker at the ends of the lists.
	tparm.RangeStart = -1;
	TranslationParms[0].Push (tparm);
	TranslationParms[1].Push (tparm);
	// Sort the translation lookups for fast binary searching.
	qsort (&TranslationLookup[0], TranslationLookup.Size(), sizeof(TranslationLookup[0]), TranslationMapCompare);

	NumTextColors = index;
	assert (NumTextColors >= NUM_TEXT_COLORS);
}

//==========================================================================
//
// TranslationMapCompare
//
//==========================================================================

static int TranslationMapCompare (const void *a, const void *b)
{
	return int(((const TranslationMap *)a)->Name.GetIndex()) - int(((const TranslationMap *)b)->Name.GetIndex());
}

//==========================================================================
//
// V_FindFontColor
//
// Returns the color number for a particular named color range.
//
//==========================================================================

EColorRange V_FindFontColor (FName name)
{
	int min = 0, max = TranslationLookup.Size() - 1;

	while (min <= max)
	{
		unsigned int mid = (min + max) / 2;
		const TranslationMap *probe = &TranslationLookup[mid];
		if (probe->Name == name)
		{
			return EColorRange(probe->Number);
		}
		else if (probe->Name < name)
		{
			min = mid + 1;
		}
		else
		{
			max = mid - 1;
		}
	}
	return CR_UNTRANSLATED;
}

//==========================================================================
//
// CreateLuminosityTranslationRanges
//
// Create universal remap ranges for hardware rendering.
//
//==========================================================================
static PalEntry* paletteptr;

static void CreateLuminosityTranslationRanges()
{
	paletteptr = (PalEntry*)ImageArena.Alloc(256 * ((NumTextColors * 2)) * sizeof(PalEntry));
	for (int l = 0; l < 2; l++)
	{
		auto parmstart = &TranslationParms[l][0];
		// Put the data into the image arena where it gets deleted with the rest of the texture data.
		for (int p = 0; p < NumTextColors; p++)
		{
			// Intended storage order is Range 1, variant 1 - Range 1, variant 2, Range 2, variant 1, and so on.
			// The storage of the ranges forces us to go through this differently...
			PalEntry* palette = paletteptr + p * 512 + l * 256;
			for (int v = 0; v < 256; v++)
			{
				palette[v].b = palette[v].g = palette[v].r = (uint8_t)v;
			}
			if (p != CR_UNTRANSLATED) // This table skips the untranslated entry. Do I need to say that the stored data format is garbage? >)
			{
				for (int v = 0; v < 256; v++)
				{
					// Find the color range that this luminosity value lies within.
					const TranslationParm* parms = parmstart - 1;
					do
					{
						parms++;
						if (parms->RangeStart <= v && parms->RangeEnd >= v)
							break;
					} while (parms[1].RangeStart > parms[0].RangeEnd);

					// Linearly interpolate to find out which color this luminosity level gets.
					int rangev = ((v - parms->RangeStart) << 8) / (parms->RangeEnd - parms->RangeStart);
					int r = ((parms->Start[0] << 8) + rangev * (parms->End[0] - parms->Start[0])) >> 8; // red
					int g = ((parms->Start[1] << 8) + rangev * (parms->End[1] - parms->Start[1])) >> 8; // green
					int b = ((parms->Start[2] << 8) + rangev * (parms->End[2] - parms->Start[2])) >> 8; // blue
					palette[v].r = (uint8_t)clamp(r, 0, 255);
					palette[v].g = (uint8_t)clamp(g, 0, 255);
					palette[v].b = (uint8_t)clamp(b, 0, 255);
				}
				// Advance to the next color range.
				while (parmstart[1].RangeStart > parmstart[0].RangeEnd)
				{
					parmstart++;
				}
				parmstart++;
			}
		}
	}
}

//==========================================================================
//
// V_ApplyLuminosityTranslation
//
// Applies the translation to a bitmap for texture generation.
//
//==========================================================================

void V_ApplyLuminosityTranslation(int translation, uint8_t* pixel, int size)
{
	int colorrange = (translation >> 16) & 0x3fff;
	if (colorrange >= NumTextColors * 2) return;
	int lum_min = (translation >> 8) & 0xff;
	int lum_max = translation & 0xff;
	int lum_range = (lum_max - lum_min + 1);
	PalEntry* remap = paletteptr + colorrange * 256;

	for (int i = 0; i < size; i++, pixel += 4)
	{
		// we must also process the transparent pixels here to ensure proper filtering on the characters' edges.
		int gray = PalEntry(255, pixel[2], pixel[1], pixel[0]).Luminance();
		int lumadjust = (gray - lum_min) * 255 / lum_range;
		int index = clamp(lumadjust, 0, 255);
		PalEntry newcol = remap[index];
		// extend the range if we find colors outside what initial analysis provided.
		if (gray < lum_min && lum_min != 0)
		{
			newcol.r = newcol.r * gray / lum_min;
			newcol.g = newcol.g * gray / lum_min;
			newcol.b = newcol.b * gray / lum_min;
		}
		else if (gray > lum_max && lum_max != 0)
		{
			newcol.r = clamp(newcol.r * gray / lum_max, 0, 255);
			newcol.g = clamp(newcol.g * gray / lum_max, 0, 255);
			newcol.b = clamp(newcol.b * gray / lum_max, 0, 255);
		}
		pixel[0] = newcol.b;
		pixel[1] = newcol.g;
		pixel[2] = newcol.r;
	}
}

//==========================================================================
//
// SetDefaultTranslation
//
// Builds a translation to map the stock font to a mod provided replacement.
// This probably won't work that well if the original font is extremely colorful.
//
//==========================================================================

static void CalcDefaultTranslation(FFont* base, int index)
{
	uint32_t othercolors[256] = {};
	base->RecordAllTextureColors(othercolors);

	TArray<double> otherluminosity;
	base->GetLuminosity(othercolors, otherluminosity);

	PalEntry *remap = &paletteptr[index * 256];
	memset(remap, 0, 1024);

	for (unsigned i = 0; i < 256; i++)
	{
		auto lum = otherluminosity[i];
		if (lum >= 0 && lum <= 1)
		{
			int index = int(lum * 255);
			remap[index] = GPalette.BaseColors[i];
			remap[index].a = 255;
		}
	}

	// todo: fill the gaps.
	//remap[0] = 0;
	int lowindex = 0;
	while (lowindex < 255 && remap[lowindex].a == 0) lowindex++;
	lowindex++;
	int highindex = lowindex + 1;

	while (lowindex < 255)
	{
		while (highindex <= 255 && remap[highindex].a == 0) highindex++;
		if (lowindex == 0)
		{
			for (int i = 0; i < highindex; i++) remap[i] = remap[highindex];
			lowindex = highindex++;
		}
		else if (highindex > 256)
		{
			for (int i = lowindex + 1; i < highindex; i++) remap[i] = remap[lowindex];
			break;
		}
		else
		{
			for (int i = lowindex + 1; i < highindex; i++)
			{
				PalEntry color1 = remap[lowindex];
				PalEntry color2 = remap[highindex];
				double weight = (i - lowindex) / double(highindex - lowindex);
				int r = int(color1.r + weight * (color2.r - color1.r));
				int g = int(color1.g + weight * (color2.g - color1.g));
				int b = int(color1.b + weight * (color2.b - color1.b));
				r = clamp(r, 0, 255);
				g = clamp(g, 0, 255);
				b = clamp(b, 0, 255);
				remap[i] = PalEntry(255, r, g, b);
			}
			lowindex = highindex++;
		}
	}
	
}

//==========================================================================
//
// V_LogColorFromColorRange
//
// Returns the color to use for text in the startup/error log window.
//
//==========================================================================

PalEntry V_LogColorFromColorRange (EColorRange range)
{
	if ((unsigned int)range >= TranslationColors.Size())
	{ // Return default color
		return DEFAULT_LOG_COLOR;
	}
	return TranslationColors[range];
}

//==========================================================================
//
// V_ParseFontColor
//
// Given a pointer to a color identifier (presumably just after a color
// escape character), return the color it identifies and advances
// color_value to just past it.
//
//==========================================================================

EColorRange V_ParseFontColor (const uint8_t *&color_value, int normalcolor, int boldcolor)
{
	const uint8_t *ch = color_value;
	int newcolor = *ch++;

	if (newcolor == '-')			// Normal
	{
		newcolor = normalcolor;
	}
	else if (newcolor == '+')		// Bold
	{
		newcolor = boldcolor;
	}
	else if (newcolor == '!')		// Team chat
	{
		newcolor = PrintColors[PRINT_TEAMCHAT];
	}
	else if (newcolor == '*')		// Chat
	{
		newcolor = PrintColors[PRINT_CHAT];
	}
	else if (newcolor == '[')		// Named
	{
		const uint8_t *namestart = ch;
		while (*ch != ']' && *ch != '\0')
		{
			ch++;
		}
		FName rangename((const char *)namestart, int(ch - namestart), true);
		if (*ch != '\0')
		{
			ch++;
		}
		newcolor = V_FindFontColor (rangename);
	}
	else if (newcolor >= 'A' && newcolor < NUM_TEXT_COLORS + 'A')	// Standard, uppercase
	{
		newcolor -= 'A';
	}
	else if (newcolor >= 'a' && newcolor < NUM_TEXT_COLORS + 'a')	// Standard, lowercase
	{
		newcolor -= 'a';
	}
	else							// Incomplete!
	{
		color_value = ch - (newcolor == '\0');
		return CR_UNDEFINED;
	}
	color_value = ch;
	return EColorRange(newcolor);
}

//==========================================================================
//
// V_InitFonts
//
// Fixme: This really needs to be a bit more flexible 
// and less rigidly tied to the original game data.
//
//==========================================================================

void V_InitFonts()
{
	CreateLuminosityTranslationRanges();
	V_InitCustomFonts();

	FFont *CreateHexLumpFont(const char *fontname, int lump);
	FFont *CreateHexLumpFont2(const char *fontname, int lump);

	auto lump = fileSystem.CheckNumForFullName("newconsolefont.hex", 0);	// This is always loaded from gzdoom.pk3 to prevent overriding it with incomplete replacements.
	if (lump == -1) I_FatalError("newconsolefont.hex not found");	// This font is needed - do not start up without it.
	NewConsoleFont = CreateHexLumpFont("NewConsoleFont", lump);
	NewSmallFont = CreateHexLumpFont2("NewSmallFont", lump);
	CurrentConsoleFont = NewConsoleFont;
	ConFont = V_GetFont("ConsoleFont", "CONFONT");
	V_GetFont("IndexFont", "INDEXFON");	// detect potential replacements for this one.
}

void V_LoadTranslations()
{
	for (auto font = FFont::FirstFont; font; font = font->Next)
	{
		if (!font->noTranslate) font->LoadTranslations();
	}

	if (BigFont)
	{
		CalcDefaultTranslation(BigFont, CR_UNTRANSLATED * 2 + 1);
		if (OriginalBigFont != nullptr && OriginalBigFont != BigFont)
		{
			int sometrans = OriginalBigFont->Translations[0];
			sometrans &= ~(0x3fff << 16);
			sometrans |= (CR_UNTRANSLATED * 2 + 1) << 16;
			OriginalBigFont->Translations[CR_UNTRANSLATED] = sometrans;
			OriginalBigFont->forceremap = true;
		}
	}
	if (SmallFont)
	{
		CalcDefaultTranslation(SmallFont, CR_UNTRANSLATED * 2);
		if (OriginalSmallFont != nullptr && OriginalSmallFont != SmallFont)
		{
			int sometrans = OriginalSmallFont->Translations[0];
			sometrans &= ~(0x3fff << 16);
			sometrans |= (CR_UNTRANSLATED * 2) << 16;
			OriginalSmallFont->Translations[CR_UNTRANSLATED] = sometrans;
			OriginalSmallFont->forceremap = true;
		}
		if (NewSmallFont != nullptr)
		{
			int sometrans = NewSmallFont->Translations[0];
			sometrans &= ~(0x3fff << 16);
			sometrans |= (CR_UNTRANSLATED * 2) << 16;
			NewSmallFont->Translations[CR_UNTRANSLATED] = sometrans;
			NewSmallFont->forceremap = true;
		}
	}
	translationsLoaded = true;
}


void V_ClearFonts()
{
	while (FFont::FirstFont != nullptr)
	{
		delete FFont::FirstFont;
	}
	FFont::FirstFont = nullptr;
	AlternativeSmallFont = OriginalSmallFont = CurrentConsoleFont = NewSmallFont = NewConsoleFont = SmallFont = SmallFont2 = BigFont = ConFont = IntermissionFont = nullptr;
}

//==========================================================================
//
// CleanseString
//
// Does some mild sanity checking on a string: If it ends with an incomplete
// color escape, the escape is removed.
//
//==========================================================================

char* CleanseString(char* str)
{
	char* escape = strrchr(str, TEXTCOLOR_ESCAPE);
	if (escape != NULL)
	{
		if (escape[1] == '\0')
		{
			*escape = '\0';
		}
		else if (escape[1] == '[')
		{
			char* close = strchr(escape + 2, ']');
			if (close == NULL)
			{
				*escape = '\0';
			}
		}
	}
	return str;
}

