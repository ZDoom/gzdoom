/*
** t_load.cpp
** FraggleScript loader
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
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


#include "w_wad.h"
#include "tarray.h"
#include "g_level.h"
#include "sc_man.h"
#include "s_sound.h"
#include "r_sky.h"
#include "t_script.h"
#include "cmdlib.h"
#include "p_lnspec.h"
#include "gi.h"
#include "xlat/xlat.h"

void T_Init();

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

	void ParseInfoCmd(char *line, FString &scriptsrc);
public:
	bool ParseInfo(MapData * map);
};

struct FFsOptions : public FOptionalMapinfoData
{
	FFsOptions()
	{
		identifier = "fragglescript";
		nocheckposition = false;
	}
	virtual FOptionalMapinfoData *Clone() const
	{
		FFsOptions *newopt = new FFsOptions;
		newopt->identifier = identifier;
		newopt->nocheckposition = nocheckposition;
		return newopt;
	}
	bool nocheckposition;
};

DEFINE_MAP_OPTION(fs_nocheckposition, false)
{
	FFsOptions *opt = info->GetOptData<FFsOptions>("fragglescript");

	if (parse.CheckAssign())
	{
		parse.sc.MustGetNumber();
		opt->nocheckposition = !!parse.sc.Number;
	}
	else
	{
		opt->nocheckposition = true;
	}
}

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
			level.LevelName = beg;
		}
		else if (sc.Compare("partime"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			level.partime=sc.Number;
		}
		else if (sc.Compare("music"))
		{
			bool FS_ChangeMusic(const char * string);

			sc.MustGetStringName("=");
			sc.MustGetString();
			if (!FS_ChangeMusic(sc.String))
			{
				S_ChangeMusic(level.Music, level.musicorder);
			}
		}
		else if (sc.Compare("skyname"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
		
			sky2texture = sky1texture = level.skytexture1 = level.skytexture2 = TexMan.GetTexture (sc.String, FTexture::TEX_Wall, FTextureManager::TEXMAN_Overridable|FTextureManager::TEXMAN_ReturnFirst);
			R_InitSkyMap ();
		}
		else if (sc.Compare("interpic"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			level.info->ExitPic = sc.String;
		}
		else if (sc.Compare("gravity"))
		{
			sc.MustGetStringName("=");
			sc.MustGetNumber();
			level.gravity=sc.Number*8.f;
		}
		else if (sc.Compare("nextlevel"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			level.NextMap = sc.String;
		}
		else if (sc.Compare("nextsecret"))
		{
			sc.MustGetStringName("=");
			sc.MustGetString();
			level.NextSecretMap = sc.String;
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
			FS_EmulateCmd(beg);
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
		int lumpnum=Wads.CheckNumForName("FSGLOBAL");
		if (lumpnum<0) return false;
		lumpsize=Wads.LumpLength(lumpnum);
		if (lumpsize==0) return false;
		fsglobal=true;
		lump=new char[lumpsize+3];
		Wads.ReadLump(lumpnum,lump);
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
		new DFraggleThinker;
		DFraggleThinker::ActiveThinker->LevelScript->data = copystring(scriptsrc.GetChars());

		if (drownflag==-1) drownflag = (level.maptype != MAPTYPE_DOOM || fsglobal);
		if (!drownflag) level.airsupply=0;	// Legacy doesn't to water damage so we need to check if it has to be disabled here.

		FFsOptions *opt = level.info->GetOptData<FFsOptions>("fragglescript", false);
		if (opt != NULL)
		{
			DFraggleThinker::ActiveThinker->nocheckposition = opt->nocheckposition;
		}
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

void T_LoadScripts(MapData *map)
{
	FScriptLoader parser;
	
	T_Init();

	bool HasScripts = parser.ParseInfo(map);

	// Hack for Legacy compatibility: Since 272 is normally an MBF sky transfer we have to patch it.
	// It could be done with an additional translator but that would be sub-optimal for the user.
	// To handle this the default translator defines the proper Legacy type at index 270.
	// This code then then swaps 270 and 272 - but only if this is either Doom or Heretic and 
	// the default translator is being used.
	// Custom translators will not be patched.
	if ((gameinfo.gametype == GAME_Doom || gameinfo.gametype == GAME_Heretic) && level.info->Translator.IsEmpty() &&
		level.maptype == MAPTYPE_DOOM && SimpleLineTranslations.Size() > 272 && SimpleLineTranslations[272 - 2*HasScripts].special == FS_Execute)
	{
		FLineTrans t = SimpleLineTranslations[270];
		SimpleLineTranslations[270] = SimpleLineTranslations[272];
		SimpleLineTranslations[272] = t;
	}
}


//-----------------------------------------------------------------------------
//
// Adds an actor to the list of spawned things
//
//-----------------------------------------------------------------------------

void T_AddSpawnedThing(AActor * ac)
{
	if (DFraggleThinker::ActiveThinker)
	{
		TArray<TObjPtr<AActor> > &SpawnedThings = DFraggleThinker::ActiveThinker->SpawnedThings;
		SpawnedThings.Push(GC::ReadBarrier(ac));
	}
}
