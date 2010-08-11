/*
** compatibility.cpp
** Handles compatibility flags for maps that are unlikely to be updated.
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
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
** This file is for maps that have been rendered broken by bug fixes or other
** changes that seemed minor at the time, and it is unlikely that the maps
** will be changed. If you are making a map and you know it needs a
** compatibility option to play properly, you are advised to specify so with
** a MAPINFO.
*/

// HEADER FILES ------------------------------------------------------------

#include "compatibility.h"
#include "sc_man.h"
#include "cmdlib.h"
#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "c_dispatch.h"
#include "gi.h"
#include "g_level.h"
#include "p_lnspec.h"
#include "r_state.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FCompatOption
{
	const char *Name;
	int CompatFlags;
	int BCompatFlags;
};

enum
{
	CP_END,
	CP_CLEARFLAGS,
	CP_SETFLAGS,
	CP_SETSPECIAL
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TMap<FMD5Holder, FCompatValues, FMD5HashTraits> BCompatMap;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FCompatOption Options[] =
{
	{ "setslopeoverflow",		0, BCOMPATF_SETSLOPEOVERFLOW },
	{ "resetplayerspeed",		0, BCOMPATF_RESETPLAYERSPEED },
	{ "vileghosts",				0, BCOMPATF_VILEGHOSTS },

	// list copied from g_mapinfo.cpp
	{ "shorttex",				COMPATF_SHORTTEX, 0 },
	{ "stairs",					COMPATF_STAIRINDEX, 0 },
	{ "limitpain",				COMPATF_LIMITPAIN, 0 },
	{ "nopassover",				COMPATF_NO_PASSMOBJ, 0 },
	{ "notossdrops",			COMPATF_NOTOSSDROPS, 0 },
	{ "useblocking", 			COMPATF_USEBLOCKING, 0 },
	{ "nodoorlight",			COMPATF_NODOORLIGHT, 0 },
	{ "ravenscroll",			COMPATF_RAVENSCROLL, 0 },
	{ "soundtarget",			COMPATF_SOUNDTARGET, 0 },
	{ "dehhealth",				COMPATF_DEHHEALTH, 0 },
	{ "trace",					COMPATF_TRACE, 0 },
	{ "dropoff",				COMPATF_DROPOFF, 0 },
	{ "boomscroll",				COMPATF_BOOMSCROLL, 0 },
	{ "invisibility",			COMPATF_INVISIBILITY, 0 },
	{ "silentinstantfloors",	COMPATF_SILENT_INSTANT_FLOORS, 0 },
	{ "sectorsounds",			COMPATF_SECTORSOUNDS, 0 },
	{ "missileclip",			COMPATF_MISSILECLIP, 0 },
	{ "crossdropoff",			COMPATF_CROSSDROPOFF, 0 },
	{ "wallrun",				COMPATF_WALLRUN, 0 },		// [GZ] Added for CC MAP29
	{ "anybossdeath",			COMPATF_ANYBOSSDEATH, 0},	// [GZ] Added for UAC_DEAD
	{ "mushroom",				COMPATF_MUSHROOM, 0},
	{ "mbfmonstermove",			COMPATF_MBFMONSTERMOVE, 0 },
	{ "corpsegibs",				COMPATF_CORPSEGIBS, 0 },
	{ "noblockfriends",			COMPATF_NOBLOCKFRIENDS, 0 },
	{ "spritesort",				COMPATF_SPRITESORT, 0 },
	{ "hitscan",				COMPATF_HITSCAN, 0 },
	{ "lightlevel",				COMPATF_LIGHT, 0 },
	{ NULL, 0, 0 }
};

static TArray<int> CompatParams;
static int ii_compatparams;

// CODE --------------------------------------------------------------------

//==========================================================================
//
// ParseCompatibility
//
//==========================================================================

void ParseCompatibility()
{
	TArray<FMD5Holder> md5array;
	FMD5Holder md5;
	FCompatValues flags;
	int i, x;
	unsigned int j;

	// The contents of this file are not cumulative, as it should not
	// be present in user-distributed maps.
	FScanner sc(Wads.GetNumForFullName("compatibility.txt"));

	while (sc.GetString())	// Get MD5 signature
	{
		do
		{
			if (strlen(sc.String) != 32)
			{
				sc.ScriptError("MD5 signature must be exactly 32 characters long");
			}
			for (i = 0; i < 32; ++i)
			{
				if (sc.String[i] >= '0' && sc.String[i] <= '9')
				{
					x = sc.String[i] - '0';
				}
				else
				{
					sc.String[i] |= 'a' ^ 'A';
					if (sc.String[i] >= 'a' && sc.String[i] <= 'f')
					{
						x = sc.String[i] - 'a' + 10;
					}
					else
					{
						x = 0;
						sc.ScriptError("MD5 signature must be a hexadecimal value");
					}
				}
				if (!(i & 1))
				{
					md5.Bytes[i / 2] = x << 4;
				}
				else
				{
					md5.Bytes[i / 2] |= x;
				}
			}
			md5array.Push(md5);
			sc.MustGetString();
		} while (!sc.Compare("{"));
		flags.CompatFlags = 0;
		flags.BCompatFlags = 0;
		flags.ExtCommandIndex = ~0u;
		while (sc.GetString())
		{
			if ((i = sc.MatchString(&Options[0].Name, sizeof(*Options))) >= 0)
			{
				flags.CompatFlags |= Options[i].CompatFlags;
				flags.BCompatFlags |= Options[i].BCompatFlags;
			}
			else if (sc.Compare("clearlineflags"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_CLEARFLAGS);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setlineflags"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETFLAGS);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setlinespecial"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETSPECIAL);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);

				sc.MustGetString();
				CompatParams.Push(P_FindLineSpecial(sc.String, NULL, NULL));
				for(int i=0;i<5;i++)
				{
					sc.MustGetNumber();
					CompatParams.Push(sc.Number);
				}
			}
			else 
			{
				sc.UnGet();
				break;
			}
		}
		if (flags.ExtCommandIndex != ~0u) 
		{
			CompatParams.Push(CP_END);
		}
		sc.MustGetStringName("}");
		for (j = 0; j < md5array.Size(); ++j)
		{
			BCompatMap[md5array[j]] = flags;
		}
		md5array.Clear();
	}
}

