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
#include "p_tags.h"
#include "r_state.h"
#include "w_wad.h"
#include "textures.h"
#include "g_levellocals.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

struct FCompatOption
{
	const char *Name;
	uint32_t CompatFlags;
	int WhichSlot;
};

enum
{
	SLOT_COMPAT,
	SLOT_COMPAT2,
	SLOT_BCOMPAT
};

enum
{
	CP_END,
	CP_CLEARFLAGS,
	CP_SETFLAGS,
	CP_SETSPECIAL,
	CP_CLEARSPECIAL,
	CP_SETACTIVATION,
	CP_SETSECTOROFFSET,
	CP_SETSECTORSPECIAL,
	CP_SETWALLYSCALE,
	CP_SETWALLTEXTURE,
	CP_SETTHINGZ,
	CP_SETTAG,
	CP_SETTHINGFLAGS,
	CP_SETVERTEX,
	CP_SETTHINGSKILLS,
	CP_SETSECTORTEXTURE,
	CP_SETSECTORLIGHT,
	CP_SETLINESECTORREF,
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------
extern TArray<FMapThing> MapThingsConverted;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

TMap<FMD5Holder, FCompatValues, FMD5HashTraits> BCompatMap;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static FCompatOption Options[] =
{
	{ "setslopeoverflow",		BCOMPATF_SETSLOPEOVERFLOW, SLOT_BCOMPAT },
	{ "resetplayerspeed",		BCOMPATF_RESETPLAYERSPEED, SLOT_BCOMPAT },
	{ "vileghosts",				BCOMPATF_VILEGHOSTS, SLOT_BCOMPAT },
	{ "ignoreteleporttags",		BCOMPATF_BADTELEPORTERS, SLOT_BCOMPAT },
	{ "rebuildnodes",			BCOMPATF_REBUILDNODES, SLOT_BCOMPAT },
	{ "linkfrozenprops",		BCOMPATF_LINKFROZENPROPS, SLOT_BCOMPAT },
	{ "floatbob",				BCOMPATF_FLOATBOB, SLOT_BCOMPAT },
	{ "noslopeid",				BCOMPATF_NOSLOPEID, SLOT_BCOMPAT },
	{ "clipmidtex",				BCOMPATF_CLIPMIDTEX, SLOT_BCOMPAT },

	// list copied from g_mapinfo.cpp
	{ "shorttex",				COMPATF_SHORTTEX, SLOT_COMPAT },
	{ "stairs",					COMPATF_STAIRINDEX, SLOT_COMPAT },
	{ "limitpain",				COMPATF_LIMITPAIN, SLOT_COMPAT },
	{ "nopassover",				COMPATF_NO_PASSMOBJ, SLOT_COMPAT },
	{ "notossdrops",			COMPATF_NOTOSSDROPS, SLOT_COMPAT },
	{ "useblocking", 			COMPATF_USEBLOCKING, SLOT_COMPAT },
	{ "nodoorlight",			COMPATF_NODOORLIGHT, SLOT_COMPAT },
	{ "ravenscroll",			COMPATF_RAVENSCROLL, SLOT_COMPAT },
	{ "soundtarget",			COMPATF_SOUNDTARGET, SLOT_COMPAT },
	{ "dehhealth",				COMPATF_DEHHEALTH, SLOT_COMPAT },
	{ "trace",					COMPATF_TRACE, SLOT_COMPAT },
	{ "dropoff",				COMPATF_DROPOFF, SLOT_COMPAT },
	{ "boomscroll",				COMPATF_BOOMSCROLL, SLOT_COMPAT },
	{ "invisibility",			COMPATF_INVISIBILITY, SLOT_COMPAT },
	{ "silentinstantfloors",	COMPATF_SILENT_INSTANT_FLOORS, SLOT_COMPAT },
	{ "sectorsounds",			COMPATF_SECTORSOUNDS, SLOT_COMPAT },
	{ "missileclip",			COMPATF_MISSILECLIP, SLOT_COMPAT },
	{ "crossdropoff",			COMPATF_CROSSDROPOFF, SLOT_COMPAT },
	{ "wallrun",				COMPATF_WALLRUN, SLOT_COMPAT },		// [GZ] Added for CC MAP29
	{ "anybossdeath",			COMPATF_ANYBOSSDEATH, SLOT_COMPAT },// [GZ] Added for UAC_DEAD
	{ "mushroom",				COMPATF_MUSHROOM, SLOT_COMPAT },
	{ "mbfmonstermove",			COMPATF_MBFMONSTERMOVE, SLOT_COMPAT },
	{ "corpsegibs",				COMPATF_CORPSEGIBS, SLOT_COMPAT },
	{ "noblockfriends",			COMPATF_NOBLOCKFRIENDS, SLOT_COMPAT },
	{ "spritesort",				COMPATF_SPRITESORT, SLOT_COMPAT },
	{ "hitscan",				COMPATF_HITSCAN, SLOT_COMPAT },
	{ "lightlevel",				COMPATF_LIGHT, SLOT_COMPAT },
	{ "polyobj",				COMPATF_POLYOBJ, SLOT_COMPAT },
	{ "maskedmidtex",			COMPATF_MASKEDMIDTEX, SLOT_COMPAT },
	{ "badangles",				COMPATF2_BADANGLES, SLOT_COMPAT2 },
	{ "floormove",				COMPATF2_FLOORMOVE, SLOT_COMPAT2 },
	{ "soundcutoff",			COMPATF2_SOUNDCUTOFF, SLOT_COMPAT2 },
	{ "pointonline",			COMPATF2_POINTONLINE, SLOT_COMPAT2 },
	{ "multiexit",				COMPATF2_MULTIEXIT, SLOT_COMPAT2 },
	{ "teleport",				COMPATF2_TELEPORT, SLOT_COMPAT2 },
	{ "disablepushwindowcheck",	COMPATF2_PUSHWINDOW, SLOT_COMPAT2 },
	{ NULL, 0, 0 }
};

static const char *const LineSides[] =
{
	"Front", "Back", NULL
};

static const char *const WallTiers[] =
{
	"Top", "Mid", "Bot", NULL
};

static const char *const SectorPlanes[] =
{
	"floor", "ceil", NULL
};

static TArray<int> CompatParams;
static int ii_compatparams;
static TArray<FString> TexNames;

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

