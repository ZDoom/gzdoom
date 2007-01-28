/*
** p_xlat.cpp
** Translate old Doom format maps to the Hexen format
**
**---------------------------------------------------------------------------
** Copyright 1998-2007 Randy Heit
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
** The linedef translations are read from a WAD lump (DOOMX or HERETICX),
** so most of this file's behavior can be easily customized without
** recompiling.
*/

#include "doomtype.h"
#include "g_level.h"
#include "p_lnspec.h"
#include "doomdata.h"
#include "r_data.h"
#include "m_swap.h"
#include "p_spec.h"
#include "p_local.h"
#include "a_sharedglobal.h"
#include "gi.h"
#include "w_wad.h"
#include "sc_man.h"
#include "cmdlib.h"

// define names for the TriggerType field of the general linedefs

typedef enum
{
	WalkOnce,
	WalkMany,
	SwitchOnce,
	SwitchMany,
	GunOnce,
	GunMany,
	PushOnce,
	PushMany,
} triggertype_e;

void P_TranslateLineDef (line_t *ld, maplinedef_t *mld)
{
	static FMemLump tlatebase;
	const BYTE *tlate;
	short special = LittleShort(mld->special);
	short tag = LittleShort(mld->tag);
	DWORD flags = LittleShort(mld->flags);
	INTBOOL passthrough;
	int i;

	if (flags & ML_TRANSLUCENT_STRIFE)
	{
		ld->alpha = 255*3/4;
	}
	if (gameinfo.gametype == GAME_Strife)
	{
		// It might be useful to make these usable by all games.
		// Unfortunately, there aren't enough flag bits left to do that,
		// so they're Strife-only.
		if (flags & ML_RAILING_STRIFE)
		{
			flags |= ML_RAILING;
		}
		if (flags & ML_BLOCK_FLOATERS_STRIFE)
		{
			flags |= ML_BLOCK_FLOATERS;
		}
		passthrough = 0;
	}
	else
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			if (flags & ML_RESERVED_ETERNITY)
			{
				flags &= 0x1FF;
			}
		}
		passthrough = (flags & ML_PASSUSE_BOOM);
	}
	flags = flags & 0xFFFF01FF;	// Ignore flags unknown to DOOM

	// For purposes of maintaining BOOM compatibility, each
	// line also needs to have its ID set to the same as its tag.
	// An external conversion program would need to do this more
	// intelligently.
	ld->id = tag;

	// 0 specials are never translated.
	if (special == 0)
	{
		ld->special = 0;
		ld->flags = flags;
		ld->args[0] = mld->tag;
		memset (ld->args+1, 0, sizeof(ld->args)-sizeof(ld->args[0]));
		return;
	}

	// The translation lump is cached across calls to P_TranslateLineDef.
	if (tlatebase.GetMem() == NULL)
	{
		if (gameinfo.gametype == GAME_Doom)
		{
			tlatebase = Wads.ReadLump ("DOOMX");
		}
		else if (gameinfo.gametype == GAME_Strife)
		{
			tlatebase = Wads.ReadLump ("STRIFEX");
		}
		else
		{
			tlatebase = Wads.ReadLump ("HERETICX");
		}
	}
	tlate = (const BYTE *)tlatebase.GetMem();

	// Check if this is a regular linetype
	if (tlate[0] == 'N' && tlate[1] == 'O' && tlate[2] == 'R' && tlate[3] == 'M')
	{
		int count = (tlate[4] << 8) | tlate[5];
		tlate += 6;
		while (count)
		{
			int low = (tlate[0] << 8) | tlate[1];
			int high = (tlate[2] << 8) | tlate[3];
			tlate += 4;
			if (special >= low && special <= high)
			{ // found it, so use the LUT
				const BYTE *specialmap = tlate + (special - low) * 7;

				ld->flags = flags | ((specialmap[0] & 0x1f) << 9);

				if (passthrough && (GET_SPAC(ld->flags) == SPAC_USE))
				{
					ld->flags &= ~ML_SPAC_MASK;
					ld->flags |= SPAC_USETHROUGH << ML_SPAC_SHIFT;
				}
				ld->special = specialmap[1];
				ld->args[0] = specialmap[2];
				ld->args[1] = specialmap[3];
				ld->args[2] = specialmap[4];
				ld->args[3] = specialmap[5];
				ld->args[4] = specialmap[6];
				switch (specialmap[0] & 0xe0)
				{
				case 7<<5:					// First two arguments are tags
					ld->args[1] = tag;
				case 1<<5: case 6<<5:		// First argument is a tag
					ld->args[0] = tag;
					break;

				case 2<<5:					// Second argument is a tag
					ld->args[1] = tag;
					break;

				case 3<<5:					// Third argument is a tag
					ld->args[2] = tag;
					break;

				case 4<<5:					// Fourth argument is a tag
					ld->args[3] = tag;
					break;

				case 5<<5:					// Fifth argument is a tag
					ld->args[4] = tag;
					break;
				}
				if ((ld->flags & ML_SECRET) && (GET_SPAC(ld->flags) == SPAC_USE || GET_SPAC(ld->flags) == SPAC_USETHROUGH))
				{
					ld->flags &= ~ML_MONSTERSCANACTIVATE;
				}
				return;
			}
			tlate += (high - low + 1) * 7;
			count--;
		}
	}

	// Check if this is a BOOM generalized linetype
	if (tlate[0] == 'B' && tlate[1] == 'O' && tlate[2] == 'O' && tlate[3] == 'M')
	{
		int count = (tlate[4] << 8) | tlate[5];
		tlate += 6;

		// BOOM translators are stored on disk as:
		//
		// WORD <first linetype in range>
		// WORD <last linetype in range>
		// BYTE <new special>
		// repeat [BYTE op BYTES parms]
		//
		// op consists of some bits:
		//
		// 76543210
		// ||||||++-- Dest is arg[(op&3)+1] (arg[0] is always tag)
		// |||||+---- 0 = store, 1 = or with existing value
		// ||||+----- 0 = this is normal, 1 = x-op in next byte
		// ++++------ # of elements in list, or 0 to always use a constant value
		//
		// If a constant value is used, parms is a single byte containing that value.
		// Otherwise, parms has the format:
		//
		// WORD <value to AND with linetype>
		// repeat [WORD <if result is this> BYTE <use this>]
		//
		// These x-ops are defined:
		//
		// 0 = end of this BOOM translator
		// 1 = dest is flags

		while (count)
		{
			int low = (tlate[0] << 8) | tlate[1];
			int high = (tlate[2] << 8) | tlate[3];
			tlate += 4;

			DWORD oflags = flags;

			// Assume we found it and translate
			switch (special & 0x0007)
			{
			case WalkMany:
				flags |= ML_REPEAT_SPECIAL;
			case WalkOnce:
				flags |= SPAC_CROSS << ML_SPAC_SHIFT;
				break;

			case SwitchMany:
			case PushMany:
				flags |= ML_REPEAT_SPECIAL;
			case SwitchOnce:
			case PushOnce:
				if (passthrough)
					flags |= SPAC_USETHROUGH << ML_SPAC_SHIFT;
				else
					flags |= SPAC_USE << ML_SPAC_SHIFT;
				break;

			case GunMany:
				flags |= ML_REPEAT_SPECIAL;
			case GunOnce:
				flags |= SPAC_IMPACT << ML_SPAC_SHIFT;
				break;
			}

			ld->args[0] = tag;
			ld->args[1] = ld->args[2] = ld->args[3] = ld->args[4] = 0;

			ld->special = *tlate++;
			for (;;)
			{
				int *destp;
				int flagtemp;
				BYTE op = *tlate++;
				BYTE dest;
				BYTE val = 0;	// quiet, GCC
				bool found;
				int lsize;

				dest = op & 3;
				if (op & 8)
				{
					BYTE xop = *tlate++;
					if (xop == 0)
						break;
					else if (xop == 1)
						dest = 4;
				}
				if (dest < 4)
				{
					destp = &ld->args[dest+1];
				}
				else
				{
					flagtemp = ((flags >> 9) & 0x3f);
					destp = &flagtemp;
				}
				lsize = op >> 4;
				if (lsize == 0)
				{
					val = *tlate++;
					found = true;
				}
				else
				{
					WORD mask = (tlate[0] << 8) | tlate[1];
					tlate += 2;
					found = false;
					for (i = 0; i < lsize; i++)
					{
						WORD filter = (tlate[0] << 8) | tlate[1];
						if ((special & mask) == filter)
						{
							val = tlate[2];
							found = true;
						}
						tlate += 3;
					}
				}
				if (found)
				{
					if (op & 4)
					{
						*destp |= val;
					}
					else
					{
						*destp = val;
					}
					if (dest == 4)
					{
						flags = (flags & ~0x7e00) | (flagtemp << 9);
					}
				}
			}
			if (special >= low && special <= high)
			{ // Really found it, so we're done
				// We treat push triggers like switch triggers with zero tags.
				if ((special & 7) == PushMany || (special & 7) == PushOnce)
				{
					if (ld->special == Generic_Door)
					{
						ld->args[2] |= 128;
					}
					else
					{
						ld->args[0] = 0;
					}
				}
				ld->flags = flags;
				return;
			}

			flags = oflags;
			count--;
		}
	}

	// Don't know what to do, so 0 it
	ld->special = 0;
	ld->flags = flags;
	memset (ld->args, 0, sizeof(ld->args));
}

