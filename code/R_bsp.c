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
//		BSP traversal, handling of LineSegs for rendering.
//
//-----------------------------------------------------------------------------



#include "m_alloc.h"

#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"

#include "r_main.h"
#include "r_plane.h"
#include "r_draw.h"
#include "r_things.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "v_palett.h"

//#include "r_local.h"



seg_t*			curline;
side_t* 		sidedef;
line_t* 		linedef;
sector_t*		frontsector;
sector_t*		backsector;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int				doorclosed;

int				MaxDrawSegs;
drawseg_t		*drawsegs;
drawseg_t*		ds_p;

cvar_t*			r_drawflat;		// [RH] Don't texture segs?


void R_StoreWallRange (int start, int stop);




//
// R_ClearDrawSegs
//
void R_ClearDrawSegs (void)
{
	if (!drawsegs) {
		MaxDrawSegs = 256;		// [RH] Default. Increased as needed.
		drawsegs = Malloc (MaxDrawSegs * sizeof(drawseg_t));
	}
	ds_p = drawsegs;
}



//
// ClipWallSegment
// Clips the given range of columns
// and includes it in the new clip list.
//
//
// 1/11/98 killough: Since a type "short" is sufficient, we
// should use it, since smaller arrays fit better in cache.
//

typedef struct {
	short first, last;		// killough
} cliprange_t;


int				MaxSegs;

// newend is one past the last valid seg
cliprange_t*	newend;
cliprange_t		*solidsegs;
cliprange_t		*lastsolidseg;



//
// R_ClipSolidWallSegment
// Does handle solid walls,
//	e.g. single sided LineDefs (middle texture)
//	that entirely block the view.
// 
void R_ClipSolidWallSegment (int first, int last)
{
	cliprange_t *next, *start;

	// Find the first range that touches the range
	//	(adjacent pixels are touching).
	start = solidsegs;
	while (start->last < first-1)
		start++;

	if (first < start->first)
	{
		if (last < start->first-1)
		{
			// Post is entirely visible (above start), so insert a new clippost.
			R_StoreWallRange (first, last);

			// 1/11/98 killough: performance tuning using fast memmove
			memmove(start+1,start,(++newend-start)*sizeof(*start));
			start->first = first;
			start->last = last;
			return;
		}
				
		// There is a fragment above *start.
		R_StoreWallRange (first, start->first - 1);

		// Now adjust the clip size.
		start->first = first;	
	}

	// Bottom contained in start?
	if (last <= start->last)
		return; 				
				
	next = start;
	while (last >= (next+1)->first-1)
	{
		// There is a fragment between two posts.
		R_StoreWallRange (next->last + 1, (next+1)->first - 1);
		next++;
		
		if (last <= next->last)
		{
			// Bottom is contained in next. Adjust the clip size.
			start->last = next->last;	
			goto crunch;
		}
	}
		
	// There is a fragment after *next.
	R_StoreWallRange (next->last + 1, last);
	// Adjust the clip size.
	start->last = last;
		
	// Remove start+1 to next from the clip list,
	// because start now covers their area.
  crunch:
	if (next == start)
	{
		// Post just extended past the bottom of one post.
		return;
	}
	

	while (next++ != newend)
	{
		// Remove a post.
		*++start = *next;
	}

	newend = start+1;
}



//
// R_ClipPassWallSegment
// Clips the given range of columns,
//	but does not includes it in the clip list.
// Does handle windows,
//	e.g. LineDefs with upper and lower texture.
//
void R_ClipPassWallSegment (int first, int last)
{
	cliprange_t *start;

	// Find the first range that touches the range
	//	(adjacent pixels are touching).
	start = solidsegs;
	while (start->last < first-1)
		start++;

	if (first < start->first)
	{
		if (last < start->first-1)
		{
			// Post is entirely visible (above start).
			R_StoreWallRange (first, last);
			return;
		}
				
		// There is a fragment above *start.
		R_StoreWallRange (first, start->first - 1);
	}

	// Bottom contained in start?
	if (last <= start->last)
		return; 				
				
	while (last >= (start+1)->first-1)
	{
		// There is a fragment between two posts.
		R_StoreWallRange (start->last + 1, (start+1)->first - 1);
		start++;
		
		if (last <= start->last)
			return;
	}
		
	// There is a fragment after *next.
	R_StoreWallRange (start->last + 1, last);
}



