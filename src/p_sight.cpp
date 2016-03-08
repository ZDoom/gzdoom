//**************************************************************************
//**
//** p_sight.cpp : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile: p_sight.c,v $
//** $Revision: 1.1 $
//** $Date: 95/05/11 00:22:50 $
//** $Author: bgokey $
//**
//**************************************************************************

#include <assert.h>

#include "doomdef.h"
#include "i_system.h"
#include "p_local.h"
#include "p_maputl.h"
#include "p_blockmap.h"
#include "m_random.h"
#include "m_bbox.h"
#include "p_lnspec.h"
#include "g_level.h"
#include "po_man.h"
#include "r_utility.h"
#include "b_bot.h"
#include "p_spec.h"

// State.
#include "r_state.h"

#include "stats.h"

static FRandom pr_botchecksight ("BotCheckSight");
static FRandom pr_checksight ("CheckSight");

/*
==============================================================================

							P_CheckSight

This uses specialized forms of the maputils routines for optimized performance

==============================================================================
*/

// Performance meters
static int sightcounts[6];
static cycle_t SightCycles;
static cycle_t MaxSightCycles;

enum
{
	SO_TOPFRONT = 1,
	SO_TOPBACK = 2,
	SO_BOTTOMFRONT = 4,
	SO_BOTTOMBACK = 8,

};

struct SightOpening
{
	fixed_t top;
	fixed_t bottom;
	int range;
	int portalflags;

	void SwapSides()
	{
		portalflags = ((portalflags & (SO_TOPFRONT | SO_BOTTOMFRONT)) << 1) | ((portalflags & (SO_TOPBACK | SO_BOTTOMBACK)) >> 1);
	}
};

struct SightTask
{
	fixed_t frac;
	fixed_t topslope;
	fixed_t bottomslope;
	int direction;
	int portalgroup;
};


static TArray<intercept_t> intercepts (128);
static TArray<SightTask> portals(32);

class SightCheck
{
	fixedvec3 sightstart;
	fixedvec2 sightend;
	fixed_t startfrac;

	AActor * seeingthing;
	fixed_t lastztop;				// z at last line
	fixed_t lastzbottom;				// z at last line
	sector_t * lastsector;			// last sector being entered by trace
	fixed_t topslope, bottomslope;	// slopes to top and bottom of target
	int Flags;
	divline_t trace;
	int portaldir;
	int portalgroup;
	bool portalfound;
	unsigned int myseethrough;

	void P_SightOpening(SightOpening &open, const line_t *linedef, fixed_t x, fixed_t y);
	bool PTR_SightTraverse (intercept_t *in);
	bool P_SightCheckLine (line_t *ld);
	int P_SightBlockLinesIterator (int x, int y);
	bool P_SightTraverseIntercepts ();

public:
	bool P_SightPathTraverse ();

	void init(AActor * t1, AActor * t2, sector_t *startsector, SightTask *task, int flags)
	{
		sightstart = t1->PosRelative(task->portalgroup);
		sightend = t2->PosRelative(task->portalgroup);
		sightstart.z += t1->height - (t1->height >> 2);

		startfrac = task->frac;
		trace = { sightstart.x, sightstart.y, sightend.x - sightstart.x, sightend.y - sightstart.y };
		lastztop = lastzbottom = sightstart.z;
		lastsector = startsector;
		seeingthing=t2;
		topslope = task->topslope;
		bottomslope = task->bottomslope;
		Flags = flags;
		portaldir = task->direction;
		portalfound = false;

		myseethrough = FF_SEETHROUGH;
	}
};

//==========================================================================
//
// P_SightOpening
//
// Simplified version that removes everything not needed for a sight check
//
//==========================================================================

