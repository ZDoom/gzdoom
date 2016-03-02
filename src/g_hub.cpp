/*
** g_hub.cpp
**
** Intermission stats for hubs
**
**---------------------------------------------------------------------------
** Copyright 2005 Christoph Oelckers
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

#include "doomstat.h"
#include "g_hub.h"
#include "tarray.h"
#include "g_level.h"
#include "g_game.h"
#include "gi.h"
#include "files.h"
#include "m_png.h"
#include "gstrings.h"
#include "wi_stuff.h"
#include "farchive.h"


//==========================================================================
//
// Player is leaving the current level
//
//==========================================================================

struct FHubInfo
{
	int			finished_ep;
	
	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	wbplayerstruct_t	plyr[MAXPLAYERS];

	FHubInfo &operator=(const wbstartstruct_t &wbs)
	{
		finished_ep = wbs.finished_ep;
		maxkills = wbs.maxkills;
		maxsecret= wbs.maxsecret;
		maxitems = wbs.maxitems;
		maxfrags = wbs.maxfrags;
		memcpy(plyr, wbs.plyr, sizeof(plyr));
		return *this;
	}
};


static TArray<FHubInfo> hubdata;

void G_LeavingHub(int mode, cluster_info_t * cluster, wbstartstruct_t * wbs)
{
	unsigned int i,j;

	if (cluster->flags & CLUSTER_HUB)
	{
		for(i=0;i<hubdata.Size();i++)
		{
			if (hubdata[i].finished_ep==level.levelnum)
			{
				hubdata[i]=*wbs;
				break;
			}
		}
		if (i==hubdata.Size())
		{
			hubdata[hubdata.Reserve(1)] = *wbs;
		}

		hubdata[i].finished_ep=level.levelnum;
		if (!multiplayer && !deathmatch)
		{
			// The player counters don't work in hubs
			hubdata[i].plyr[0].skills=level.killed_monsters;
			hubdata[i].plyr[0].sitems=level.found_items;
			hubdata[i].plyr[0].ssecret=level.found_secrets;
		}


		if (mode!=FINISH_SameHub)
		{
			wbs->maxkills=wbs->maxitems=wbs->maxsecret=0;
			for(i=0;i<MAXPLAYERS;i++)
			{
				wbs->plyr[i].sitems=wbs->plyr[i].skills=wbs->plyr[i].ssecret=0;
			}

			for(i=0;i<hubdata.Size();i++)
			{
				wbs->maxkills += hubdata[i].maxkills;
				wbs->maxitems += hubdata[i].maxitems;
				wbs->maxsecret += hubdata[i].maxsecret;
				for(j=0;j<MAXPLAYERS;j++)
				{
					wbs->plyr[j].sitems += hubdata[i].plyr[j].sitems;
					wbs->plyr[j].skills += hubdata[i].plyr[j].skills;
					wbs->plyr[j].ssecret += hubdata[i].plyr[j].ssecret;
				}
			}
			if (cluster->ClusterName.IsNotEmpty()) 
			{
				if (cluster->flags & CLUSTER_LOOKUPNAME)
				{
					level.LevelName = GStrings(cluster->ClusterName);
				}
				else
				{
					level.LevelName = cluster->ClusterName;
				}
			}
		}
	}
	if (mode!=FINISH_SameHub) hubdata.Clear();
}

//==========================================================================
//
// Serialize intermission info for hubs
//
//==========================================================================
#define HUBS_ID MAKE_ID('h','u','B','s')

static void G_SerializeHub(FArchive & arc)
{
	int i=hubdata.Size();
	arc << i;
	if (i>0)
	{
		if (arc.IsStoring()) arc.Write(&hubdata[0], i * sizeof(FHubInfo));
		else 
		{
			hubdata.Resize(i);
			arc.Read(&hubdata[0], i * sizeof(FHubInfo));
		}
	}
	else hubdata.Clear();
}

void G_WriteHubInfo (FILE *file)
{
	FPNGChunkArchive arc(file, HUBS_ID);
	G_SerializeHub(arc);
}

void G_ReadHubInfo (PNGHandle *png)
{
	int chunklen;

	if ((chunklen = M_FindPNGChunk (png, HUBS_ID)) != 0)
	{
		FPNGChunkArchive arc (png->File->GetFile(), HUBS_ID, chunklen);
		G_SerializeHub(arc);
	}
}

void G_ClearHubInfo()
{
	hubdata.Clear();
}