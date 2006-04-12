/*
** p_saveg.cpp
** Code for serializing the world state in an archive
**
**---------------------------------------------------------------------------
** Copyright 1998-2005 Randy Heit
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



//
// P_ArchivePlayers
//
void P_SerializePlayers (FArchive &arc)
{
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		arc << playeringame[i];
		if (playeringame[i])
		{
			players[i].Serialize (arc);
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
			arc << si->textureoffset
				<< si->rowoffset;

			if (arc.IsStoring ())
			{
				TexMan.WriteTexture (arc, si->toptexture);
				TexMan.WriteTexture (arc, si->bottomtexture);
				TexMan.WriteTexture (arc, si->midtexture);
			}
			else
			{
				si->toptexture = TexMan.ReadTexture (arc);
				si->bottomtexture = TexMan.ReadTexture (arc);
				si->midtexture = TexMan.ReadTexture (arc);
			}
			arc << si->Light
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
			PO_MovePolyobj (po->tag, deltaX, deltaY);
		}
	}
}
