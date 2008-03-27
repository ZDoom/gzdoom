/*
** p_saveg.cpp
** Code for serializing the world state in an archive
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

#include "i_system.h"
#include "p_local.h"

// State.
#include "dobject.h"
#include "doomstat.h"
#include "r_state.h"
#include "m_random.h"
#include "p_saveg.h"
#include "p_acs.h"
#include "s_sndseq.h"
#include "v_palette.h"
#include "a_sharedglobal.h"

static void CopyPlayer (player_t *dst, player_t *src, const char *name);
static void ReadOnePlayer (FArchive &arc);
static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow);
static void SpawnExtraPlayers ();

//
// P_ArchivePlayers
//
void P_SerializePlayers (FArchive &arc)
{
	BYTE numPlayers, numPlayersNow;
	int i;

	// Count the number of players present right now.
	for (numPlayersNow = 0, i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			++numPlayersNow;
		}
	}

	if (arc.IsStoring())
	{
		// Record the number of players in this save.
		arc << numPlayersNow;

		// Record each player's name, followed by their data.
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i])
			{
				arc.WriteString (players[i].userinfo.netname);
				players[i].Serialize (arc);
			}
		}
	}
	else
	{
		arc << numPlayers;

		// If there is only one player in the game, they go to the
		// first player present, no matter what their name.
		if (numPlayers == 1)
		{
			ReadOnePlayer (arc);
		}
		else
		{
			ReadMultiplePlayers (arc, numPlayers, numPlayersNow);
		}
		if (numPlayersNow > numPlayers)
		{
			SpawnExtraPlayers ();
		}
	}
}

static void ReadOnePlayer (FArchive &arc)
{
	int i;
	char *name = NULL;
	bool didIt = false;

	arc << name;

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			if (!didIt)
			{
				didIt = true;
				player_t playerTemp;
				playerTemp.Serialize (arc);
				CopyPlayer (&players[i], &playerTemp, name);
			}
			else
			{
				if (players[i].mo != NULL)
				{
					players[i].mo->Destroy();
					players[i].mo = NULL;
				}
			}
		}
	}
	delete[] name;
}

static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow)
{
	// For two or more players, read each player into a temporary array.
	int i, j;
	char **nametemp = new char *[numPlayers];
	player_t *playertemp = new player_t[numPlayers];
	BYTE *tempPlayerUsed = new BYTE[numPlayers];
	BYTE *playerUsed = new BYTE[MAXPLAYERS];

	for (i = 0; i < numPlayers; ++i)
	{
		nametemp[i] = NULL;
		arc << nametemp[i];
		playertemp[i].Serialize (arc);
		tempPlayerUsed[i] = 0;
	}
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		playerUsed[i] = playeringame[i] ? 0 : 2;
	}

	// Now try to match players from the savegame with players present
	// based on their names. If two players in the savegame have the
	// same name, then they are assigned to players in the current game
	// on a first-come, first-served basis.
	for (i = 0; i < numPlayers; ++i)
	{
		for (j = 0; j < MAXPLAYERS; ++j)
		{
			if (playerUsed[j] == 0 && stricmp(players[j].userinfo.netname, nametemp[i]) == 0)
			{ // Found a match, so copy our temp player to the real player
				Printf ("Found player %d (%s) at %d\n", i, nametemp[i], j);
				CopyPlayer (&players[j], &playertemp[i], nametemp[i]);
				playerUsed[j] = 1;
				tempPlayerUsed[i] = 1;
				break;
			}
		}
	}

	// Any players that didn't have matching names are assigned to existing
	// players on a first-come, first-served basis.
	for (i = 0; i < numPlayers; ++i)
	{
		if (tempPlayerUsed[i] == 0)
		{
			for (j = 0; j < MAXPLAYERS; ++j)
			{
				if (playerUsed[j] == 0)
				{
					Printf ("Assigned player %d (%s) to %d (%s)\n", i, nametemp[i], j, players[j].userinfo.netname);
					CopyPlayer (&players[j], &playertemp[i], nametemp[i]);
					playerUsed[j] = 1;
					tempPlayerUsed[i] = 1;
					break;
				}
			}
		}
	}

	// Make sure any extra players don't have actors spawned yet.
	for (j = 0; j < MAXPLAYERS; ++j)
	{
		if (playerUsed[j] == 0)
		{
			if (players[j].mo != NULL)
			{
				players[j].mo->Destroy();
				players[j].mo = NULL;
			}
		}
	}

	delete[] playerUsed;
	delete[] tempPlayerUsed;
	delete[] playertemp;
	for (i = 0; i < numPlayers; ++i)
	{
		delete[] nametemp[i];
	}
	delete[] nametemp;
}

static void CopyPlayer (player_t *dst, player_t *src, const char *name)
{
	// The userinfo needs to be saved for real players, but it
	// needs to come from the save for bots.
	userinfo_t uibackup = dst->userinfo;
	int chasecam = dst->cheats & CF_CHASECAM;	// Remember the chasecam setting
	*dst = *src;
	dst->cheats |= chasecam;

	if (dst->isbot)
	{
		botinfo_t *thebot = bglobal->botinfo;
		while (thebot && stricmp (name, thebot->name))
		{
			thebot = thebot->next;
		}
		if (thebot)
		{
			thebot->inuse = true;
		}
		bglobal->botnum++;
		bglobal->botingame[dst - players] = true;
	}
	else
	{
		dst->userinfo = uibackup;
	}
}

static void SpawnExtraPlayers ()
{
	// If there are more players now than there were in the savegame,
	// be sure to spawn the extra players.
	int i;

	if (deathmatch)
	{
		return;
	}

	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].mo == NULL)
		{
			players[i].playerstate = PST_ENTER;
			P_SpawnPlayer (&playerstarts[i]);
		}
	}
}

//
// P_ArchiveWorld
//
void P_SerializeWorld (FArchive &arc)
{
	int i, j;
	sector_t *sec;
	line_t *li;
	zone_t *zn;

	// do sectors
	for (i = 0, sec = sectors; i < numsectors; i++, sec++)
	{
		arc << sec->floorplane
			<< sec->ceilingplane
			<< sec->floortexz
			<< sec->ceilingtexz;

		if (arc.IsStoring ())
		{
			TexMan.WriteTexture (arc, sec->floorpic);
			TexMan.WriteTexture (arc, sec->ceilingpic);
		}
		else
		{
			sec->floorpic = TexMan.ReadTexture (arc);
			sec->ceilingpic = TexMan.ReadTexture (arc);
		}
		arc << sec->lightlevel
			<< sec->special
			<< sec->tag
			<< sec->soundtraversed
			<< sec->seqType
			<< sec->friction
			<< sec->movefactor
			<< sec->floordata
			<< sec->ceilingdata
			<< sec->lightingdata
			<< sec->stairlock
			<< sec->prevsec
			<< sec->nextsec
			<< sec->floor_xoffs << sec->floor_yoffs
			<< sec->ceiling_xoffs << sec->ceiling_yoffs
			<< sec->floor_xscale << sec->floor_yscale
			<< sec->ceiling_xscale << sec->ceiling_yscale
			<< sec->floor_angle << sec->ceiling_angle
			<< sec->base_ceiling_angle << sec->base_ceiling_yoffs
			<< sec->base_floor_angle << sec->base_floor_yoffs
			<< sec->heightsec
			<< sec->bottommap << sec->midmap << sec->topmap
			<< sec->gravity
			<< sec->damage
			<< sec->mod
			<< sec->SoundTarget
			<< sec->SecActTarget
			<< sec->FloorLight
			<< sec->CeilingLight
			<< sec->FloorFlags
			<< sec->CeilingFlags
			<< sec->sky
			<< sec->MoreFlags
			<< sec->FloorSkyBox << sec->CeilingSkyBox
			<< sec->ZoneNumber
			<< sec->oldspecial;

		sec->e->Serialize(arc);
		if (arc.IsStoring ())
		{
			arc << sec->ColorMap->Color
				<< sec->ColorMap->Fade;
			BYTE sat = sec->ColorMap->Desaturate;
			arc << sat;
		}
		else
		{
			PalEntry color, fade;
			BYTE desaturate;
			arc << color << fade
				<< desaturate;
			sec->ColorMap = GetSpecialLights (color, fade, desaturate);
		}
	}

	// do lines
	for (i = 0, li = lines; i < numlines; i++, li++)
	{
		arc << li->flags
			<< li->special
			<< li->alpha
			<< li->id
			<< li->args[0] << li->args[1] << li->args[2] << li->args[3] << li->args[4];

		for (j = 0; j < 2; j++)
		{
			if (li->sidenum[j] == NO_SIDE)
				continue;

			side_t *si = &sides[li->sidenum[j]];
			arc << si->textures[side_t::top]
				<< si->textures[side_t::mid]
				<< si->textures[side_t::bottom]
				<< si->Light
				<< si->Flags
				<< si->LeftSide
				<< si->RightSide;
			DBaseDecal::SerializeChain (arc, &si->AttachedDecals);
		}
	}

	// do zones
	arc << numzones;

	if (arc.IsLoading())
	{
		if (zones != NULL)
		{
			delete[] zones;
		}
		zones = new zone_t[numzones];
	}

	for (i = 0, zn = zones; i < numzones; ++i, ++zn)
	{
		arc << zn->Environment;
	}
}

void extsector_t::Serialize(FArchive &arc)
{
	arc << FakeFloor.Sectors
		<< Midtex.Floor.AttachedLines 
		<< Midtex.Floor.AttachedSectors
		<< Midtex.Ceiling.AttachedLines
		<< Midtex.Ceiling.AttachedSectors
		<< Linked.Floor.Sectors
		<< Linked.Ceiling.Sectors;
}

//
// Thinkers
//

//
// P_ArchiveThinkers
//

void P_SerializeThinkers (FArchive &arc, bool hubLoad)
{
	DImpactDecal::SerializeTime (arc);
	DThinker::SerializeAll (arc, hubLoad);
}

//==========================================================================
//
// ArchiveSounds
//
//==========================================================================

void P_SerializeSounds (FArchive &arc)
{
	DSeqNode::SerializeSequences (arc);
	char *name = NULL;
	BYTE order;

	if (arc.IsStoring ())
	{
		order = S_GetMusic (&name);
	}
	arc << name << order;
	if (arc.IsLoading ())
	{
		if (!S_ChangeMusic (name, order))
			if (level.cdtrack == 0 || !S_ChangeCDMusic (level.cdtrack, level.cdid))
				S_ChangeMusic (level.music, level.musicorder);
	}
	delete[] name;
}

//==========================================================================
//
// ArchivePolyobjs
//
//==========================================================================
#define ASEG_POLYOBJS	104

void P_SerializePolyobjs (FArchive &arc)
{
	int i;
	polyobj_t *po;

	if (arc.IsStoring ())
	{
		int seg = ASEG_POLYOBJS;
		arc << seg << po_NumPolyobjs;
		for(i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc << po->tag << po->angle << po->startSpot[0] <<
				po->startSpot[1] << po->startSpot[2];
  		}
	}
	else
	{
		int data;
		angle_t angle;
		fixed_t deltaX, deltaY, deltaZ;

		arc << data;
		if (data != ASEG_POLYOBJS)
			I_Error ("Polyobject marker missing");

		arc << data;
		if (data != po_NumPolyobjs)
		{
			I_Error ("UnarchivePolyobjs: Bad polyobj count");
		}
		for (i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc << data;
			if (data != po->tag)
			{
				I_Error ("UnarchivePolyobjs: Invalid polyobj tag");
			}
			arc << angle;
			PO_RotatePolyobj (po->tag, angle);
			arc << deltaX << deltaY << deltaZ;
			deltaX -= po->startSpot[0];
			deltaY -= po->startSpot[1];
			deltaZ -= po->startSpot[2];
			PO_MovePolyobj (po->tag, deltaX, deltaY, true);
		}
	}
}