void SightCheck::P_SightOpening(SightOpening &open, const line_t *linedef, fixed_t x, fixed_t y)
{
	open.portalflags = 0;
	sector_t *front = linedef->frontsector;
	sector_t *back = linedef->backsector;

	if (back == NULL)
	{
		// single sided line
		if (linedef->flags & ML_PORTALCONNECT)
		{
			if (!front->PortalBlocksSight(sector_t::ceiling)) open.portalflags |= SO_TOPFRONT;
			if (!front->PortalBlocksSight(sector_t::floor)) open.portalflags |= SO_BOTTOMFRONT;
		}

		open.range = 0;
		return;
	}


	fixed_t fc = 0, ff = 0, bc = 0, bf = 0;

	if (linedef->flags & ML_PORTALCONNECT)
	{
		if (!front->PortalBlocksSight(sector_t::ceiling)) fc = FIXED_MAX, open.portalflags |= SO_TOPFRONT;
		if (!back->PortalBlocksSight(sector_t::ceiling)) bc = FIXED_MAX, open.portalflags |= SO_TOPBACK;
		if (!front->PortalBlocksSight(sector_t::floor)) ff = FIXED_MIN, open.portalflags |= SO_BOTTOMFRONT;
		if (!back->PortalBlocksSight(sector_t::floor)) bf = FIXED_MIN, open.portalflags |= SO_BOTTOMBACK;
	}

	if (fc == 0) fc = front->ceilingplane.ZatPoint(x, y);
	if (bc == 0) bc = back->ceilingplane.ZatPoint(x, y);
	if (ff == 0) ff = front->floorplane.ZatPoint(x, y);
	if (bf == 0) bf = back->floorplane.ZatPoint(x, y);

	open.bottom = MAX(ff, bf);
	open.top = MIN(fc, bc);

	// we only want to know if there is an opening, not how large it is.
	open.range = open.bottom >= open.top ? 0 : 1;
}


/*
==============
=
= PTR_SightTraverse
=
==============
*/

