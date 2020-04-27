//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2017 Christoph Oelckers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// FraggleScript loader
//
//---------------------------------------------------------------------------
//


#include "filesystem.h"
#include "g_level.h"
#include "s_sound.h"
#include "r_sky.h"
#include "t_script.h"
#include "cmdlib.h"
#include "gi.h"
#include "g_levellocals.h"
#include "xlat/xlat.h"
#include "maploader/maploader.h"
#include "s_music.h"
#include "texturemanager.h"

class FScriptLoader
{
	enum
	{
	  RT_SCRIPT,
	  RT_INFO,
	  RT_OTHER,
	} readtype;


	int drownflag;
	bool HasScripts;
	bool IgnoreInfo;
	FLevelLocals *Level;

	void ParseInfoCmd(char *line, FString &scriptsrc);
public:
	FScriptLoader(FLevelLocals *l)
	{
		Level = l;
	}
	bool ParseInfo(MapData * map);
};

//-----------------------------------------------------------------------------
//
// Process the lump to strip all unneeded information from it
//
//-----------------------------------------------------------------------------
void FScriptLoader::ParseInfoCmd(char *line, FString &scriptsrc)
{
	char *temp;
	
	// clear any control chars
	for(temp=line; *temp; temp++) if (*temp<32) *temp=32;

	if(readtype != RT_SCRIPT)       // not for scripts
	{
		temp = line+strlen(line)-1;

		// strip spaces at the beginning and end of the line
		while(*temp == ' ')	*temp-- = 0;
		while(*line == ' ') line++;
		
		if(!*line) return;
		
		if((line[0] == '/' && line[1] == '/') ||     // comment
			line[0] == '#' || line[0] == ';') return;
	}
	
	if(*line == '[')                // a new section seperator
	{
		line++;
		
		if(!strnicmp(line, "scripts", 7))
		{
			readtype = RT_SCRIPT;
			HasScripts = true;    // has scripts
		}
		else if (!strnicmp(line, "level info", 10))
		{
			readtype = RT_INFO;
		}
		return;
	}
	
	if (readtype==RT_SCRIPT)
	{
		scriptsrc << line << '\n';
	}
	else if (readtype==RT_INFO)
	{
		// Read the usable parts of the level info header 
		// and ignore the rest.
		FScanner sc;
		sc.OpenMem("LEVELINFO", line, (int)strlen(line));
		sc.SetCMode(true);
		sc.MustGetString();
		if (sc.Compare("levelname"))
		{
			char * beg = strchr(line, '=')+1;
			while (*beg<=' ') beg++;
			char * comm = strstr(beg, "//");
			if (comm) *comm=0;
			Level->LevelName = beg;
		}
		else if (sc.Compare("partime"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			Level->partime=sc.Number;
		}
		else if (sc.Compare("music"))
		{
			bool FS_ChangeMusic(const char * string);

			sc.MustGetStringName("=");
			sc.MustGetString();
			if (!FS_ChangeMusic(sc.String))
			{
				S_ChangeMusic(Level->Music, Level->musicorder);
			}
		}
		else if (sc.Compare("skyname"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
		
			Level->skytexture1 = Level->skytexture2 = TexMan.GetTextureID (sc.String, ETextureType::Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
			InitSkyMap (Level);
		}
		else if (sc.Compare("interpic"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			Level->info->ExitPic = sc.String;
		}
		else if (sc.Compare("gravity"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			Level->gravity=sc.Number*8.f;
		}
		else if (sc.Compare("nextlevel"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			Level->NextMap = sc.String;
		}
		else if (sc.Compare("nextsecret"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			Level->NextSecretMap = sc.String;
		}
		else if (sc.Compare("drown"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			drownflag=!!sc.Number;
		}
		else if (sc.Compare("consolecmd"))
		{
			char * beg = strchr(line, '=')+1;
			while (*beg<' ') beg++;
			char * comm = strstr(beg, "//");
			if (comm) *comm=0;
			FS_EmulateCmd(Level, beg);
		}
		else if (sc.Compare("ignore"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			IgnoreInfo=!!sc.Number;
		}
		// Ignore anything unknows
		sc.Close();
	}
}

//-----------------------------------------------------------------------------
//
// Loads the scripts for the current map
// Initializes all FS data
//
//-----------------------------------------------------------------------------

bool FScriptLoader::ParseInfo(MapData * map)
{
	char *lump;
	char *rover;
	char *startofline;
	int lumpsize;
	bool fsglobal=false;
	FString scriptsrc;

	// Global initializazion if not done yet.
	static bool done=false;
					
	// Load the script lump
	IgnoreInfo = false;
	lumpsize = map->Size(0);
	if (lumpsize==0)
	{
		// Try a global FS lump
		int lumpnum=fileSystem.CheckNumForName("FSGLOBAL");
		if (lumpnum<0) return false;
		lumpsize=fileSystem.FileLength(lumpnum);
		if (lumpsize==0) return false;
		fsglobal=true;
		lump=new char[lumpsize+3];
		fileSystem.ReadFile(lumpnum,lump);
	}
	else
	{
		lump=new char[lumpsize+3];
		map->Read(0, lump);
	}
	// Append a new line. The parser likes to crash when the last character is a valid token.
	lump[lumpsize]='\n';
	lump[lumpsize+1]='\r';
	lump[lumpsize+2]=0;
	lumpsize+=2;
	
	rover = startofline = lump;
	HasScripts=false;
	drownflag=-1;

	readtype = RT_OTHER;
	while(rover < lump+lumpsize)
    {
		if(*rover == '\n') // end of line
		{
			*rover = 0;               // make it an end of string (0)
			if (!IgnoreInfo) ParseInfoCmd(startofline, scriptsrc);
			startofline = rover+1; // next line
			*rover = '\n';            // back to end of line
		}
		rover++;
    }
	if (HasScripts) 
	{
		if (Level->FraggleScriptThinker)
		{
			I_Error("Only one FraggleThinker is allowed to exist at a time.\nCheck your code.");
		}

		auto th = Level->CreateThinker<DFraggleThinker>();
		th->LevelScript->Data.Resize((unsigned)scriptsrc.Len() + 1);
		memcpy(th->LevelScript->Data.Data(), scriptsrc.GetChars(), scriptsrc.Len() + 1);
		Level->FraggleScriptThinker = th;

		if (drownflag==-1) drownflag = (Level->maptype != MAPTYPE_DOOM || fsglobal);
		if (!drownflag) Level->airsupply=0;	// Legacy doesn't to water damage so we need to check if it has to be disabled here.
	}


	delete[] lump;
	return HasScripts;
}

//-----------------------------------------------------------------------------
//
// Starts the level info parser
// and patches the global linedef translation table
//
//-----------------------------------------------------------------------------

void T_LoadScripts(FLevelLocals *Level, MapData *map)
{
	FScriptLoader parser(Level);
	bool HasScripts = parser.ParseInfo(map);

	// Hack for Legacy compatibility: Since 272 is normally an MBF sky transfer we have to patch it.
	// It could be done with an additional translator but that would be sub-optimal for the user.
	// To handle this the default translator defines the proper Legacy type at index 270.
	// This code then then swaps 270 and 272 - but only if this is either Doom or Heretic and 
	// the default translator is being used.
	// Custom translators will not be patched.
	if ((gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Heretic) && Level->info->Translator.IsEmpty() &&
		Level->maptype == MAPTYPE_DOOM && Level->Translator->SimpleLineTranslations.Size() > 272 && Level->Translator->SimpleLineTranslations[272 - 2*HasScripts].special == FS_Execute)
	{
		std::swap(Level->Translator->SimpleLineTranslations[270], Level->Translator->SimpleLineTranslations[272]);
	}
}


//-----------------------------------------------------------------------------
//
// Adds an actor to the list of spawned things
//
//-----------------------------------------------------------------------------

void T_AddSpawnedThing(FLevelLocals *Level, AActor * ac)
{
	if (Level->FraggleScriptThinker)
	{
		auto &SpawnedThings = Level->FraggleScriptThinker->SpawnedThings;
		SpawnedThings.Push(GC::ReadBarrier(ac));
	}
}