//
// R_ClearClipSegs
//
void R_ClearClipSegs (void)
{
	if (!solidsegs) {
		MaxSegs = 32;	// [RH] Default. Increased as needed.
		solidsegs = Malloc (MaxSegs * sizeof(cliprange_t));
		lastsolidseg = &solidsegs[MaxSegs];
	}
	solidsegs[0].first = -0x7fff;	// new short limit --  killough
	solidsegs[0].last = -1;
	solidsegs[1].first = viewwidth;
	solidsegs[1].last = 0x7fff;		// new short limit --  killough
	newend = solidsegs+2;
}

// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// It assumes that Doom has already ruled out a door being closed because
// of front-back closure (e.g. front floor is taller than back ceiling).

int R_DoorClosed(void)
{
	return

		// if door is closed because back is shut:
		backsector->ceilingheight <= backsector->floorheight

		// preserve a kind of transparent door/lift special effect:
		&& (backsector->ceilingheight >= frontsector->ceilingheight ||
			curline->sidedef->toptexture)

		&& (backsector->floorheight <= frontsector->floorheight ||
			curline->sidedef->bottomtexture)

		// properly render skies (consider door "open" if both ceilings are sky):
		&& (backsector->ceilingpic !=skyflatnum ||
			frontsector->ceilingpic!=skyflatnum);
}

//
// killough 3/7/98: Hack floor/ceiling heights for deep water etc.
//
// If player's view height is underneath fake floor, lower the
// drawn ceiling to be just under the floor height, and replace
// the drawn floor and ceiling textures, and light level, with
// the control sector's.
//
// Similar for ceiling, only reflected.
//
// killough 4/11/98, 4/13/98: fix bugs, add 'back' parameter
//

sector_t *R_FakeFlat(sector_t *sec, sector_t *tempsec,
					 int *floorlightlevel, int *ceilinglightlevel,
					 BOOL back)
{
	if (floorlightlevel)
		*floorlightlevel = sec->floorlightsec == -1 ?
			sec->lightlevel : sectors[sec->floorlightsec].lightlevel;

	if (ceilinglightlevel)
		*ceilinglightlevel = sec->ceilinglightsec == -1 ? // killough 4/11/98
			sec->lightlevel : sectors[sec->ceilinglightsec].lightlevel;

	if (sec->heightsec != -1)
	{
		const sector_t *s = &sectors[sec->heightsec];
		int heightsec = camera->subsector->sector->heightsec;
		int underwater = heightsec!=-1 && viewz<=sectors[heightsec].floorheight;

		// Replace sector being drawn, with a copy to be hacked
		*tempsec = *sec;

		// Replace floor and ceiling height with other sector's heights.
		tempsec->floorheight   = s->floorheight;
		tempsec->ceilingheight = s->ceilingheight;

		if ((underwater && (tempsec->  floorheight = sec->floorheight,
							tempsec->ceilingheight = s->floorheight-1,
							!back)) || viewz <= s->floorheight)
		{					// head-below-floor hack
			tempsec->floorpic    = s->floorpic;
			tempsec->floor_xoffs = s->floor_xoffs;
			tempsec->floor_yoffs = s->floor_yoffs;

			if (underwater) {
				if (s->ceilingpic == skyflatnum)
				{
					tempsec->floorheight   = tempsec->ceilingheight+1;
					tempsec->ceilingpic    = tempsec->floorpic;
					tempsec->ceiling_xoffs = tempsec->floor_xoffs;
					tempsec->ceiling_yoffs = tempsec->floor_yoffs;
				}
				else
				{
					tempsec->ceilingpic    = s->ceilingpic;
					tempsec->ceiling_xoffs = s->ceiling_xoffs;
					tempsec->ceiling_yoffs = s->ceiling_yoffs;
				}
			}

			tempsec->lightlevel  = s->lightlevel;

			if (floorlightlevel)
				*floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
					sectors[s->floorlightsec].lightlevel; // killough 3/16/98

			if (ceilinglightlevel)
				*ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
					sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
		}
		else
			if (heightsec != -1 && viewz >= sectors[heightsec].ceilingheight &&
				sec->ceilingheight > s->ceilingheight)
			{	// Above-ceiling hack
				tempsec->ceilingheight = s->ceilingheight;
				tempsec->floorheight   = s->ceilingheight + 1;

				tempsec->floorpic    = tempsec->ceilingpic    = s->ceilingpic;
				tempsec->floor_xoffs = tempsec->ceiling_xoffs = s->ceiling_xoffs;
				tempsec->floor_yoffs = tempsec->ceiling_yoffs = s->ceiling_yoffs;

				if (s->floorpic != skyflatnum)
				{
					tempsec->ceilingheight = sec->ceilingheight;
					tempsec->floorpic      = s->floorpic;
					tempsec->floor_xoffs   = s->floor_xoffs;
					tempsec->floor_yoffs   = s->floor_yoffs;
				}

				tempsec->lightlevel  = s->lightlevel;

				if (floorlightlevel)
					*floorlightlevel = s->floorlightsec == -1 ? s->lightlevel :
						sectors[s->floorlightsec].lightlevel; // killough 3/16/98

				if (ceilinglightlevel)
					*ceilinglightlevel = s->ceilinglightsec == -1 ? s->lightlevel :
						sectors[s->ceilinglightsec].lightlevel; // killough 4/11/98
			}
		sec = tempsec;					// Use other sector
	}
	return sec;
}


