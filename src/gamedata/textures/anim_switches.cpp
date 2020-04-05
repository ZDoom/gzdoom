/*
** p_switch.cpp
** Switch and button maintenance and animation
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
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

#include "templates.h"
#include "textures/textures.h"
#include "s_sound.h"
#include "r_state.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "gi.h"


static int SortSwitchDefs (const void *a, const void *b)
{
	return (*(FSwitchDef **)a)->PreTexture - (*(FSwitchDef **)b)->PreTexture;
}

//==========================================================================
//
// P_InitSwitchList
// Only called at game initialization.
//
// [RH] Rewritten to use a BOOM-style SWITCHES lump and remove the
//		MAXSWITCHES limit.
//
//==========================================================================

void FTextureManager::InitSwitchList ()
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	int lump = Wads.CheckNumForName ("SWITCHES");

	if (lump != -1)
	{
		FMemLump lumpdata = Wads.ReadLump (lump);
		const char *alphSwitchList = (const char *)lumpdata.GetMem();
		const char *list_p;
		FSwitchDef *def1, *def2;

		for (list_p = alphSwitchList; list_p[18] || list_p[19]; list_p += 20)
		{
			// [RH] Check for switches that aren't really switches
			if (stricmp (list_p, list_p+9) == 0)
			{
				Printf ("Switch %s in SWITCHES has the same 'on' state\n", list_p);
				continue;
			}
			// [RH] Skip this switch if its textures can't be found.
			if (CheckForTexture (list_p /* .name1 */, ETextureType::Wall, texflags).Exists() &&
				CheckForTexture (list_p + 9 /* .name2 */, ETextureType::Wall, texflags).Exists())
			{
				def1 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
				def2 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
				def1->PreTexture = def2->frames[0].Texture = CheckForTexture (list_p /* .name1 */, ETextureType::Wall, texflags);
				def2->PreTexture = def1->frames[0].Texture = CheckForTexture (list_p + 9, ETextureType::Wall, texflags);
				def1->Sound = def2->Sound = 0;
				def1->NumFrames = def2->NumFrames = 1;
				def1->frames[0].TimeMin = def2->frames[0].TimeMin = 0;
				def1->frames[0].TimeRnd = def2->frames[0].TimeRnd = 0;
				AddSwitchPair(def1, def2);
			}
		}
	}

	mSwitchDefs.ShrinkToFit ();
	qsort (&mSwitchDefs[0], mSwitchDefs.Size(), sizeof(FSwitchDef *), SortSwitchDefs);
}

//==========================================================================
//
// Parse a switch block in ANIMDEFS and add the definitions to mSwitchDefs
//
//==========================================================================

void FTextureManager::ProcessSwitchDef (FScanner &sc)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	FString picname;
	FSwitchDef *def1, *def2;
	FTextureID picnum;
	int gametype;
	bool quest = false;

	def1 = def2 = NULL;
	sc.MustGetString ();
	if (sc.Compare ("doom"))
	{
		gametype = GAME_DoomChex;
		sc.CheckNumber();	// skip the gamemission filter
	}
	else if (sc.Compare ("heretic"))
	{
		gametype = GAME_Heretic;
	}
	else if (sc.Compare ("hexen"))
	{
		gametype = GAME_Hexen;
	}
	else if (sc.Compare ("strife"))
	{
		gametype = GAME_Strife;
	}
	else if (sc.Compare ("any"))
	{
		gametype = GAME_Any;
	}
	else
	{
		// There is no game specified; just treat as any
		//max = 240;
		gametype = GAME_Any;
		sc.UnGet ();
	}

	sc.MustGetString ();
	picnum = CheckForTexture (sc.String, ETextureType::Wall, texflags);
	picname = sc.String;
	while (sc.GetString ())
	{
		if (sc.Compare ("quest"))
		{
			quest = true;
		}
		else if (sc.Compare ("on"))
		{
			if (def1 != NULL)
			{
				sc.ScriptError ("Switch already has an on state");
			}
			def1 = ParseSwitchDef (sc, !picnum.Exists());
		}
		else if (sc.Compare ("off"))
		{
			if (def2 != NULL)
			{
				sc.ScriptError ("Switch already has an off state");
			}
			def2 = ParseSwitchDef (sc, !picnum.Exists());
		}
		else
		{
			sc.UnGet ();
			break;
		}
	}

	if (def1 == NULL || !picnum.Exists() ||
		(gametype != GAME_Any && !(gametype & gameinfo.gametype)))
	{
		if (def2 != NULL)
		{
			M_Free (def2);
		}
		if (def1 != NULL)
		{
			M_Free (def1);
		}
		return;
	}

	// If the switch did not have an off state, create one that just returns
	// it to the original texture without doing anything interesting
	if (def2 == NULL)
	{
		def2 = (FSwitchDef *)M_Malloc (sizeof(FSwitchDef));
		def2->Sound = def1->Sound;
		def2->NumFrames = 1;
		def2->frames[0].TimeMin = 0;
		def2->frames[0].TimeRnd = 0;
		def2->frames[0].Texture = picnum;
	}

	def1->PreTexture = picnum;
	def2->PreTexture = def1->frames[def1->NumFrames-1].Texture;
	if (def1->PreTexture == def2->PreTexture)
	{
		sc.ScriptError ("The on state for switch %s must end with a texture other than %s", picname.GetChars(), picname.GetChars());
	}
	AddSwitchPair(def1, def2);
	def1->QuestPanel = def2->QuestPanel = quest;
}

