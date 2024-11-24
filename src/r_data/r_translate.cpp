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


#include "r_data/r_translate.h"
#include "v_video.h"
#include "g_game.h"
#include "d_netinf.h"
#include "sc_man.h"
#include "engineerrors.h"
#include "filesystem.h"
#include "serializer.h"
#include "d_player.h"
#include "r_data/sprites.h"
#include "r_state.h"
#include "vm.h"
#include "v_text.h"
#include "m_crc32.h"
#include "g_levellocals.h"
#include "palutil.h"

#include "gi.h"

//----------------------------------------------------------------------------
//
// helper stuff for serializing TRANSLATION_User
//
//----------------------------------------------------------------------------

static TArray<std::pair<FTranslationID, FRemapTable>> usertransmap;

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static void SerializeRemap(FSerializer &arc, FRemapTable &remap)
{
	arc("numentries", remap.NumEntries);
	arc.Array("remap", remap.Remap, remap.NumEntries);
	arc.Array("palette", remap.Palette, remap.NumEntries);
}

void StaticSerializeTranslations(FSerializer &arc)
{
	usertransmap.Clear();
	if (arc.BeginArray("translations"))
	{
		// Does this level have custom translations?
		FRemapTable *trans;
		int w;
		if (arc.isWriting())
		{
			auto size = GPalette.NumTranslations(TRANSLATION_LevelScripted);
			for (unsigned int i = 0; i < size; ++i)
			{
				trans = GPalette.GetTranslation(TRANSLATION_LevelScripted, i);
				if (trans != NULL && !trans->IsIdentity())
				{
					if (arc.BeginObject(nullptr))
					{
						arc("index", i);
						SerializeRemap(arc, *trans);
						arc.EndObject();
					}
				}
			}
		}
		else
		{
			while (arc.BeginObject(nullptr))
			{
				arc("index", w);
				FRemapTable remap;
				SerializeRemap(arc, remap);
				GPalette.UpdateTranslation(TRANSLATION(TRANSLATION_LevelScripted, w), &remap);
				arc.EndObject();
			}
		}
		arc.EndArray();
	}
	if (arc.BeginArray("usertranslations"))
	{
		// Does this level have custom translations?
		FRemapTable* trans;
		int w;
		if (arc.isWriting())
		{
			auto size = GPalette.NumTranslations(TRANSLATION_User);
			for (unsigned int i = 0; i < size; ++i)
			{
				trans = GPalette.GetTranslation(TRANSLATION_User, i);
				if (trans != NULL && !trans->IsIdentity())
				{
					if (arc.BeginObject(nullptr))
					{
						arc("index", i);
						SerializeRemap(arc, *trans);
						arc.EndObject();
					}
				}
			}
		}
		else
		{
			while (arc.BeginObject(nullptr))
			{
				arc("index", w);
				FRemapTable remap;
				SerializeRemap(arc, remap);
				// do not add the translation to the global list yet. We want to avoid adding tables that are not needed anymore.
				usertransmap.Push(std::make_pair(TRANSLATION(TRANSLATION_User, w), remap));
				arc.EndObject();
			}
		}
		arc.EndArray();
	}
}

void StaticClearSerializeTranslationsData()
{
	usertransmap.Reset();
}

