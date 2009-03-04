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
#include "xlat/xlat.h"

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
	unsigned short special = (unsigned short) LittleShort(mld->special);
	short tag = LittleShort(mld->tag);
	DWORD flags = LittleShort(mld->flags);
	INTBOOL passthrough;

	if (flags & ML_TRANSLUCENT_STRIFE)
	{
		ld->Alpha = FRACUNIT*3/4;
	}
	if (gameinfo.gametype == GAME_Strife)
	{
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
		if (gameinfo.gametype & GAME_DoomChex)
		{
			if (flags & ML_RESERVED_ETERNITY)
			{
				flags &= 0x1FF;
			}
		}
		if (flags & ML_3DMIDTEX_ETERNITY)
		{
			flags |= ML_3DMIDTEX;
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

	FLineTrans *linetrans = NULL;
	if (special < SimpleLineTranslations.Size()) linetrans = &SimpleLineTranslations[special];
	if (linetrans != NULL && linetrans->special != 0)
	{
		ld->special = linetrans->special;
		ld->args[0] = linetrans->args[0];
		ld->args[1] = linetrans->args[1];
		ld->args[2] = linetrans->args[2];
		ld->args[3] = linetrans->args[3];
		ld->args[4] = linetrans->args[4];

		ld->flags = flags | ((linetrans->flags & 0x1f) << 9);
		if (linetrans->flags & 0x20) ld->flags |= ML_FIRSTSIDEONLY;
		ld->activation = 1 << GET_SPAC(ld->flags);
		if (ld->activation == SPAC_AnyCross) ld->activation = SPAC_Impact|SPAC_PCross;	// this is really PTouch
		ld->flags &= ~ML_SPAC_MASK;

		if (passthrough && ld->activation == SPAC_Use)
		{
			ld->activation = SPAC_UseThrough;
		}
		switch (linetrans->flags & LINETRANS_TAGMASK)
		{
		case LINETRANS_HAS2TAGS:	// First two arguments are tags
			ld->args[1] = tag;
		case LINETRANS_HASTAGAT1:	// First argument is a tag
			ld->args[0] = tag;
			break;

		case LINETRANS_HASTAGAT2:	// Second argument is a tag
			ld->args[1] = tag;
			break;

		case LINETRANS_HASTAGAT3:	// Third argument is a tag
			ld->args[2] = tag;
			break;

		case LINETRANS_HASTAGAT4:	// Fourth argument is a tag
			ld->args[3] = tag;
			break;

		case LINETRANS_HASTAGAT5:	// Fifth argument is a tag
			ld->args[4] = tag;
			break;
		}
		if ((ld->flags & ML_SECRET) && ld->activation & (SPAC_Use|SPAC_UseThrough))
		{
			ld->flags &= ~ML_MONSTERSCANACTIVATE;
		}
		return;
	}

	for(int i=0;i<NumBoomish;i++)
	{
		FBoomTranslator *b = &Boomish[i];

		if (special >= b->FirstLinetype && special <= b->LastLinetype)
		{
			ld->special = b->NewSpecial;

			switch (special & 0x0007)
			{
			case WalkMany:
				flags |= ML_REPEAT_SPECIAL;
			case WalkOnce:
				ld->activation = SPAC_Cross;
				break;

			case SwitchMany:
			case PushMany:
				flags |= ML_REPEAT_SPECIAL;
			case SwitchOnce:
			case PushOnce:
				if (passthrough)
					ld->activation = SPAC_UseThrough;
				else
					ld->activation = SPAC_Use;
				break;

			case GunMany:
				flags |= ML_REPEAT_SPECIAL;
			case GunOnce:
				ld->activation = SPAC_Impact;
				break;
			}

			ld->args[0] = tag;
			ld->args[1] = ld->args[2] = ld->args[3] = ld->args[4] = 0;

			for(unsigned j=0; j < b->Args.Size(); j++)
			{
				FBoomArg *arg = &b->Args[j];
				int *destp;
				int flagtemp;
				BYTE val = 0;	// quiet, GCC
				bool found;

				if (arg->ArgNum < 4)
				{
					destp = &ld->args[arg->ArgNum+1];
				}
				else
				{
					flagtemp = ((flags >> 9) & 0x3f);
					destp = &flagtemp;
				}
				if (arg->ListSize == 0)
				{
					val = arg->ConstantValue;
					found = true;
				}
				else
				{
					found = false;
					for (int k = 0; k < arg->ListSize; k++)
					{
						if ((special & arg->AndValue) == arg->ResultFilter[k])
						{
							val = arg->ResultValue[k];
							found = true;
						}
					}
				}
				if (found)
				{
					if (arg->bOrExisting)
					{
						*destp |= val;
					}
					else
					{
						*destp = val;
					}
					if (arg->ArgNum == 4)
					{
						flags = (flags & ~0x7e00) | (flagtemp << 9);
					}
				}
			}
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
			if (flags & ML_MONSTERSCANACTIVATE && ld->activation == SPAC_Cross)
			{
				// In Boom anything can activate such a line so set the proper type here.
				ld->activation = SPAC_AnyCross;
			}
			return;
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
	AActor *dest;
	TThinkerIterator<AActor> iterator(NAME_TeleportDest);
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

int P_TranslateSectorSpecial (int special)
{
	int mask = 0;

	for(int i = SectorMasks.Size()-1; i>=0; i--)
	{
		int newmask = special & SectorMasks[i].mask;
		if (newmask)
		{
			special &= ~newmask;
			if (SectorMasks[i].op == 1)
				newmask <<= SectorMasks[i].shift;
			else if (SectorMasks[i].op == -1)
				newmask >>= SectorMasks[i].shift;
			else if (SectorMasks[i].op == 0 && SectorMasks[i].shift == 1)
				newmask = 0;
			mask |= newmask;
		}
	}
	
	if ((unsigned)special < SectorTranslations.Size())
	{
		if (!SectorTranslations[special].bitmask_allowed && mask)
			special = 0;
		else
			special = SectorTranslations[special].newtype;
	}
	return special | mask;
}

