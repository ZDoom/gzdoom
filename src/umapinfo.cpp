//-----------------------------------------------------------------------------
//
// Copyright 2017 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <string.h>
#include "w_wad.h"
#include "g_level.h"
#include "r_defs.h"
#include "p_setup.h"
#include "gi.h"

FName MakeEndPic(const char *string);

struct BossAction
{
	int type;
	int special;
	int tag;
};

struct UMapEntry
{
	FString MapName;
	FString LevelName;
	FString InterText;
	FString InterTextSecret;
	TArray<FSpecialAction> BossActions;

	char levelpic[9] = "";
	char nextmap[9] = "";
	char nextsecret[9] = "";
	char music[9] = "";
	char skytexture[9] = "";
	char endpic[9] = "";
	char exitpic[9] = "";
	char enterpic[9] = "";
	char interbackdrop[9] = "FLOOR4_8";
	char intermusic[9] = "";
	int partime = 0;
	int nointermission = 0;
};

static TArray<UMapEntry> Maps;


// -----------------------------------------------
//
// Parses a set of string and concatenates them
//
// -----------------------------------------------

static FString ParseMultiString(FScanner &scanner, int error)
{
	FString build;
	
	if (scanner.CheckToken(TK_Identifier))
	{
		if (!stricmp(scanner.String, "clear"))
		{
			return "-";
		}
		else
		{
			scanner.ScriptError("Either 'clear' or string constant expected");
		}
	}
	
	do
	{
		scanner.MustGetToken(TK_StringConst);
		if (build.Len() > 0) build += "\n";
		build += scanner.String;
	} 
	while (scanner.CheckToken(','));
	return build;
}

// -----------------------------------------------
//
// Parses a lump name. The buffer must be at least 9 characters.
//
// -----------------------------------------------

static int ParseLumpName(FScanner &scanner, char *buffer)
{
	scanner.MustGetToken(TK_StringConst);
	if (strlen(scanner.String) > 8)
	{
		scanner.ScriptError("String too long. Maximum size is 8 characters.");
		return 0;
	}
	uppercopy(buffer, scanner.String);
	return 1;
}

// -----------------------------------------------
//
// Parses a standard property that is already known
// These do not get stored in the property list
// but in dedicated struct member variables.
//
// -----------------------------------------------