FTranslationID RemapUserTranslation(FTranslationID trans)
{
	if (GetTranslationType(trans) == TRANSLATION_User)
	{
		for (auto& check : usertransmap)
		{
			if (trans == check.first)
				return GPalette.AddTranslation(TRANSLATION_User, &check.second);
		}
	}
	return trans;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static TArray<PalEntry> BloodTranslationColors;

FTranslationID CreateBloodTranslation(PalEntry color)
{
	unsigned int i;

	if (BloodTranslationColors.Size() == 0)
	{
		// Don't use the first slot.
		GPalette.PushIdentityTable(TRANSLATION_Blood);
		BloodTranslationColors.Push(0);
	}

	for (i = 1; i < BloodTranslationColors.Size(); i++)
	{
		if (color.r == BloodTranslationColors[i].r &&
			color.g == BloodTranslationColors[i].g &&
			color.b == BloodTranslationColors[i].b)
		{
			// A duplicate of this translation already exists
			return TRANSLATION(TRANSLATION_Blood, i);
		}
	}
	if (BloodTranslationColors.Size() >= MAX_DECORATE_TRANSLATIONS)
	{
		I_Error("Too many blood colors");
	}
	FRemapTable trans;
	trans.Palette[0] = 0;
	trans.Remap[0] = 0;
	for (i = 1; i < 256; i++)
	{
		int bright = max(std::max(GPalette.BaseColors[i].r, GPalette.BaseColors[i].g), GPalette.BaseColors[i].b);
		PalEntry pe = PalEntry(255, color.r*bright/255, color.g*bright/255, color.b*bright/255);
		int entry = ColorMatcher.Pick(pe.r, pe.g, pe.b);

		trans.Palette[i] = pe;
		trans.Remap[i] = entry;
	}
	GPalette.AddTranslation(TRANSLATION_Blood, &trans);
	return TRANSLATION(TRANSLATION_Blood, BloodTranslationColors.Push(color));
}

//----------------------------------------------------------------------------
//
// R_InitTranslationTables
// Creates the translation tables to map the green color ramp to gray,
// brown, red. Assumes a given structure of the PLAYPAL.
//
//----------------------------------------------------------------------------

void R_InitTranslationTables ()
{
	int i;

	// Each player gets two translations. Doom and Strife don't use the
	// extra ones, but Heretic and Hexen do. These are set up during
	// netgame arbitration and as-needed, so they just get to be identity
	// maps until then so they won't be invalid.
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		GPalette.PushIdentityTable(TRANSLATION_Players);
		GPalette.PushIdentityTable(TRANSLATION_PlayersExtra);
		GPalette.PushIdentityTable(TRANSLATION_RainPillar);
	}
	// The menu player also gets a separate translation table
	GPalette.PushIdentityTable(TRANSLATION_Players);

	// The three standard translations from Doom or Heretic (seven for Strife),
	// plus the generic ice translation.
	FRemapTable stdremaps[8];
	for (i = 0; i < 8; ++i)
	{
		stdremaps[i].MakeIdentity();
	}

	// Each player corpse has its own translation so they won't change
	// color if the player who created them changes theirs.
	for (i = 0; i < FLevelLocals::BODYQUESIZE; ++i)
	{
		GPalette.PushIdentityTable(TRANSLATION_PlayerCorpses);
	}

	// Create the standard translation tables
	if (gameinfo.gametype & GAME_DoomChex)
	{
		for (i = 0x70; i < 0x80; i++)
		{ // map green ramp to gray, brown, red
			stdremaps[0].Remap[i] = 0x60 + (i&0xf);
			stdremaps[1].Remap[i] = 0x40 + (i&0xf);
			stdremaps[2].Remap[i] = 0x20 + (i&0xf);

			stdremaps[0].Palette[i] = GPalette.BaseColors[0x60 + (i&0xf)] | MAKEARGB(255,0,0,0);
			stdremaps[1].Palette[i] = GPalette.BaseColors[0x40 + (i&0xf)] | MAKEARGB(255,0,0,0);
			stdremaps[2].Palette[i] = GPalette.BaseColors[0x20 + (i&0xf)] | MAKEARGB(255,0,0,0);
		}
	}
	else if (gameinfo.gametype == GAME_Heretic)
	{
		for (i = 225; i <= 240; i++)
		{
			stdremaps[0].Remap[i] = 114+(i-225); // yellow
			stdremaps[1].Remap[i] = 145+(i-225); // red
			stdremaps[2].Remap[i] = 190+(i-225); // blue
			
			stdremaps[0].Palette[i] = GPalette.BaseColors[114+(i-225)] | MAKEARGB(255,0,0,0);
			stdremaps[1].Palette[i] = GPalette.BaseColors[145+(i-225)] | MAKEARGB(255,0,0,0);
			stdremaps[2].Palette[i] = GPalette.BaseColors[190+(i-225)] | MAKEARGB(255,0,0,0);
		}
	}
	else if (gameinfo.gametype == GAME_Strife)
	{
		for (i = 0x20; i <= 0x3F; ++i)
		{
			stdremaps[0].Remap[i] = i - 0x20;
			stdremaps[1].Remap[i] = i - 0x20;
			stdremaps[2].Remap[i] = 0xD0 + (i&0xf);
			stdremaps[3].Remap[i] = 0xD0 + (i&0xf);
			stdremaps[4].Remap[i] = i - 0x20;
			stdremaps[5].Remap[i] = i - 0x20;
			stdremaps[6].Remap[i] = i - 0x20;
		}
		for (i = 0x50; i <= 0x5F; ++i)
		{
			// Merchant hair
			stdremaps[4].Remap[i] = 0x80 + (i&0xf);
			stdremaps[5].Remap[i] = 0x10 + (i&0xf);
			stdremaps[6].Remap[i] = 0x40 + (i&0xf);
		}
		for (i = 0x80; i <= 0x8F; ++i)
		{
			stdremaps[0].Remap[i] = 0x40 + (i&0xf); // red
			stdremaps[1].Remap[i] = 0xB0 + (i&0xf); // rust
			stdremaps[2].Remap[i] = 0x10 + (i&0xf); // gray
			stdremaps[3].Remap[i] = 0x30 + (i&0xf); // dark green
			stdremaps[4].Remap[i] = 0x50 + (i&0xf); // gold
			stdremaps[5].Remap[i] = 0x60 + (i&0xf); // bright green
			stdremaps[6].Remap[i] = 0x90 + (i&0xf); // blue
		}
		for (i = 0xC0; i <= 0xCF; ++i)
		{
			stdremaps[4].Remap[i] = 0xA0 + (i&0xf);
			stdremaps[5].Remap[i] = 0x20 + (i&0xf);
			stdremaps[6].Remap[i] = (i&0xf);
		}
		stdremaps[6].Remap[0xC0] = 1;
		for (i = 0xD0; i <= 0xDF; ++i)
		{
			stdremaps[4].Remap[i] = 0xB0 + (i&0xf);
			stdremaps[5].Remap[i] = 0x30 + (i&0xf);
			stdremaps[6].Remap[i] = 0x10 + (i&0xf);
		}
		for (i = 0xF1; i <= 0xF6; ++i)
		{
			stdremaps[0].Remap[i] = 0xDF + (i&0xf);
		}
		for (i = 0xF7; i <= 0xFB; ++i)
		{
			stdremaps[0].Remap[i] = i - 6;
		}
		for (i = 0; i < 7; ++i)
		{
			for (int j = 0x20; j <= 0xFB; ++j)
			{
				stdremaps[i].Palette[j] =
					GPalette.BaseColors[stdremaps[i].Remap[j]] | MAKEARGB(255,0,0,0);
			}
		}
	}

	stdremaps[7] = GPalette.IceMap; // this must also be inserted into the translation manager to be usable by sprites.
	GPalette.AddTranslation(TRANSLATION_Standard, stdremaps, 8);

}

