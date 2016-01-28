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
#include "s_sndseq.h"
#include "v_palette.h"
#include "a_sharedglobal.h"
#include "r_data/r_interpolate.h"
#include "g_level.h"
#include "po_man.h"
#include "p_setup.h"
#include "r_data/colormaps.h"
#include "farchive.h"
#include "p_lnspec.h"
#include "p_acs.h"
#include "p_terrain.h"

static void CopyPlayer (player_t *dst, player_t *src, const char *name);
static void ReadOnePlayer (FArchive &arc, bool skipload);
static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow, bool skipload);
static void SpawnExtraPlayers ();

inline FArchive &operator<< (FArchive &arc, FLinkedSector &link)
{
	arc << link.Sector << link.Type;
	return arc;
}

//
// P_ArchivePlayers
//
void P_SerializePlayers (FArchive &arc, bool skipload)
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
				arc.WriteString (players[i].userinfo.GetName());
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
			ReadOnePlayer (arc, skipload);
		}
		else
		{
			ReadMultiplePlayers (arc, numPlayers, numPlayersNow, skipload);
		}
		if (!skipload && numPlayersNow > numPlayers)
		{
			SpawnExtraPlayers ();
		}
		// Redo pitch limits, since the spawned player has them at 0.
		players[consoleplayer].SendPitchLimits();
	}
}

static void ReadOnePlayer (FArchive &arc, bool skipload)
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
				if (!skipload)
				{
					CopyPlayer (&players[i], &playerTemp, name);
				}
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

