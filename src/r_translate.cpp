/*
** r_translate.cpp
** Translatioo table handling
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

#include <stddef.h>

#include "templates.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_translate.h"
#include "v_video.h"
#include "g_game.h"
#include "colormatcher.h"
#include "d_netinf.h"
#include "v_palette.h"

#include "gi.h"
#include "stats.h"

TAutoGrowArray<FRemapTablePtr, FRemapTable *> translationtables[NUM_TRANSLATION_TABLES];

const BYTE IcePalette[16][3] =
{
	{  10,  8, 18 },
	{  15, 15, 26 },
	{  20, 16, 36 },
	{  30, 26, 46 },
	{  40, 36, 57 },
	{  50, 46, 67 },
	{  59, 57, 78 },
	{  69, 67, 88 },
	{  79, 77, 99 },
	{  89, 87,109 },
	{  99, 97,120 },
	{ 109,107,130 },
	{ 118,118,141 },
	{ 128,128,151 },
	{ 138,138,162 },
	{ 148,148,172 }
};

/****************************************************/
/****************************************************/

FRemapTable::FRemapTable(int count)
{
	assert(count <= 256);

	Alloc(count);

	// Note that the tables are left uninitialized. It is assumed that
	// the caller will do that next, if only by calling MakeIdentity().
}

FRemapTable::~FRemapTable()
{
	Free();
}

void FRemapTable::Alloc(int count)
{
	Remap = (BYTE *)M_Malloc(count*sizeof(*Remap) + count*sizeof(*Palette));
	assert (Remap != NULL);
	Palette = (PalEntry *)(Remap + count*(sizeof(*Remap)));
	Native = NULL;
	NumEntries = count;
}

void FRemapTable::Free()
{
	KillNative();
	if (Remap != NULL)
	{
		M_Free(Remap);
		Remap = NULL;
		Palette = NULL;
		NumEntries = 0;
	}
}

FRemapTable::FRemapTable(const FRemapTable &o)
{
	Remap = NULL;
	Native = NULL;
	NumEntries = 0;
	operator= (o);
}

FRemapTable &FRemapTable::operator=(const FRemapTable &o)
{
	if (&o == this)
	{
		return *this;
	}
	if (o.NumEntries != NumEntries)
	{
		Free();
	}
	if (Remap == NULL)
	{
		Alloc(o.NumEntries);
	}
	memcpy(Remap, o.Remap, NumEntries*sizeof(*Remap) + NumEntries*sizeof(*Palette));
	return *this;
}

bool FRemapTable::operator==(const FRemapTable &o)
{
	// Two translations are identical when they have the same amount of colors
	// and the palette values for both are identical.
	if (&o == this) return true;
	if (o.NumEntries != NumEntries) return false;
	return !memcmp(o.Palette, Palette, NumEntries * sizeof(*Palette));
}

void FRemapTable::Serialize(FArchive &arc)
{
	int n = NumEntries;

	arc << NumEntries;
	if (arc.IsStoring())
	{
		arc.Write (Remap, NumEntries);
	}
	else
	{
		if (n != NumEntries)
		{
			Free();
			Alloc(NumEntries);
		}
		arc.Read (Remap, NumEntries);
	}
	for (int j = 0; j < NumEntries; ++j)
	{
		arc << Palette[j];
	}
}


void FRemapTable::MakeIdentity()
{
	int i;

	for (i = 0; i < NumEntries; ++i)
	{
		Remap[i] = i;
	}
	for (i = 0; i < NumEntries; ++i)
	{
		Palette[i] = GPalette.BaseColors[i];
	}
	for (i = 1; i < NumEntries; ++i)
	{
		Palette[i].a = 255;
	}
}

bool FRemapTable::IsIdentity() const
{
	for (int j = 0; j < 256; ++j)
	{
		if (Remap[j] != j)
		{
			return false;
		}
	}
	return true;
}

void FRemapTable::KillNative()
{
	if (Native != NULL)
	{
		delete Native;
		Native = NULL;
	}
}

void FRemapTable::UpdateNative()
{
	if (Native != NULL)
	{
		Native->Update();
	}
}

FNativePalette *FRemapTable::GetNative()
{
	if (Native == NULL)
	{
		Native = screen->CreatePalette(this);
	}
	return Native;
}