//----------------------------------------------------------------------------
//
// [RH] Create a player's translation table based on a given mid-range color.
// [GRB] Split to 2 functions (because of player setup menu)
//
//----------------------------------------------------------------------------

static void SetRemap(FRemapTable *table, int i, float r, float g, float b)
{
	int ir = clamp (int(r * 255.f), 0, 255);
	int ig = clamp (int(g * 255.f), 0, 255);
	int ib = clamp (int(b * 255.f), 0, 255);
	table->Remap[i] = ColorMatcher.Pick (ir, ig, ib);
	table->Palette[i] = PalEntry(255, ir, ig, ib);
}

//----------------------------------------------------------------------------
//
// Sets the translation for Heretic's rain pillar
// This tries to create a translation that preserves the brightness of
// the rain projectiles so that their effect isn't ruined.
//
//----------------------------------------------------------------------------

static void SetPillarRemap(FRemapTable *table, int i, float h, float s, float v)
{
	float ph, ps, pv;
	float fr = GPalette.BaseColors[i].r / 255.f;
	float fg = GPalette.BaseColors[i].g / 255.f;
	float fb = GPalette.BaseColors[i].b / 255.f;
	RGBtoHSV(fr, fg, fb, &ph, &ps, &pv);
	HSVtoRGB(&fr, &fg, &fb, h, s, (v*0.2f + pv*0.8f));
	int ir = clamp (int(fr * 255.f), 0, 255);
	int ig = clamp (int(fg * 255.f), 0, 255);
	int ib = clamp (int(fb * 255.f), 0, 255);
	table->Remap[i] = ColorMatcher.Pick (ir, ig, ib);
	table->Palette[i] = PalEntry(255, ir, ig, ib);
}

