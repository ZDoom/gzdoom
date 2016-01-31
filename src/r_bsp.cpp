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
//		BSP traversal, handling of LineSegs for rendering.
//
// This file contains some code from the Build Engine.
//
// "Build Engine & Tools" Copyright (c) 1993-1997 Ken Silverman
// Ken Silverman's official web site: "http://www.advsys.net/ken"
// See the included license file "BUILDLIC.TXT" for license info.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>

#include "templates.h"

#include "doomdef.h"

#include "m_bbox.h"

#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"

#include "r_main.h"
#include "r_plane.h"
#include "r_draw.h"
#include "r_things.h"
#include "r_3dfloors.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"

// State.
#include "doomstat.h"
#include "r_state.h"
#include "r_bsp.h"
#include "r_segs.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"

seg_t*			curline;
side_t* 		sidedef;
line_t* 		linedef;
sector_t*		frontsector;
sector_t*		backsector;

// killough 4/7/98: indicates doors closed wrt automap bugfix:
int				doorclosed;

bool			r_fakingunderwater;

extern bool		rw_prepped;
extern bool		rw_havehigh, rw_havelow;
extern int		rw_floorstat, rw_ceilstat;
extern bool		rw_mustmarkfloor, rw_mustmarkceiling;
extern short	walltop[MAXWIDTH];	// [RH] record max extents of wall
extern short	wallbottom[MAXWIDTH];
extern short	wallupper[MAXWIDTH];
extern short	walllower[MAXWIDTH];

fixed_t			rw_backcz1, rw_backcz2;
fixed_t			rw_backfz1, rw_backfz2;
fixed_t			rw_frontcz1, rw_frontcz2;
fixed_t			rw_frontfz1, rw_frontfz2;


size_t			MaxDrawSegs;
drawseg_t		*drawsegs;
drawseg_t*		firstdrawseg;
drawseg_t*		ds_p;

size_t			FirstInterestingDrawseg;
TArray<size_t>	InterestingDrawsegs;

FWallCoords		WallC;
FWallTmapVals	WallT;

static BYTE		FakeSide;

int WindowLeft, WindowRight;
WORD MirrorFlags;
seg_t *ActiveWallMirror;
TArray<size_t> WallMirrors;

static subsector_t *InSubsector;

CVAR (Bool, r_drawflat, false, 0)		// [RH] Don't texture segs?


void R_StoreWallRange (int start, int stop);

