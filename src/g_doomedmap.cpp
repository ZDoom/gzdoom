/*
** g_doomedmap.cpp
**
**---------------------------------------------------------------------------
** Copyright 1998-2015 Randy Heit
** Copyright 2015 Christoph Oelckers
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
**
*/

#include "info.h"
#include "p_lnspec.h"
#include "m_fixed.h"
#include "c_dispatch.h"
#include "templates.h"
#include "cmdlib.h"
#include "g_level.h"
#include "v_text.h"
#include "i_system.h"


const char *SpecialMapthingNames[] = {
	"$Player1Start",
	"$Player2Start",
	"$Player3Start",
	"$Player4Start",
	"$Player5Start",
	"$Player6Start",
	"$Player7Start",
	"$Player8Start",
	"$DeathmatchStart",
	"$SSeqOverride",
	"$PolyAnchor",
	"$PolySpawn",
	"$PolySpawnCrush",
	"$PolySpawnHurt",
	"$SlopeFloorPointLine",
	"$SlopeCeilingPointLine",
	"$SetFloorSlope",
	"$SetCeilingSlope",
	"$VavoomFloor",
	"$VavoomCeiling",
	"$CopyFloorPlane",
	"$CopyCeilingPlane",
	"$VertexFloorZ",
	"$VertexCeilingZ",

};
//==========================================================================
//
// Stuff that's only valid during definition time
//
//==========================================================================

struct MapinfoEdMapItem
{
	FName classname;	// DECORATE is read after MAPINFO so we do not have the actual classes available here yet.
	short special;
	signed char argsdefined;
	int args[5];
	// These are for error reporting. We must store the file information because it's no longer available when these items get resolved.
	FString filename;
	int linenum;
};

typedef TMap<int, MapinfoEdMapItem> IdMap;

static IdMap DoomEdFromMapinfo;

//==========================================================================
//
//
//==========================================================================

FDoomEdMap DoomEdMap;

static int STACK_ARGS sortnums (const void *a, const void *b)
{
	return (*(const FDoomEdMap::Pair**)a)->Key - (*(const FDoomEdMap::Pair**)b)->Key;
}

CCMD (dumpmapthings)
{
	TArray<FDoomEdMap::Pair*> infos(DoomEdMap.CountUsed());
	FDoomEdMap::Iterator it(DoomEdMap);
	FDoomEdMap::Pair *pair;

	while (it.NextPair(pair))
	{
		infos.Push(pair);
	}

	if (infos.Size () == 0)
	{
		Printf ("No map things registered\n");
	}
	else
	{
		qsort (&infos[0], infos.Size (), sizeof(FDoomEdMap::Pair*), sortnums);

		for (unsigned i = 0; i < infos.Size (); ++i)
		{
			if (infos[i]->Value.Type != NULL)
			{
				Printf("%6d %s\n", infos[i]->Key, infos[i]->Value.Type->TypeName.GetChars());
			}
			else if (infos[i]->Value.Special > 0)
			{
				Printf("%6d %s\n", infos[i]->Key, SpecialMapthingNames[infos[i]->Value.Special - 1]);
			}
			else
			{
				Printf("%6d none\n", infos[i]->Key);
			}

		}
	}
}


void FMapInfoParser::ParseDoomEdNums()
{
	TMap<int, bool> defined;
	int error = 0;

	MapinfoEdMapItem editem;

	editem.filename = sc.ScriptName;

	ParseOpenBrace();
	while (true)
	{
		if (sc.CheckString("}")) return;
		else if (sc.CheckNumber())
		{
			int ednum = sc.Number;
			sc.MustGetStringName("=");
			sc.MustGetString();

			bool *def = defined.CheckKey(ednum);
			if (def != NULL)
			{
				sc.ScriptMessage("Editor Number %d defined more than once", ednum);
				error++;
			}
			defined[ednum] = true;
			if (sc.String[0] == '$')
			{
				// todo: add special stuff like playerstarts and sound sequence overrides here, too.
				editem.classname = NAME_None;
				editem.special = sc.MustMatchString(SpecialMapthingNames) + 1; // todo: assign proper constants
			}
			else
			{
				editem.classname = sc.String;
				editem.special = -1;
			}
			memset(editem.args, 0, sizeof(editem.args));
			editem.argsdefined = 0;

			int minargs = 0;
			int maxargs = 5;
			FString specialname;
			if (sc.CheckString(","))
			{
				editem.argsdefined = 5; // mark args as used - if this is done we need to prevent assignment of map args in P_SpawnMapThing.
				if (editem.special < 0) editem.special = 0;
				if (!sc.CheckNumber())
				{
					sc.MustGetString();
					specialname = sc.String;	// save for later error reporting.
					editem.special = P_FindLineSpecial(sc.String, &minargs, &maxargs);
					if (editem.special == 0 || minargs == -1)
					{
						sc.ScriptMessage("Invalid special %s for Editor Number %d", sc.String, ednum);
						error++;
						minargs = 0;
						maxargs = 5;
					}
					if (!sc.CheckString(","))
					{
						// special case: Special without arguments
						if (minargs != 0)
						{
							sc.ScriptMessage("Incorrect number of args for special %s, min = %d, max = %d, found = 0", specialname.GetChars(), minargs, maxargs);
							error++;
						}
						DoomEdFromMapinfo.Insert(ednum, editem);
						continue;
					}
					sc.MustGetNumber();
				}
				int i = 0;
				while (i < 5)
				{
					editem.args[i] = sc.Number;
					i++;
					if (!sc.CheckString(",")) break;
					// special check for the ambient sounds which combine the arg being set here with the ones on the mapthing.
					if (sc.CheckString("+"))
					{
						editem.argsdefined = i;
						break;
					}
					sc.MustGetNumber();

				}
				if (specialname.IsNotEmpty() && (i < minargs || i > maxargs))
				{
					sc.ScriptMessage("Incorrect number of args for special %s, min = %d, max = %d, found = %d", specialname.GetChars(), minargs, maxargs, i);
					error++;
				}
			}
			DoomEdFromMapinfo.Insert(ednum, editem);
		}
		else
		{
			sc.ScriptError("Number expected");
		}
	}
	if (error > 0)
	{
		sc.ScriptError("%d errors encountered in DoomEdNum definition");
	}
}

void InitActorNumsFromMapinfo()
{
	DoomEdMap.Clear();
	IdMap::Iterator it(DoomEdFromMapinfo);
	IdMap::Pair *pair;
	int error = 0;

	while (it.NextPair(pair))
	{
		const PClass *cls = NULL;
		if (pair->Value.classname != NAME_None)
		{
			cls = PClass::FindClass(pair->Value.classname);
			if (cls == NULL)
			{
				Printf(TEXTCOLOR_RED "Script error, \"%s\" line %d:\nUnknown actor class %s\n",
					pair->Value.filename.GetChars(), pair->Value.linenum, pair->Value.classname.GetChars());
				error++;
			}
		}
		FDoomEdEntry ent;
		ent.Type = cls;
		ent.Special = pair->Value.special;
		ent.ArgsDefined = pair->Value.argsdefined;
		memcpy(ent.Args, pair->Value.args, sizeof(ent.Args));
		DoomEdMap.Insert(pair->Key, ent);
	}
	if (error > 0)
	{
		I_Error("%d unknown actor classes found", error);
	}
	DoomEdFromMapinfo.Clear();	// we do not need this any longer
}