static int ParseStandardProperty(FScanner &scanner, UMapEntry *mape)
{
	// find the next line with content.
	// this line is no property.
	
	scanner.MustGetToken(TK_Identifier);
	FString pname = scanner.String;
	scanner.MustGetToken('=');

	if (!pname.CompareNoCase("levelname"))
	{
		scanner.MustGetToken(TK_StringConst);
		mape->LevelName = scanner.String;
	}
	else if (!pname.CompareNoCase("next"))
	{
		ParseLumpName(scanner, mape->nextmap);
	}
	else if (!pname.CompareNoCase("nextsecret"))
	{
		ParseLumpName(scanner, mape->nextsecret);
	}
	else if (!pname.CompareNoCase("levelpic"))
	{
		ParseLumpName(scanner, mape->levelpic);
	}
	else if (!pname.CompareNoCase("skytexture"))
	{
		ParseLumpName(scanner, mape->skytexture);
	}
	else if (!pname.CompareNoCase("music"))
	{
		ParseLumpName(scanner, mape->music);
	}
	else if (!pname.CompareNoCase("endpic"))
	{
		ParseLumpName(scanner, mape->endpic);
	}
	else if (!pname.CompareNoCase("endcast"))
	{
		scanner.MustGetBoolToken();
		if (scanner.Number) strcpy(mape->endpic, "$CAST");
		else strcpy(mape->endpic, "-");
	}
	else if (!pname.CompareNoCase("endbunny"))
	{
		scanner.MustGetBoolToken();
		if (scanner.Number) strcpy(mape->endpic, "$BUNNY");
		else strcpy(mape->endpic, "-");
	}
	else if (!pname.CompareNoCase("endgame"))
	{
		scanner.MustGetBoolToken();
		if (scanner.Number) strcpy(mape->endpic, "!");
		else strcpy(mape->endpic, "-");
	}
	else if (!pname.CompareNoCase("exitpic"))
	{
		ParseLumpName(scanner, mape->exitpic);
	}
	else if (!pname.CompareNoCase("enterpic"))
	{
		ParseLumpName(scanner, mape->enterpic);
	}
	else if (!pname.CompareNoCase("nointermission"))
	{
		scanner.MustGetBoolToken();
		mape->nointermission = scanner.Number;
	}
	else if (!pname.CompareNoCase("partime"))
	{
		scanner.MustGetValue(false);
		mape->partime = TICRATE * scanner.Number;
	}
	else if (!pname.CompareNoCase("intertext"))
	{
		mape->InterText = ParseMultiString(scanner, 1);
		if (mape->InterText.IsEmpty()) return 0;
	}
	else if (!pname.CompareNoCase("intertextsecret"))
	{
		mape->InterTextSecret = ParseMultiString(scanner, 1);
		if (mape->InterTextSecret.IsEmpty()) return 0;
	}
	else if (!pname.CompareNoCase("interbackdrop"))
	{
		ParseLumpName(scanner, mape->interbackdrop);
	}
	else if (!pname.CompareNoCase("intermusic"))
	{
		ParseLumpName(scanner, mape->intermusic);
	}
	else if (!pname.CompareNoCase("episode"))
	{
		FString Episode = ParseMultiString(scanner, 1);
		if (Episode.IsEmpty()) return 0;
		if (Episode.Compare("-") == 0)
		{
			// clear the given episode
			for (unsigned i = 0; i < AllEpisodes.Size(); i++)
			{
				if (AllEpisodes[i].mEpisodeMap.CompareNoCase(mape->MapName) == 0)
				{
					AllEpisodes.Delete(i);
					break;
				}
			}
		}
		else
		{
			auto split = Episode.Split("\n");
			// add the given episode
			FEpisode epi;

			epi.mEpisodeName = split[1];
			epi.mEpisodeMap = mape->MapName;
			epi.mPicName = split[0];
			epi.mShortcut = split[2][0];

			unsigned i;
			for (i = 0; i < AllEpisodes.Size(); i++)
			{
				if (AllEpisodes[i].mEpisodeMap.CompareNoCase(mape->MapName) == 0)
				{
					AllEpisodes[i] = std::move(epi);
					break;
				}
			}
			if (i == AllEpisodes.Size())
			{
				AllEpisodes.Push(epi);
			}
		}
	}
	else if (!pname.CompareNoCase("bossaction"))
	{
		scanner.MustGetToken(TK_Identifier);
		int classnum, special, tag;
		if (!stricmp(scanner.String, "clear"))
		{
			// mark level free of boss actions
			classnum = special = tag = -1;
			mape->BossActions.Clear();
		}
		else
		{
			FName type = scanner.String;
			scanner.MustGetToken(',');
			scanner.MustGetValue(false);
			int special = scanner.Number;
			scanner.MustGetToken(',');
			scanner.MustGetValue(false);
			int tag = scanner.Number;
			// allow no 0-tag specials here, unless a level exit.
			if (tag != 0 || special == 11 || special == 51 || special == 52 || special == 124)
			{
				FSpecialAction & bossact = mape->BossActions[mape->BossActions.Reserve(1)];
				line_t line;
				maplinedef_t mld;
				mld.special = special;
				mld.tag = tag;
				P_TranslateLineDef(&line, &mld);
				bossact = { type, (uint8_t)line.special, {line.args[0], line.args[1],line.args[2],line.args[3],line.args[4]} };
			}
		}
	}
	else
	{
		// Skip over all unknown properties.
		do
		{
			if (!scanner.CheckValue(true))
			{
				scanner.MustGetAnyToken();
				if (scanner.TokenType != TK_Identifier && scanner.TokenType != TK_StringConst && scanner.TokenType != TK_True && scanner.TokenType != TK_False)
				{
					scanner.ScriptError("Identifier or value expected");
				}
			}
			
		} while (scanner.CheckToken(','));
	}
	return 1;
}

// -----------------------------------------------
//
// Parses a complete map entry
//
// -----------------------------------------------

static int ParseMapEntry(FScanner &scanner, UMapEntry *val)
{
	scanner.MustGetToken(TK_Identifier);

	val->MapName = scanner.String;
	scanner.MustGetToken('{');
	while(!scanner.CheckToken('}'))
	{
		ParseStandardProperty(scanner, val);
	}
	return 1;
}

// -----------------------------------------------
//
// Parses a complete UMAPINFO lump
//
// -----------------------------------------------

int ParseUMapInfo(int lumpnum)
{
	P_LoadTranslator(gameinfo.translator);

	FScanner scanner(lumpnum);
	unsigned int i;

	while (scanner.GetToken())
	{
		scanner.TokenMustBe(TK_Map);

		UMapEntry parsed;
		ParseMapEntry(scanner, &parsed);

		// Endpic overrides level exits.
		if (parsed.endpic[0])
		{
			parsed.nextmap[0] = parsed.nextsecret[0] = 0;
			if (parsed.endpic[0] == '!') parsed.endpic[0] = 0;
		}
		/*
		else if (!parsed.nextmap[0] && !parsed.endpic[0])
		{
			if (!parsed.MapName.CompareNoCase("MAP30")) uppercopy(parsed.endpic, "$CAST");
			else if (!parsed.MapName.CompareNoCase("E1M8"))  uppercopy(parsed.endpic, gameinfo.creditPages.Last());
			else if (!parsed.MapName.CompareNoCase("E2M8"))  uppercopy(parsed.endpic, "VICTORY");
			else if (!parsed.MapName.CompareNoCase("E3M8"))  uppercopy(parsed.endpic, "$BUNNY");
			else if (!parsed.MapName.CompareNoCase("E4M8"))  uppercopy(parsed.endpic, "ENDPIC");
			else if (gameinfo.gametype == GAME_Chex && !parsed.MapName.CompareNoCase("E1M5"))  uppercopy(parsed.endpic, "CREDIT");
			else
			{
				parsed.nextmap[0] = 0;	// keep previous setting
			}
		}
		*/

		// Does this property already exist? If yes, replace it.
		for(i = 0; i < Maps.Size(); i++)
		{
			if (!parsed.MapName.Compare(Maps[i].MapName))
			{
				Maps[i] = parsed;
				return 1;
			}
		}
		// Not found so create a new one.
		Maps.Push(parsed);
		
	}
	return 1;
}


