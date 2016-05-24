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

#include "r_local.h"
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

double			rw_backcz1, rw_backcz2;
double			rw_backfz1, rw_backfz2;
double			rw_frontcz1, rw_frontcz2;
double			rw_frontfz1, rw_frontfz2;


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
TArray<PortalDrawseg> WallPortals(1000);	// note: this array needs to go away as reallocation can cause crashes.


subsector_t *InSubsector;

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
			(heightsec && heightsec->floorplane.PointOnSide(ViewPos) <= 0);
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

		double refceilz = s->ceilingplane.ZatPoint(ViewPos);
		double orgceilz = sec->ceilingplane.ZatPoint(ViewPos);

#if 1
		// [RH] Allow viewing underwater areas through doors/windows that
		// are underwater but not in a water sector themselves.
		// Only works if you cannot see the top surface of any deep water
		// sectors at the same time.
		if (back && !r_fakingunderwater && curline->frontsector->heightsec == NULL)
		{
			if (rw_frontcz1 <= s->floorplane.ZatPoint(curline->v1) &&
				rw_frontcz2 <= s->floorplane.ZatPoint(curline->v2))
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
			tempsec->ceilingplane.ChangeHeight(-1 / 65536.);
			tempsec->ColorMap = s->ColorMap;
		}

		// killough 11/98: prevent sudden light changes from non-water sectors:
		if ((underwater && !back) || doorunderwater)
		{					// head-below-floor hack
			tempsec->SetTexture(sector_t::floor, diffTex ? sec->GetTexture(sector_t::floor) : s->GetTexture(sector_t::floor), false);
			tempsec->planes[sector_t::floor].xform = s->planes[sector_t::floor].xform;

			tempsec->ceilingplane		= s->floorplane;
			tempsec->ceilingplane.FlipVert ();
			tempsec->ceilingplane.ChangeHeight (-1 / 65536.);
			if (s->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				tempsec->floorplane			= tempsec->ceilingplane;
				tempsec->floorplane.FlipVert ();
				tempsec->floorplane.ChangeHeight (+1 / 65536.);
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
		else if (heightsec && heightsec->ceilingplane.PointOnSide(ViewPos) <= 0 &&
				 orgceilz > refceilz && !(s->MoreFlags & SECF_FAKEFLOORONLY))
		{	// Above-ceiling hack
			tempsec->ceilingplane		= s->ceilingplane;
			tempsec->floorplane			= s->ceilingplane;
			tempsec->floorplane.FlipVert ();
			tempsec->floorplane.ChangeHeight (+1 / 65536.);
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


bool R_SkyboxCompare(sector_t *frontsector, sector_t *backsector)
{
	FSectorPortal *frontc = frontsector->GetPortal(sector_t::ceiling);
	FSectorPortal *frontf = frontsector->GetPortal(sector_t::floor);
	FSectorPortal *backc = backsector->GetPortal(sector_t::ceiling);
	FSectorPortal *backf = backsector->GetPortal(sector_t::floor);

	// return true if any of the planes has a linedef-based portal (unless both sides have the same one.
	// Ideally this should also check thing based portals but the omission of this check had been abused to hell and back for those.
	// (Note: This may require a compatibility option if some maps ran into this for line based portals as well.)
	if (!frontc->MergeAllowed()) return (frontc != backc);
	if (!frontf->MergeAllowed()) return (frontf != backf);
	if (!backc->MergeAllowed()) return true;
	if (!backf->MergeAllowed()) return true;
	return false;
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
	DVector2		pt1, pt2;

	curline = line;

	// [RH] Color if not texturing line
	dc_color = (((int)(line - segs) * 8) + 4) & 255;

	pt1 = line->v1->fPos() - ViewPos;
	pt2 = line->v2->fPos() - ViewPos;

	// Reject lines not facing viewer
	if (pt1.Y * (pt1.X - pt2.X) + pt1.X * (pt2.Y - pt1.Y) >= 0)
		return;

	if (WallC.Init(pt1, pt2, 32.0 / (1 << 12)))
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

	// reject lines that aren't seen from the portal (if any)
	// [ZZ] 10.01.2016: lines inside a skybox shouldn't be clipped, although this imposes some limitations on portals in skyboxes.
	if (!CurrentPortalInSkybox && CurrentPortal && P_ClipLineToPortal(line->linedef, CurrentPortal->dst, ViewPos))
		return;

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
		WallT.InitFromLine(v1->fPos() - ViewPos, v2->fPos() - ViewPos);
	}

	if (!(fake3D & FAKE3D_FAKEBACK))
	{
		backsector = line->backsector;
	}
	rw_frontcz1 = frontsector->ceilingplane.ZatPoint(line->v1);
	rw_frontfz1 = frontsector->floorplane.ZatPoint(line->v1);
	rw_frontcz2 = frontsector->ceilingplane.ZatPoint(line->v2);
	rw_frontfz2 = frontsector->floorplane.ZatPoint(line->v2);

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

		rw_backcz1 = backsector->ceilingplane.ZatPoint(line->v1);
		rw_backfz1 = backsector->floorplane.ZatPoint(line->v1);
		rw_backcz2 = backsector->ceilingplane.ZatPoint(line->v2);
		rw_backfz2 = backsector->floorplane.ZatPoint(line->v2);

		if (fake3D & FAKE3D_FAKEBACK)
		{
			if (rw_frontfz1 >= rw_backfz1 && rw_frontfz2 >= rw_backfz2)
			{
				fake3D |= FAKE3D_CLIPBOTFRONT;
			}
			if (rw_frontcz1 <= rw_backcz1 && rw_frontcz2 <= rw_backcz2)
			{
				fake3D |= FAKE3D_CLIPTOPFRONT;
			}
		}

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

		// Portal
		if (line->linedef->isVisualPortal() && line->sidedef == line->linedef->sidedef[0])
		{
			solid = true;
		}
		// Closed door.
		else if ((rw_backcz1 <= rw_frontfz1 && rw_backcz2 <= rw_frontfz2) ||
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
		else if (R_SkyboxCompare(frontsector, backsector))
		{
			solid = false;
		}
		else if (backsector->lightlevel != frontsector->lightlevel
			|| backsector->GetTexture(sector_t::floor) != frontsector->GetTexture(sector_t::floor)
			|| backsector->GetTexture(sector_t::ceiling) != frontsector->GetTexture(sector_t::ceiling)
			|| curline->sidedef->GetTexture(side_t::mid).isValid()

			// killough 3/7/98: Take flats offsets into account:
			|| backsector->planes[sector_t::floor].xform != frontsector->planes[sector_t::floor].xform
			|| backsector->planes[sector_t::ceiling].xform != frontsector->planes[sector_t::ceiling].xform

			|| backsector->GetPlaneLight(sector_t::floor) != frontsector->GetPlaneLight(sector_t::floor)
			|| backsector->GetPlaneLight(sector_t::ceiling) != frontsector->GetPlaneLight(sector_t::ceiling)
			|| backsector->GetVisFlags(sector_t::floor) != frontsector->GetVisFlags(sector_t::floor)
			|| backsector->GetVisFlags(sector_t::ceiling) != frontsector->GetVisFlags(sector_t::ceiling)

			// [RH] Also consider colormaps
			|| backsector->ColorMap != frontsector->ColorMap



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
bool FWallCoords::Init(const DVector2 &pt1, const DVector2 &pt2, double too_close)
{
	tleft.X =  float(pt1.X * ViewSin - pt1.Y * ViewCos);
	tright.X = float(pt2.X * ViewSin - pt2.Y * ViewCos);

	tleft.Y =  float(pt1.X * ViewTanCos + pt1.Y * ViewTanSin);
	tright.Y = float(pt2.X * ViewTanCos + pt2.Y * ViewTanSin);

	if (MirrorFlags & RF_XFLIP)
	{
		float t = -tleft.X;
		tleft.X = -tright.X;
		tright.X = t;
		swapvalues(tleft.Y, tright.Y);
	}

	if (tleft.X >= -tleft.Y)
	{
		if (tleft.X > tleft.Y) return true;	// left edge is off the right side
		if (tleft.Y == 0) return true;
		sx1 = xs_RoundToInt(CenterX + tleft.X * CenterX / tleft.Y);
		sz1 = tleft.Y;
	}
	else
	{
		if (tright.X < -tright.Y) return true;	// wall is off the left side
		float den = tleft.X - tright.X - tright.Y + tleft.Y;
		if (den == 0) return true;
		sx1 = 0;
		sz1 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X + tleft.Y) / den;
	}

	if (sz1 < too_close)
		return true;

	if (tright.X <= tright.Y)
	{
		if (tright.X < -tright.Y) return true;	// right edge is off the left side
		if (tright.Y == 0) return true;
		sx2 = xs_RoundToInt(CenterX + tright.X * CenterX / tright.Y);
		sz2 = tright.Y;
	}
	else
	{
		if (tleft.X > tleft.Y) return true;	// wall is off the right side
		float den = tright.Y - tleft.Y - tright.X + tleft.X;
		if (den == 0) return true;
		sx2 = viewwidth;
		sz2 = tleft.Y + (tright.Y - tleft.Y) * (tleft.X - tleft.Y) / den;
	}

	if (sz2 < too_close || sx2 <= sx1)
		return true;

	return false;
}

void FWallTmapVals::InitFromWallCoords(const FWallCoords *wallc)
{
	const FVector2 *left = &wallc->tleft;
	const FVector2 *right = &wallc->tright;

	if (MirrorFlags & RF_XFLIP)
	{
		swapvalues(left, right);
	}
	UoverZorg = left->X * centerx;
	UoverZstep = -left->Y;
	InvZorg = (left->X - right->X) * centerx;
	InvZstep = right->Y - left->Y;
}

void FWallTmapVals::InitFromLine(const DVector2 &left, const DVector2 &right)
{ // Coordinates should have already had viewx,viewy subtracted
	double fullx1 = left.X * ViewSin - left.Y * ViewCos;
	double fullx2 = right.X * ViewSin - right.Y * ViewCos;
	double fully1 = left.X * ViewTanCos + left.Y * ViewTanSin;
	double fully2 = right.X * ViewTanCos + right.Y * ViewTanSin;

	if (MirrorFlags & RF_XFLIP)
	{
		fullx1 = -fullx1;
		fullx2 = -fullx2;
	}

	UoverZorg = float(fullx1 * centerx);
	UoverZstep = float(-fully1);
	InvZorg = float((fullx1 - fullx2) * centerx);
	InvZstep = float(fully2 - fully1);
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


static bool R_CheckBBox (float *bspcoord)	// killough 1/28/98: static
{
	int 				boxx;
	int 				boxy;
	int 				boxpos;

	double	 			x1, y1, x2, y2;
	double				rx1, ry1, rx2, ry2;
	int					sx1, sx2;
	
	cliprange_t*		start;

	// Find the corners of the box
	// that define the edges from current viewpoint.
	if (ViewPos.X <= bspcoord[BOXLEFT])
		boxx = 0;
	else if (ViewPos.X < bspcoord[BOXRIGHT])
		boxx = 1;
	else
		boxx = 2;

	if (ViewPos.Y >= bspcoord[BOXTOP])
		boxy = 0;
	else if (ViewPos.Y > bspcoord[BOXBOTTOM])
		boxy = 1;
	else
		boxy = 2;

	boxpos = (boxy<<2)+boxx;
	if (boxpos == 5)
		return true;

	x1 = bspcoord[checkcoord[boxpos][0]] - ViewPos.X;
	y1 = bspcoord[checkcoord[boxpos][1]] - ViewPos.Y;
	x2 = bspcoord[checkcoord[boxpos][2]] - ViewPos.X;
	y2 = bspcoord[checkcoord[boxpos][3]] - ViewPos.Y;

	// check clip list for an open space

	// Sitting on a line?
	if (y1 * (x1 - x2) + x1 * (y2 - y1) >= -EQUAL_EPSILON)
		return true;

	rx1 = x1 * ViewSin - y1 * ViewCos;
	rx2 = x2 * ViewSin - y2 * ViewCos;
	ry1 = x1 * ViewTanCos + y1 * ViewTanSin;
	ry2 = x2 * ViewTanCos + y2 * ViewTanSin;

	if (MirrorFlags & RF_XFLIP)
	{
		double t = -rx1;
		rx1 = -rx2;
		rx2 = t;
		swapvalues(ry1, ry2);
	}

	if (rx1 >= -ry1)
	{
		if (rx1 > ry1) return false;	// left edge is off the right side
		if (ry1 == 0) return false;
		sx1 = xs_RoundToInt(CenterX + rx1 * CenterX / ry1);
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
		sx2 = xs_RoundToInt(CenterX + rx2 * CenterX / ry2);
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
	FSectorPortal *portal;

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

	portal = frontsector->ValidatePortal(sector_t::ceiling);

	ceilingplane = frontsector->ceilingplane.PointOnSide(ViewPos) > 0 ||
		frontsector->GetTexture(sector_t::ceiling) == skyflatnum ||
		portal != NULL ||
		(frontsector->heightsec && 
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->GetTexture(sector_t::floor) == skyflatnum) ?
		R_FindPlane(frontsector->ceilingplane,		// killough 3/8/98
					frontsector->GetTexture(sector_t::ceiling),
					ceilinglightlevel + r_actualextralight,				// killough 4/11/98
					frontsector->GetAlpha(sector_t::ceiling),
					!!(frontsector->GetFlags(sector_t::ceiling) & PLANEF_ADDITIVE),
					frontsector->planes[sector_t::ceiling].xform,
					frontsector->sky,
					portal
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
	portal = frontsector->ValidatePortal(sector_t::floor);

	floorplane = frontsector->floorplane.PointOnSide(ViewPos) > 0 || // killough 3/7/98
		frontsector->GetTexture(sector_t::floor) == skyflatnum ||
		portal != NULL ||
		(frontsector->heightsec &&
		 !(frontsector->heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC) &&
		 frontsector->heightsec->GetTexture(sector_t::ceiling) == skyflatnum) ?
		R_FindPlane(frontsector->floorplane,
					frontsector->GetTexture(sector_t::floor),
					floorlightlevel + r_actualextralight,				// killough 3/16/98
					frontsector->GetAlpha(sector_t::floor),
					!!(frontsector->GetFlags(sector_t::floor) & PLANEF_ADDITIVE),
					frontsector->planes[sector_t::floor].xform,
					frontsector->sky,
					portal
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
			if (fakeFloor->bottom.plane->isSlope()) continue;
			if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES|FF_RENDERSIDES)))
			{
				R_3D_AddHeight(fakeFloor->top.plane, frontsector);
			}
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (fakeFloor->flags & FF_THISINSIDE && fakeFloor->flags & FF_INVERTSECTOR) continue;
			fakeAlpha = MIN<fixed_t>(Scale(fakeFloor->alpha, OPAQUE, 255), OPAQUE);
			if (fakeFloor->validcount != validcount)
			{
				fakeFloor->validcount = validcount;
				R_3D_NewClip();
			}
			double fakeHeight = fakeFloor->top.plane->ZatPoint(frontsector->centerspot);
			if (fakeHeight < ViewPos.Z &&
				fakeHeight > frontsector->floorplane.ZatPoint(frontsector->centerspot))
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
					frontsector->planes[position].xform,
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
			if (fakeFloor->top.plane->isSlope()) continue;
			if (!(fakeFloor->flags & FF_NOSHADE) || (fakeFloor->flags & (FF_RENDERPLANES|FF_RENDERSIDES)))
			{
				R_3D_AddHeight(fakeFloor->bottom.plane, frontsector);
			}
			if (!(fakeFloor->flags & FF_RENDERPLANES)) continue;
			if (fakeFloor->alpha == 0) continue;
			if (!(fakeFloor->flags & FF_THISINSIDE) && (fakeFloor->flags & (FF_SWIMMABLE|FF_INVERTSECTOR)) == (FF_SWIMMABLE|FF_INVERTSECTOR)) continue;
			fakeAlpha = MIN<fixed_t>(Scale(fakeFloor->alpha, OPAQUE, 255), OPAQUE);

			if (fakeFloor->validcount != validcount)
			{
				fakeFloor->validcount = validcount;
				R_3D_NewClip();
			}
			double fakeHeight = fakeFloor->bottom.plane->ZatPoint(frontsector->centerspot);
			if (fakeHeight > ViewPos.Z &&
				fakeHeight < frontsector->ceilingplane.ZatPoint(frontsector->centerspot))
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

				tempsec.ceilingplane.ChangeHeight(-1 / 65536.);
				if (fixedlightlev < 0 && sub->sector->e->XFloor.lightlist.Size())
				{
					light = P_GetPlaneLight(sub->sector, &frontsector->ceilingplane, false);
					basecolormap = light->extra_colormap;
					ceilinglightlevel = *light->p_lightlevel;
				}
				tempsec.ceilingplane.ChangeHeight(1 / 65536.);

				floorplane = NULL;
				ceilingplane = R_FindPlane(frontsector->ceilingplane,		// killough 3/8/98
					frontsector->GetTexture(sector_t::ceiling),
					ceilinglightlevel + r_actualextralight,				// killough 4/11/98
					frontsector->GetAlpha(sector_t::ceiling),
					!!(fakeFloor->flags & FF_ADDITIVETRANS),
					frontsector->planes[position].xform,
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
		int side = R_PointOnSide (ViewPos, bsp);

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
