/*
** edf.cpp
** Parses Eternity EDF lumps
**
**---------------------------------------------------------------------------
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
*/

#include "w_wad.h"
#include "m_argv.h"
#include "zstring.h"
#include "sc_man.h"
#include "g_level.h"
#include "doomdata.h"
#include "r_defs.h"
#include "info.h"


struct FEdfOptions : public FOptionalMapinfoData
{
	FEdfOptions()
	{
		identifier = "EDF";
	}
	virtual FOptionalMapinfoData *Clone() const
	{
		FEdfOptions *newopt = new FEdfOptions;
		newopt->identifier = identifier;
		newopt->edfName = edfName;
		newopt->acsName = acsName;
		return newopt;
	}
	FString edfName;
	FString acsName;
};

DEFINE_MAP_OPTION(edf, false)
{
	FEdfOptions *opt = info->GetOptData<FEdfOptions>("EDF");

	parse.ParseAssign();
	parse.sc.MustGetString();
	opt->edfName = parse.sc.String;
}

DEFINE_MAP_OPTION(loadacs, false)
{
	FEdfOptions *opt = info->GetOptData<FEdfOptions>("EDF");

	parse.ParseAssign();
	parse.sc.MustGetString();
	opt->acsName = parse.sc.String;
}

struct EDFMapthing
{
	int recordnum;
	int tid;
	int type;
	fixed_t height;
	int args[5];
	WORD skillfilter;
	DWORD flags;
};

struct EDFLinedef
{
	int recordnum;
	int special;
	int tag;
	int id;
	int args[5];
	float alpha;
	DWORD flags;
};

struct EDFSector
{
	int recordnum;

	DWORD flags;
	DWORD flagsRemove;
	DWORD flagsAdd;

	int damageamount;
	int damageinterval;
	FNameNoInit damagetype;

	// These do not represent any of ZDoom's features. They are maintained like this so that the Add and Remove versions work as intended.
	int damageflags;
	int damageflagsAdd;
	int damageflagsRemove;

	// floorterrain (Type TBD)

	// ceilingterrain is ignored
	// colormaptop/mid/bottom need to be translated into color values (the colormap implementation in Eternity is not the same as in Boom!)

	FTransform planexform[2];
	DWORD portalflags[2];
	fixed_t overlayalpha[2];
};

static TMap<int, EDFLinedef> EDFLines;
static TMap<int, EDFSector> EDFSectors;
static TMap<int, EDFMapthing> EDFThings;


static void parseLinedef(FScanner &sc)
{
}

static void parseSector(FScanner &sc)
{
}

static void parseMapthing(FScanner &sc)
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		EDFMapthing mt;

		memset(&mt, 0, sizeof(mt));
		mt.flags |= MTF_SINGLE | MTF_COOPERATIVE | MTF_DEATHMATCH;	// EDF uses inverse logic, like Doom.exe
		sc.MustGetString();
		if (sc.Compare("recordnum"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			mt.recordnum = sc.Number;
		}
		else if (sc.Compare("tid"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			mt.tid = sc.Number;
		}
		else if (sc.Compare("type"))
		{
			sc.CheckString("=");
			if (sc.CheckNumber())
			{
				mt.type = sc.Number;
			}
			else
			{
				// Class name.
				sc.MustGetString();
				// According to the Eternity Wiki a name may be prefixed with 'thing:'.
				const char *pos = strchr(sc.String, ':');	// Eternity never checks if the prefix actually is 'thing'.
				if (pos) pos++;
				else pos = sc.String;
				const PClass *cls = PClass::FindClass(pos);
				if (cls != NULL)
				{
					FDoomEdMap::Iterator it(DoomEdMap);
					FDoomEdMap::Pair *pair;
					while (it.NextPair(pair))
					{
						if (pair->Value.Type == cls)
						{
							mt.type = pair->Key;
							break;
						}
					}
				}
				else
				{
					 //Let's hope not something internal to Eternity...
					sc.ScriptError("Unknown type '%s'", sc.String);
				}

			}
		}
		else if (sc.Compare("args"))
		{
			sc.CheckString("=");
			sc.MustGetStringName("{");
			int c = 0;
			while (!sc.CheckString("}"))
			{
				sc.MustGetNumber();
				mt.args[c++] = sc.Number;

			}
		}
		else if (sc.Compare("height"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();	// no idea if Eternity allows fractional numbers. Better be safe and do it anyway.
			mt.height = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("options"))
		{
			do
			{
				sc.MustGetString();
				for (const char *tok = strtok(sc.String, ",+ \t"); tok != NULL; tok = strtok(NULL, ",+ \t"))
				{
					if (!stricmp(tok, "EASY")) mt.skillfilter |= 3;
					else if (!stricmp(tok, "NORMAL")) mt.skillfilter |= 4;
					else if (!stricmp(tok, "HARD")) mt.skillfilter |= 24;
					else if (!stricmp(tok, "AMBUSH")) mt.flags |= MTF_AMBUSH;
					else if (!stricmp(tok, "NOTSINGLE")) mt.flags &= ~MTF_SINGLE;
					else if (!stricmp(tok, "NOTDM")) mt.flags &= ~MTF_DEATHMATCH;
					else if (!stricmp(tok, "NOTCOOP")) mt.flags &= ~MTF_COOPERATIVE;
					else if (!stricmp(tok, "FRIEND")) mt.flags |= MTF_FRIENDLY;
					else if (!stricmp(tok, "DORMANT")) mt.flags |= MTF_DORMANT;
					else sc.ScriptError("Unknown option '%s'", tok);
				}
			} while (sc.CheckString("|"));	// Unquoted strings with '|' separator - parse as a separate string in the loop.
		}
		else
		{
			sc.ScriptError("Unknown property '%s'", sc.String);
		}
	}
}

void loadEDF()
{
	FString filename;
	FScanner sc;

	EDFLines.Clear();
	EDFSectors.Clear();
	EDFThings.Clear();

	const char *arg = Args->CheckValue("-edf");

	if (arg != NULL) filename = arg;
	else
	{
		FEdfOptions *opt = level.info->GetOptData<FEdfOptions>("EDF", false);
		if (opt != NULL)
		{
			filename = opt->edfName;
		}
	}

	if (filename.IsEmpty()) return;
	int lump = Wads.CheckNumForFullName(filename, true, ns_global);
	if (lump == -1) return;
	sc.OpenLumpNum(lump);

	sc.SetCMode(true);
	while (sc.GetString())
	{
		if (sc.Compare("linedef"))
		{
			parseLinedef(sc);
		}
		else if (sc.Compare("mapthing"))
		{
			parseMapthing(sc);
		}
		else if (sc.Compare("sector"))
		{
			parseSector(sc);
		}
		else
		{
			sc.ScriptError("Unknown keyword '%s'", sc.String);
		}
	}


}