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

#define WALLFRACBITS	4
#define WALLFRACUNIT	(1<<WALLFRACBITS)


//
// Refresh internal data structures,
//	for rendering.
//

// needed for texture pegging
extern fixed_t* 		textureheight;

extern "C" int			viewwidth;
extern "C" int			realviewwidth;
extern "C" int			viewheight;
extern "C" int			realviewheight;

extern int				firstflat;

// for global animation
extern bool*			flatwarp;
extern byte**			warpedflats;
extern int*				flatwarpedwhen;
extern int*				flattranslation;
		
extern int* 			texturetranslation; 	

// Sprite....
extern int				firstspritelump;
extern int				lastspritelump;
extern int				numspritelumps;

extern size_t			numskins;	// [RH]
extern playerskin_t*	skins;		// [RH]



//
// Lookup tables for map data.
//
extern int				numsprites;
extern spritedef_t* 	sprites;

extern int				numvertexes;
extern vertex_t*		vertexes;

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

inline FArchive &operator<< (FArchive &arc, sector_t *sec)
{
	if (sec)
		return arc << (WORD)(sec - sectors);
	else
		return arc << (WORD)0xffff;
}
inline FArchive &operator>> (FArchive &arc, sector_t *&sec)
{
	WORD ofs;
	arc >> ofs;
	if (ofs == 0xffff)
		sec = NULL;
	else
		sec = sectors + ofs;
	return arc;
}

inline FArchive &operator<< (FArchive &arc, line_t *line)
{
	if (line)
		return arc << (WORD)(line - lines);
	else
		return arc << (WORD)0xffff;
}
inline FArchive &operator>> (FArchive &arc, line_t *&line)
{
	WORD ofs;
	arc >> ofs;
	if (ofs == 0xffff)
		line = NULL;
	else
		line = lines + ofs;
	return arc;
}


//
// POV data.
//
extern fixed_t			viewx;
extern fixed_t			viewy;
extern fixed_t			viewz;

extern angle_t			viewangle;
extern AActor*			camera;		// [RH] camera instead of viewplayer

extern angle_t			clipangle;

extern int				viewangletox[FINEANGLES/2];
extern angle_t			*xtoviewangle;
//extern fixed_t		finetangent[FINEANGLES/2];

extern fixed_t			rw_distance;
extern angle_t			rw_normalangle;



// angle to line origin
extern int				rw_angle1;


extern visplane_t*		floorplane;
extern visplane_t*		ceilingplane;


#endif // __R_STATE_H__
