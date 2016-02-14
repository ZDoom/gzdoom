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
#include "m_random.h"
#include "m_bbox.h"
#include "p_lnspec.h"
#include "g_level.h"
#include "po_man.h"

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

static TArray<intercept_t> intercepts (128);

class SightCheck
{
	fixed_t sightzstart;				// eye z of looker
	const AActor * sightthing;
	const AActor * seeingthing;
	fixed_t lastztop;				// z at last line
	fixed_t lastzbottom;				// z at last line
	sector_t * lastsector;			// last sector being entered by trace
	fixed_t topslope, bottomslope;	// slopes to top and bottom of target
	int Flags;
	divline_t trace;
	unsigned int myseethrough;

	bool PTR_SightTraverse (intercept_t *in);
	bool P_SightCheckLine (line_t *ld);
	bool P_SightBlockLinesIterator (int x, int y);
	bool P_SightTraverseIntercepts ();

public:
	bool P_SightPathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2);

	SightCheck(const AActor * t1, const AActor * t2, int flags)
	{
		lastztop = lastzbottom = sightzstart = t1->Z() + t1->height - (t1->height>>2);
		lastsector = t1->Sector;
		sightthing=t1;
		seeingthing=t2;
		bottomslope = t2->Z() - sightzstart;
		topslope = bottomslope + t2->height;
		Flags = flags;

		myseethrough = FF_SEETHROUGH;
	}
};

/*
==============
=
= PTR_SightTraverse
=
==============
*/