//
// R_ClearDrawSegs
//
void R_ClearDrawSegs (void)
{
	if (drawsegs == NULL)
	{
		MaxDrawSegs = 256;		// [RH] Default. Increased as needed.
		firstdrawseg = drawsegs = (drawseg_t *)M_Malloc (MaxDrawSegs * sizeof(drawseg_t));
	}
	FirstInterestingDrawseg = 0;
	InterestingDrawsegs.Clear ();
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

struct cliprange_t
{
	short first, last;		// killough
};


// newend is one past the last valid seg
static cliprange_t     *newend;
static cliprange_t		solidsegs[MAXWIDTH/2+2];



//==========================================================================
//
// R_ClipWallSegment
//
// Clips the given range of columns, possibly including it in the clip list.
// Handles both windows (e.g. LineDefs with upper and lower textures) and
// solid walls (e.g. single sided LineDefs [middle texture]) that entirely
// block the view.
//
//==========================================================================

bool R_ClipWallSegment (int first, int last, bool solid)
{
	cliprange_t *next, *start;
	int i, j;
	bool res = false;

	// Find the first range that touches the range
	// (adjacent pixels are touching).
	start = solidsegs;
	while (start->last < first)
		start++;

	if (first < start->first)
	{
		res = true;
		if (last <= start->first)
		{
			// Post is entirely visible (above start).
			R_StoreWallRange (first, last);
			if (fake3D & FAKE3D_FAKEMASK)
			{
				return true;
			}

			// Insert a new clippost for solid walls.
			if (solid)
			{
				if (last == start->first)
				{
					start->first = first;
				}
				else
				{
					next = newend;
					newend++;
					while (next != start)
					{
						*next = *(next-1);
						next--;
					}
					next->first = first;
					next->last = last;
				}
			}
			return true;
		}

		// There is a fragment above *start.
		R_StoreWallRange (first, start->first);

		// Adjust the clip size for solid walls
		if (solid && !(fake3D & FAKE3D_FAKEMASK))
		{
			start->first = first;
		}
	}

	// Bottom contained in start?
	if (last <= start->last)
		return res;

	next = start;
	while (last >= (next+1)->first)
	{
		// There is a fragment between two posts.
		R_StoreWallRange (next->last, (next+1)->first);
		next++;
		
		if (last <= next->last)
		{
			// Bottom is contained in next.
			last = next->last;
			goto crunch;
		}
	}

	// There is a fragment after *next.
	R_StoreWallRange (next->last, last);

crunch:
	if (fake3D & FAKE3D_FAKEMASK)
	{
		return true;
	}
	if (solid)
	{
		// Adjust the clip size.
		start->last = last;

		if (next != start)
		{
			// Remove start+1 to next from the clip list,
			// because start now covers their area.
			for (i = 1, j = (int)(newend - next); j > 0; i++, j--)
			{
				start[i] = next[i];
			}
			newend = start+i;
		}
	}
	return true;
}

bool R_CheckClipWallSegment (int first, int last)
{
	cliprange_t *start;

	// Find the first range that touches the range
	// (adjacent pixels are touching).
	start = solidsegs;
	while (start->last < first)
		start++;

	if (first < start->first)
	{
		return true;
	}

	// Bottom contained in start?
	if (last > start->last)
	{
		return true;
	}

	return false;
}



//
// R_ClearClipSegs
//
void R_ClearClipSegs (short left, short right)
{
	solidsegs[0].first = -0x7fff;	// new short limit --  killough
	solidsegs[0].last = left;
	solidsegs[1].first = right;
	solidsegs[1].last = 0x7fff;		// new short limit --  killough
	newend = solidsegs+2;
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
					 bool back)
{
	// [RH] allow per-plane lighting
	if (floorlightlevel != NULL)
	{
		*floorlightlevel = sec->GetFloorLight ();
	}

	if (ceilinglightlevel != NULL)
	{
		*ceilinglightlevel = sec->GetCeilingLight ();
	}

	FakeSide = FAKED_Center;

	const sector_t *s = sec->GetHeightSec();
	if (s != NULL)
	{
		sector_t *heightsec = viewsector->heightsec;
		bool underwater = r_fakingunderwater ||
			(heightsec && heightsec->floorplane.PointOnSide(viewx, viewy, viewz) <= 0);
		bool doorunderwater = false;
		int diffTex = (s->MoreFlags & SECF_CLIPFAKEPLANES);

		// Replace sector being drawn with a copy to be hacked
		*tempsec = *sec;

		// Replace floor and ceiling height with control sector's heights.
		if (diffTex)
		{
			if (s->floorplane.CopyPlaneIfValid (&tempsec->floorplane, &sec->ceilingplane))
			{
				tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
			}
			else if (s->MoreFlags & SECF_FAKEFLOORONLY)
			{
				if (underwater)
				{
					tempsec->ColorMap = s->ColorMap;
					if (!(s->MoreFlags & SECF_NOFAKELIGHT))
					{
						tempsec->lightlevel = s->lightlevel;

						if (floorlightlevel != NULL)
						{
							*floorlightlevel = s->GetFloorLight ();
						}

						if (ceilinglightlevel != NULL)
						{
							*ceilinglightlevel = s->GetCeilingLight ();
						}
					}
					FakeSide = FAKED_BelowFloor;
					return tempsec;
				}
				return sec;
			}
		}
		else
		{
			tempsec->floorplane = s->floorplane;
		}

		if (!(s->MoreFlags & SECF_FAKEFLOORONLY))
		{
			if (diffTex)
			{
				if (s->ceilingplane.CopyPlaneIfValid (&tempsec->ceilingplane, &sec->floorplane))
				{
					tempsec->SetTexture(sector_t::ceiling, s->GetTexture(sector_t::ceiling), false);
				}
			}
			else
			{
				tempsec->ceilingplane  = s->ceilingplane;
			}
		}

		fixed_t refceilz = s->ceilingplane.ZatPoint (viewx, viewy);
		fixed_t orgceilz = sec->ceilingplane.ZatPoint (viewx, viewy);

#if 1
		// [RH] Allow viewing underwater areas through doors/windows that
		// are underwater but not in a water sector themselves.
		// Only works if you cannot see the top surface of any deep water
		// sectors at the same time.
		if (back && !r_fakingunderwater && curline->frontsector->heightsec == NULL)
		{
			if (rw_frontcz1 <= s->floorplane.ZatPoint (curline->v1->x, curline->v1->y) &&
				rw_frontcz2 <= s->floorplane.ZatPoint (curline->v2->x, curline->v2->y))
			{
				// Check that the window is actually visible
				for (int z = WallC.sx1; z < WallC.sx2; ++z)
				{
					if (floorclip[z] > ceilingclip[z])
					{
						doorunderwater = true;
						r_fakingunderwater = true;
						break;
					}
				}
			}
		}
#endif

		if (underwater || doorunderwater)
		{
			tempsec->floorplane = sec->floorplane;
			tempsec->ceilingplane = s->floorplane;
			tempsec->ceilingplane.FlipVert ();
			tempsec->ceilingplane.ChangeHeight (-1);
			tempsec->ColorMap = s->ColorMap;
		}

		// killough 11/98: prevent sudden light changes from non-water sectors:
		if ((underwater && !back) || doorunderwater)
		{					// head-below-floor hack
			tempsec->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
			tempsec->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			tempsec->ceilingplane		= s->floorplane;
			tempsec->ceilingplane.FlipVert ();
			tempsec->ceilingplane.ChangeHeight (-1);
			if (s->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				tempsec->floorplane			= tempsec->ceilingplane;
				tempsec->floorplane.FlipVert ();
				tempsec->floorplane.ChangeHeight (+1);
				tempsec->SetTexture(sector_t::ceiling, tempsec->GetTexture(sector_t::floor), false);
				tempsec->planes[sector_t::ceiling].xform = tempsec->planes[sector_t::floor].xform;
			}
			else
			{
				tempsec->SetTexture(sector_t::ceiling, diffTex ? s->GetTexture(sector_t::floor) : s->GetTexture(sector_t::ceiling), false);
				tempsec->planes[sector_t::ceiling].xform = s->planes[sector_t::ceiling].xform;
			}

			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				tempsec->lightlevel = s->lightlevel;

				if (floorlightlevel != NULL)
				{
					*floorlightlevel = s->GetFloorLight ();
				}

				if (ceilinglightlevel != NULL)
				{
					*ceilinglightlevel = s->GetCeilingLight ();
				}
			}
			FakeSide = FAKED_BelowFloor;
		}
		else if (heightsec && heightsec->ceilingplane.PointOnSide(viewx, viewy, viewz) <= 0 &&
				 orgceilz > refceilz && !(s->MoreFlags & SECF_FAKEFLOORONLY))
		{	// Above-ceiling hack
			tempsec->ceilingplane		= s->ceilingplane;
			tempsec->floorplane			= s->ceilingplane;
			tempsec->floorplane.FlipVert ();
			tempsec->floorplane.ChangeHeight (+1);
			tempsec->ColorMap			= s->ColorMap;
			tempsec->ColorMap			= s->ColorMap;

			tempsec->SetTexture(sector_t::ceiling, diffTex ? sec->GetTexture(sector_t::ceiling) : s->GetTexture(sector_t::ceiling), false);
			tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::ceiling), false);
			tempsec->planes[sector_t::ceiling].xform = tempsec->planes[sector_t::floor].xform = s->planes[sector_t::ceiling].xform;

			if (s->GetTexture(sector_t::floor) != skyflatnum)
			{
				tempsec->ceilingplane	= sec->ceilingplane;
				tempsec->SetTexture(sector_t::floor, s->GetTexture(sector_t::floor), false);
				tempsec->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;
			}

			if (!(s->MoreFlags & SECF_NOFAKELIGHT))
			{
				tempsec->lightlevel  = s->lightlevel;

				if (floorlightlevel != NULL)
				{
					*floorlightlevel = s->GetFloorLight ();
				}

				if (ceilinglightlevel != NULL)
				{
					*ceilinglightlevel = s->GetCeilingLight ();
				}
			}
			FakeSide = FAKED_AboveCeiling;
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
	static sector_t tempsec;	// killough 3/8/98: ceiling/water hack
	bool			solid;
	fixed_t			tx1, tx2, ty1, ty2;

	curline = line;

	// [RH] Color if not texturing line
	dc_color = (((int)(line - segs) * 8) + 4) & 255;

	tx1 = line->v1->x - viewx;
	tx2 = line->v2->x - viewx;
	ty1 = line->v1->y - viewy;
	ty2 = line->v2->y - viewy;

	// Reject lines not facing viewer
	if (DMulScale32 (ty1, tx1-tx2, tx1, ty2-ty1) >= 0)
		return;

	if (WallC.Init(tx1, ty1, tx2, ty2, 32))
		return;

	if (WallC.sx1 >= WindowRight || WallC.sx2 <= WindowLeft)
		return;

	if (line->linedef == NULL)
	{
		if (R_CheckClipWallSegment (WallC.sx1, WallC.sx2))
		{
			InSubsector->flags |= SSECF_DRAWN;
		}
		return;
	}

	vertex_t *v1, *v2;

	v1 = line->linedef->v1;
	v2 = line->linedef->v2;

	if ((v1 == line->v1 && v2 == line->v2) || (v2 == line->v1 && v1 == line->v2))
	{ // The seg is the entire wall.
		WallT.InitFromWallCoords(&WallC);
	}
	else
	{ // The seg is only part of the wall.
		if (line->linedef->sidedef[0] != line->sidedef)
		{
			swapvalues (v1, v2);
		}
		WallT.InitFromLine(v1->x - viewx, v1->y - viewy, v2->x - viewx, v2->y - viewy);
	}

	if (!(fake3D & FAKE3D_FAKEBACK))
	{
		backsector = line->backsector;
	}
	rw_frontcz1 = frontsector->ceilingplane.ZatPoint (line->v1->x, line->v1->y);
	rw_frontfz1 = frontsector->floorplane.ZatPoint (line->v1->x, line->v1->y);
	rw_frontcz2 = frontsector->ceilingplane.ZatPoint (line->v2->x, line->v2->y);
	rw_frontfz2 = frontsector->floorplane.ZatPoint (line->v2->x, line->v2->y);

	rw_mustmarkfloor = rw_mustmarkceiling = false;
	rw_havehigh = rw_havelow = false;

	// Single sided line?
	if (backsector == NULL)
	{
		solid = true;
	}
	else
	{
		// kg3D - its fake, no transfer_heights
		if (!(fake3D & FAKE3D_FAKEBACK))
		{ // killough 3/8/98, 4/4/98: hack for invisible ceilings / deep water
			backsector = R_FakeFlat (backsector, &tempsec, NULL, NULL, true);
		}
		doorclosed = 0;		// killough 4/16/98

		rw_backcz1 = backsector->ceilingplane.ZatPoint (line->v1->x, line->v1->y);
		rw_backfz1 = backsector->floorplane.ZatPoint (line->v1->x, line->v1->y);
		rw_backcz2 = backsector->ceilingplane.ZatPoint (line->v2->x, line->v2->y);
		rw_backfz2 = backsector->floorplane.ZatPoint (line->v2->x, line->v2->y);

		// Cannot make these walls solid, because it can result in
		// sprite clipping problems for sprites near the wall
		if (rw_frontcz1 > rw_backcz1 || rw_frontcz2 > rw_backcz2)
		{
			rw_havehigh = true;
			WallMost (wallupper, backsector->ceilingplane, &WallC);
		}
		if (rw_frontfz1 < rw_backfz1 || rw_frontfz2 < rw_backfz2)
		{
			rw_havelow = true;
			WallMost (walllower, backsector->floorplane, &WallC);
		}

		// Closed door.
		if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
			(rw_backfz1 >= rw_frontcz1 && rw_backfz2 >= rw_frontcz2))
		{
			solid = true;
		}
		else if (
		// properly render skies (consider door "open" if both ceilings are sky):
		(backsector->GetTexture(sector_t::ceiling) != skyflatnum || frontsector->GetTexture(sector_t::ceiling) != skyflatnum)

		// if door is closed because back is shut:
		&& rw_backcz1 <= rw_backfz1 && rw_backcz2 <= rw_backfz2

		// preserve a kind of transparent door/lift special effect:
		&& ((rw_backcz1 >= rw_frontcz1 && rw_backcz2 >= rw_frontcz2) || line->sidedef->GetTexture(side_t::top).isValid())
		&& ((rw_backfz1 <= rw_frontfz1 && rw_backfz2 <= rw_frontfz2) || line->sidedef->GetTexture(side_t::bottom).isValid()))
		{
		// killough 1/18/98 -- This function is used to fix the automap bug which
		// showed lines behind closed doors simply because the door had a dropoff.
		//
		// It assumes that Doom has already ruled out a door being closed because
		// of front-back closure (e.g. front floor is taller than back ceiling).

		// This fixes the automap floor height bug -- killough 1/18/98:
		// killough 4/7/98: optimize: save result in doorclosed for use in r_segs.c
			doorclosed = true;
			solid = true;
		}
		else if (frontsector->ceilingplane != backsector->ceilingplane ||
			frontsector->floorplane != backsector->floorplane)
		{
		// Window.
			solid = false;
		}
		else if (backsector->lightlevel != frontsector->lightlevel
			|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)
			|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)
			|| curline->sidedef->GetTexture(side_t::mid).isValid()

			// killough 3/7/98: Take flats offsets into account:
			|| backsector->GetXOffset(sector_t::floor) != frontsector->GetXOffset(sector_t::floor)
			|| backsector->GetYOffset(sector_t::floor) != frontsector->GetYOffset(sector_t::floor)
			|| backsector->GetXOffset(sector_t::ceiling) != frontsector->GetXOffset(sector_t::ceiling)
			|| backsector->GetYOffset(sector_t::ceiling) != frontsector->GetYOffset(sector_t::ceiling)

			|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
			|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
			|| backsector->GetFlags(sector_t::floor) != frontsector->GetFlags(sector_t::floor)
			|| backsector->GetFlags(sector_t::ceiling) != frontsector->GetFlags(sector_t::ceiling)

			// [RH] Also consider colormaps
			|| backsector->ColorMap != frontsector->ColorMap

			// [RH] and scaling
			|| backsector->GetXScale(sector_t::floor) != frontsector->GetXScale(sector_t::floor)
			|| backsector->GetYScale(sector_t::floor) != frontsector->GetYScale(sector_t::floor)
			|| backsector->GetXScale(sector_t::ceiling) != frontsector->GetXScale(sector_t::ceiling)
			|| backsector->GetYScale(sector_t::ceiling) != frontsector->GetYScale(sector_t::ceiling)

			// [RH] and rotation
			|| backsector->GetAngle(sector_t::floor) != frontsector->GetAngle(sector_t::floor)
			|| backsector->GetAngle(sector_t::ceiling) != frontsector->GetAngle(sector_t::ceiling)

			// kg3D - and fake lights
			|| (frontsector->e && frontsector->e->XFloor.lightlist.Size())
			|| (backsector->e && backsector->e->XFloor.lightlist.Size())
			)
		{
			solid = false;
		}
		else
		{
			// Reject empty lines used for triggers and special events.
			// Identical floor and ceiling on both sides, identical light levels
			// on both sides, and no middle texture.

			// When using GL nodes, do a clipping test for these lines so we can
			// mark their subsectors as visible for automap texturing.
			if (hasglnodes && !(InSubsector->flags & SSECF_DRAWN))
			{
				if (R_CheckClipWallSegment(WallC.sx1, WallC.sx2))
				{
					InSubsector->flags |= SSECF_DRAWN;
				}
			}
			return;
		}
	}

	rw_prepped = false;

	if (line->linedef->special == Line_Horizon)
	{
		// Be aware: Line_Horizon does not work properly with sloped planes
		clearbufshort (walltop+WallC.sx1, WallC.sx2 - WallC.sx1, centery);
		clearbufshort (wallbottom+WallC.sx1, WallC.sx2 - WallC.sx1, centery);
	}
	else
	{
		rw_ceilstat = WallMost (walltop, frontsector->ceilingplane, &WallC);
		rw_floorstat = WallMost (wallbottom, frontsector->floorplane, &WallC);

		// [RH] treat off-screen walls as solid
#if 0	// Maybe later...
		if (!solid)
		{
			if (rw_ceilstat == 12 && line->sidedef->GetTexture(side_t::top) != 0)
			{
				rw_mustmarkceiling = true;
				solid = true;
			}
			if (rw_floorstat == 3 && line->sidedef->GetTexture(side_t::bottom) != 0)
			{
				rw_mustmarkfloor = true;
				solid = true;
			}
		}
#endif
	}

	if (R_ClipWallSegment (WallC.sx1, WallC.sx2, solid))
	{
		InSubsector->flags |= SSECF_DRAWN;
	}
}

