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
#include "p_lnspec.h"
#include "p_setup.h"


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
	fixed_t alpha;
	DWORD flags;
	DWORD activation;
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
	DWORD damageflags;
	DWORD damageflagsAdd;
	DWORD damageflagsRemove;

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
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		EDFLinedef ld;
		bool argsset = false;

		memset(&ld, 0, sizeof(ld));
		ld.alpha = FRACUNIT;
		sc.MustGetString();
		if (sc.Compare("recordnum"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			ld.recordnum = sc.Number;
		}
		else if (sc.Compare("tag"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			ld.tag = sc.Number;
		}
		else if (sc.Compare("id"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			ld.id = sc.Number;
		}
		else if (sc.Compare("special"))
		{
			sc.CheckString("=");
			if (sc.CheckNumber())
			{
				// Oh joy, this is going to be fun...
				// Here we cannot do anything because we need the tag to make this work. 
				// For now just store a negative number.
				ld.special = -sc.Number;
			}
			else
			{
				sc.MustGetString();
				ld.special = P_FindLineSpecial(sc.String);
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
				ld.args[c++] = sc.Number;

			}
			argsset = true;
		}
		else if (sc.Compare("alpha"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			ld.alpha = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("options"))
		{
			// these are needed to build the proper activation mask out of the possible flags which do not match ZDoom 1:1.
			DWORD actmethod = 0;
			DWORD acttype = 0;
			do
			{
				sc.MustGetString();
				for (const char *tok = strtok(sc.String, ",+ \t"); tok != NULL; tok = strtok(NULL, ",+ \t"))
				{
					if (!stricmp(tok, "USE")) actmethod |= SPAC_Use | SPAC_MUse;
					else if (!stricmp(tok, "CROSS")) actmethod |= SPAC_Cross | SPAC_MCross | SPAC_PCross;
					else if (!stricmp(tok, "IMPACT")) ld.activation |= SPAC_Impact;
					else if (!stricmp(tok, "PUSH")) actmethod |= SPAC_Push;
					else if (!stricmp(tok, "PLAYER")) acttype |= SPAC_Use | SPAC_Cross | SPAC_Push;
					else if (!stricmp(tok, "MONSTER")) acttype |= SPAC_MUse | SPAC_MCross | SPAC_MPush;
					else if (!stricmp(tok, "MISSILE")) acttype |= SPAC_PCross;
					else if (!stricmp(tok, "REPEAT")) ld.flags |= ML_REPEAT_SPECIAL;
					else if (!stricmp(tok, "1SONLY")) ld.flags |= ML_FIRSTSIDEONLY;
					else if (!stricmp(tok, "ADDITIVE")) ld.flags |= ML_ADDTRANS;
					else if (!stricmp(tok, "BLOCKALL")) ld.flags |= ML_BLOCKEVERYTHING;
					else if (!stricmp(tok, "ZONEBOUNDARY")) ld.flags |= ML_ZONEBOUNDARY;
					else if (!stricmp(tok, "CLIPMIDTEX")) ld.flags |= ML_CLIP_MIDTEX;
					else sc.ScriptError("Unknown option '%s'", tok);
				}
			} while (sc.CheckString("|"));	// Unquoted strings with '|' separator - parse as a separate string in the loop.

			// and finally we must mask in the activation method
			ld.activation |= (actmethod & acttype);
		}
		else
		{
			sc.ScriptError("Unknown property '%s'", sc.String);
		}
		if (ld.tag == 0) ld.tag = ld.id;	// urgh...
		if (ld.special < 0)	// translate numeric specials.
		{
			line_t line;
			maplinedef_t mld;
			mld.special = -ld.special;
			mld.tag = ld.tag;
			P_TranslateLineDef(&line, &mld);
			ld.special = line.special;
			if (!argsset) memcpy(ld.args, line.args, sizeof(ld.args));
		}
	}
}

static void parseSector(FScanner &sc)
{
	sc.MustGetStringName("{");
	while (!sc.CheckString("}"))
	{
		EDFSector sec;

		memset(&sec, 0, sizeof(sec));
		sec.overlayalpha[sector_t::floor] = sec.overlayalpha[sector_t::ceiling] = FRACUNIT;
		sc.MustGetString();
		if (sc.Compare("recordnum"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			sec.recordnum = sc.Number;
		}
		else if (sc.Compare("flags"))
		{
			DWORD *flagvar = NULL;
			if (sc.CheckString("."))
			{
				sc.MustGetString();
				if (sc.Compare("add"))
				{
					flagvar = &sec.flagsAdd;
				}
				else if (sc.Compare("remove"))
				{
					flagvar = &sec.flagsRemove;
				}
				else
				{
					sc.ScriptError("Invalid property  'flags.%s'", sc.String);
				}
			}
			else
			{
				flagvar = &sec.flags;
			}
			do
			{
				sc.MustGetString();
				for (const char *tok = strtok(sc.String, ",+ \t"); tok != NULL; tok = strtok(NULL, ",+ \t"))
				{
					if (!stricmp(tok, "SECRET")) *flagvar |= SECF_SECRET;
					else if (!stricmp(tok, "FRICTION")) *flagvar |= SECF_FRICTION;
					else if (!stricmp(tok, "PUSH")) *flagvar |= SECF_PUSH;
					else if (!stricmp(tok, "KILLSOUND")) *flagvar |= SECF_SILENT;
					else if (!stricmp(tok, "KILLMOVESOUND")) *flagvar |= SECF_SILENTMOVE;
					else sc.ScriptError("Unknown option '%s'", tok);
				}
			} while (sc.CheckString("|"));	// Unquoted strings with '|' separator - parse as a separate string in the loop.
		}
		else if (sc.Compare("damage"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			sec.damageamount = sc.Number;
		}
		else if (sc.Compare("damagemask"))
		{
			sc.CheckString("=");
			sc.MustGetNumber();
			sec.damageinterval = sc.Number;
		}
		else if (sc.Compare("damageflags"))
		{
			DWORD *flagvar = NULL;
			if (sc.CheckString("."))
			{
				sc.MustGetString();
				if (sc.Compare("add"))
				{
					flagvar = &sec.damageflagsAdd;
				}
				else if (sc.Compare("remove"))
				{
					flagvar = &sec.damageflagsRemove;
				}
				else
				{
					sc.ScriptError("Invalid property  'flags.%s'", sc.String);
				}
			}
			else
			{
				flagvar = &sec.damageflags;
			}
			do
			{
				sc.MustGetString();
				for (const char *tok = strtok(sc.String, ",+ \t"); tok != NULL; tok = strtok(NULL, ",+ \t"))
				{
					if (!stricmp(tok, "LEAKYSUIT")) *flagvar |= 1;
					else if (!stricmp(tok, "IGNORESUIT")) *flagvar |= 2;	// these first 2 bits will be used to set 'leakychance', but this can only be done when the sector gets initialized
					else if (!stricmp(tok, "ENDGODMODE")) *flagvar |= SECF_ENDGODMODE;
					else if (!stricmp(tok, "ENDLEVEL")) *flagvar |= SECF_ENDLEVEL;
					else if (!stricmp(tok, "TERRAINHIT")) *flagvar |= SECF_DMGTERRAINFX;
					else sc.ScriptError("Unknown option '%s'", tok);
				}
			} while (sc.CheckString("|"));	// Unquoted strings with '|' separator - parse as a separate string in the loop.
		}
		else if (sc.Compare("floorterrain"))
		{
			sc.CheckString("=");
			sc.MustGetString();
			// ZDoom does not implement this yet.
		}
		else if (sc.Compare("floorangle"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::floor].angle = angle_t(sc.Float * ANGLE_90 / 90.);
		}
		else if (sc.Compare("flooroffsetx"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::floor].xoffs = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("flooroffsety"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::floor].yoffs = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("ceilingangle"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::ceiling].angle = angle_t(sc.Float * ANGLE_90 / 90.);
		}
		else if (sc.Compare("ceilingoffsetx"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::ceiling].xoffs = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("ceilingoffsety"))
		{
			sc.CheckString("=");
			sc.MustGetFloat();
			sec.planexform[sector_t::ceiling].yoffs = FLOAT2FIXED(sc.Float);
		}
		else if (sc.Compare("colormaptop") || sc.Compare("colormapbottom"))
		{
			sc.CheckString("=");
			sc.MustGetString();
			// not implemented by ZDoom
		}
		else if (sc.Compare("colormapmid"))
		{
			sc.CheckString("=");
			sc.MustGetString();
			// the colormap should be analyzed and converted into an RGB color value.
		}
		else if (sc.Compare("overlayalpha"))
		{
			sc.MustGetStringName(".");
			sc.MustGetString();
			if (sc.Compare("floor"))
			{
				sc.MustGetFloat();
				sec.overlayalpha[sector_t::floor] = FLOAT2FIXED(sc.Float);
			}
			else if (sc.Compare("ceiling"))
			{
				sc.MustGetFloat();
				sec.overlayalpha[sector_t::floor] = FLOAT2FIXED(sc.Float);
			}
		}
		else if (sc.Compare("portalflags"))
		{
			int dest = 0;
			sc.MustGetStringName(".");
			sc.MustGetString();
			if (sc.Compare("floor")) dest = sector_t::floor;
			else if (sc.Compare("ceiling")) dest = sector_t::ceiling;
			else sc.ScriptError("Unknown portal type '%s'", sc.String);

			do
			{
				sc.MustGetString();
				for (const char *tok = strtok(sc.String, ",+ \t"); tok != NULL; tok = strtok(NULL, ",+ \t"))
				{
					if (!stricmp(tok, "DISABLED")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "NORENDER")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "NOPASS")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "BLOCKSOUND")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "OVERLAY")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "ADDITIVE")) sec.portalflags[dest] |= 0;
					else if (!stricmp(tok, "USEGLOBALTEX")) {}	// not implemented
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