void FRemapTable::AddIndexRange(int start, int end, int pal1, int pal2)
{
	fixed_t palcol, palstep;

	if (start > end)
	{
		swap (start, end);
		swap (pal1, pal2);
	}
	else if (start == end)
	{
		Remap[start] = pal1;
		Palette[start] = GPalette.BaseColors[pal1];
		Palette[start].a = start==0? 0:255;
		return;
	}
	palcol = pal1 << FRACBITS;
	palstep = ((pal2 << FRACBITS) - palcol) / (end - start);
	for (int i = start; i <= end; palcol += palstep, ++i)
	{
		Remap[i] = palcol >> FRACBITS;
		Palette[i] = GPalette.BaseColors[palcol >> FRACBITS];
		Palette[i].a = i==0? 0:255;
	}
}

void FRemapTable::AddColorRange(int start, int end, int _r1,int _g1, int _b1, int _r2, int _g2, int _b2)
{
	fixed_t r1 = _r1 << FRACBITS;
	fixed_t g1 = _g1 << FRACBITS;
	fixed_t b1 = _b1 << FRACBITS;
	fixed_t r2 = _r2 << FRACBITS;
	fixed_t g2 = _g2 << FRACBITS;
	fixed_t b2 = _b2 << FRACBITS;
	fixed_t r, g, b;
	fixed_t rs, gs, bs;

	if (start > end)
	{
		swap (start, end);
		r = r2;
		g = g2;
		b = b2;
		rs = r1 - r2;
		gs = g1 - g2;
		bs = b1 - b2;
	}
	else
	{
		r = r1;
		g = g1;
		b = b1;
		rs = r2 - r1;
		gs = g2 - g1;
		bs = b2 - b1;
	}
	if (start == end)
	{
		Remap[start] = ColorMatcher.Pick
			(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
		Palette[start] = PalEntry(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
		Palette[start].a = start==0? 0:255;
	}
	else
	{
		rs /= (end - start);
		gs /= (end - start);
		bs /= (end - start);
		for (int i = start; i <= end; ++i)
		{
			Remap[i] = ColorMatcher.Pick
				(r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
			Palette[i] = PalEntry(start==0? 0:255, r >> FRACBITS, g >> FRACBITS, b >> FRACBITS);
			r += rs;
			g += gs;
			b += bs;
		}
	}
}


FRemapTable *TranslationToTable(int translation)
{
	unsigned int type = GetTranslationType(translation);
	unsigned int index = GetTranslationIndex(translation);
	TAutoGrowArray<FRemapTablePtr, FRemapTable *> *slots;

	if (type <= 0 || type >= NUM_TRANSLATION_TABLES)
	{
		return NULL;
	}
	slots = &translationtables[type];
	if (index >= slots->Size())
	{
		return NULL;
	}
	return slots->operator[](index);
}

static void PushIdentityTable(int slot)
{
	FRemapTable *table = new FRemapTable;
	table->MakeIdentity();
	translationtables[slot].Push(table);
}

//
// R_InitTranslationTables
// Creates the translation tables to map the green color ramp to gray,
// brown, red. Assumes a given structure of the PLAYPAL.
//
void R_InitTranslationTables ()
{
	int i, j;

	// Each player gets two translations. Doom and Strife don't use the
	// extra ones, but Heretic and Hexen do. These are set up during
	// netgame arbitration and as-needed, so they just get to be identity
	// maps until then so they won't be invalid.
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		PushIdentityTable(TRANSLATION_Players);
		PushIdentityTable(TRANSLATION_PlayersExtra);
	}
	// The menu player also gets a separate translation table
	PushIdentityTable(TRANSLATION_Players);

	// The three standard translations from Doom or Heretic (seven for Strife),
	// plus the generic ice translation.
	for (i = 0; i < 8; ++i)
	{
		PushIdentityTable(TRANSLATION_Standard);
	}

	// Each player corpse has its own translation so they won't change
	// color if the player who created them changes theirs.
	for (i = 0; i < BODYQUESIZE; ++i)
	{
		PushIdentityTable(TRANSLATION_PlayerCorpses);
	}

	// Create the standard translation tables
	if (gameinfo.gametype & GAME_DoomChex)
	{
		for (i = 0x70; i < 0x80; i++)
		{ // map green ramp to gray, brown, red
			translationtables[TRANSLATION_Standard][0]->Remap[i] = 0x60 + (i&0xf);
			translationtables[TRANSLATION_Standard][1]->Remap[i] = 0x40 + (i&0xf);
			translationtables[TRANSLATION_Standard][2]->Remap[i] = 0x20 + (i&0xf);

			translationtables[TRANSLATION_Standard][0]->Palette[i] = GPalette.BaseColors[0x60 + (i&0xf)] | MAKEARGB(255,0,0,0);
			translationtables[TRANSLATION_Standard][1]->Palette[i] = GPalette.BaseColors[0x40 + (i&0xf)] | MAKEARGB(255,0,0,0);
			translationtables[TRANSLATION_Standard][2]->Palette[i] = GPalette.BaseColors[0x20 + (i&0xf)] | MAKEARGB(255,0,0,0);
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		for (i = 225; i <= 240; i++)
		{
			translationtables[TRANSLATION_Standard][0]->Remap[i] = 114+(i-225); // yellow
			translationtables[TRANSLATION_Standard][1]->Remap[i] = 145+(i-225); // red
			translationtables[TRANSLATION_Standard][2]->Remap[i] = 190+(i-225); // blue
			
			translationtables[TRANSLATION_Standard][0]->Palette[i] = GPalette.BaseColors[114+(i-225)] | MAKEARGB(255,0,0,0);
			translationtables[TRANSLATION_Standard][1]->Palette[i] = GPalette.BaseColors[145+(i-225)] | MAKEARGB(255,0,0,0);
			translationtables[TRANSLATION_Standard][2]->Palette[i] = GPalette.BaseColors[190+(i-225)] | MAKEARGB(255,0,0,0);
		}
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		for (i = 0x20; i <= 0x3F; ++i)
		{
			translationtables[TRANSLATION_Standard][0]->Remap[i] = i - 0x20;
			translationtables[TRANSLATION_Standard][1]->Remap[i] = i - 0x20;
			translationtables[TRANSLATION_Standard][2]->Remap[i] = 0xD0 + (i&0xf);
			translationtables[TRANSLATION_Standard][3]->Remap[i] = 0xD0 + (i&0xf);
			translationtables[TRANSLATION_Standard][4]->Remap[i] = i - 0x20;
			translationtables[TRANSLATION_Standard][5]->Remap[i] = i - 0x20;
			translationtables[TRANSLATION_Standard][6]->Remap[i] = i - 0x20;
		}
		for (i = 0x50; i <= 0x5F; ++i)
		{
			// Merchant hair
			translationtables[TRANSLATION_Standard][4]->Remap[i] = 0x80 + (i&0xf);
			translationtables[TRANSLATION_Standard][5]->Remap[i] = 0x10 + (i&0xf);
			translationtables[TRANSLATION_Standard][6]->Remap[i] = 0x40 + (i&0xf);
		}
		for (i = 0x80; i <= 0x8F; ++i)
		{
			translationtables[TRANSLATION_Standard][0]->Remap[i] = 0x40 + (i&0xf); // red
			translationtables[TRANSLATION_Standard][1]->Remap[i] = 0xB0 + (i&0xf); // rust
			translationtables[TRANSLATION_Standard][2]->Remap[i] = 0x10 + (i&0xf); // gray
			translationtables[TRANSLATION_Standard][3]->Remap[i] = 0x30 + (i&0xf); // dark green
			translationtables[TRANSLATION_Standard][4]->Remap[i] = 0x50 + (i&0xf); // gold
			translationtables[TRANSLATION_Standard][5]->Remap[i] = 0x60 + (i&0xf); // bright green
			translationtables[TRANSLATION_Standard][6]->Remap[i] = 0x90 + (i&0xf); // blue
		}
		for (i = 0xC0; i <= 0xCF; ++i)
		{
			translationtables[TRANSLATION_Standard][4]->Remap[i] = 0xA0 + (i&0xf);
			translationtables[TRANSLATION_Standard][5]->Remap[i] = 0x20 + (i&0xf);
			translationtables[TRANSLATION_Standard][6]->Remap[i] = (i&0xf);
		}
		translationtables[TRANSLATION_Standard][6]->Remap[0xC0] = 1;
		for (i = 0xD0; i <= 0xDF; ++i)
		{
			translationtables[TRANSLATION_Standard][4]->Remap[i] = 0xB0 + (i&0xf);
			translationtables[TRANSLATION_Standard][5]->Remap[i] = 0x30 + (i&0xf);
			translationtables[TRANSLATION_Standard][6]->Remap[i] = 0x10 + (i&0xf);
		}
		for (i = 0xF1; i <= 0xF6; ++i)
		{
			translationtables[TRANSLATION_Standard][0]->Remap[i] = 0xDF + (i&0xf);
		}
		for (i = 0xF7; i <= 0xFB; ++i)
		{
			translationtables[TRANSLATION_Standard][0]->Remap[i] = i - 6;
		}
		for (i = 0; i < 7; ++i)
		{
			for (int j = 0x20; j <= 0xFB; ++j)
			{
				translationtables[TRANSLATION_Standard][i]->Palette[j] =
					GPalette.BaseColors[translationtables[TRANSLATION_Standard][i]->Remap[j]] | MAKEARGB(255,0,0,0);
			}
		}
	}

	// Create the ice translation table, based on Hexen's. Alas, the standard
	// Doom palette has no good substitutes for these bluish-tinted grays, so
	// they will just look gray unless you use a different PLAYPAL with Doom.

	BYTE IcePaletteRemap[16];
	for (i = 0; i < 16; ++i)
	{
		IcePaletteRemap[i] = ColorMatcher.Pick (IcePalette[i][0], IcePalette[i][1], IcePalette[i][2]);
	}
	FRemapTable *remap = translationtables[TRANSLATION_Standard][7];
	for (i = 0; i < 256; ++i)
	{
		int r = GPalette.BaseColors[i].r;
		int g = GPalette.BaseColors[i].g;
		int b = GPalette.BaseColors[i].b;
		int v = (r*77 + g*143 + b*37) >> 12;
		remap->Remap[i] = IcePaletteRemap[v];
		remap->Palette[i] = PalEntry(255, IcePalette[v][0], IcePalette[v][1], IcePalette[v][2]);
	}

	// set up shading tables for shaded columns
	// 16 colormap sets, progressing from full alpha to minimum visible alpha

	BYTE *table = shadetables;

	// Full alpha
	for (i = 0; i < 16; ++i)
	{
		ShadeFakeColormap[i].Color = -1;
		ShadeFakeColormap[i].Desaturate = -1;
		ShadeFakeColormap[i].Next = NULL;
		ShadeFakeColormap[i].Maps = table;

		for (j = 0; j < NUMCOLORMAPS; ++j)
		{
			int a = (NUMCOLORMAPS - j) * 256 / NUMCOLORMAPS * (16-i);
			for (int k = 0; k < 256; ++k)
			{
				BYTE v = (((k+2) * a) + 256) >> 14;
				table[k] = MIN<BYTE> (v, 64);
			}
			table += 256;
		}
	}
	for (i = 0; i < NUMCOLORMAPS*16*256; ++i)
	{
		assert(shadetables[i] <= 64);
	}

	// Set up a guaranteed identity map
	for (i = 0; i < 256; ++i)
	{
		identitymap[i] = i;
	}
}

void R_DeinitTranslationTables()
{
	for (int i = 0; i < NUM_TRANSLATION_TABLES; ++i)
	{
		for (unsigned int j = 0; j < translationtables[i].Size(); ++j)
		{
			if (translationtables[i][j] != NULL)
			{
				delete translationtables[i][j];
				translationtables[i][j] = NULL;
			}
		}
	}
}

// [RH] Create a player's translation table based on a given mid-range color.
// [GRB] Split to 2 functions (because of player setup menu)
static void SetRemap(FRemapTable *table, int i, float r, float g, float b)
{
	int ir = clamp (int(r * 255.f), 0, 255);
	int ig = clamp (int(g * 255.f), 0, 255);
	int ib = clamp (int(b * 255.f), 0, 255);
	table->Remap[i] = ColorMatcher.Pick (ir, ig, ib);
	table->Palette[i] = PalEntry(255, ir, ig, ib);
}

static void R_CreatePlayerTranslation (float h, float s, float v, FPlayerSkin *skin, FRemapTable *table, FRemapTable *alttable)
{
	int i;
	BYTE start = skin->range0start;
	BYTE end = skin->range0end;
	float r, g, b;
	float bases, basev;
	float sdelta, vdelta;
	float range;

	// Set up the base translation for this skin. If the skin was created
	// for the current game, then this is just an identity translation.
	// Otherwise, it remaps the colors from the skin's original palette to
	// the current one.
	if (skin->othergame)
	{
		memcpy (table->Remap, OtherGameSkinRemap, 256);
		memcpy (table->Palette, OtherGameSkinPalette, sizeof(table->Palette));
	}
	else
	{
		for (i = 0; i < 256; ++i)
		{
			table->Remap[i] = i;
		}
		memcpy(table->Palette, GPalette.BaseColors, sizeof(table->Palette));
	}
	for (i = 1; i < 256; ++i)
	{
		table->Palette[i].a = 255;
	}

	// [GRB] Don't translate skins with color range 0-0 (APlayerPawn default)
	if (start == 0 && end == 0)
	{
		table->UpdateNative();
		return;
	}

	range = (float)(end-start+1);

	bases = s;
	basev = v;

	if (gameinfo.gametype & GAME_DoomStrifeChex)
	{
		// Build player sprite translation
		s -= 0.23f;
		v += 0.1f;
		sdelta = 0.23f / range;
		vdelta = -0.94112f / range;

		for (i = start; i <= end; i++)
		{
			float uses, usev;
			uses = clamp (s, 0.f, 1.f);
			usev = clamp (v, 0.f, 1.f);
			HSVtoRGB (&r, &g, &b, h, uses, usev);
			SetRemap(table, i, r, g, b);
			s += sdelta;
			v += vdelta;
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		float vdelta = 0.418916f / range;

		// Build player sprite translation
		for (i = start; i <= end; i++)
		{
			v = vdelta * (float)(i - start) + basev - 0.2352937f;
			v = clamp (v, 0.f, 1.f);
			HSVtoRGB (&r, &g, &b, h, s, v);
			SetRemap(table, i, r, g, b);
		}

		// Build rain/lifegem translation
		if (alttable)
		{
			bases = MIN (bases*1.3f, 1.f);
			basev = MIN (basev*1.3f, 1.f);
			for (i = 145; i <= 168; i++)
			{
				s = MIN (bases, 0.8965f - 0.0962f*(float)(i - 161));
				v = MIN (1.f, (0.2102f + 0.0489f*(float)(i - 144)) * basev);
				HSVtoRGB (&r, &g, &b, h, s, v);
				SetRemap(alttable, i, r, g, b);
			}
			alttable->UpdateNative();
		}
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		if (memcmp (sprites[skin->sprite].name, "PLAY", 4) == 0)
		{ // The fighter is different! He gets a brown hairy loincloth, but the other
		  // two have blue capes.
			float vs[9] = { .28f, .32f, .38f, .42f, .47f, .5f, .58f, .71f, .83f };

			// Build player sprite translation
			//h = 45.f;
			v = MAX (0.1f, v);

			for (i = start; i <= end; i++)
			{
				HSVtoRGB (&r, &g, &b, h, s, vs[(i-start)*9/(int)range]*basev);
				SetRemap(table, i, r, g, b);
			}
		}
		else
		{
			float ms[18] = { .95f, .96f, .89f, .97f, .97f, 1.f, 1.f, 1.f, .97f, .99f, .87f, .77f, .69f, .62f, .57f, .47f, .43f };
			float mv[18] = { .16f, .19f, .22f, .25f, .31f, .35f, .38f, .41f, .47f, .54f, .60f, .65f, .71f, .77f, .83f, .89f, .94f, 1.f };

			// Build player sprite translation
			v = MAX (0.1f, v);

			for (i = start; i <= end; i++)
			{
				HSVtoRGB (&r, &g, &b, h, ms[(i-start)*18/(int)range]*bases, mv[(i-start)*18/(int)range]*basev);
				SetRemap(table, i, r, g, b);
			}
		}

		// Build lifegem translation
		if (alttable)
		{
			for (i = 164; i <= 185; ++i)
			{
				const PalEntry *base = &GPalette.BaseColors[i];
				float dummy;

				RGBtoHSV (base->r/255.f, base->g/255.f, base->b/255.f, &dummy, &s, &v);
				HSVtoRGB (&r, &g, &b, h, s*bases, v*basev);
				SetRemap(alttable, i, r, g, b);
			}
			alttable->UpdateNative();
		}
	}
	table->UpdateNative();
}

void R_BuildPlayerTranslation (int player)
{
	float h, s, v;

	D_GetPlayerColor (player, &h, &s, &v);

	R_CreatePlayerTranslation (h, s, v,
		&skins[players[player].userinfo.skin],
		translationtables[TRANSLATION_Players][player],
		translationtables[TRANSLATION_PlayersExtra][player]);
}

void R_GetPlayerTranslation (int color, FPlayerSkin *skin, FRemapTable *table)
{
	float h, s, v;

	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		&h, &s, &v);

	R_CreatePlayerTranslation (h, s, v, skin, table, NULL);
}