//==========================================================================
//
// CheckCompatibility
//
//==========================================================================

void CheckCompatibility(MapData *map)
{
	FMD5Holder md5;
	FCompatValues *flags;

	// When playing Doom IWAD levels force COMPAT_SHORTTEX and COMPATF_LIGHT.
	// I'm not sure if the IWAD maps actually need COMPATF_LIGHT but it certainly does not hurt.
	// TNT's MAP31 also needs COMPATF_STAIRINDEX but that only gets activated for TNT.WAD.
	if (Wads.GetLumpFile(map->lumpnum) == 1 && (gameinfo.flags & GI_COMPATSHORTTEX) && !(level.flags & LEVEL_HEXENFORMAT))
	{
		ii_compatflags = COMPATF_SHORTTEX|COMPATF_LIGHT;
		if (gameinfo.flags & GI_COMPATSTAIRS) ii_compatflags |= COMPATF_STAIRINDEX;
		ib_compatflags = 0;
		ii_compatparams = -1;
	}
	else
	{
		map->GetChecksum(md5.Bytes);

		flags = BCompatMap.CheckKey(md5);

		if (developer)
		{
			Printf("MD5 = ");
			for (size_t j = 0; j < sizeof(md5.Bytes); ++j)
			{
				Printf("%02X", md5.Bytes[j]);
			}
			if (flags != NULL) Printf(", cflags = %08x, bflags = %08x\n", flags->CompatFlags, flags->BCompatFlags);
			else Printf("\n");
		}

		if (flags != NULL)
		{
			ii_compatflags = flags->CompatFlags;
			ib_compatflags = flags->BCompatFlags;
			ii_compatparams = flags->ExtCommandIndex;
		}
		else
		{
			ii_compatflags = 0;
			ib_compatflags = 0;
			ii_compatparams = -1;
		}
	}
	// Reset i_compatflags
	compatflags.Callback();
}

//==========================================================================
//
// SetCompatibilityParams
//
//==========================================================================

void SetCompatibilityParams()
{
	if (ii_compatparams != -1)
	{
		unsigned i = ii_compatparams;

		while (CompatParams[i] != CP_END && i < CompatParams.Size())
		{
			switch (CompatParams[i])
			{
				case CP_CLEARFLAGS:
				{
					if (CompatParams[i+1] < numlines)
					{
						line_t *line = &lines[CompatParams[i+1]];
						line->flags &= ~CompatParams[i+2];
					}
					i+=3;
					break;
				}
				case CP_SETFLAGS:
				{
					if (CompatParams[i+1] < numlines)
					{
						line_t *line = &lines[CompatParams[i+1]];
						line->flags |= CompatParams[i+2];
					}
					i+=3;
					break;
				}
				case CP_SETSPECIAL:
				{
					if (CompatParams[i+1] < numlines)
					{
						line_t *line = &lines[CompatParams[i+1]];
						line->special = CompatParams[i+2];
						for(int ii=0;ii<5;ii++)
						{
							line->args[ii] = CompatParams[i+ii+3];
						}
					}
					i+=8;
					break;
				}
			}
		}
	}
}

//==========================================================================
//
// CCMD mapchecksum
//
//==========================================================================

CCMD (mapchecksum)
{
	MapData *map;
	BYTE cksum[16];

	if (argv.argc() < 2)
	{
		Printf("Usage: mapchecksum <map> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		map = P_OpenMapData(argv[i]);
		if (map == NULL)
		{
			Printf("Cannot load %s as a map\n", argv[i]);
		}
		else
		{
			map->GetChecksum(cksum);
			delete map;
			for (size_t j = 0; j < sizeof(cksum); ++j)
			{
				Printf("%02X", cksum[j]);
			}
			Printf(" // %s\n", argv[i]);
		}
	}
}

//==========================================================================
//
// CCMD hiddencompatflags
//
//==========================================================================

CCMD (hiddencompatflags)
{
	Printf("%08x %08x\n", ii_compatflags, ib_compatflags);
}