//
// FWallCoords :: Init
//
// Transform and clip coordinates. Returns true if it was clipped away
//
bool FWallCoords::Init(int x1, int y1, int x2, int y2, int too_close)
{
	tx1 = DMulScale20(x1, viewsin, -y1, viewcos);
	tx2 = DMulScale20(x2, viewsin, -y2, viewcos);

	ty1 = DMulScale20(x1, viewtancos, y1, viewtansin);
	ty2 = DMulScale20(x2, viewtancos, y2, viewtansin);

	if (MirrorFlags & RF_XFLIP)
	{
		int t = 256 - tx1;
		tx1 = 256 - tx2;
		tx2 = t;
		swapvalues(ty1, ty2);
	}

	if (tx1 >= -ty1)
	{
		if (tx1 > ty1) return true;	// left edge is off the right side
		if (ty1 == 0) return true;
		sx1 = (centerxfrac + Scale(tx1, centerxfrac, ty1)) >> FRACBITS;
		if (tx1 >= 0) sx1 = MIN(viewwidth, sx1+1); // fix for signed divide
		sz1 = ty1;
	}
	else
	{
		if (tx2 < -ty2) return true;	// wall is off the left side
		fixed_t den = tx1 - tx2 - ty2 + ty1;	
		if (den == 0) return true;
		sx1 = 0;
		sz1 = ty1 + Scale(ty2 - ty1, tx1 + ty1, den);
	}

	if (sz1 < too_close)
		return true;

	if (tx2 <= ty2)
	{
		if (tx2 < -ty2) return true;	// right edge is off the left side
		if (ty2 == 0) return true;
		sx2 = (centerxfrac + Scale(tx2, centerxfrac, ty2)) >> FRACBITS;
		if (tx2 >= 0) sx2 = MIN(viewwidth, sx2+1);	// fix for signed divide
		sz2 = ty2;
	}
	else
	{
		if (tx1 > ty1) return true;	// wall is off the right side
		fixed_t den = ty2 - ty1 - tx2 + tx1;
		if (den == 0) return true;
		sx2 = viewwidth;
		sz2 = ty1 + Scale(ty2 - ty1, tx1 - ty1, den);
	}

	if (sz2 < too_close || sx2 <= sx1)
		return true;

	return false;
}

