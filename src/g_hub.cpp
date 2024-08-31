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
#include "g_level.h"
#include "g_game.h"
#include "m_png.h"
#include "gstrings.h"
#include "wi_stuff.h"
#include "serializer.h"
#include "g_levellocals.h"


//==========================================================================
//
// Player is leaving the current level
//
//==========================================================================

struct FHubInfo
{
	int			levelnum;
	
	int			totalkills;
	int			maxkills;
	int			maxitems;
	int			maxsecret;
	int			maxfrags;

	wbplayerstruct_t	plyr[MAXPLAYERS];

	FHubInfo &operator=(const wbstartstruct_t &wbs)
	{
		levelnum	= wbs.finished_ep;
		totalkills	= wbs.totalkills;
		maxkills	= wbs.maxkills;
		maxsecret	= wbs.maxsecret;
		maxitems	= wbs.maxitems;
		maxfrags	= wbs.maxfrags;
		memcpy(plyr, wbs.plyr, sizeof(plyr));
		return *this;
	}
};


static TArray<FHubInfo> hubdata;

void G_LeavingHub(FLevelLocals *Level, int mode, cluster_info_t * cluster, wbstartstruct_t * wbs)
{
	unsigned int i, j;

	if (cluster->flags & CLUSTER_HUB)
	{
		for (i = 0; i < hubdata.Size(); i++)
		{
			if (hubdata[i].levelnum == Level->levelnum)
			{
				hubdata[i] = *wbs;
				break;
			}
		}
		if (i == hubdata.Size())
		{
			hubdata[hubdata.Reserve(1)] = *wbs;
		}

		hubdata[i].levelnum = Level->levelnum;
		if (!multiplayer && !deathmatch)
		{
			// The player counters don't work in hubs
			hubdata[i].plyr[0].skills = Level->killed_monsters;
			hubdata[i].plyr[0].sitems = Level->found_items;
			hubdata[i].plyr[0].ssecret = Level->found_secrets;
		}


		if (mode != FINISH_SameHub)
		{
			wbs->totalkills = Level->killed_monsters;
			wbs->maxkills = wbs->maxitems = wbs->maxsecret = 0;
			for (i = 0; i < MAXPLAYERS; i++)
			{
				wbs->plyr[i].sitems = wbs->plyr[i].skills = wbs->plyr[i].ssecret = 0;
			}

			for (i = 0; i < hubdata.Size(); i++)
			{
				wbs->maxkills += hubdata[i].maxkills;
				wbs->maxitems += hubdata[i].maxitems;
				wbs->maxsecret += hubdata[i].maxsecret;
				for (j = 0; j < MAXPLAYERS; j++)
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
					wbs->thisname = GStrings.GetString(cluster->ClusterName);
				}
				else
				{
					wbs->thisname = cluster->ClusterName;
					wbs->thisauthor = "";
				}
				wbs->LName0.SetInvalid();	// The level's own name was just invalidated, and so was its name patch.
			}
		}
	}
	if (mode != FINISH_SameHub) hubdata.Clear();
}

//==========================================================================
//
// Serialize intermission info for hubs
//
//==========================================================================

FSerializer &Serialize(FSerializer &arc, const char *key, wbplayerstruct_t &h, wbplayerstruct_t *def)
{
	if (arc.BeginObject(key))
	{
		arc("kills", h.skills)
			("items", h.sitems)
			("secrets", h.ssecret)
			("time", h.stime)
			("fragcount", h.fragcount)
			.Array("frags", h.frags, MAXPLAYERS)
			.EndObject();
	}
	return arc;
}

FSerializer &Serialize(FSerializer &arc, const char *key, FHubInfo &h, FHubInfo *def)
{
	if (arc.BeginObject(key))
	{
		arc("levelnum", h.levelnum)
			("totalkills", h.totalkills)
			("maxkills", h.maxkills)
			("maxitems", h.maxitems)
			("maxsecret", h.maxsecret)
			("maxfrags", h.maxfrags)
			.Array("players", h.plyr, MAXPLAYERS)
			.EndObject();
	}
	return arc;
}

void G_SerializeHub(FSerializer &arc)
{
	arc("hubinfo", hubdata);
}

void G_ClearHubInfo()
{
	hubdata.Clear();
}