// This will get called if after an UMAPINFO lump a regular (Z)MAPINFO is found or when MAPINFO parsing is complete.
void CommitUMapinfo(level_info_t *defaultinfo)
{
	for (auto &map : Maps)
	{
		auto levelinfo = FindLevelInfo(map.MapName);
		if (levelinfo == nullptr)
		{
			// Map did not exist yet.
			auto levelindex = wadlevelinfos.Reserve(1);
			levelinfo = &wadlevelinfos[levelindex];
			*levelinfo = *defaultinfo;
		}
		if (map.MapName.IsNotEmpty()) levelinfo->MapName = map.MapName;
		if (map.LevelName.IsNotEmpty())
		{
			levelinfo->LevelName = map.LevelName;
			levelinfo->PName = "";	// clear the map name patch to force the string version to be shown - unless explicitly overridden right next.
		}
		if (map.levelpic[0]) levelinfo->PName = map.levelpic;
		if (map.nextmap[0]) levelinfo->NextMap = map.nextmap;
		else if (map.endpic[0])
		{
			FName name;

			if (!stricmp(map.endpic, "$CAST"))
			{
				name = "INTER_CAST";
			}
			else if (!stricmp(map.endpic, "$BUNNY"))
			{
				name = "INTER_BUNNY";
			}
			else
			{
				name = MakeEndPic(map.endpic);
			}
			if (name != NAME_None)
			{
				levelinfo->NextMap.Format("enDSeQ%04x", int(name));
			}
		}

		if (map.nextsecret[0]) levelinfo->NextSecretMap = map.nextsecret;
		if (map.music[0])
		{
			levelinfo->Music = map.music;
			levelinfo->musicorder = 0;
		}
		if (map.skytexture[0])
		{
			levelinfo->SkyPic1 = map.skytexture;
			levelinfo->skyspeed1 = 0;
			levelinfo->SkyPic2 = "";
			levelinfo->skyspeed2 = 0;
		}
		if (map.partime > 0) levelinfo->partime = map.partime;
		if (map.enterpic[0]) levelinfo->EnterPic = map.enterpic;
		if (map.exitpic[0]) levelinfo->ExitPic = map.exitpic;
		if (map.intermusic[0])
		{
			levelinfo->InterMusic = map.intermusic;
			levelinfo->intermusicorder = 0;
		}
		if (map.BossActions.Size() > 0)
		{
			// Setting a boss action will deactivate the flag based monster actions.
			levelinfo->specialactions = std::move(map.BossActions);
			levelinfo->flags &= ~(LEVEL_BRUISERSPECIAL | LEVEL_CYBORGSPECIAL | LEVEL_SPIDERSPECIAL | LEVEL_MAP07SPECIAL | LEVEL_MINOTAURSPECIAL | LEVEL_HEADSPECIAL | LEVEL_SORCERER2SPECIAL | LEVEL_SPECACTIONSMASK | LEVEL_SPECKILLMONSTERS);
		}

		const int exflags = FExitText::DEF_TEXT | FExitText::DEF_BACKDROP | FExitText::DEF_MUSIC;
		if (map.InterText.IsNotEmpty())
		{
			if (map.InterText.Compare("-") != 0)
				levelinfo->ExitMapTexts[NAME_Normal] = { exflags, 0, map.InterText, map.interbackdrop, map.intermusic[0]? map.intermusic : gameinfo.intermissionMusic };
			else
				levelinfo->ExitMapTexts[NAME_Normal] = { 0, 0 };
		}
		if (map.InterTextSecret.IsNotEmpty())
		{
			if (map.InterTextSecret.Compare("-") != 0)
				levelinfo->ExitMapTexts[NAME_Secret] = { exflags, 0, map.InterTextSecret, map.interbackdrop, map.intermusic[0] ? map.intermusic : gameinfo.intermissionMusic };
			else
				levelinfo->ExitMapTexts[NAME_Secret] = { 0, 0 };
		}
		if (map.nointermission) levelinfo->flags |= LEVEL_NOINTERMISSION;
	}


	// All done. If we get here again, start fresh.
	Maps.Clear();
	Maps.ShrinkToFit();
}