static void ReadMultiplePlayers (FArchive &arc, int numPlayers, int numPlayersNow, bool skipload)
{
	// For two or more players, read each player into a temporary array.
	int i, j;
	char **nametemp = new char *[numPlayers];
	player_t *playertemp = new player_t[numPlayers];
	BYTE *tempPlayerUsed = new BYTE[numPlayers];
	BYTE playerUsed[MAXPLAYERS];

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

	if (!skipload)
	{
		// Now try to match players from the savegame with players present
		// based on their names. If two players in the savegame have the
		// same name, then they are assigned to players in the current game
		// on a first-come, first-served basis.
		for (i = 0; i < numPlayers; ++i)
		{
			for (j = 0; j < MAXPLAYERS; ++j)
			{
				if (playerUsed[j] == 0 && stricmp(players[j].userinfo.GetName(), nametemp[i]) == 0)
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
						Printf ("Assigned player %d (%s) to %d (%s)\n", i, nametemp[i], j, players[j].userinfo.GetName());
						CopyPlayer (&players[j], &playertemp[i], nametemp[i]);
						playerUsed[j] = 1;
						tempPlayerUsed[i] = 1;
						break;
					}
				}
			}
		}

		// Make sure any extra players don't have actors spawned yet. Happens if the players
		// present now got the same slots as they had in the save, but there are not as many
		// as there were in the save.
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

		// Remove any temp players that were not used. Happens if there are fewer players
		// than there were in the save, and they got shuffled.
		for (i = 0; i < numPlayers; ++i)
		{
			if (tempPlayerUsed[i] == 0)
			{
				playertemp[i].mo->Destroy();
				playertemp[i].mo = NULL;
			}
		}
	}

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
	userinfo_t uibackup;
	userinfo_t uibackup2;

	uibackup.TransferFrom(dst->userinfo);
	uibackup2.TransferFrom(src->userinfo);

	int chasecam = dst->cheats & CF_CHASECAM;	// Remember the chasecam setting
	bool attackdown = dst->attackdown;
	bool usedown = dst->usedown;

	
	*dst = *src;		// To avoid memory leaks at this point the userinfo in src must be empty which is taken care of by the TransferFrom call above.

	dst->cheats |= chasecam;

	if (dst->Bot != NULL)
	{
		botinfo_t *thebot = bglobal.botinfo;
		while (thebot && stricmp (name, thebot->name))
		{
			thebot = thebot->next;
		}
		if (thebot)
		{
			thebot->inuse = BOTINUSE_Yes;
		}
		bglobal.botnum++;
		dst->userinfo.TransferFrom(uibackup2);
	}
	else
	{
		dst->userinfo.TransferFrom(uibackup);
	}
	// Validate the skin
	dst->userinfo.SkinNumChanged(R_FindSkin(skins[dst->userinfo.GetSkin()].name, dst->CurrentPlayerClass));

	// Make sure the player pawn points to the proper player struct.
	if (dst->mo != NULL)
	{
		dst->mo->player = dst;
	}
	// These 2 variables may not be overwritten.
	dst->attackdown = attackdown;
	dst->usedown = usedown;
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
			P_SpawnPlayer(&playerstarts[i], i, (level.flags2 & LEVEL2_PRERAISEWEAPON) ? SPF_WEAPONFULLYUP : 0);
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
			<< sec->ceilingplane;
		if (SaveVersion < 3223)
		{
			BYTE bytelight;
			arc << bytelight;
			sec->lightlevel = bytelight;
		}
		else
		{
			arc << sec->lightlevel;
		}
		arc << sec->special;
		if (SaveVersion < 4523)
		{
			short tag;
			arc << tag;
		}
		arc << sec->soundtraversed
			<< sec->seqType
			<< sec->friction
			<< sec->movefactor
			<< sec->floordata
			<< sec->ceilingdata
			<< sec->lightingdata
			<< sec->stairlock
			<< sec->prevsec
			<< sec->nextsec
			<< sec->planes[sector_t::floor]
			<< sec->planes[sector_t::ceiling]
			<< sec->heightsec
			<< sec->bottommap << sec->midmap << sec->topmap
			<< sec->gravity;
		if (SaveVersion >= 4530)
		{
			P_SerializeTerrain(arc, sec->terrainnum[0]);
			P_SerializeTerrain(arc, sec->terrainnum[1]);
		}
		if (SaveVersion >= 4529)
		{
			arc << sec->damageamount;
		}
		else
		{
			short dmg;
			arc << dmg;
			sec->damageamount = dmg;
		}
		if (SaveVersion >= 4528)
		{
			arc << sec->damageinterval
				<< sec->leakydamage
				<< sec->damagetype;
		}
		else
		{
			short damagemod;
			arc << damagemod;
			sec->damagetype = MODtoDamageType(damagemod);
			if (sec->damageamount < 20)
			{
				sec->leakydamage = 0;
				sec->damageinterval = 32;
			}
			else if (sec->damageamount < 50)
			{
				sec->leakydamage = 5;
				sec->damageinterval = 32;
			}
			else
			{
				sec->leakydamage = 256;
				sec->damageinterval = 1;
			}
		}

		arc << sec->SoundTarget
			<< sec->SecActTarget
			<< sec->sky
			<< sec->MoreFlags
			<< sec->Flags
			<< sec->SkyBoxes[sector_t::floor] << sec->SkyBoxes[sector_t::ceiling]
			<< sec->ZoneNumber;
		if (SaveVersion < 4529)
		{
			short secretsector;
			arc << secretsector;
			if (secretsector) sec->Flags |= SECF_WASSECRET;
			P_InitSectorSpecial(sec, sec->special, true);
		}
		arc	<< sec->interpolations[0]
			<< sec->interpolations[1]
			<< sec->interpolations[2]
			<< sec->interpolations[3]
			<< sec->SeqName;

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
			<< li->activation
			<< li->special
			<< li->Alpha;

		if (SaveVersion < 4523)
		{
			int id;
			arc << id;
		}
		if (P_IsACSSpecial(li->special))
		{
			P_SerializeACSScriptNumber(arc, li->args[0], false);
		}
		else
		{
			arc << li->args[0];
		}
		arc << li->args[1] << li->args[2] << li->args[3] << li->args[4];

		for (j = 0; j < 2; j++)
		{
			if (li->sidedef[j] == NULL)
				continue;

			side_t *si = li->sidedef[j];
			arc << si->textures[side_t::top]
				<< si->textures[side_t::mid]
				<< si->textures[side_t::bottom]
				<< si->Light
				<< si->Flags
				<< si->LeftSide
				<< si->RightSide
				<< si->Index;
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

FArchive &operator<< (FArchive &arc, side_t::part &p)
{
	arc << p.xoffset << p.yoffset << p.interpolation << p.texture 
		<< p.xscale << p.yscale;// << p.Light;
	return arc;
}

FArchive &operator<< (FArchive &arc, sector_t::splane &p)
{
	arc << p.xform.xoffs << p.xform.yoffs << p.xform.xscale << p.xform.yscale 
		<< p.xform.angle << p.xform.base_yoffs << p.xform.base_angle
		<< p.Flags << p.Light << p.Texture << p.TexZ << p.alpha;
	return arc;
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
	S_SerializeSounds (arc);
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
				S_ChangeMusic (level.Music, level.musicorder);
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
	FPolyObj *po;

	if (arc.IsStoring ())
	{
		int seg = ASEG_POLYOBJS;
		arc << seg << po_NumPolyobjs;
		for(i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc << po->tag << po->angle << po->StartSpot.x <<
				po->StartSpot.y << po->interpolation;
  		}
	}
	else
	{
		int data;
		angle_t angle;
		fixed_t deltaX, deltaY;

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
			po->RotatePolyobj (angle, true);
			arc << deltaX << deltaY << po->interpolation;
			deltaX -= po->StartSpot.x;
			deltaY -= po->StartSpot.y;
			po->MovePolyobj (deltaX, deltaY, true);
		}
	}
}