//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//
void R_AddLine (seg_t *line)
{
	int 			x1;
	int 			x2;
	angle_t 		angle1;
	angle_t 		angle2;
	angle_t 		span;
	angle_t 		tspan;
	static sector_t tempsec;	// killough 3/8/98: ceiling/water hack
	
	curline = line;

	// [RH] Color if not texturing line
	dc_color = ((line - segs) & 31) * 4;

	// OPTIMIZE: quickly reject orthogonal back sides.
	angle1 = R_PointToAngle (line->v1->x, line->v1->y);
	angle2 = R_PointToAngle (line->v2->x, line->v2->y);
	
	// Clip to view edges.
	// OPTIMIZE: make constant out of 2*clipangle (FIELDOFVIEW).
	span = angle1 - angle2;
	
	// Back side? I.e. backface culling?
	if (span >= ANG180)
		return; 		

	// Global angle needed by segcalc.
	rw_angle1 = angle1;
	angle1 -= viewangle;
	angle2 -= viewangle;
		
	tspan = angle1 + clipangle;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return;
		
		angle1 = clipangle;
	}
	tspan = clipangle - angle2;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return; 	
		angle2 = (unsigned) (-(int)clipangle);
	}
	
	// The seg is in the view range,
	// but not necessarily visible.
	angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
	angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;

	// killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
	x1 = viewangletox[angle1];
	x2 = viewangletox[angle2];

	// Does not cross a pixel?
	if (x1 >= x2)	// killough 1/31/98 -- change == to >= for robustness
		return;

	backsector = line->backsector;

	// Single sided line?
	if (!backsector)
		goto clipsolid;

	// killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
	backsector = R_FakeFlat (backsector, &tempsec, NULL, NULL, true);

	doorclosed = 0;		// killough 4/16/98

	// Closed door.
	if (backsector->ceilingheight <= frontsector->floorheight
		|| backsector->floorheight >= frontsector->ceilingheight)
		goto clipsolid;

	// This fixes the automap floor height bug -- killough 1/18/98:
	// killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
	if ((doorclosed = R_DoorClosed()))
		goto clipsolid;

	// Window.
	if (backsector->ceilingheight != frontsector->ceilingheight
		|| backsector->floorheight != frontsector->floorheight)
		goto clippass;	
				
	// Reject empty lines used for triggers
	//	and special events.
	// Identical floor and ceiling on both sides,
	// identical light levels on both sides,
	// and no middle texture.
	if (backsector->lightlevel == frontsector->lightlevel
		&& backsector->floorpic == frontsector->floorpic
		&& backsector->ceilingpic == frontsector->ceilingpic
		&& curline->sidedef->midtexture == 0

		// killough 3/7/98: Take flats offsets into account:
		&& backsector->floor_xoffs == frontsector->floor_xoffs
		&& backsector->floor_yoffs == frontsector->floor_yoffs
		&& backsector->ceiling_xoffs == frontsector->ceiling_xoffs
		&& backsector->ceiling_yoffs == frontsector->ceiling_yoffs

		// killough 4/16/98: consider altered lighting
		&& backsector->floorlightsec == frontsector->floorlightsec
		&& backsector->ceilinglightsec == frontsector->ceilinglightsec

		// [RH] Also consider colormaps
		&& backsector->colormap == frontsector->colormap
		)
	{
		return;
	}
	
								
  clippass:
	R_ClipPassWallSegment (x1, x2-1);	
	return;
				
  clipsolid:
	R_ClipSolidWallSegment (x1, x2-1);
}