	BCompatMap.Clear();
	CompatParams.Clear();

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
		memset(flags.CompatFlags, 0, sizeof(flags.CompatFlags));
		flags.ExtCommandIndex = ~0u;
		while (sc.GetString())
		{
			if ((i = sc.MatchString(&Options[0].Name, sizeof(*Options))) >= 0)
			{
				flags.CompatFlags[Options[i].WhichSlot] |= Options[i].CompatFlags;
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
				for (int i = 0; i < 5; i++)
				{
					sc.MustGetNumber();
					CompatParams.Push(sc.Number);
				}
			}
			else if (sc.Compare("setlinesectorref"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETLINESECTORREF);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(LineSides));
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				flags.CompatFlags[SLOT_BCOMPAT] |= BCOMPATF_REBUILDNODES;
			}
			else if (sc.Compare("clearlinespecial"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_CLEARSPECIAL);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setactivation"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETACTIVATION);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setsectoroffset"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETSECTOROFFSET);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(SectorPlanes));
				sc.MustGetFloat();
				CompatParams.Push(int(sc.Float*65536.));
			}
			else if (sc.Compare("setsectorspecial"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETSECTORSPECIAL);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setwallyscale"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETWALLYSCALE);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(LineSides));
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(WallTiers));
				sc.MustGetFloat();
				CompatParams.Push(int(sc.Float*65536.));
			}
			else if (sc.Compare("setwalltexture"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETWALLTEXTURE);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(LineSides));
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(WallTiers));
				sc.MustGetString();
				const FString texName = sc.String;
				const unsigned int texIndex = TexNames.Find(texName);
				const unsigned int texCount = TexNames.Size();
				if (texIndex == texCount)
				{
					TexNames.Push(texName);
				}
				CompatParams.Push(texIndex);
			}
			else if (sc.Compare("setthingz"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETTHINGZ);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetFloat();
				CompatParams.Push(int(sc.Float*256));	// do not use full fixed here so that it can eventually handle larger levels
			}
			else if (sc.Compare("setsectortag"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETTAG);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setthingflags"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETTHINGFLAGS);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setvertex"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETVERTEX);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetFloat();
				CompatParams.Push(int(sc.Float * 256));	// do not use full fixed here so that it can eventually handle larger levels
				sc.MustGetFloat();
				CompatParams.Push(int(sc.Float * 256));	// do not use full fixed here so that it can eventually handle larger levels
				flags.CompatFlags[SLOT_BCOMPAT] |= BCOMPATF_REBUILDNODES;
			}
			else if (sc.Compare("setthingskills"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETTHINGSKILLS);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
			}
			else if (sc.Compare("setsectortexture"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETSECTORTEXTURE);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetString();
				CompatParams.Push(sc.MustMatchString(SectorPlanes));
				sc.MustGetString();
				const FString texName = sc.String;
				const unsigned int texIndex = TexNames.Find(texName);
				const unsigned int texCount = TexNames.Size();
				if (texIndex == texCount)
				{
					TexNames.Push(texName);
				}
				CompatParams.Push(texIndex);
			}
			else if (sc.Compare("setsectorlight"))
			{
				if (flags.ExtCommandIndex == ~0u) flags.ExtCommandIndex = CompatParams.Size();
				CompatParams.Push(CP_SETSECTORLIGHT);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
				sc.MustGetNumber();
				CompatParams.Push(sc.Number);
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

	ii_compatflags = 0;
	ii_compatflags2 = 0;
	ib_compatflags = 0;
	ii_compatparams = -1;

	// When playing Doom IWAD levels force COMPAT_SHORTTEX and COMPATF_LIGHT.
	// I'm not sure if the IWAD maps actually need COMPATF_LIGHT but it certainly does not hurt.
	// TNT's MAP31 also needs COMPATF_STAIRINDEX but that only gets activated for TNT.WAD.
	if (Wads.GetLumpFile(map->lumpnum) == Wads.GetIwadNum() && (gameinfo.flags & GI_COMPATSHORTTEX) && level.maptype == MAPTYPE_DOOM)
	{
		ii_compatflags = COMPATF_SHORTTEX|COMPATF_LIGHT;
		if (gameinfo.flags & GI_COMPATSTAIRS) ii_compatflags |= COMPATF_STAIRINDEX;
	}

	map->GetChecksum(md5.Bytes);

	flags = BCompatMap.CheckKey(md5);

	if (developer >= DMSG_NOTIFY)
	{
		Printf("MD5 = ");
		for (size_t j = 0; j < sizeof(md5.Bytes); ++j)
		{
			Printf("%02X", md5.Bytes[j]);
		}
		if (flags != NULL)
		{
			Printf(", cflags = %08x, cflags2 = %08x, bflags = %08x\n",
				flags->CompatFlags[SLOT_COMPAT], flags->CompatFlags[SLOT_COMPAT2], flags->CompatFlags[SLOT_BCOMPAT]);
		}
		else
		{
			Printf("\n");
		}
	}

	if (flags != NULL)
	{
		ii_compatflags |= flags->CompatFlags[SLOT_COMPAT];
		ii_compatflags2 |= flags->CompatFlags[SLOT_COMPAT2];
		ib_compatflags |= flags->CompatFlags[SLOT_BCOMPAT];
		ii_compatparams = flags->ExtCommandIndex;
	}

	// Reset i_compatflags
	compatflags.Callback();
	compatflags2.Callback();
	// Set floatbob compatibility for all maps with an original Hexen MAPINFO.
	if (level.flags2 & LEVEL2_HEXENHACK)
	{
		ib_compatflags |= BCOMPATF_FLOATBOB;
	}
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

		while (i < CompatParams.Size() && CompatParams[i] != CP_END)
		{
			switch (CompatParams[i])
			{
				case CP_CLEARFLAGS:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i+1]];
						line->flags &= ~CompatParams[i+2];
					}
					i+=3;
					break;
				}
				case CP_SETFLAGS:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i+1]];
						line->flags |= CompatParams[i+2];
					}
					i+=3;
					break;
				}
				case CP_SETSPECIAL:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i+1]];
						line->special = CompatParams[i+2];
						for(int ii=0;ii<5;ii++)
						{
							line->args[ii] = CompatParams[i+ii+3];
						}
					}
					i+=8;
					break;
				}
				case CP_CLEARSPECIAL:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i+1]];
						line->special = 0;
						memset(line->args, 0, sizeof(line->args));
					}
					i += 2;
					break;
				}
				case CP_SETACTIVATION:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i+1]];
						line->activation = CompatParams[i+2];
					}
					i += 3;
					break;
				}
				case CP_SETSECTOROFFSET:
				{
					if ((unsigned)CompatParams[i+1] < level.sectors.Size())
					{
						sector_t *sec = &level.sectors[CompatParams[i+1]];
						const double delta = CompatParams[i + 3] / 65536.0;
						secplane_t& plane = sector_t::floor == CompatParams[i + 2] 
							? sec->floorplane 
							: sec->ceilingplane;
						plane.ChangeHeight(delta);
						sec->ChangePlaneTexZ(CompatParams[i + 2], delta);
					}
					i += 4;
					break;
				}
				case CP_SETSECTORSPECIAL:
				{
					const unsigned index = CompatParams[i + 1];
					if (index < level.sectors.Size())
					{
						level.sectors[index].special = CompatParams[i + 2];
					}
					i += 3;
					break;
				}
				case CP_SETWALLYSCALE:
				{
					if ((unsigned)CompatParams[i+1] < level.lines.Size())
					{
						side_t *side = level.lines[CompatParams[i+1]].sidedef[CompatParams[i+2]];
						if (side != NULL)
						{
							side->SetTextureYScale(CompatParams[i+3], CompatParams[i+4] / 65536.);
						}
					}
					i += 5;
					break;
				}
				case CP_SETWALLTEXTURE:
				{
					if ((unsigned)CompatParams[i + 1] < level.lines.Size())
					{
						side_t *side = level.lines[CompatParams[i + 1]].sidedef[CompatParams[i + 2]];
						if (side != NULL)
						{
							assert(TexNames.Size() > (unsigned int)CompatParams[i + 4]);
							const FTextureID texID = TexMan.GetTexture(TexNames[CompatParams[i + 4]], ETextureType::Any);
							side->SetTexture(CompatParams[i + 3], texID);
						}
					}
					i += 5;
					break;
				}
				case CP_SETTHINGZ:
				{
					// When this is called, the things haven't been spawned yet so we can alter the position inside the MapThings array.
					if ((unsigned)CompatParams[i+1] < MapThingsConverted.Size())
					{
						MapThingsConverted[CompatParams[i+1]].pos.Z = CompatParams[i+2]/256.;
					}
					i += 3;
					break;
				}	
				case CP_SETTAG:
				{
					if ((unsigned)CompatParams[i + 1] < level.sectors.Size())
					{
						// this assumes that the sector does not have any tags yet!
						if (CompatParams[i + 2] == 0)
						{
							tagManager.RemoveSectorTags(CompatParams[i + 1]);
						}
						else
						{
							tagManager.AddSectorTag(CompatParams[i + 1], CompatParams[i + 2]);
						}
					}
					i += 3;
					break;
				}
				case CP_SETTHINGFLAGS:
				{
					if ((unsigned)CompatParams[i + 1] < MapThingsConverted.Size())
					{
						MapThingsConverted[CompatParams[i + 1]].flags = CompatParams[i + 2];
					}
					i += 3;
					break;
				}
				case CP_SETVERTEX:
				{
					if ((unsigned)CompatParams[i + 1] < level.vertexes.Size())
					{
						level.vertexes[CompatParams[i + 1]].p.X = CompatParams[i + 2] / 256.;
						level.vertexes[CompatParams[i + 1]].p.Y = CompatParams[i + 3] / 256.;
					}
					i += 4;
					break;
				}
				case CP_SETTHINGSKILLS:
				{
					if ((unsigned)CompatParams[i + 1] < MapThingsConverted.Size())
					{
						MapThingsConverted[CompatParams[i + 1]].SkillFilter = CompatParams[i + 2];
					}
					i += 3;
					break;
				}
				case CP_SETSECTORTEXTURE:
				{
					if ((unsigned)CompatParams[i + 1] < level.sectors.Size())
					{
						sector_t *sec = &level.sectors[CompatParams[i+1]];
						assert (sec != nullptr);
						secplane_t& plane = sector_t::floor == CompatParams[i + 2] 
							? sec->floorplane 
							: sec->ceilingplane;
						assert(TexNames.Size() > (unsigned int)CompatParams[i + 3]);
						const FTextureID texID = TexMan.GetTexture(TexNames[CompatParams[i + 3]], ETextureType::Any);

						sec->SetTexture(CompatParams[i + 2], texID);
					}
					i += 4;
					break;
				}
				case CP_SETSECTORLIGHT:
				{
					if ((unsigned)CompatParams[i + 1] < level.sectors.Size())
					{
						sector_t *sec = &level.sectors[CompatParams[i+1]];
						assert (sec != nullptr);

						sec->SetLightLevel((unsigned)CompatParams[i + 2]);
					}
					i += 3;
					break;
				}
				case CP_SETLINESECTORREF:
				{
					if ((unsigned)CompatParams[i + 1] < level.lines.Size())
					{
						line_t *line = &level.lines[CompatParams[i + 1]];
						assert(line != nullptr);
						side_t *side = line->sidedef[CompatParams[i + 2]];
						if (side != nullptr && (unsigned)CompatParams[i + 3] < level.sectors.Size())
						{
							side->sector = &level.sectors[CompatParams[i + 3]];
						}
					}
					i += 4;
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
	uint8_t cksum[16];

	if (argv.argc() < 2)
	{
		Printf("Usage: mapchecksum <map> ...\n");
	}
	for (int i = 1; i < argv.argc(); ++i)
	{
		map = P_OpenMapData(argv[i], true);
		if (map == NULL)
		{
			Printf("Cannot load %s as a map\n", argv[i]);
		}
		else
		{
			map->GetChecksum(cksum);
			const char *wadname = Wads.GetWadName(Wads.GetLumpFile(map->lumpnum));
			delete map;
			for (size_t j = 0; j < sizeof(cksum); ++j)
			{
				Printf("%02X", cksum[j]);
			}
			Printf(" // %s %s\n", wadname, argv[i]);
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
	Printf("%08x %08x %08x\n", ii_compatflags, ii_compatflags2, ib_compatflags);
}