//==========================================================================
//
// RecalculateDrawnSubsectors
//
// In case the subsector data is unusable this function tries to reconstruct
// if from the linedefs' ML_MAPPED info.
//
//==========================================================================

void RecalculateDrawnSubsectors()
{
	for(int i=0;i<numsubsectors;i++)
	{
		subsector_t *sub = &subsectors[i];
		for(unsigned int j=0;j<sub->numlines;j++)
		{
			if (sub->firstline[j].linedef != NULL && 
				(sub->firstline[j].linedef->flags & ML_MAPPED))
			{
				sub->flags |= SSECF_DRAWN;
			}
		}
	}
}

//==========================================================================
//
// ArchiveSubsectors
//
//==========================================================================

void P_SerializeSubsectors(FArchive &arc)
{
	int num_verts, num_subs, num_nodes;	
	BYTE by;

	if (arc.IsStoring())
	{
		if (hasglnodes)
		{
			arc << numvertexes << numsubsectors << numnodes;	// These are only for verification
			for(int i=0;i<numsubsectors;i+=8)
			{
				by = 0;
				for(int j=0;j<8;j++)
				{
					if (i+j<numsubsectors && (subsectors[i+j].flags & SSECF_DRAWN))
					{
						by |= (1<<j);
					}
				}
				arc << by;
			}
		}
		else
		{
			int v = 0;
			arc << v << v << v;
		}
	}
	else
	{
		arc << num_verts << num_subs << num_nodes;
		if (num_verts != numvertexes ||
			num_subs != numsubsectors ||
			num_nodes != numnodes)
		{
			// Nodes don't match - we can't use this info
			for(int i=0;i<num_subs;i+=8)
			{
				// Skip the subsector info.
				arc << by;
			}
			if (hasglnodes)
			{
				RecalculateDrawnSubsectors();
			}
			return;
		}
		else
		{
			for(int i=0;i<numsubsectors;i+=8)
			{
				arc << by;
				for(int j=0;j<8;j++)
				{
					if ((by & (1<<j)) && i+j<numsubsectors)
					{
						subsectors[i+j].flags |= SSECF_DRAWN;
					}
				}
			}
		}
	}
}