//----------------------------------------------------------------------------

static bool SetRange(FRemapTable *table, int start, int end, int first, int last)
{
	bool identity = true;
	if (start == end)
	{
		int pi = (first + last) / 2;
		table->Remap[start] = GPalette.Remap[pi];
		identity &= (pi == start);
		table->Palette[start] = GPalette.BaseColors[table->Remap[start]];
		table->Palette[start].a = 255;
	}
	else
	{
		int palrange = last - first;
		for (int i = start; i <= end; ++i)
		{
			int pi = first + palrange * (i - start) / (end - start);
			table->Remap[i] = GPalette.Remap[pi];
			identity &= (pi == i);
			table->Palette[i] = GPalette.BaseColors[table->Remap[i]];
			table->Palette[i].a = 255;
		}
	}
	return identity;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static void R_CreatePlayerTranslation (float h, float s, float v, const FPlayerColorSet *colorset,
	FPlayerSkin *skin, FRemapTable *table, FRemapTable *alttable, FRemapTable *pillartable)
{
	int i;
	uint8_t start = skin->range0start;
	uint8_t end = skin->range0end;
	float r, g, b;
	float bases, basev;
	float sdelta, vdelta;
	float range;

	if (alttable) alttable->MakeIdentity();
	if (pillartable) pillartable->MakeIdentity();

	// Set up the base translation for this skin. If the skin was created
	// for the current game, then this is just an identity translation.
	// Otherwise, it remaps the colors from the skin's original palette to
	// the current one.
	if (skin->othergame)
	{
		memcpy (table->Remap, OtherGameSkinRemap, table->NumEntries);
		memcpy (table->Palette, OtherGameSkinPalette, sizeof(*table->Palette) * table->NumEntries);
	}
	else
	{
		for (i = 0; i < table->NumEntries; ++i)
		{
			table->Remap[i] = i;
		}
		memcpy(table->Palette, GPalette.BaseColors, sizeof(*table->Palette) * table->NumEntries);
	}
	for (i = 1; i < table->NumEntries; ++i)
	{
		table->Palette[i].a = 255;
	}

	// [GRB] Don't translate skins with color range 0-0 (PlayerPawn default)
	if (start == 0 && end == 0)
	{
		table->Inactive = true;
		return;
	}

	table->Inactive = false;
	range = (float)(end-start+1);

	bases = s;
	basev = v;

	if (colorset != NULL && colorset->Lump >= 0 && fileSystem.FileLength(colorset->Lump) < 256)
	{ // Bad table length. Ignore it.
		colorset = NULL;
	}

	if (colorset != NULL)
	{
		bool identity = true;
		// Use the pre-defined range instead of a custom one.
		if (colorset->Lump < 0)
		{
			identity &= SetRange(table, start, end, colorset->FirstColor, colorset->LastColor);
			for (i = 0; i < colorset->NumExtraRanges; ++i)
			{
				identity &= SetRange(table,
					colorset->Extra[i].RangeStart, colorset->Extra[i].RangeEnd,
					colorset->Extra[i].FirstColor, colorset->Extra[i].LastColor);
			}
		}
		else
		{
			auto translump = fileSystem.ReadFile(colorset->Lump);
			auto trans = translump.bytes();
			for (i = start; i <= end; ++i)
			{
				table->Remap[i] = GPalette.Remap[trans[i]];
				identity &= (trans[i] == i);
				table->Palette[i] = GPalette.BaseColors[table->Remap[i]];
				table->Palette[i].a = 255;
			}
		}
		// If the colorset created an identity translation mark it as inactive
		table->Inactive = identity;
	}
	else if (gameinfo.gametype & GAME_DoomStrifeChex)
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
	}
	else if (gameinfo.gametype == GAME_Hexen)
	{
		if (memcmp (sprites[skin->sprite].name, "PLAY", 4) == 0)
		{ // The fighter is different! He gets a brown hairy loincloth, but the other
		  // two have blue capes.
			float vs[9] = { .28f, .32f, .38f, .42f, .47f, .5f, .58f, .71f, .83f };

			// Build player sprite translation
			//h = 45.f;
			v = max (0.1f, v);

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
			v = max (0.1f, v);

			for (i = start; i <= end; i++)
			{
				HSVtoRGB (&r, &g, &b, h, ms[(i-start)*18/(int)range]*bases, mv[(i-start)*18/(int)range]*basev);
				SetRemap(table, i, r, g, b);
			}
		}
	}

	if (alttable != NULL)
	{
		if (gameinfo.gametype == GAME_Heretic)
		{
			// Build rain/lifegem translation
			bases = min(bases * 1.3f, 1.f);
			basev = min(basev * 1.3f, 1.f);
			for (i = 145; i <= 168; i++)
			{
				s = min(bases, 0.8965f - 0.0962f * (float)(i - 161));
				v = min(1.f, (0.2102f + 0.0489f * (float)(i - 144)) * basev);
				HSVtoRGB(&r, &g, &b, h, s, v);
				SetRemap(alttable, i, r, g, b);
				SetPillarRemap(pillartable, i, h, s, v);
			}
		}
		else if (gameinfo.gametype == GAME_Hexen)
		{
			// Build Hexen's lifegem translation.

			// Is the player's translation range the same as the gem's and we are using a
			// predefined translation? If so, then use the same one for the gem. Otherwise,
			// build one as per usual.
			if (colorset != NULL && start == 164 && end == 185)
			{
				*alttable = *table;
			}
			else
			{
				for (i = 164; i <= 185; ++i)
				{
					const PalEntry* base = &GPalette.BaseColors[i];
					float dummy;

					RGBtoHSV(base->r / 255.f, base->g / 255.f, base->b / 255.f, &dummy, &s, &v);
					HSVtoRGB(&r, &g, &b, h, s * bases, v * basev);
					SetRemap(alttable, i, r, g, b);
				}
			}
		}
	}
		
	
 }

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void R_BuildPlayerTranslation (int player)
{
	float h, s, v;
	FPlayerColorSet *colorset;

	D_GetPlayerColor (player, &h, &s, &v, &colorset);

	FRemapTable remaps[3];
	R_CreatePlayerTranslation (h, s, v, colorset, &Skins[players[player].userinfo.GetSkin()], &remaps[0], &remaps[1], &remaps[2]);

	GPalette.UpdateTranslation(TRANSLATION(TRANSLATION_Players, player), &remaps[0]);
	GPalette.UpdateTranslation(TRANSLATION(TRANSLATION_PlayersExtra, player), &remaps[1]);
	GPalette.UpdateTranslation(TRANSLATION(TRANSLATION_RainPillar, player), &remaps[2]);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void R_GetPlayerTranslation (int color, const FPlayerColorSet *colorset, FPlayerSkin *skin, FRemapTable *table)
{
	float h, s, v;

	if (colorset != NULL)
	{
		color = colorset->RepresentativeColor;
	}
	RGBtoHSV (RPART(color)/255.f, GPART(color)/255.f, BPART(color)/255.f,
		&h, &s, &v);

	R_CreatePlayerTranslation (h, s, v, colorset, skin, table, NULL, NULL);
}


DEFINE_ACTION_FUNCTION(_Translation, SetPlayerTranslation)
{
	PARAM_PROLOGUE;
	PARAM_UINT(tgroup);
	PARAM_UINT(tnum);
	PARAM_UINT(pnum);
	PARAM_POINTER(cls, FPlayerClass);

	if (pnum >= MAXPLAYERS || tgroup >= NUM_TRANSLATION_TABLES)
	{
		ACTION_RETURN_BOOL(false);
	}
	auto self = &players[pnum];
	int PlayerColor = self->userinfo.GetColor();
	int	PlayerSkin = self->userinfo.GetSkin();
	int PlayerColorset = self->userinfo.GetColorSet();

	if (cls != nullptr)
	{
		PlayerSkin = R_FindSkin(Skins[PlayerSkin].Name.GetChars(), int(cls - &PlayerClasses[0]));
		FRemapTable remap;
		R_GetPlayerTranslation(PlayerColor, GetColorSet(cls->Type, PlayerColorset),
			&Skins[PlayerSkin], &remap);
		GPalette.UpdateTranslation(TRANSLATION(tgroup, tnum), &remap);
	}
	ACTION_RETURN_BOOL(true);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------
static TMap<FName, FTranslationID> customTranslationMap;

FTranslationID R_FindCustomTranslation(FName name)
{
	switch (name.GetIndex())
	{
	case NAME_Ice:
		// Ice is a special case which will remain in its original slot.
		return TRANSLATION(TRANSLATION_Standard, 7);

	case NAME_None:
		return NO_TRANSLATION;

	case NAME_RainPillar1:
	case NAME_RainPillar2:
	case NAME_RainPillar3:
	case NAME_RainPillar4:
	case NAME_RainPillar5:
	case NAME_RainPillar6:
	case NAME_RainPillar7:
	case NAME_RainPillar8:
		return TRANSLATION(TRANSLATION_RainPillar, name.GetIndex() - NAME_RainPillar1);

	case NAME_Player1:
	case NAME_Player2:
	case NAME_Player3:
	case NAME_Player4:
	case NAME_Player5:
	case NAME_Player6:
	case NAME_Player7:
	case NAME_Player8:
		return TRANSLATION(TRANSLATION_Players, name.GetIndex() - NAME_Player1);

	}
	auto t = customTranslationMap.CheckKey(name);
	return (t != nullptr)? *t : INVALID_TRANSLATION;
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void R_ParseTrnslate()
{
	customTranslationMap.Clear();
	GPalette.ClearTranslationSlot(TRANSLATION_Custom);

	int lump;
	int lastlump = 0;
	while (-1 != (lump = fileSystem.FindLump("TRNSLATE", &lastlump)))
	{
		FScanner sc(lump);
		while (sc.GetToken())
		{
			sc.TokenMustBe(TK_Identifier);

			FName newtrans = sc.String;
			FRemapTable NewTranslation;
			if (sc.CheckToken(':'))
			{
				sc.MustGetAnyToken();
				if (sc.TokenType == TK_IntConst)
				{
					int max = 6;
					if (sc.Number < 0 || sc.Number > max)
					{
						sc.ScriptError("Translation must be in the range [0,%d]", max);
					}
					NewTranslation = *GPalette.GetTranslation(TRANSLATION_Standard, sc.Number);
				}
				else if (sc.TokenType == TK_Identifier)
				{
					auto tnum = R_FindCustomTranslation(sc.String);
					if (tnum == INVALID_TRANSLATION)
					{
						sc.ScriptError("Base translation '%s' not found in '%s'", sc.String, newtrans.GetChars());
					}
					NewTranslation = *GPalette.TranslationToTable(tnum.index());
				}
				else
				{
					// error out.
					sc.TokenMustBe(TK_Identifier);
				}
			}
			else NewTranslation.MakeIdentity();
			sc.MustGetToken('=');
			do
			{
				sc.MustGetToken(TK_StringConst);
				int pallump = fileSystem.CheckNumForAnyName(sc.String, ns_global);
				if (pallump >= 0)	// 
				{
					int start = 0;
					if (sc.CheckToken(','))
					{
						sc.MustGetValue(false);
						start = sc.Number;
					}
					uint8_t palette[768];
					int numcolors = ReadPalette(pallump, palette);
					NewTranslation.AddColors(start, numcolors, palette);
				}
				else
				{
					try
					{
						NewTranslation.AddToTranslation(sc.String);
					}
					catch (CRecoverableError & err)
					{
						sc.ScriptMessage("Error in translation '%s':\n" TEXTCOLOR_YELLOW "%s\n", sc.String, err.GetMessage());
					}
				}
			} while (sc.CheckToken(','));

			auto trans = GPalette.StoreTranslation(TRANSLATION_Custom, &NewTranslation);
			customTranslationMap[newtrans] = trans;
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

struct FTranslation
{
	PalEntry colors[256];
};

DEFINE_ACTION_FUNCTION(_Translation, AddTranslation)
{
	PARAM_SELF_STRUCT_PROLOGUE(FTranslation);

	FRemapTable NewTranslation;
	memcpy(&NewTranslation.Palette[0], self->colors, 256 * sizeof(PalEntry));
	for (int i = 0; i < 256; i++)
	{
		NewTranslation.Remap[i] = ColorMatcher.Pick(self->colors[i]);
	}
	auto trans = GPalette.StoreTranslation(TRANSLATION_User, &NewTranslation);
	ACTION_RETURN_INT(trans.index());
}