// Now that ZDoom again gives the option of using Doom's original teleport
// behavior, only teleport dests in a sector with a 0 tag need to be
// given a TID. And since Doom format maps don't have TIDs, we can safely
// give them TID 1.

void P_TranslateTeleportThings ()
{
	ATeleportDest *dest;
	TThinkerIterator<ATeleportDest> iterator;
	bool foundSomething = false;

	while ( (dest = iterator.Next()) )
	{
		if (dest->Sector->tag == 0)
		{
			dest->tid = 1;
			dest->AddToHash ();
			foundSomething = true;
		}
	}

	if (foundSomething)
	{
		for (int i = 0; i < numlines; ++i)
		{
			if (lines[i].special == Teleport)
			{
				if (lines[i].args[1] == 0)
				{
					lines[i].args[0] = 1;
				}
			}
			else if (lines[i].special == Teleport_NoFog)
			{
				if (lines[i].args[2] == 0)
				{
					lines[i].args[0] = 1;
				}
			}
			else if (lines[i].special == Teleport_ZombieChanger)
			{
				if (lines[i].args[1] == 0)
				{
					lines[i].args[0] = 1;
				}
			}
		}
	}
}

static short sectortables[2][256];
static int boommask=0, boomshift=0;
static bool secparsed;

void P_ReadSectorSpecials()
{
	secparsed=true;
	for(int i=0;i<256;i++)
	{
		sectortables[0][i]=-1;
		sectortables[1][i]=i;
	}
	
	int lastlump=0, lump;

	lastlump = 0;
	while ((lump = Wads.FindLump ("SECTORX", &lastlump)) != -1)
	{
		SC_OpenLumpNum (lump, "SECTORX");
		SC_SetCMode(true);
		while (SC_GetString())
		{
			if (SC_Compare("IFDOOM"))
			{
				SC_MustGetStringName("{");
				if (gameinfo.gametype != GAME_Doom)
				{
					do
					{
						if (!SC_GetString())
						{
							SC_ScriptError("Unexpected end of file");
						}
					}
					while (!SC_Compare("}"));
				}
			}
			else if (SC_Compare("IFHERETIC"))
			{
				SC_MustGetStringName("{");
				if (gameinfo.gametype != GAME_Heretic)
				{
					do
					{
						if (!SC_GetString())
						{
							SC_ScriptError("Unexpected end of file");
						}
					}
					while (!SC_Compare("}"));
				}
			}
			else if (SC_Compare("IFSTRIFE"))
			{
				SC_MustGetStringName("{");
				if (gameinfo.gametype != GAME_Strife)
				{
					do
					{
						if (!SC_GetString())
						{
							SC_ScriptError("Unexpected end of file");
						}
					}
					while (!SC_Compare("}"));
				}
			}
			else if (SC_Compare("}"))
			{
				// ignore
			}
			else if (SC_Compare("BOOMMASK"))
			{
				SC_MustGetNumber();
				boommask = sc_Number;
				SC_MustGetStringName(",");
				SC_MustGetNumber();
				boomshift = sc_Number;
			}
			else if (SC_Compare("["))
			{
				int start;
				int end;
				
				SC_MustGetNumber();
				start = sc_Number;
				SC_MustGetStringName(",");
				SC_MustGetNumber();
				end = sc_Number;
				SC_MustGetStringName("]");
				SC_MustGetStringName(":");
				SC_MustGetNumber();
				for(int j=start;j<=end;j++)
				{
					sectortables[!!boommask][j]=sc_Number + j - start;
				}
			}
			else if (IsNum(sc_String))
			{
				int start;
				
				start = atoi(sc_String);
				SC_MustGetStringName(":");
				SC_MustGetNumber();
				sectortables[!!boommask][start]=sc_Number;
			}
			else
			{
				SC_ScriptError(NULL);
			}
		}
		SC_Close ();
	}
}

int P_TranslateSectorSpecial (int special)
{
	int high;

	// Allow any supported sector special by or-ing 0x8000 to it in Doom format maps
	// That's for those who like to mess around with existing maps. ;)
	if (special & 0x8000)
	{
		return special & 0x7fff;
	}
	
	if (!secparsed) P_ReadSectorSpecials();
	
	if (special>=0 && special<=255)
	{
		if (sectortables[0][special]>=0) return sectortables[0][special];
	}
	high = (special & boommask) << boomshift;
	special &= (~boommask) & 255;
	
	return sectortables[1][special] | high;
}