void FWallTmapVals::InitFromWallCoords(const FWallCoords *wallc)
{
	if (MirrorFlags & RF_XFLIP)
	{
		UoverZorg = (float)wallc->tx2 * centerx;
		UoverZstep = (float)(-wallc->ty2);
		InvZorg = (float)(wallc->tx2 - wallc->tx1) * centerx;
		InvZstep = (float)(wallc->ty1 - wallc->ty2);
	}
	else
	{
		UoverZorg = (float)wallc->tx1 * centerx;
		UoverZstep = (float)(-wallc->ty1);
		InvZorg = (float)(wallc->tx1 - wallc->tx2) * centerx;
		InvZstep = (float)(wallc->ty2 - wallc->ty1);
	}
}

void FWallTmapVals::InitFromLine(int tx1, int ty1, int tx2, int ty2)
{ // Coordinates should have already had viewx,viewy subtracted
	fixed_t fullx1 = DMulScale20 (tx1, viewsin, -ty1, viewcos);
	fixed_t fullx2 = DMulScale20 (tx2, viewsin, -ty2, viewcos);
	fixed_t fully1 = DMulScale20 (tx1, viewtancos, ty1, viewtansin);
	fixed_t fully2 = DMulScale20 (tx2, viewtancos, ty2, viewtansin);

	if (MirrorFlags & RF_XFLIP)
	{
		fullx1 = -fullx1;
		fullx2 = -fullx2;
	}

	UoverZorg = (float)fullx1 * centerx;
	UoverZstep = (float)(-fully1);
	InvZorg = (float)(fullx1 - fullx2) * centerx;
	InvZstep = (float)(fully2 - fully1);
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true if some part of the bbox might be visible.
//
extern "C" const int checkcoord[12][4] =
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


static bool R_CheckBBox (fixed_t *bspcoord)	// killough 1/28/98: static
{
	int 				boxx;
	int 				boxy;
	int 				boxpos;

	fixed_t 			x1, y1, x2, y2;
	fixed_t				rx1, ry1, rx2, ry2;
	int					sx1, sx2;
	
	cliprange_t*		start;

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

	x1 = bspcoord[checkcoord[boxpos][0]] - viewx;
	y1 = bspcoord[checkcoord[boxpos][1]] - viewy;
	x2 = bspcoord[checkcoord[boxpos][2]] - viewx;
	y2 = bspcoord[checkcoord[boxpos][3]] - viewy;

	// check clip list for an open space

	// Sitting on a line?
	if (DMulScale32 (y1, x1-x2, x1, y2-y1) >= 0)
		return true;

	rx1 = DMulScale20 (x1, viewsin, -y1, viewcos);
	rx2 = DMulScale20 (x2, viewsin, -y2, viewcos);
	ry1 = DMulScale20 (x1, viewtancos, y1, viewtansin);
	ry2 = DMulScale20 (x2, viewtancos, y2, viewtansin);

	if (MirrorFlags & RF_XFLIP)
	{
		int t = 256-rx1;
		rx1 = 256-rx2;
		rx2 = t;
		swapvalues (ry1, ry2);
	}

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1) return false;	// left edge is off the right side
		if (ry1 == 0) return false;
		sx1 = (centerxfrac + Scale (rx1, centerxfrac, ry1)) >> FRACBITS;
		if (rx1 >= 0) sx1 = MIN<int> (viewwidth, sx1+1);	// fix for signed divide
	}
	else
	{
		if (rx2 < -ry2) return false;	// wall is off the left side
		if (rx1 - rx2 - ry2 + ry1 == 0) return false;	// wall does not intersect view volume
		sx1 = 0;
	}

	if (rx2 <= ry2)
	{
		if (rx2 < -ry2) return false;	// right edge is off the left side
		if (ry2 == 0) return false;
		sx2 = (centerxfrac + Scale (rx2, centerxfrac, ry2)) >> FRACBITS;
		if (rx2 >= 0) sx2 = MIN<int> (viewwidth, sx2+1);	// fix for signed divide
	}
	else
	{
		if (rx1 > ry1) return false;	// wall is off the right side
		if (ry2 - ry1 - rx2 + rx1 == 0) return false;	// wall does not intersect view volume
		sx2 = viewwidth;
	}

	// Find the first clippost that touches the source post
	//	(adjacent pixels are touching).

	// Does not cross a pixel.
	if (sx2 <= sx1)
		return false;

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


