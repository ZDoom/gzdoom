/*
**
** statistics.cpp
** Save game statistics to a file
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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

#include <stdio.h>
#include <time.h>

#include "strnatcmp.h"
#include "gi.h"
#include "g_level.h"
#include "gstrings.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "d_net.h"
#include "g_game.h"
#include "m_png.h"
#include "engineerrors.h"
#include "filesystem.h"
#include "p_local.h"
#include "p_setup.h"
#include "s_sound.h"
#include "cmdlib.h"
#include "p_terrain.h"
#include "decallib.h"
#include "stats.h"
#include "a_sharedglobal.h"
#include "p_lnspec.h"
#include "serializer.h"
#include "g_levellocals.h"

CVAR(Int, savestatistics, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(String, statfile, "zdoomstat.txt", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

//==========================================================================
//
// Global statistics data
//
//==========================================================================

// This struct is used to track statistics data in game
struct OneLevel
{
	int totalkills, killcount;
	int totalitems, itemcount;
	int totalsecrets, secretcount;
	int leveltime;
	FString Levelname;
};

// Current game's statistics
static TArray<OneLevel> LevelData;
static FEpisode *StartEpisode;

// The statistics for one level
struct FLevelStatistics
{
	char info[60];
	short skill;
	short playerclass;
	char name[24];
	int timeneeded;
};

// Statistics for one episode playthrough
struct FSessionStatistics : public FLevelStatistics
{
	TArray<FLevelStatistics> levelstats;
};

// Collected statistics for one episode
struct FStatistics
{
	TArray<FSessionStatistics> stats;
	FString epi_name;
	FString epi_header;
};

// All statistics ever collected
static TArray<FStatistics> EpisodeStatistics;

extern TArray<level_info_t> wadlevelinfos;

//==========================================================================
//
// Initializes statistics data from external file
//
//==========================================================================

static void ParseStatistics(const char *fn, TArray<FStatistics> &statlist)
{
	statlist.Clear();
	try
	{
		FScanner sc;
		if (!sc.OpenFile(fn)) return;

		while (sc.GetString())
		{
			FStatistics &ep_entry = statlist[statlist.Reserve(1)];

			ep_entry.epi_header = sc.String;
			sc.MustGetString();
			ep_entry.epi_name = sc.String;

			sc.MustGetStringName("{");
			while (!sc.CheckString("}"))
			{
				FSessionStatistics &session = ep_entry.stats[ep_entry.stats.Reserve(1)];

				sc.MustGetString();
				sc.MustGetString();
				strncpy(session.name, sc.String, 24);
				sc.MustGetString();
				strncpy(session.info, sc.String, 60);

				int h,m,s;
				sc.MustGetString();
				sscanf(sc.String, "%d:%d:%d", &h, &m, &s);
				session.timeneeded= ((((h*60)+m)*60)+s)*TICRATE;

				sc.MustGetNumber();
				session.skill=sc.Number;
				if (sc.CheckString("{"))
				{
					while (!sc.CheckString("}"))
					{
						FLevelStatistics &lstats = session.levelstats[session.levelstats.Reserve(1)];

						sc.MustGetString();
						strncpy(lstats.name, sc.String, 24);
						sc.MustGetString();
						strncpy(lstats.info, sc.String, 60);

						int h,m,s;
						sc.MustGetString();
						sscanf(sc.String, "%d:%d:%d", &h, &m, &s);
						lstats.timeneeded= ((((h*60)+m)*60)+s)*TICRATE;

						lstats.skill = 0;
					}
				}
			}
		}
	}
	catch(CRecoverableError &)
	{
	}
}


// ====================================================================
//
// Reads the statistics file
//
// ====================================================================

void ReadStatistics()
{
	ParseStatistics(statfile, EpisodeStatistics);
}

// ====================================================================
//
// Saves the statistics file
// Sorting helpers.
//
// ====================================================================

int compare_episode_names(const void *a, const void *b)
{
	FStatistics *A = (FStatistics*)a;
	FStatistics *B = (FStatistics*)b;

	return strnatcasecmp(A->epi_header, B->epi_header);
}

int compare_dates(const void *a, const void *b)
{
	FLevelStatistics *A = (FLevelStatistics*)a;
	FLevelStatistics *B = (FLevelStatistics*)b;
	char *p;

	int aday = strtol(A->name, &p, 10);
	int amonth = strtol(p+1, &p, 10);
	int ayear = strtol(p+1, &p, 10);
	int av = aday + 100 * amonth + 2000*ayear;

	int bday = strtol(B->name, &p, 10);
	int bmonth = strtol(p+1, &p, 10);
	int byear = strtol(p+1, &p, 10);
	int bv = bday + 100 * bmonth + 2000*byear;

	return av-bv;
}


// ====================================================================
//
// Main save routine
//
// ====================================================================

inline int hours(int v) { return v / (60*60*TICRATE); }
inline int minutes(int v) { return (v % (60*60*TICRATE)) / (60*TICRATE); }
inline int seconds(int v) { return (v % (60*TICRATE))/TICRATE; }

static void SaveStatistics(const char *fn, TArray<FStatistics> &statlist)
{
	unsigned int j;

	FileWriter *fw = FileWriter::Open(fn);
	if (fw == nullptr)
	{
		Printf(PRINT_HIGH, "Unable to save statistics to %s\n", fn);
		return;
	}

	qsort(&statlist[0], statlist.Size(), sizeof(statlist[0]), compare_episode_names);
	for(unsigned i=0;i<statlist.Size ();i++)
	{
		FStatistics &ep_stats = statlist[i];

		qsort(&ep_stats.stats[0], ep_stats.stats.Size(), sizeof(ep_stats.stats[0]), compare_dates);

		fw->Printf("%s \"%s\"\n{\n", ep_stats.epi_header.GetChars(), ep_stats.epi_name.GetChars());
		for(j=0;j<ep_stats.stats.Size();j++)
		{
			FSessionStatistics *sst = &ep_stats.stats[j];
			if (sst->info[0]>0)
			{
				fw->Printf("\t%2i. %10s \"%-33s\" %02d:%02d:%02d %i\n", j+1, sst->name, sst->info, 
					hours(sst->timeneeded),	minutes(sst->timeneeded), seconds(sst->timeneeded),	sst->skill);

				TArray<FLevelStatistics> &ls = sst->levelstats;
				if (ls.Size() > 0)
				{
					fw->Printf("\t{\n");

					for(unsigned k=0;k<ls.Size ();k++)
					{
						fw->Printf("\t\t%-8s \"%-33s\" %02d:%02d:%02d\n", ls[k].name, ls[k].info, 
							hours(ls[k].timeneeded), minutes(ls[k].timeneeded), seconds(ls[k].timeneeded));
					}
					fw->Printf("\t}\n");
				}
			}
		}
		fw->Printf("}\n\n");
	}
	delete fw;
}


// ====================================================================
//
// Gets list for current episode
//
// ====================================================================
static FStatistics *GetStatisticsList(TArray<FStatistics> &statlist, const char *section, const char *fullname)
{
	for(unsigned int i=0;i<statlist.Size();i++)
	{
		if (!stricmp(section, statlist[i].epi_header)) 
		{
			return &statlist[i];
		}
	}
	FStatistics * stats = &statlist[statlist.Reserve(1)];
	stats->epi_header = section;
	stats->epi_name = fullname;
	return stats;
}

// ====================================================================
//
// Adds a statistics entry
//
// ====================================================================
static FSessionStatistics *StatisticsEntry(FStatistics *stats, const char *text, int playtime)
{
	FSessionStatistics s;
	time_t clock;
	struct tm *lt;

	time (&clock);
	lt = localtime (&clock);

	if (lt != NULL)
		mysnprintf(s.name, countof(s.name), "%02d.%02d.%04d",lt->tm_mday, lt->tm_mon+1, lt->tm_year+1900);
	else
		strcpy(s.name,"00.00.0000");

	s.skill=G_SkillProperty(SKILLP_ACSReturn);
	strcpy(s.info, text);
	s.timeneeded=playtime;

	stats->stats.Push(s);
	return &stats->stats[stats->stats.Size()-1];
}

// ====================================================================
//
// Adds a statistics entry
//
// ====================================================================
static void LevelStatEntry(FSessionStatistics *es, const char *level, const char *text, int playtime)
{
	FLevelStatistics s;
	time_t clock;
	struct tm *lt;

	time (&clock);
	lt = localtime (&clock);

	strcpy(s.name, level);
	strcpy(s.info, text);
	s.timeneeded=playtime;
	es->levelstats.Push(s);
}



//==========================================================================
//
// STAT_StartNewGame: called when a new game starts. Sets the current episode
//
//==========================================================================

void STAT_StartNewGame(const char *mapname)
{
	LevelData.Clear();
	if (!deathmatch && !multiplayer)
	{
		for(unsigned int j=0;j<AllEpisodes.Size();j++)
		{
			if (!AllEpisodes[j].mEpisodeMap.CompareNoCase(mapname))
			{
				StartEpisode = &AllEpisodes[j];
				return;
			}
		}
	}
	StartEpisode = NULL;
}

//==========================================================================
//
// Store the current level's statistics
//
//==========================================================================

int compare_level_names(const void* a, const void* b)
{
	OneLevel* A = (OneLevel*)a;
	OneLevel* B = (OneLevel*)b;

	return strnatcasecmp(A->Levelname, B->Levelname);
}


static void StoreLevelStats(FLevelLocals *Level)
{
	unsigned int i;

	if (gamestate != GS_LEVEL) return;

	if (!(Level->flags2&LEVEL2_NOSTATISTICS))	// don't consider maps that were excluded from statistics
	{
		for(i=0;i<LevelData.Size();i++)
		{
			if (!LevelData[i].Levelname.CompareNoCase(Level->MapName)) break;
		}
		if (i==LevelData.Size())
		{
			LevelData.Reserve(1);
			LevelData[i].Levelname = Level->MapName;
		}
		LevelData[i].totalkills = Level->total_monsters;
		LevelData[i].killcount = Level->killed_monsters;
		LevelData[i].totalitems = Level->total_items;
		LevelData[i].itemcount = Level->found_items;
		LevelData[i].totalsecrets = Level->total_secrets;
		LevelData[i].secretcount = Level->found_secrets;
		LevelData[i].leveltime = Level->maptime;

		// Check for living monsters. On some maps it can happen
		// that the counter misses some. 
		auto it = Level->GetThinkerIterator<AActor>();
		AActor *ac;
		int mc = 0;

		while ((ac = it.Next()))
		{
			if ((ac->flags & MF_COUNTKILL) && ac->health > 0) mc++;
		}
		if (mc == 0) LevelData[i].killcount = LevelData[i].totalkills;
	}
	// sort level names alphabetically to bring the newly added level to its proper place when playing a hub.
	qsort(&LevelData[0], LevelData.Size(), sizeof(LevelData[0]), compare_level_names);
}

//==========================================================================
//
// STAT_ChangeLevel: called when the level changes or the current statistics are
// requested
//
//==========================================================================

void STAT_ChangeLevel(const char *newl, FLevelLocals *Level)
{
	// record the current level's stats.
	StoreLevelStats(Level);

	level_info_t *thisinfo = Level->info;
	level_info_t *nextinfo = NULL;
	
	if (strncmp(newl, "enDSeQ", 6))
	{
		level_info_t *l = FindLevelInfo (newl);
		nextinfo = l->CheckLevelRedirect ();
		if (nextinfo == NULL) nextinfo = l;
	}

	if (savestatistics == 1)
	{
		if ((nextinfo == NULL || (nextinfo->flags2 & LEVEL2_ENDGAME)) && StartEpisode != NULL)
		{
			// we reached the end of this episode
			int wad = 0;
			MapData * map = P_OpenMapData(StartEpisode->mEpisodeMap, false);
			if (map != NULL)
			{
				wad = fileSystem.GetFileContainer(map->lumpnum);
				delete map;
			}
			const char * name = fileSystem.GetResourceFileName(wad);
			FString section = ExtractFileBase(name) + "." + StartEpisode->mEpisodeMap;
			section.ToUpper();

			const char *ep_name = StartEpisode->mEpisodeName;
			if (*ep_name == '$') ep_name = GStrings(ep_name+1);
			FStatistics *sl = GetStatisticsList(EpisodeStatistics, section, ep_name);

			int statvals[6] = {0,0,0,0,0,0};
			FString infostring;
			int validlevels = LevelData.Size();
			for(unsigned i = 0; i < LevelData.Size(); i++)
			{
				statvals[0] += LevelData[i].killcount;
				statvals[1] += LevelData[i].totalkills;
				statvals[2] += LevelData[i].itemcount;
				statvals[3] += LevelData[i].totalitems;
				statvals[4] += LevelData[i].secretcount;
				statvals[5] += LevelData[i].totalsecrets;
			}

			infostring.Format("%4d/%4d, %4d/%4d, %3d/%3d, %2d", statvals[0], statvals[1], statvals[2], statvals[3], statvals[4], statvals[5], validlevels);
			FSessionStatistics *es = StatisticsEntry(sl, infostring, Level->totaltime);

			for(unsigned i = 0; i < LevelData.Size(); i++)
			{
				FString lsection = LevelData[i].Levelname;
				lsection.ToUpper();
				infostring.Format("%4d/%4d, %4d/%4d, %3d/%3d",
					 LevelData[i].killcount, LevelData[i].totalkills, LevelData[i].itemcount, LevelData[i].totalitems, LevelData[i].secretcount, LevelData[i].totalsecrets);

				LevelStatEntry(es, lsection, infostring, LevelData[i].leveltime);
			}
			SaveStatistics(statfile, EpisodeStatistics);
		}
	}
	else if (savestatistics == 2)	// todo: handle single level statistics.
	{
	}
}


//==========================================================================
//
// saves statistics info to savegames
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, OneLevel &l, OneLevel *def)
{
	if (arc.BeginObject(key))
	{
		arc("totalkills", l.totalkills)
			("killcount", l.killcount)
			("totalitems", l.totalitems)
			("itemcount", l.itemcount)
			("totalsecrets", l.totalsecrets)
			("secretcount", l.secretcount)
			("leveltime", l.leveltime)
			("levelname", l.Levelname)
			.EndObject();
	}
	return arc;
}

void STAT_Serialize(FSerializer &arc)
{
	FString startlevel;
	int i = LevelData.Size();

	if (arc.BeginObject("statistics"))
	{
		if (arc.isReading())
		{
			arc("startlevel", startlevel);
			StartEpisode = NULL;
			for (unsigned int j = 0; j < AllEpisodes.Size(); j++)
			{
				if (!AllEpisodes[j].mEpisodeMap.CompareNoCase(startlevel))
				{
					StartEpisode = &AllEpisodes[j];
					break;
				}
			}
			LevelData.Resize(i);
		}
		else
		{
			if (StartEpisode != NULL) startlevel = StartEpisode->mEpisodeMap;
			arc("startlevel", startlevel);
		}
		arc("levels", LevelData);
		arc.EndObject();
	}
}


//==========================================================================
//
// show statistics
//
//==========================================================================

FString GetStatString()
{
	FString compose;
	for(unsigned i = 0; i < LevelData.Size(); i++)
	{
		OneLevel *l = &LevelData[i];
		compose.AppendFormat("Level %s - Kills: %d/%d - Items: %d/%d - Secrets: %d/%d - Time: %d:%02d\n", 
			l->Levelname.GetChars(), l->killcount, l->totalkills, l->itemcount, l->totalitems, l->secretcount, l->totalsecrets,
			l->leveltime/(60*TICRATE), (l->leveltime/TICRATE)%60);
	}
	return compose;
}

CCMD(printstats)
{
	StoreLevelStats(primaryLevel);	// Refresh the current level's results.
	Printf("%s", GetStatString().GetChars());
}


CCMD(finishgame)
{
	bool gamestatecheck = gamestate == GS_LEVEL || gamestate == GS_CUTSCENE;
	if (!gamestatecheck)
	{
		Printf("Cannot use 'finishgame' while not in a game!\n");
		return;
	}
	// This CCMD simulates an end-of-game action and exists to end mods that never exit their last Level->
	Net_WriteByte(DEM_FINISHGAME);
}

ADD_STAT(statistics)
{
	StoreLevelStats(primaryLevel);	// Refresh the current level's results.
	return GetStatString();
}

ADD_STAT(velocity)
{
	FString compose;
	if (players[consoleplayer].mo != NULL && gamestate == GS_LEVEL) {
		compose.AppendFormat("Current velocity: %.2f\n", players[consoleplayer].mo->Vel.Length());
		compose.AppendFormat("Level %s - Velocity Max: %.2f, Velocity Average: %.2f\n", primaryLevel->MapName.GetChars(), primaryLevel->max_velocity, primaryLevel->avg_velocity);
	}
	return compose;
}