bool SightCheck::PTR_SightTraverse (intercept_t *in)
{
	line_t  *li;
	fixed_t slope;
	SightOpening open;
	int  frontflag = -1;

	li = in->d.line;

//
// crosses a two sided line
//

	// ignore self referencing sectors if COMPAT_TRACE is on
	if ((i_compatflags & COMPATF_TRACE) && li->frontsector == li->backsector) 
		return true;

	fixed_t trX=trace.x + FixedMul (trace.dx, in->frac);
	fixed_t trY=trace.y + FixedMul (trace.dy, in->frac);
	P_SightOpening (open, li, trX, trY);

	FLinePortal *lport = li->getPortal();

	if (open.range == 0 && open.portalflags == 0 && (lport == NULL || lport->mType != PORTT_LINKED))		// quick test for totally closed doors (must be delayed if portal checks are needed, though)
		return false;		// stop

	// check bottom
	if (open.bottom > FIXED_MIN)
	{
		slope = FixedDiv(open.bottom - sightstart.z, in->frac);
		if (slope > bottomslope)
			bottomslope = slope;
	}

	// check top
	if (open.top < FIXED_MAX)
	{
		slope = FixedDiv(open.top - sightstart.z, in->frac);
		if (slope < topslope)
			topslope = slope;
	}

	if (open.portalflags)
	{
		sector_t *frontsec, *backsec;
		frontflag = P_PointOnLineSidePrecise(sightstart.x, sightstart.y, li);
		if (!frontflag)
		{
			frontsec = li->frontsector;
			backsec = li->backsector;
		}
		else
		{
			frontsec = li->backsector;
			if (!frontsec) return false;	// We are looking through the backside of a one-sided line. Just abort if that happens.
			backsec = li->frontsector;
			open.SwapSides();				// swap flags to make the next checks simpler.
		}

		if (portaldir != sector_t::floor && (open.portalflags & SO_TOPBACK) && !(open.portalflags & SO_TOPFRONT))
		{
			portals.Push({ in->frac, topslope, bottomslope, sector_t::ceiling, backsec->SkyBoxes[sector_t::ceiling]->Sector->PortalGroup });
		}
		if (portaldir != sector_t::ceiling && (open.portalflags & SO_BOTTOMBACK) && !(open.portalflags & SO_BOTTOMFRONT))
		{
			portals.Push({ in->frac, topslope, bottomslope, sector_t::floor, backsec->SkyBoxes[sector_t::floor]->Sector->PortalGroup });
		}
	}
	if (lport)
	{
		portals.Push({ in->frac, topslope, bottomslope, portaldir, lport->mDestination->frontsector->PortalGroup });
		return false;
	}

	if (topslope <= bottomslope || open.range == 0)
		return false;		// stop

	// now handle 3D-floors
	if(li->frontsector->e->XFloor.ffloors.Size() || li->backsector->e->XFloor.ffloors.Size())
	{
		if (frontflag == -1) frontflag = P_PointOnLineSidePrecise(sightstart.x, sightstart.y, li);
		
		//Check 3D FLOORS!
		for(int i=1;i<=2;i++)
		{
			sector_t * s=i==1? li->frontsector:li->backsector;
			fixed_t    highslope, lowslope;

			fixed_t topz= FixedMul (topslope, in->frac) + sightstart.z;
			fixed_t bottomz= FixedMul (bottomslope, in->frac) + sightstart.z;

			for(unsigned int j=0;j<s->e->XFloor.ffloors.Size();j++)
			{
				F3DFloor*  rover=s->e->XFloor.ffloors[j];

				if((rover->flags & FF_SEETHROUGH) == myseethrough || !(rover->flags & FF_EXISTS)) continue;
				if ((Flags & SF_IGNOREWATERBOUNDARY) && (rover->flags & FF_SOLID) == 0) continue;
				
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(trX, trY);
				fixed_t ff_top=rover->top.plane->ZatPoint(trX, trY);

				highslope = FixedDiv (ff_top - sightstart.z, in->frac);
				lowslope = FixedDiv (ff_bottom - sightstart.z, in->frac);

				if (highslope>=topslope)
				{
					// blocks completely
					if (lowslope<=bottomslope) return false;	
					// blocks upper edge of view
					if (lowslope<topslope) topslope=lowslope;
				}
				else if (lowslope<=bottomslope)
				{
					// blocks lower edge of view
					if (highslope>bottomslope)  bottomslope=highslope;
				}
				else
				{
					// the 3D-floor is inside the viewing cone but neither clips the top nor the bottom so by 
					// itself it can't be view blocking.
					// However, if there's a 3D-floor on the other side that obstructs the same vertical range
					// the 2 together will block sight.
					sector_t * sb=i==2? li->frontsector:li->backsector;

					for(unsigned int k=0;k<sb->e->XFloor.ffloors.Size();k++)
					{
						F3DFloor*  rover2=sb->e->XFloor.ffloors[k];

						if((rover2->flags & FF_SEETHROUGH) == myseethrough || !(rover2->flags & FF_EXISTS)) continue;
						if ((Flags & SF_IGNOREWATERBOUNDARY) && (rover->flags & FF_SOLID) == 0) continue;
						
						fixed_t ffb_bottom=rover2->bottom.plane->ZatPoint(trX, trY);
						fixed_t ffb_top=rover2->top.plane->ZatPoint(trX, trY);

						if ( (ffb_bottom >= ff_bottom && ffb_bottom<=ff_top) ||
							(ffb_top <= ff_top && ffb_top >= ff_bottom) ||
							(ffb_top >= ff_top && ffb_bottom <= ff_bottom) ||
							(ffb_top <= ff_top && ffb_bottom >= ff_bottom) )
						{
							return false;
						}
					}
				}
				// trace is leaving a sector with a 3d-floor
				if (s==lastsector && frontflag==i-1)
				{
					// upper slope intersects with this 3d-floor
					if (lastztop<=ff_bottom && topz>ff_top)
					{
						topslope=lowslope;
					}
					// lower slope intersects with this 3d-floor
					if (lastzbottom>=ff_top && bottomz<ff_top)
					{
						bottomslope=highslope;
					}
				}
				if (topslope <= bottomslope) return false;		// stop
			}
		}
		lastsector = frontflag==0 ? li->backsector : li->frontsector;
	}
	else lastsector=NULL;	// don't need it if there are no 3D-floors

	lastztop= FixedMul (topslope, in->frac) + sightstart.z;
	lastzbottom= FixedMul (bottomslope, in->frac) + sightstart.z;

	return true;			// keep going
}



/*
==================
=
= P_SightCheckLine
=
===================
*/