void R_Subsector (subsector_t *sub);
static void R_AddPolyobjs(subsector_t *sub)
{
	if (sub->BSP == NULL || sub->BSP->bDirty)
	{
		sub->BuildPolyBSP();
	}
	if (sub->BSP->Nodes.Size() == 0)
	{
		R_Subsector(&sub->BSP->Subsectors[0]);
	}
	else
	{
		R_RenderBSPNode(&sub->BSP->Nodes.Last());
	}
}

// kg3D - add fake segs, never rendered
void R_FakeDrawLoop(subsector_t *sub)
{
	int 		 count;
	seg_t*		 line;

	count = sub->numlines;
	line = sub->firstline;

	while (count--)
	{
		if ((line->sidedef) && !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			R_AddLine (line);
		}
		line++;
	}
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//
void R_Subsector (subsector_t *sub)
{
	int 		 count;
	seg_t*		 line;
	sector_t     tempsec;				// killough 3/7/98: deep water hack
	int          floorlightlevel;		// killough 3/16/98: set floor lightlevel
	int          ceilinglightlevel;		// killough 4/11/98
	bool		 outersubsector;
	int	fll, cll, position;
	ASkyViewpoint *skybox;

	// kg3D - fake floor stuff
	visplane_t *backupfp;
	visplane_t *backupcp;
	//secplane_t templane;
	lightlist_t *light;

	if (InSubsector != NULL)
	{ // InSubsector is not NULL. This means we are rendering from a mini-BSP.
		outersubsector = false;
	}
	else
	{
		outersubsector = true;
		InSubsector = sub;
	}

#ifdef RANGECHECK
	if (outersubsector && sub - subsectors >= (ptrdiff_t)numsubsectors)
		I_Error ("R_Subsector: ss %ti with numss = %i", sub - subsectors, numsubsectors);
#endif

	assert(sub->sector != NULL);

	if (sub->polys)
	{ // Render the polyobjs in the subsector first
		R_AddPolyobjs(sub);
		if (outersubsector)
		{
			InSubsector = NULL;
		}
		return;
	}

	frontsector = sub->sector;
	frontsector->MoreFlags |= SECF_DRAWN;
	count = sub->numlines;
	line = sub->firstline;

	// killough 3/8/98, 4/4/98: Deep water / fake ceiling effect
	frontsector = R_FakeFlat(frontsector, &tempsec, &floorlightlevel,
						   &ceilinglightlevel, false);	// killough 4/11/98

	fll = floorlightlevel;
	cll = ceilinglightlevel;

	// [RH] set foggy flag
	foggy = level.fadeto || frontsector->ColorMap->Fade || (level.flags & LEVEL_HASFADETABLE);
	r_actualextralight = foggy ? 0 : extralight << 4;

	// kg3D - fake lights
	if (fixedlightlev < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
	{
		light = P_GetPlaneLight(frontsector, &frontsector->ceilingplane, false);
		basecolormap = light->extra_colormap;
		// If this is the real ceiling, don't discard plane lighting R_FakeFlat()
		// accounted for.
		if (light->p_lightlevel != &frontsector->lightlevel)
		{
			ceilinglightlevel = *light->p_lightlevel;
		}
	}
	else
	{
		basecolormap = frontsector->ColorMap;
	}

	skybox = frontsector->GetSkyBox(sector_t::ceiling);

	ceilingplane = frontsector->ceilingplane.PointOnSide(viewx, viewy, viewz) > 0 ||
		frontsector->GetTexture(sector_t::ceiling) == skyflatnum ||
		(skybox != NULL && skybox->bAlways) ||
		(frontsector->heightsec && 
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->GetTexture(sector_t::floor) == skyflatnum) ?
		R_FindPlane(frontsector->ceilingplane,		// killough 3/8/98
					frontsector->GetTexture(sector_t::ceiling),
					ceilinglightlevel + r_actualextralight,				// killough 4/11/98
					frontsector->GetAlpha(sector_t::ceiling),
					!!(frontsector->GetFlags(sector_t::ceiling) & PLANEF_ADDITIVE),
					frontsector->GetXOffset(sector_t::ceiling),		// killough 3/7/98
					frontsector->GetYOffset(sector_t::ceiling),		// killough 3/7/98
					frontsector->GetXScale(sector_t::ceiling),
					frontsector->GetYScale(sector_t::ceiling),
					frontsector->GetAngle(sector_t::ceiling),
					frontsector->sky,
					skybox
					) : NULL;

	if (fixedlightlev < 0 && frontsector->e && frontsector->e->XFloor.lightlist.Size())
	{
		light = P_GetPlaneLight(frontsector, &frontsector->floorplane, false);
		basecolormap = light->extra_colormap;
		// If this is the real floor, don't discard plane lighting R_FakeFlat()
		// accounted for.
		if (light->p_lightlevel != &frontsector->lightlevel)
		{
			floorlightlevel = *light->p_lightlevel;
		}
	}
	else
	{
		basecolormap = frontsector->ColorMap;
	}

	// killough 3/7/98: Add (x,y) offsets to flats, add deep water check
	// killough 3/16/98: add floorlightlevel
	// killough 10/98: add support for skies transferred from sidedefs
	skybox = frontsector->GetSkyBox(sector_t::floor);
	floorplane = frontsector->floorplane.PointOnSide(viewx, viewy, viewz) > 0 || // killough 3/7/98
		frontsector->GetTexture(sector_t::floor) == skyflatnum ||
		(skybox != NULL && skybox->bAlways) ||
		(frontsector->heightsec &&
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->GetTexture(sector_t::ceiling) == skyflatnum) ?
		R_FindPlane(frontsector->floorplane,
					frontsector->GetTexture(sector_t::floor),
					floorlightlevel + r_actualextralight,				// killough 3/16/98
					frontsector->GetAlpha(sector_t::floor),
					!!(frontsector->GetFlags(sector_t::floor) & PLANEF_ADDITIVE),
					frontsector->GetXOffset(sector_t::floor),		// killough 3/7/98
					frontsector->GetYOffset(sector_t::floor),		// killough 3/7/98
					frontsector->GetXScale(sector_t::floor),
					frontsector->GetYScale(sector_t::floor),
					frontsector->GetAngle(sector_t::floor),
					frontsector->sky,
					skybox
					) : NULL;

	// kg3D - fake planes rendering
	if (r_3dfloors && frontsector->e && frontsector->e->XFloor.ffloors.Size())
	{
		backupfp = floorplane;
		backupcp = ceilingplane;
		// first check all floors
		for (int i = 0; i < (int)frontsector->e->XFloor.ffloors.Size(); i++)
		{
			fakeFloor = frontsector->e->XFloor.ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!fakeFloor->model) continue;
			if (fakeFloor->bottom.plane->a || fakeFloor->bottom.plane->b) continue;
			if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES|FF_RENDERSIDES)))
			{
				R_3D_AddHeight(fakeFloor->top.plane, frontsector);
			}
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (fakeFloor->flags & FF_THISINSIDE && fakeFloor->flags & FF_INVERTSECTOR) continue;
			fakeAlpha = MIN(Scale(fakeFloor->alpha, OPAQUE, 255), OPAQUE);
			if (fakeFloor->validcount != validcount)
			{
				fakeFloor->validcount = validcount;
				R_3D_NewClip();
			}
			fakeHeight = fakeFloor->top.plane->ZatPoint(frontsector->soundorg[0], frontsector->soundorg[0]);
			if (fakeHeight < viewz &&
				fakeHeight > frontsector->floorplane.ZatPoint(frontsector->soundorg[0], frontsector->soundorg[1]))
			{
				fake3D = FAKE3D_FAKEFLOOR;
				tempsec = *fakeFloor->model;
				tempsec.floorplane = *fakeFloor->top.plane;
				tempsec.ceilingplane = *fakeFloor->bottom.plane;
				if (!(fakeFloor->flags & FF_THISINSIDE) && !(fakeFloor->flags & FF_INVERTSECTOR))
				{
					tempsec.SetTexture(sector_t::floor, tempsec.GetTexture(sector_t::ceiling));
					position = sector_t::ceiling;
				} else position = sector_t::floor;
				frontsector = &tempsec;

				if (fixedlightlev < 0 && sub->sector->e->XFloor.lightlist.Size())
				{
					light = P_GetPlaneLight(sub->sector, &frontsector->floorplane, false);
					basecolormap = light->extra_colormap;
					floorlightlevel = *light->p_lightlevel;
				}

				ceilingplane = NULL;
				floorplane = R_FindPlane(frontsector->floorplane,
					frontsector->GetTexture(sector_t::floor),
					floorlightlevel + r_actualextralight,				// killough 3/16/98
					frontsector->GetAlpha(sector_t::floor),
					!!(fakeFloor->flags & FF_ADDITIVETRANS),
					frontsector->GetXOffset(position),		// killough 3/7/98
					frontsector->GetYOffset(position),		// killough 3/7/98
					frontsector->GetXScale(position),
					frontsector->GetYScale(position),
					frontsector->GetAngle(position),
					frontsector->sky,
					NULL);

				R_FakeDrawLoop(sub);
				fake3D = 0;
				frontsector = sub->sector;
			}
		}
		// and now ceilings
		for (unsigned int i = 0; i < frontsector->e->XFloor.ffloors.Size(); i++)
		{
			fakeFloor = frontsector->e->XFloor.ffloors[i];
			if (!(fakeFloor->flags & FF_EXISTS)) continue;
			if (!fakeFloor->model) continue;
			if (fakeFloor->top.plane->a || fakeFloor->top.plane->b) continue;
			if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES|FF_RENDERSIDES)))
			{
				R_3D_AddHeight(fakeFloor->bottom.plane, frontsector);
			}
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (!(fakeFloor->flags & FF_THISINSIDE) && (fakeFloor->flags & (FF_SWIMMABLE|FF_INVERTSECTOR)) == (FF_SWIMMABLE|FF_INVERTSECTOR)) continue;
			fakeAlpha = MIN(Scale(fakeFloor->alpha, OPAQUE, 255), OPAQUE);

			if (fakeFloor->validcount != validcount)
			{
				fakeFloor->validcount = validcount;
				R_3D_NewClip();
			}
			fakeHeight = fakeFloor->bottom.plane->ZatPoint(frontsector->soundorg[0], frontsector->soundorg[1]);
			if (fakeHeight > viewz &&
				fakeHeight < frontsector->ceilingplane.ZatPoint(frontsector->soundorg[0], frontsector->soundorg[1]))
			{
				fake3D = FAKE3D_FAKECEILING;
				tempsec = *fakeFloor->model;
				tempsec.floorplane = *fakeFloor->top.plane;
				tempsec.ceilingplane = *fakeFloor->bottom.plane;
				if ((!(fakeFloor->flags & FF_THISINSIDE) && !(fakeFloor->flags & FF_INVERTSECTOR)) ||
					(fakeFloor->flags & FF_THISINSIDE && fakeFloor->flags & FF_INVERTSECTOR))
				{
					tempsec.SetTexture(sector_t::ceiling, tempsec.GetTexture(sector_t::floor));
					position = sector_t::floor;
				} else position = sector_t::ceiling;
				frontsector = &tempsec;

				tempsec.ceilingplane.ChangeHeight(-1);
				if (fixedlightlev < 0 && sub->sector->e->XFloor.lightlist.Size())
				{
					light = P_GetPlaneLight(sub->sector, &frontsector->ceilingplane, false);
					basecolormap = light->extra_colormap;
					ceilinglightlevel = *light->p_lightlevel;
				}
				tempsec.ceilingplane.ChangeHeight(1);

				floorplane = NULL;
				ceilingplane = R_FindPlane(frontsector->ceilingplane,		// killough 3/8/98
					frontsector->GetTexture(sector_t::ceiling),
					ceilinglightlevel + r_actualextralight,				// killough 4/11/98
					frontsector->GetAlpha(sector_t::ceiling),
					!!(fakeFloor->flags & FF_ADDITIVETRANS),
					frontsector->GetXOffset(position),		// killough 3/7/98
					frontsector->GetYOffset(position),		// killough 3/7/98
					frontsector->GetXScale(position),
					frontsector->GetYScale(position),
					frontsector->GetAngle(position),
					frontsector->sky,
					NULL);

				R_FakeDrawLoop(sub);
				fake3D = 0;
				frontsector = sub->sector;
			}
		}
		fakeFloor = NULL;
		floorplane = backupfp;
		ceilingplane = backupcp;
	}

	basecolormap = frontsector->ColorMap;
	floorlightlevel = fll;
	ceilinglightlevel = cll;

	// killough 9/18/98: Fix underwater slowdown, by passing real sector 
	// instead of fake one. Improve sprite lighting by basing sprite
	// lightlevels on floor & ceiling lightlevels in the surrounding area.
	// [RH] Handle sprite lighting like Duke 3D: If the ceiling is a sky, sprites are lit by
	// it, otherwise they are lit by the floor.
	R_AddSprites (sub->sector, frontsector->GetTexture(sector_t::ceiling) == skyflatnum ?
		ceilinglightlevel : floorlightlevel, FakeSide);

	// [RH] Add particles
	if ((unsigned int)(sub - subsectors) < (unsigned int)numsubsectors)
	{ // Only do it for the main BSP.
		int shade = LIGHT2SHADE((floorlightlevel + ceilinglightlevel)/2 + r_actualextralight);
		for (WORD i = ParticlesInSubsec[(unsigned int)(sub-subsectors)]; i != NO_PARTICLE; i = Particles[i].snext)
		{
			R_ProjectParticle (Particles + i, subsectors[sub-subsectors].sector, shade, FakeSide);
		}
	}

	count = sub->numlines;
	line = sub->firstline;

	while (count--)
	{
		if (!outersubsector || line->sidedef == NULL || !(line->sidedef->Flags & WALLF_POLYOBJ))
		{
			// kg3D - fake planes bounding calculation
			if (r_3dfloors && line->backsector && frontsector->e && line->backsector->e->XFloor.ffloors.Size())
			{
				backupfp = floorplane;
				backupcp = ceilingplane;
				floorplane = NULL;
				ceilingplane = NULL;
				for (unsigned int i = 0; i < line->backsector->e->XFloor.ffloors.Size(); i++)
				{
					fakeFloor = line->backsector->e->XFloor.ffloors[i];
					if (!(fakeFloor->flags & FF_EXISTS)) continue;
					if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
					if (!fakeFloor->model) continue;
					fake3D = FAKE3D_FAKEBACK;
					tempsec = *fakeFloor->model;
					tempsec.floorplane = *fakeFloor->top.plane;
					tempsec.ceilingplane = *fakeFloor->bottom.plane;
					backsector = &tempsec;
					if (fakeFloor->validcount != validcount)
					{
						fakeFloor->validcount = validcount;
						R_3D_NewClip();
					}
					if (frontsector->CenterFloor() >= backsector->CenterFloor())
					{
						fake3D |= FAKE3D_CLIPBOTFRONT;
					}
					if (frontsector->CenterCeiling() <= backsector->CenterCeiling())
					{
						fake3D |= FAKE3D_CLIPTOPFRONT;
					}
					R_AddLine(line); // fake
				}
				fakeFloor = NULL;
				fake3D = 0;
				floorplane = backupfp;
				ceilingplane = backupcp;
			}
			R_AddLine (line); // now real
		}
		line++;
	}
	if (outersubsector)
	{
		InSubsector = NULL;
	}
}

//
// RenderBSPNode
// Renders all subsectors below a given node, traversing subtree recursively.
// Just call with BSP root and -1.
// killough 5/2/98: reformatted, removed tail recursion

void R_RenderBSPNode (void *node)
{
	if (numnodes == 0)
	{
		R_Subsector (subsectors);
		return;
	}
	while (!((size_t)node & 1))  // Keep going until found a subsector
	{
		node_t *bsp = (node_t *)node;

		// Decide which side the view point is on.
		int side = R_PointOnSide (viewx, viewy, bsp);

		// Recursively divide front space (toward the viewer).
		R_RenderBSPNode (bsp->children[side]);

		// Possibly divide back space (away from the viewer).
		side ^= 1;
		if (!R_CheckBBox (bsp->bbox[side]))
			return;

		node = bsp->children[side];
	}
	R_Subsector ((subsector_t *)((BYTE *)node - 1));
}
