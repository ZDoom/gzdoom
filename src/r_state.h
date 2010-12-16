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
// DESCRIPTION:
//		Refresh/render internal state variables (global).
//
//-----------------------------------------------------------------------------


#ifndef __R_STATE_H__
#define __R_STATE_H__

// Need data structure definitions.
#include "d_player.h"
#include "r_data.h"

//
// Refresh internal data structures,
//	for rendering.
//

extern "C" int			viewwidth;
extern "C" int			viewheight;

// Sprite....
extern int				firstspritelump;
extern int				lastspritelump;
extern int				numspritelumps;

extern size_t			numskins;	// [RH]
extern FPlayerSkin	*	skins;		// [RH]

extern BYTE				OtherGameSkinRemap[256];
extern PalEntry			OtherGameSkinPalette[256];


//
// Lookup tables for map data.
//
extern TArray<spritedef_t> sprites;
extern DWORD NumStdSprites;

extern int				numvertexes;
extern vertex_t*		vertexes;
extern int				numvertexdatas;
extern vertexdata_t*		vertexdatas;

extern int				numsegs;
extern seg_t*			segs;

extern int				numsectors;
extern sector_t*		sectors;

extern int				numsubsectors;
extern subsector_t* 	subsectors;

extern int				numnodes;
extern node_t*			nodes;

extern int				numlines;
extern line_t*			lines;

extern int				numsides;
extern side_t*			sides;

extern int				numzones;
extern zone_t*			zones;

extern node_t * 		gamenodes;
extern int 				numgamenodes;

extern subsector_t * 	gamesubsectors;
extern int 				numgamesubsectors;


inline FArchive &operator<< (FArchive &arc, sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

inline FArchive &operator<< (FArchive &arc, const sector_t *&sec)
{
	return arc.SerializePointer (sectors, (BYTE **)&sec, sizeof(*sectors));
}

inline FArchive &operator<< (FArchive &arc, line_t *&line)
{
	return arc.SerializePointer (lines, (BYTE **)&line, sizeof(*lines));
}

inline FArchive &operator<< (FArchive &arc, vertex_t *&vert)
{
	return arc.SerializePointer (vertexes, (BYTE **)&vert, sizeof(*vertexes));
}

inline FArchive &operator<< (FArchive &arc, side_t *&side)
{
	return arc.SerializePointer (sides, (BYTE **)&side, sizeof(*sides));
}

inline FArchive &operator<< (FArchive &arc, FLinkedSector &link)
{
	arc << link.Sector << link.Type;
	return arc;
}


//
// POV data.
//
extern fixed_t			viewz;
extern angle_t			viewangle;

extern AActor*			camera;		// [RH] camera instead of viewplayer
extern sector_t*		viewsector;	// [RH] keep track of sector viewing from

extern angle_t			xtoviewangle[MAXWIDTH+1];
extern int				FieldOfView;

#endif // __R_STATE_H__