bool SightCheck::P_SightCheckLine (line_t *ld)
{
	divline_t dl;

	if (ld->validcount == validcount)
	{
		return true;
	}
	ld->validcount = validcount;
	if (P_PointOnDivlineSidePrecise (ld->v1->x, ld->v1->y, &trace) ==
		P_PointOnDivlineSidePrecise (ld->v2->x, ld->v2->y, &trace))
	{
		return true;		// line isn't crossed
	}
	P_MakeDivline (ld, &dl);
	if (P_PointOnDivlineSidePrecise (trace.x, trace.y, &dl) ==
		P_PointOnDivlineSidePrecise (trace.x+trace.dx, trace.y+trace.dy, &dl))
	{
		return true;		// line isn't crossed
	}

	// try to early out the check
	if (!ld->backsector || !(ld->flags & ML_TWOSIDED) || (ld->flags & ML_BLOCKSIGHT))
		return false;	// stop checking

	// [RH] don't see past block everything lines
	if (ld->flags & ML_BLOCKEVERYTHING)
	{
		if (!(Flags & SF_SEEPASTBLOCKEVERYTHING))
		{
			return false;
		}
		// Pretend the other side is invisible if this is not an impact line
		// that runs a script on the current map. Used to prevent monsters
		// from trying to attack through a block everything line unless
		// there's a chance their attack will make it nonblocking.
		if (!(Flags & SF_SEEPASTSHOOTABLELINES))
		{
			if (!(ld->activation & SPAC_Impact))
			{
				return false;
			}
			if (ld->special != ACS_Execute && ld->special != ACS_ExecuteAlways)
			{
				return false;
			}
			if (ld->args[1] != 0 && ld->args[1] != level.levelnum)
			{
				return false;
			}
		}
	}

	sightcounts[3]++;
	// store the line for later intersection testing
	intercept_t newintercept;
	newintercept.isaline = true;
	newintercept.d.line = ld;
	intercepts.Push (newintercept);

	return true;
}

/*
==================
=
= P_SightBlockLinesIterator
=
===================
*/

int SightCheck::P_SightBlockLinesIterator (int x, int y)
{
	int offset;
	int *list;
	int res = 1;

	polyblock_t *polyLink;
	unsigned int i;
	extern polyblock_t **PolyBlockMap;

	offset = y*bmapwidth+x;

	// if any of the previous blocks may contain a portal we may abort the collection of lines here, but we may not abort the sight check.
	// (We still try to delay activating this for as long as possible.)
	portalfound = portalfound || PortalBlockmap(x, y).containsLinkedPortals;

	polyLink = PolyBlockMap[offset];
	portalfound |= (polyLink && PortalBlockmap.hasLinkedPolyPortals);
	while (polyLink)
	{
		if (polyLink->polyobj)
		{ // only check non-empty links
			if (polyLink->polyobj->validcount != validcount)
			{
				polyLink->polyobj->validcount = validcount;
				for (i = 0; i < polyLink->polyobj->Linedefs.Size(); i++)
				{
					if (!P_SightCheckLine(polyLink->polyobj->Linedefs[i]))
					{
						if (!portalfound) return 0;
						else res = -1;
					}
				}
			}
		}
		polyLink = polyLink->next;
	}

	offset = *(blockmap + offset);

	for (list = blockmaplump + offset + 1; *list != -1; list++)
	{
		if (!P_SightCheckLine (&lines[*list]))
		{
			if (!portalfound) return 0;
			else res = -1;
		}
	}

	return res;			// everything was checked
}

/*
====================
=
= P_SightTraverseIntercepts
=
= Returns true if the traverser function returns true for all lines
====================
*/

bool SightCheck::P_SightTraverseIntercepts ()
{
	unsigned count;
	fixed_t dist;
	intercept_t *scan, *in;
	unsigned scanpos;
	divline_t dl;

	count = intercepts.Size ();
//
// calculate intercept distance
//
	for (scanpos = 0; scanpos < intercepts.Size (); scanpos++)
	{
		scan = &intercepts[scanpos];
		P_MakeDivline (scan->d.line, &dl);
		scan->frac = P_InterceptVector (&trace, &dl);
		if (scan->frac < startfrac)
		{
			scan->frac = FIXED_MAX;
			count--;
		}
	}

//
// go through in order
// proper order is needed to handle 3D floors and portals.
//
	in = NULL;

	while (count--)
	{
		dist = FIXED_MAX;
		for (scanpos = 0; scanpos < intercepts.Size (); scanpos++)
		{
			scan = &intercepts[scanpos];
			if (scan->frac < dist)
			{
				dist = scan->frac;
				in = scan;
			}
		}

		if (in != NULL)
		{
			if (!PTR_SightTraverse (in))
				return false;					// don't bother going farther
			in->frac = FIXED_MAX;
		}
	}

	if (lastsector==seeingthing->Sector && lastsector->e->XFloor.ffloors.Size())
	{
		// we must do one last check whether the trace has crossed a 3D floor in the last sector

		fixed_t topz= topslope + sightstart.z;
		fixed_t bottomz= bottomslope + sightstart.z;
		
		for(unsigned int i=0;i<lastsector->e->XFloor.ffloors.Size();i++)
		{
			F3DFloor*  rover = lastsector->e->XFloor.ffloors[i];

			if((rover->flags & FF_SOLID) == myseethrough || !(rover->flags & FF_EXISTS)) continue;
			if ((Flags & SF_IGNOREWATERBOUNDARY) && (rover->flags & FF_SOLID) == 0) continue;
			
			fixed_t ff_bottom=rover->bottom.plane->ZatPoint(seeingthing);
			fixed_t ff_top=rover->top.plane->ZatPoint(seeingthing);

			if (lastztop<=ff_bottom && topz>ff_bottom && lastzbottom<=ff_bottom && bottomz>ff_bottom) return false;
			if (lastzbottom>=ff_top && bottomz<ff_top && lastztop>=ff_top && topz<ff_top) return false;
		}
	
	}
	return true;			// everything was traversed
}