//==========================================================================
//
// Parse one switch frame
//
//==========================================================================

FSwitchDef *FTextureManager::ParseSwitchDef (FScanner &sc, bool ignoreBad)
{
	const BITFIELD texflags = TEXMAN_Overridable | TEXMAN_TryAny;
	FSwitchDef *def;
	TArray<FSwitchDef::frame> frames;
	FSwitchDef::frame thisframe;
	FTextureID picnum;
	bool bad;
	FSoundID sound = 0;

	bad = false;

	while (sc.GetString ())
	{
		if (sc.Compare ("sound"))
		{
			if (sound != 0)
			{
				sc.ScriptError ("Switch state already has a sound");
			}
			sc.MustGetString ();
			sound = sc.String;
		}
		else if (sc.Compare ("pic"))
		{
			sc.MustGetString ();
			picnum = CheckForTexture (sc.String, ETextureType::Wall, texflags);
			if (!picnum.Exists() && !ignoreBad)
			{
				//Printf ("Unknown switch texture %s\n", sc.String);
				bad = true;
			}
			thisframe.Texture = picnum;
			sc.MustGetString ();
			if (sc.Compare ("tics"))
			{
				sc.MustGetNumber ();
				thisframe.TimeMin = sc.Number & 65535;
				thisframe.TimeRnd = 0;
			}
			else if (sc.Compare ("rand"))
			{
				int min, max;

				sc.MustGetNumber ();
				min = sc.Number & 65535;
				sc.MustGetNumber ();
				max = sc.Number & 65535;
				if (min > max)
				{
					swapvalues (min, max);
				}
				thisframe.TimeMin = min;
				thisframe.TimeRnd = (max - min + 1);
			}
			else
			{
			    thisframe.TimeMin = 0;     // Shush, GCC.
				thisframe.TimeRnd = 0;
				sc.ScriptError ("Must specify a duration for switch frame");
			}
			frames.Push(thisframe);
		}
		else
		{
			sc.UnGet ();
			break;
		}
	}
	if (frames.Size() == 0)
	{
		sc.ScriptError ("Switch state needs at least one frame");
	}
	if (bad)
	{
		return NULL;
	}

	def = (FSwitchDef *)M_Malloc (myoffsetof (FSwitchDef, frames[0]) + frames.Size()*sizeof(frames[0]));
	def->Sound = sound;
	def->NumFrames = frames.Size();
	memcpy (&def->frames[0], &frames[0], frames.Size() * sizeof(frames[0]));
	def->PairDef = NULL;
	return def;
}

//==========================================================================
//
// 
//
//==========================================================================

void FTextureManager::AddSwitchPair (FSwitchDef *def1, FSwitchDef *def2)
{
	unsigned int i;
	FSwitchDef *sw1 = NULL;
	FSwitchDef *sw2 = NULL;
	unsigned int index1 = 0xffffffff, index2 = 0xffffffff;

	for (i = mSwitchDefs.Size (); i-- > 0; )
	{
		if (mSwitchDefs[i]->PreTexture == def1->PreTexture)
		{
			index1 = i;
			sw1 = mSwitchDefs[index1];
			if (index2 != 0xffffffff) break;
		}
		if (mSwitchDefs[i]->PreTexture == def2->PreTexture)
		{
			index2 = i;
			sw2 = mSwitchDefs[index2];
			if (index1 != 0xffffffff) break;
		}
	}

	def1->PairDef = def2;
	def2->PairDef = def1;

	if (sw1 != NULL && sw2 != NULL && sw1->PairDef == sw2 && sw2->PairDef == sw1)
	{
		//We are replacing an existing pair so we can safely delete the old definitions
		M_Free(sw1);
		M_Free(sw2);
		mSwitchDefs[index1] = def1;
		mSwitchDefs[index2] = def2;
	}
	else
	{
		// This new switch will not or only partially replace an existing pair.
		// We should not break up an old pair if the new one only redefined one
		// of the two textures. These paired definitions will only be used
		// as the return animation so their names don't matter. Better clear them to be safe.
		if (sw1 != NULL) sw1->PreTexture.SetInvalid();
		if (sw2 != NULL) sw2->PreTexture.SetInvalid();
		sw1 = NULL;
		sw2 = NULL;
		unsigned int pos = mSwitchDefs.Reserve(2);
		mSwitchDefs[pos] = def1;
		mSwitchDefs[pos+1] = def2;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

FSwitchDef *FTextureManager::FindSwitch (FTextureID texture)
{
	int mid, low, high;

	high = (int)(mSwitchDefs.Size () - 1);
	if (high >= 0)
	{
		low = 0;
		do
		{
			mid = (high + low) / 2;
			if (mSwitchDefs[mid]->PreTexture == texture)
			{
				return mSwitchDefs[mid];
			}
			else if (texture < mSwitchDefs[mid]->PreTexture)
			{
				high = mid - 1;
			}
			else
			{
				low = mid + 1;
			}
		} while (low <= high);
	}
	return NULL;
}