/*
static bool PTR_SightTraverse (intercept_t *in)
*/
bool SightCheck::PTR_SightTraverse (intercept_t *in)
{
	line_t  *li;
	fixed_t slope;
	FLineOpening open;

	li = in->d.line;

//
// crosses a two sided line
//

	// ignore self referencing sectors if COMPAT_TRACE is on
	if ((i_compatflags & COMPATF_TRACE) && li->frontsector == li->backsector) 
		return true;

	fixed_t trX=trace.x + FixedMul (trace.dx, in->frac);
	fixed_t trY=trace.y + FixedMul (trace.dy, in->frac);
	P_LineOpening (open, NULL, li, trX, trY);

	if (open.range <= 0)		// quick test for totally closed doors
		return false;		// stop

	// check bottom
	slope = FixedDiv (open.bottom - sightzstart, in->frac);
	if (slope > bottomslope)
		bottomslope = slope;

	// check top
	slope = FixedDiv (open.top - sightzstart, in->frac);
	if (slope < topslope)
		topslope = slope;

	if (topslope <= bottomslope)
		return false;		// stop

	// now handle 3D-floors
	if(li->frontsector->e->XFloor.ffloors.Size() || li->backsector->e->XFloor.ffloors.Size())
	{
		int  frontflag;
		
		frontflag = P_PointOnLineSidePrecise(sightthing->X(), sightthing->Y(), li);
		
		//Check 3D FLOORS!
		for(int i=1;i<=2;i++)
		{
			sector_t * s=i==1? li->frontsector:li->backsector;
			fixed_t    highslope, lowslope;

			fixed_t topz= FixedMul (topslope, in->frac) + sightzstart;
			fixed_t bottomz= FixedMul (bottomslope, in->frac) + sightzstart;

			for(unsigned int j=0;j<s->e->XFloor.ffloors.Size();j++)
			{
				F3DFloor*  rover=s->e->XFloor.ffloors[j];

				if((rover->flags & FF_SEETHROUGH) == myseethrough || !(rover->flags & FF_EXISTS)) continue;
				if ((Flags & SF_IGNOREWATERBOUNDARY) && (rover->flags & FF_SOLID) == 0) continue;
				
				fixed_t ff_bottom=rover->bottom.plane->ZatPoint(trX, trY);
				fixed_t ff_top=rover->top.plane->ZatPoint(trX, trY);

				highslope = FixedDiv (ff_top - sightzstart, in->frac);
				lowslope = FixedDiv (ff_bottom - sightzstart, in->frac);

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

	lastztop= FixedMul (topslope, in->frac) + sightzstart;
	lastzbottom= FixedMul (bottomslope, in->frac) + sightzstart;

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

bool SightCheck::P_SightBlockLinesIterator (int x, int y)
{
	int offset;
	int *list;

	polyblock_t *polyLink;
	unsigned int i;
	extern polyblock_t **PolyBlockMap;

	offset = y*bmapwidth+x;

	polyLink = PolyBlockMap[offset];
	while (polyLink)
	{
		if (polyLink->polyobj)
		{ // only check non-empty links
			if (polyLink->polyobj->validcount != validcount)
			{
				polyLink->polyobj->validcount = validcount;
				for (i = 0; i < polyLink->polyobj->Linedefs.Size(); i++)
				{
					if (!P_SightCheckLine (polyLink->polyobj->Linedefs[i]))
						return false;
				}
			}
		}
		polyLink = polyLink->next;
	}

	offset = *(blockmap + offset);

	for (list = blockmaplump + offset + 1; *list != -1; list++)
	{
		if (!P_SightCheckLine (&lines[*list]))
			return false;
	}

	return true;			// everything was checked
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
	}

//
// go through in order
// [RH] Is it really necessary to go through in order? All we care about is if
// the trace is obstructed, not what specifically obstructed it.
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

		fixed_t topz= topslope + sightzstart;
		fixed_t bottomz= bottomslope + sightzstart;
		
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

bool SightCheck::P_SightPathTraverse (fixed_t x1, fixed_t y1, fixed_t x2, fixed_t y2)
{
	fixed_t xt1,yt1,xt2,yt2;
	long long _x1,_y1,_x2,_y2;
	fixed_t xstep,ystep;
	fixed_t partialx, partialy;
	fixed_t xintercept, yintercept;
	int mapx, mapy, mapxstep, mapystep;
	int count;

	validcount++;
	intercepts.Clear ();

	// for FF_SEETHROUGH the following rule applies:
	// If the viewer is in an area without FF_SEETHROUGH he can only see into areas without this flag
	// If the viewer is in an area with FF_SEETHROUGH he can only see into areas with this flag
	for(unsigned int i=0;i<lastsector->e->XFloor.ffloors.Size();i++)
	{
		F3DFloor*  rover = lastsector->e->XFloor.ffloors[i];

		if(!(rover->flags & FF_EXISTS)) continue;
		
		fixed_t ff_bottom=rover->bottom.plane->ZatPoint(sightthing);
		fixed_t ff_top=rover->top.plane->ZatPoint(sightthing);

		if (sightzstart < ff_top && sightzstart >= ff_bottom) 
		{
			myseethrough = rover->flags & FF_SEETHROUGH;
			break;
		}
	}

	if ( ((x1-bmaporgx)&(MAPBLOCKSIZE-1)) == 0)
		x1 += FRACUNIT;							// don't side exactly on a line
	if ( ((y1-bmaporgy)&(MAPBLOCKSIZE-1)) == 0)
		y1 += FRACUNIT;							// don't side exactly on a line
	trace.x = x1;
	trace.y = y1;
	trace.dx = x2 - x1;
	trace.dy = y2 - y1;

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

// points should never be out of bounds, but check once instead of
// each block
	if (xt1<0 || yt1<0 || xt1>=bmapwidth || yt1>=bmapheight
	||  xt2<0 || yt2<0 || xt2>=bmapwidth || yt2>=bmapheight)
		return false;

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
		if (!P_SightBlockLinesIterator (mapx, mapy))
		{
sightcounts[1]++;
			return false;	// early out
		}

		if ((mapxstep | mapystep) == 0)
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

bool P_CheckSight (const AActor *t1, const AActor *t2, int flags)
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
	{
		SightCheck s(t1, t2, flags);
		res = s.P_SightPathTraverse (t1->X(), t1->Y(), t2->X(), t2->Y());
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


