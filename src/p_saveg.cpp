// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Archiving: SaveGame I/O.
//
//-----------------------------------------------------------------------------


#include "i_system.h"
#include "z_zone.h"
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





//
// P_ArchivePlayers
//
void P_SerializePlayers (FArchive &arc)
{
	int i;

	if (arc.IsStoring ())
	{
		for (i = 0; i < MAXPLAYERS; i++)
			arc << playeringame[i];
	}
	else
	{
		for (i = 0; i < MAXPLAYERS; i++)
			arc >> playeringame[i];
	}

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			players[i].Serialize (arc);
}

//
// P_ArchiveWorld
//
void P_SerializeWorld (FArchive &arc)
{
	int i, j;
	sector_t *sec;
	line_t *li;

	if (arc.IsStoring ())
	{ // saving to archive

		// do sectors
		for (i = 0, sec = sectors; i < numsectors; i++, sec++)
		{
			arc << sec->floorheight
				<< sec->ceilingheight
				<< sec->floorpic
				<< sec->ceilingpic
				<< sec->lightlevel
				<< sec->special
				<< sec->tag
				<< sec->soundtraversed
				<< sec->soundtarget
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
				<< sec->ceiling_xoffs << sec->ceiling_xoffs
				<< sec->floor_xscale << sec->floor_yscale
				<< sec->ceiling_xscale << sec->ceiling_yscale
				<< sec->floor_angle << sec->ceiling_angle
				<< sec->base_ceiling_angle << sec->base_ceiling_yoffs
				<< sec->base_floor_angle << sec->base_floor_yoffs
				<< sec->heightsec
				<< sec->floorlightsec << sec->ceilinglightsec
				<< sec->bottommap << sec->midmap << sec->topmap
				<< sec->gravity
				<< sec->damage
				<< sec->mod
				<< sec->floorcolormap->color
				<< sec->floorcolormap->fade
				<< sec->ceilingcolormap->color
				<< sec->ceilingcolormap->fade
				<< sec->alwaysfake
				<< sec->waterzone;
		}

		// do lines
		for (i = 0, li = lines; i < numlines; i++, li++)
		{
			arc << li->flags
				<< li->special
				<< li->lucency
				<< li->id
				<< li->args[0] << li->args[1] << li->args[2] << li->args[3] << li->args[4] << (WORD)0;

			for (j = 0; j < 2; j++)
			{
				if (li->sidenum[j] == -1)
					continue;

				side_t *si = &sides[li->sidenum[j]];
				arc << si->textureoffset
					<< si->rowoffset
					<< si->toptexture
					<< si->bottomtexture
					<< si->midtexture;
			}
		}
	}
	else
	{ // loading from archive

		// do sectors
		for (i = 0, sec = sectors; i < numsectors; i++, sec++)
		{
			unsigned int color=0, fade=0;

			arc >> sec->floorheight
				>> sec->ceilingheight
				>> sec->floorpic
				>> sec->ceilingpic
				>> sec->lightlevel
				>> sec->special
				>> sec->tag
				>> sec->soundtraversed
				>> sec->soundtarget
				>> sec->seqType
				>> sec->friction
				>> sec->movefactor
				>> sec->floordata
				>> sec->ceilingdata
				>> sec->lightingdata
				>> sec->stairlock
				>> sec->prevsec
				>> sec->nextsec
				>> sec->floor_xoffs >> sec->floor_yoffs
				>> sec->ceiling_xoffs >> sec->ceiling_xoffs
				>> sec->floor_xscale >> sec->floor_yscale
				>> sec->ceiling_xscale >> sec->ceiling_yscale
				>> sec->floor_angle >> sec->ceiling_angle
				>> sec->base_ceiling_angle >> sec->base_ceiling_yoffs
				>> sec->base_floor_angle >> sec->base_floor_yoffs
				>> sec->heightsec
				>> sec->floorlightsec >> sec->ceilinglightsec
				>> sec->bottommap >> sec->midmap >> sec->topmap
				>> sec->gravity
				>> sec->damage
				>> sec->mod
				>> color
				>> fade;
			sec->floorcolormap = GetSpecialLights (
				RPART(color), GPART(color), BPART(color),
				RPART(fade), GPART(fade), BPART(fade));
			arc >> color >> fade;
			sec->ceilingcolormap = GetSpecialLights (
				RPART(color), GPART(color), BPART(color),
				RPART(fade), GPART(fade), BPART(fade));
			arc >> sec->alwaysfake
				>> sec->waterzone;
		}

		// do lines
		for (i = 0, li = lines; i < numlines; i++, li++)
		{
		    WORD dummy;
			arc >> li->flags
				>> li->special
				>> li->lucency
				>> li->id
				>> li->args[0] >> li->args[1] >> li->args[2] >> li->args[3] >> li->args[4] >> dummy;

			for (j = 0; j < 2; j++)
			{
				if (li->sidenum[j] == -1)
					continue;

				side_t *si = &sides[li->sidenum[j]];
				arc >> si->textureoffset
					>> si->rowoffset
					>> si->toptexture
					>> si->bottomtexture
					>> si->midtexture;
			}
		}
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
		arc << (int)ASEG_POLYOBJS << po_NumPolyobjs;
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

		arc >> data;
		if (data != ASEG_POLYOBJS)
			I_Error ("Polyobject marker missing");

		arc >> data;
		if (data != po_NumPolyobjs)
		{
			I_Error ("UnarchivePolyobjs: Bad polyobj count");
		}
		for (i = 0, po = polyobjs; i < po_NumPolyobjs; i++, po++)
		{
			arc >> data;
			if (data != po->tag)
			{
				I_Error ("UnarchivePolyobjs: Invalid polyobj tag");
			}
			arc >> angle;
			PO_RotatePolyobj (po->tag, angle);
			arc >> deltaX >> deltaY >> deltaZ;
			deltaX -= po->startSpot[0];
			deltaY -= po->startSpot[1];
			deltaZ -= po->startSpot[2];
			PO_MovePolyobj (po->tag, deltaX, deltaY);
		}
	}
}