//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//	if some part of the bbox might be visible.
//
static const int checkcoord[12][4] = // killough -- static const
{
	{3,0,2,1},
	{3,0,2,0},
	{3,1,2,0},
	{0},
	{2,0,2,1},
	{0,0,0,0},
	{3,1,3,0},
	{0},
	{2,0,3,1},
	{2,1,3,1},
	{2,1,3,0}
};


static BOOL R_CheckBBox (fixed_t *bspcoord)	// killough 1/28/98: static
{
	int 				boxx;
	int 				boxy;
	int 				boxpos;

	fixed_t 			x1;
	fixed_t 			y1;
	fixed_t 			x2;
	fixed_t 			y2;
	
	angle_t 			angle1;
	angle_t 			angle2;
	angle_t 			span;
	angle_t 			tspan;
	
	cliprange_t*		start;

	int 				sx1;
	int 				sx2;
	
	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (viewx <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (viewx < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;
				
	if (viewy >= bspcoord[BOXTOP])
		boxy = 0;
	else if (viewy > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;
				
	boxpos = (boxy<<2)+boxx;
	if (boxpos == 5)
		return true;
		
	x1 = bspcoord[checkcoord[boxpos][0]];
	y1 = bspcoord[checkcoord[boxpos][1]];
	x2 = bspcoord[checkcoord[boxpos][2]];
	y2 = bspcoord[checkcoord[boxpos][3]];
	
	// check clip list for an open space
	angle1 = R_PointToAngle (x1, y1) - viewangle;
	angle2 = R_PointToAngle (x2, y2) - viewangle;
		
	span = angle1 - angle2;

	// Sitting on a line?
	if (span >= ANG180)
		return true;
	
	tspan = angle1 + clipangle;

	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return false;		

		angle1 = clipangle;
	}
	tspan = clipangle - angle2;
	if (tspan > 2*clipangle)
	{
		tspan -= 2*clipangle;

		// Totally off the left edge?
		if (tspan >= span)
			return false;
		
		angle2 = (unsigned)(-(int)clipangle);
	}


	// Find the first clippost
	//	that touches the source post
	//	(adjacent pixels are touching).
	angle1 = (angle1+ANG90)>>ANGLETOFINESHIFT;
	angle2 = (angle2+ANG90)>>ANGLETOFINESHIFT;
	sx1 = viewangletox[angle1];
	sx2 = viewangletox[angle2];

	// Does not cross a pixel.
	if (sx1 == sx2)
		return false;					
	sx2--;
		
	start = solidsegs;
	while (start->last < sx2)
		start++;
	
	if (sx1 >= start->first && sx2 <= start->last)
	{
		// The clippost contains the new span.
		return false;
	}

	return true;
}



//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
void R_Subsector (int num)
{
	int 		 count;
	seg_t*		 line;
	subsector_t *sub;
	sector_t     tempsec;				// killough 3/7/98: deep water hack
	int          floorlightlevel;		// killough 3/16/98: set floor lightlevel
	int          ceilinglightlevel;		// killough 4/11/98

#ifdef RANGECHECK
	if (num>=numsubsectors)
		I_Error ("R_Subsector: ss %i with numss = %i", num, numsubsectors);
#endif

	sub = &subsectors[num];
	frontsector = sub->sector;
	count = sub->numlines;
	line = &segs[sub->firstline];

	basecolormap = frontsector->colormap->maps;	// [RH] set basecolormap

	// killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
	frontsector = R_FakeFlat(frontsector, &tempsec, &floorlightlevel,
						   &ceilinglightlevel, false);	// killough 4/11/98

	// killough 3/7/98: Add (x,y) offsets to flats, add deep water check
	// killough 3/16/98: add floorlightlevel
	// killough 10/98: add support for skies transferred from sidedefs
	floorplane = frontsector->floorheight < viewz || // killough 3/7/98
		(frontsector->heightsec != -1 &&
		 sectors[frontsector->heightsec].ceilingpic == skyflatnum) ?
		R_FindPlane(frontsector->floorheight,
					frontsector->floorpic == skyflatnum &&  // kilough 10/98
						frontsector->sky & PL_SKYFLAT ? frontsector->sky :
						frontsector->floorpic,
					floorlightlevel,				// killough 3/16/98
					frontsector->floor_xoffs,		// killough 3/7/98
					frontsector->floor_yoffs
					) : NULL;

	ceilingplane = frontsector->ceilingheight > viewz ||
		frontsector->ceilingpic == skyflatnum ||
		(frontsector->heightsec != -1 &&
		 sectors[frontsector->heightsec].floorpic == skyflatnum) ?
		R_FindPlane(frontsector->ceilingheight,		// killough 3/8/98
					frontsector->ceilingpic == skyflatnum &&  // kilough 10/98
						frontsector->sky & PL_SKYFLAT ? frontsector->sky :
						frontsector->ceilingpic,
					ceilinglightlevel,				// killough 4/11/98
					frontsector->ceiling_xoffs,		// killough 3/7/98
					frontsector->ceiling_yoffs
					) : NULL;

	// [RH] set foggy flag
	foggy = (level.fadeto || sub->sector->colormap->fade);

	// killough 9/18/98: Fix underwater slowdown, by passing real sector 
	// instead of fake one. Improve sprite lighting by basing sprite
	// lightlevels on floor & ceiling lightlevels in the surrounding area.
	R_AddSprites (sub->sector, (floorlightlevel + ceilinglightlevel) / 2);

	if (sub->poly)
	{ // Render the polyobj in the subsector first
		int polyCount = sub->poly->numsegs;
		seg_t **polySeg = sub->poly->segs;
		while (polyCount--)
		{
			R_AddLine (*polySeg++);
		}
	}

	while (count--)
	{
		R_AddLine (line++);
	}
}




//
// RenderBSPNode
// Renders all subsectors below a given node,
//	traversing subtree recursively.
// Just call with BSP root.
// killough 5/2/98: reformatted, removed tail recursion

void R_RenderBSPNode (int bspnum)
{
	while (!(bspnum & NF_SUBSECTOR))  // Found a subsector?
	{
		node_t *bsp = &nodes[bspnum];

		// Decide which side the view point is on.
		int side = R_PointOnSide(viewx, viewy, bsp);

		// Recursively divide front space.
		R_RenderBSPNode(bsp->children[side]);

		// Possibly divide back space.

		if (!R_CheckBBox(bsp->bbox[side^1]))
			return;

		bspnum = bsp->children[side^1];
	}
	R_Subsector(bspnum == -1 ? 0 : bspnum & ~NF_SUBSECTOR);
}