/*
==================
=
= P_SightPathTraverse
=
= Traces a line from x1,y1 to x2,y2, calling the traverser function for each block
= Returns true if the traverser function returns true for all lines
==================
*/

bool SightCheck::P_SightPathTraverse ()
{
	fixed_t x1, x2, y1, y2;
	fixed_t xt1,yt1,xt2,yt2;
	long long _x1,_y1,_x2,_y2;
	fixed_t xstep,ystep;
	fixed_t partialx, partialy;
	fixed_t xintercept, yintercept;
	int mapx, mapy, mapxstep, mapystep;
	int count;

	validcount++;
	intercepts.Clear ();
	x1 = sightstart.x + FixedMul(startfrac, trace.dx);
	y1 = sightstart.y + FixedMul(startfrac, trace.dy);
	x2 = sightend.x;
	y2 = sightend.y;
	if (lastsector == NULL) lastsector = P_PointInSector(x1, y1);

	// for FF_SEETHROUGH the following rule applies:
	// If the viewer is in an area without FF_SEETHROUGH he can only see into areas without this flag
	// If the viewer is in an area with FF_SEETHROUGH he can only see into areas with this flag
	bool checkfloor = true, checkceiling = true;
	for(unsigned int i=0;i<lastsector->e->XFloor.ffloors.Size();i++)
	{
		F3DFloor*  rover = lastsector->e->XFloor.ffloors[i];

		if(!(rover->flags & FF_EXISTS)) continue;
		
		fixed_t ff_bottom=rover->bottom.plane->ZatPoint(sightstart);
		fixed_t ff_top=rover->top.plane->ZatPoint(sightstart);

		if (sightstart.z < ff_top) checkceiling = false;
		if (sightstart.z >= ff_bottom) checkfloor = false;

		if (sightstart.z < ff_top && sightstart.z >= ff_bottom) 
		{
			myseethrough = rover->flags & FF_SEETHROUGH;
			break;
		}
	}

	// We also must check if the starting sector contains  portals, and start sight checks in those as well.
	if (portaldir != sector_t::floor && checkceiling && !lastsector->PortalBlocksSight(sector_t::ceiling))
	{
		portals.Push({ 0, topslope, bottomslope, sector_t::ceiling, lastsector->SkyBoxes[sector_t::ceiling]->Sector->PortalGroup });
	}
	if (portaldir != sector_t::ceiling && checkfloor && !lastsector->PortalBlocksSight(sector_t::floor))
	{
		portals.Push({ 0, topslope, bottomslope, sector_t::floor, lastsector->SkyBoxes[sector_t::floor]->Sector->PortalGroup });
	}

	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT;							// don't side exactly on a line
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT;							// don't side exactly on a line

	_x1 = (long long)x1 - bmaporgx;
	_y1 = (long long)y1 - bmaporgy;
	x1 -= bmaporgx;
	y1 -= bmaporgy;
	xt1 = int(_x1 >> MAPBLOCKSHIFT);
	yt1 = int(_y1 >> MAPBLOCKSHIFT);

	_x2 = (long long)x2 - bmaporgx;
	_y2 = (long long)y2 - bmaporgy;
	x2 -= bmaporgx;
	y2 -= bmaporgy;
	xt2 = int(_x2 >> MAPBLOCKSHIFT);
	yt2 = int(_y2 >> MAPBLOCKSHIFT);

	if (xt2 > xt1)
	{
		mapxstep = 1;
		partialx = FRACUNIT - ((x1>>MAPBTOFRAC)&(FRACUNIT-1));
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else if (xt2 < xt1)
	{
		mapxstep = -1;
		partialx = (x1>>MAPBTOFRAC)&(FRACUNIT-1);
		ystep = FixedDiv (y2-y1,abs(x2-x1));
	}
	else
	{
		mapxstep = 0;
		partialx = FRACUNIT;
		ystep = 256*FRACUNIT;
	}
	yintercept = int(_y1>>MAPBTOFRAC) + FixedMul (partialx, ystep);


	if (yt2 > yt1)
	{
		mapystep = 1;
		partialy = FRACUNIT - ((y1>>MAPBTOFRAC)&(FRACUNIT-1));
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else if (yt2 < yt1)
	{
		mapystep = -1;
		partialy = (y1>>MAPBTOFRAC)&(FRACUNIT-1);
		xstep = FixedDiv (x2-x1,abs(y2-y1));
	}
	else
	{
		mapystep = 0;
		partialy = FRACUNIT;
		xstep = 256*FRACUNIT;
	}
	xintercept = int(_x1>>MAPBTOFRAC) + FixedMul (partialy, xstep);

	// [RH] Fix for traces that pass only through blockmap corners. In that case,
	// xintercept and yintercept can both be set ahead of mapx and mapy, so the
	// for loop would never advance anywhere.

	if (abs(xstep) == FRACUNIT && abs(ystep) == FRACUNIT)
	{
		if (ystep < 0)
		{
			partialx = FRACUNIT - partialx;
		}
		if (xstep < 0)
		{
			partialy = FRACUNIT - partialy;
		}
		if (partialx == partialy)
		{
			xintercept = xt1 << FRACBITS;
			yintercept = yt1 << FRACBITS;
		}
	}

//
// step through map blocks
// Count is present to prevent a round off error from skipping the break
	mapx = xt1;
	mapy = yt1;

	for (count = 0 ; count < 100 ; count++)
	{
		// end traversing when reaching the end of the blockmap
		// an early out is not possible because with portals a trace can easily land outside the map's bounds.
		if (mapx < 0 || mapx >= bmapwidth || mapy < 0 || mapy >= bmapheight)
		{
			break;
		}
		int res = P_SightBlockLinesIterator(mapx, mapy);
		if (res == 0)
		{
			sightcounts[1]++;
			return false;	// early out
		}

		// either reached the end or had an early-out condition with portals left to check,
		if (res == -1 || (mapxstep | mapystep) == 0)
			break;

		switch ((((yintercept >> FRACBITS) == mapy) << 1) | ((xintercept >> FRACBITS) == mapx))
		{
		case 0:		// neither xintercept nor yintercept match!
sightcounts[5]++;
			// Continuing won't make things any better, so we might as well stop right here
			count = 100;
			break;

		case 1:		// xintercept matches
			xintercept += xstep;
			mapy += mapystep;
			if (mapy == yt2)
				mapystep = 0;
			break;

		case 2:		// yintercept matches
			yintercept += ystep;
			mapx += mapxstep;
			if (mapx == xt2)
				mapxstep = 0;
			break;

		case 3:		// xintercept and yintercept both match
			sightcounts[4]++;
			// The trace is exiting a block through its corner. Not only does the block
			// being entered need to be checked (which will happen when this loop
			// continues), but the other two blocks adjacent to the corner also need to
			// be checked.
			if (!P_SightBlockLinesIterator (mapx + mapxstep, mapy) ||
				!P_SightBlockLinesIterator (mapx, mapy + mapystep))
			{
sightcounts[1]++;
				return false;
			}
			xintercept += xstep;
			yintercept += ystep;
			mapx += mapxstep;
			mapy += mapystep;
			if (mapx == xt2)
				mapxstep = 0;
			if (mapy == yt2)
				mapystep = 0;
			break;
		}
	}


//
// couldn't early out, so go through the sorted list
//
sightcounts[2]++;

	return P_SightTraverseIntercepts ( );
}

/*
=====================
=
= P_CheckSight
=
= Returns true if a straight line between t1 and t2 is unobstructed
= look from eyes of t1 to any part of t2
=
= killough 4/20/98: cleaned up, made to use new LOS struct
=
=====================
*/

bool P_CheckSight (AActor *t1, AActor *t2, int flags)
{
	SightCycles.Clock();

	bool res;

	assert (t1 != NULL);
	assert (t2 != NULL);
	if (t1 == NULL || t2 == NULL)
	{
		return false;
	}

	const sector_t *s1 = t1->Sector;
	const sector_t *s2 = t2->Sector;
	int pnum = int(s1 - sectors) * numsectors + int(s2 - sectors);

//
// check for trivial rejection
//
	if (rejectmatrix != NULL &&
		(rejectmatrix[pnum>>3] & (1 << (pnum & 7))))
	{
sightcounts[0]++;
		res = false;			// can't possibly be connected
		goto done;
	}

//
// check precisely
//
	// [RH] Andy Baker's stealth monsters:
	// Cannot see an invisible object
	if ((flags & SF_IGNOREVISIBILITY) == 0 && ((t2->renderflags & RF_INVISIBLE) || !t2->RenderStyle.IsVisible(t2->alpha)))
	{ // small chance of an attack being made anyway
		if ((bglobal.m_Thinking ? pr_botchecksight() : pr_checksight()) > 50)
		{
			res = false;
			goto done;
		}
	}

	// killough 4/19/98: make fake floors and ceilings block monster view

	if (!(flags & SF_IGNOREWATERBOUNDARY))
	{
		if ((s1->GetHeightSec() &&
			((t1->Z() + t1->height <= s1->heightsec->floorplane.ZatPoint(t1) &&
			  t2->Z() >= s1->heightsec->floorplane.ZatPoint(t2)) ||
			 (t1->Z() >= s1->heightsec->ceilingplane.ZatPoint(t1) &&
			  t2->Z() + t1->height <= s1->heightsec->ceilingplane.ZatPoint(t2))))
			||
			(s2->GetHeightSec() &&
			 ((t2->Z() + t2->height <= s2->heightsec->floorplane.ZatPoint(t2) &&
			   t1->Z() >= s2->heightsec->floorplane.ZatPoint(t1)) ||
			  (t2->Z() >= s2->heightsec->ceilingplane.ZatPoint(t2) &&
			   t1->Z() + t2->height <= s2->heightsec->ceilingplane.ZatPoint(t1)))))
		{
			res = false;
			goto done;
		}
	}

	// An unobstructed LOS is possible.
	// Now look from eyes of t1 to any part of t2.

	validcount++;
	portals.Clear();
	{
		sector_t *sec;
		fixed_t lookheight = t1->height - (t1->height >> 2);
		t1->GetPortalTransition(lookheight, &sec);

		fixed_t bottomslope = t2->Z() - (t1->Z() + lookheight);
		fixed_t topslope = bottomslope + t2->height;
		SightTask task = { 0, topslope, bottomslope, -1, sec->PortalGroup };


		SightCheck s;
		s.init(t1, t2, sec, &task, flags);
		res = s.P_SightPathTraverse ();
		if (!res)
		{
			fixed_t dist = t1->AproxDistance(t2);
			for (unsigned i = 0; i < portals.Size(); i++)
			{
				portals[i].frac += FixedDiv(FRACUNIT, dist);
				s.init(t1, t2, NULL, &portals[i], flags);
				if (s.P_SightPathTraverse())
				{
					res = true;
					break;
				}
			}
		}
	}

done:
	SightCycles.Unclock();
	return res;
}

ADD_STAT (sight)
{
	FString out;
	out.Format ("%04.1f ms (%04.1f max), %5d %2d%4d%4d%4d%4d\n",
		SightCycles.TimeMS(), MaxSightCycles.TimeMS(),
		sightcounts[3], sightcounts[0], sightcounts[1], sightcounts[2], sightcounts[4], sightcounts[5]);
	return out;
}

void P_ResetSightCounters (bool full)
{
	if (full)
	{
		MaxSightCycles.Reset();
	}
	if (SightCycles.Time() > MaxSightCycles.Time())
	{
		MaxSightCycles = SightCycles;
	}
	SightCycles.Reset();
	memset (sightcounts, 0, sizeof(sightcounts));
}


