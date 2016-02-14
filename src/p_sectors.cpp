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
//		Sector utility functions.
//
//-----------------------------------------------------------------------------

#include "p_spec.h"
#include "c_cvars.h"
#include "doomstat.h"
#include "g_level.h"
#include "nodebuild.h"
#include "p_terrain.h"
#include "po_man.h"
#include "farchive.h"
#include "r_utility.h"
#include "a_sharedglobal.h"
#include "r_data/colormaps.h"


// [RH]
// P_NextSpecialSector()
//
// Returns the next special sector attached to this sector
// with a certain special.
sector_t *sector_t::NextSpecialSector (int type, sector_t *nogood) const
{
	sector_t *tsec;
	int i;

	for (i = 0; i < linecount; i++)
	{
		line_t *ln = lines[i];

		if (NULL != (tsec = getNextSector (ln, this)) &&
			tsec != nogood &&
			tsec->special == type)
		{
			return tsec;
		}
	}
	return NULL;
}

//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t sector_t::FindLowestFloorSurrounding (vertex_t **v) const
{
	int i;
	sector_t *other;
	line_t *check;
	fixed_t floor;
	fixed_t ofloor;
	vertex_t *spot;

	if (linecount == 0) return GetPlaneTexZ(sector_t::floor);

	spot = lines[0]->v1;
	floor = floorplane.ZatPoint (spot);

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			if (ofloor < floor && ofloor < floorplane.ZatPoint (check->v1))
			{
				floor = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			if (ofloor < floor && ofloor < floorplane.ZatPoint (check->v2))
			{
				floor = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return floor;
}



//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
fixed_t sector_t::FindHighestFloorSurrounding (vertex_t **v) const
{
	int i;
	line_t *check;
	sector_t *other;
	fixed_t floor;
	fixed_t ofloor;
	vertex_t *spot;

	if (linecount == 0) return GetPlaneTexZ(sector_t::floor);

	spot = lines[0]->v1;
	floor = FIXED_MIN;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			if (ofloor > floor)
			{
				floor = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			if (ofloor > floor)
			{
				floor = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return floor;
}



//
// P_FindNextHighestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the smallest floor height in a surrounding sector larger than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// Rewritten by Lee Killough to avoid fixed array and to be faster
//
fixed_t sector_t::FindNextHighestFloor (vertex_t **v) const
{
	fixed_t height;
	fixed_t heightdiff;
	fixed_t ofloor, floor;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;

	if (linecount == 0) return GetPlaneTexZ(sector_t::floor);

	spot = lines[0]->v1;
	height = floorplane.ZatPoint (spot);
	heightdiff = FIXED_MAX;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			floor = floorplane.ZatPoint (check->v1);
			if (ofloor > floor && ofloor - floor < heightdiff && !IsLinked(other, false))
			{
				heightdiff = ofloor - floor;
				height = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			floor = floorplane.ZatPoint (check->v2);
			if (ofloor > floor && ofloor - floor < heightdiff && !IsLinked(other, false))
			{
				heightdiff = ofloor - floor;
				height = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}


//
// P_FindNextLowestFloor()
//
// Passed a sector and a floor height, returns the fixed point value
// of the largest floor height in a surrounding sector smaller than
// the floor height passed. If no such height exists the floorheight
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t sector_t::FindNextLowestFloor (vertex_t **v) const
{
	fixed_t height;
	fixed_t heightdiff;
	fixed_t ofloor, floor;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;

	if (linecount == 0) return GetPlaneTexZ(sector_t::floor);

	spot = lines[0]->v1;
	height = floorplane.ZatPoint (spot);
	heightdiff = FIXED_MAX;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			floor = floorplane.ZatPoint (check->v1);
			if (ofloor < floor && floor - ofloor < heightdiff && !IsLinked(other, false))
			{
				heightdiff = floor - ofloor;
				height = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			floor = floorplane.ZatPoint (check->v2);
			if (ofloor < floor && floor - ofloor < heightdiff && !IsLinked(other, false))
			{
				heightdiff = floor - ofloor;
				height = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}

//
// P_FindNextLowestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the largest ceiling height in a surrounding sector smaller than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t sector_t::FindNextLowestCeiling (vertex_t **v) const
{
	fixed_t height;
	fixed_t heightdiff;
	fixed_t oceil, ceil;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;


	if (linecount == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = lines[0]->v1;
	height = ceilingplane.ZatPoint (spot);
	heightdiff = FIXED_MAX;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint (check->v1);
			ceil = ceilingplane.ZatPoint (check->v1);
			if (oceil < ceil && ceil - oceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = ceil - oceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint (check->v2);
			ceil = ceilingplane.ZatPoint (check->v2);
			if (oceil < ceil && ceil - oceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = ceil - oceil;
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}

//
// P_FindNextHighestCeiling()
//
// Passed a sector and a ceiling height, returns the fixed point value
// of the smallest ceiling height in a surrounding sector larger than
// the ceiling height passed. If no such height exists the ceiling height
// passed is returned.
//
// jff 02/03/98 Twiddled Lee's P_FindNextHighestFloor to make this
//
fixed_t sector_t::FindNextHighestCeiling (vertex_t **v) const
{
	fixed_t height;
	fixed_t heightdiff;
	fixed_t oceil, ceil;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;

	if (linecount == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = lines[0]->v1;
	height = ceilingplane.ZatPoint (spot);
	heightdiff = FIXED_MAX;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint (check->v1);
			ceil = ceilingplane.ZatPoint (check->v1);
			if (oceil > ceil && oceil - ceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = oceil - ceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint (check->v2);
			ceil = ceilingplane.ZatPoint (check->v2);
			if (oceil > ceil && oceil - ceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = oceil - ceil;
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t sector_t::FindLowestCeilingSurrounding (vertex_t **v) const
{
	fixed_t height;
	fixed_t oceil;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;

	if (linecount == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = lines[0]->v1;
	height = FIXED_MAX;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint (check->v1);
			if (oceil < height)
			{
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint (check->v2);
			if (oceil < height)
			{
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
fixed_t sector_t::FindHighestCeilingSurrounding (vertex_t **v) const
{
	fixed_t height;
	fixed_t oceil;
	sector_t *other;
	vertex_t *spot;
	line_t *check;
	int i;

	if (linecount == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = lines[0]->v1;
	height = FIXED_MIN;

	for (i = 0; i < linecount; i++)
	{
		check = lines[i];
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint (check->v1);
			if (oceil > height)
			{
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint (check->v2);
			if (oceil > height)
			{
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}

//
// P_FindShortestTextureAround()
//
// Passed a sector number, returns the shortest lower texture on a
// linedef bounding the sector.
//
// jff 02/03/98 Add routine to find shortest lower texture
//

static inline void CheckShortestTex (FTextureID texnum, fixed_t &minsize)
{
	if (texnum.isValid() || (texnum.isNull() && (i_compatflags & COMPATF_SHORTTEX)))
	{
		FTexture *tex = TexMan[texnum];
		if (tex != NULL)
		{
			fixed_t h = tex->GetScaledHeight()<<FRACBITS;
			if (h < minsize)
			{
				minsize = h;
			}
		}
	}
}

fixed_t sector_t::FindShortestTextureAround () const
{
	fixed_t minsize = FIXED_MAX;

	for (int i = 0; i < linecount; i++)
	{
		if (lines[i]->flags & ML_TWOSIDED)
		{
			CheckShortestTex (lines[i]->sidedef[0]->GetTexture(side_t::bottom), minsize);
			CheckShortestTex (lines[i]->sidedef[1]->GetTexture(side_t::bottom), minsize);
		}
	}
	return minsize < FIXED_MAX ? minsize : TexMan[0]->GetHeight() * FRACUNIT;
}


//
// P_FindShortestUpperAround()
//
// Passed a sector number, returns the shortest upper texture on a
// linedef bounding the sector.
//
// Note: If no upper texture exists MAXINT is returned.
//
// jff 03/20/98 Add routine to find shortest upper texture
//
fixed_t sector_t::FindShortestUpperAround () const
{
	fixed_t minsize = FIXED_MAX;

	for (int i = 0; i < linecount; i++)
	{
		if (lines[i]->flags & ML_TWOSIDED)
		{
			CheckShortestTex (lines[i]->sidedef[0]->GetTexture(side_t::top), minsize);
			CheckShortestTex (lines[i]->sidedef[1]->GetTexture(side_t::top), minsize);
		}
	}
	return minsize < FIXED_MAX ? minsize : TexMan[0]->GetHeight() * FRACUNIT;
}


//
// P_FindModelFloorSector()
//
// Passed a floor height and a sector number, return a pointer to a
// a sector with that floor height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model floor
//  around a sector specified by sector number
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using floormove_t
//
sector_t *sector_t::FindModelFloorSector (fixed_t floordestheight) const
{
	int i;
	sector_t *sec;

	//jff 5/23/98 don't disturb sec->linecount while searching
	// but allow early exit in old demos
	for (i = 0; i < linecount; i++)
	{
		sec = getNextSector (lines[i], this);
		if (sec != NULL &&
			(sec->floorplane.ZatPoint (lines[i]->v1) == floordestheight ||
			 sec->floorplane.ZatPoint (lines[i]->v2) == floordestheight))
		{
			return sec;
		}
	}
	return NULL;
}


//
// P_FindModelCeilingSector()
//
// Passed a ceiling height and a sector number, return a pointer to a
// a sector with that ceiling height across the lowest numbered two sided
// line surrounding the sector.
//
// Note: If no sector at that height bounds the sector passed, return NULL
//
// jff 02/03/98 Add routine to find numeric model ceiling
//  around a sector specified by sector number
//  used only from generalized ceiling types
// jff 3/14/98 change first parameter to plain height to allow call
//  from routine not using ceiling_t
//
sector_t *sector_t::FindModelCeilingSector (fixed_t floordestheight) const
{
	int i;
	sector_t *sec;

	//jff 5/23/98 don't disturb sec->linecount while searching
	// but allow early exit in old demos
	for (i = 0; i < linecount; i++)
	{
		sec = getNextSector (lines[i], this);
		if (sec != NULL &&
			(sec->ceilingplane.ZatPoint (lines[i]->v1) == floordestheight ||
			 sec->ceilingplane.ZatPoint (lines[i]->v2) == floordestheight))
		{
			return sec;
		}
	}
	return NULL;
}

//
// Find minimum light from an adjacent sector
//
int sector_t::FindMinSurroundingLight (int min) const
{
	int 		i;
	line_t* 	line;
	sector_t*	check;
		
	for (i = 0; i < linecount; i++)
	{
		line = lines[i];
		if (NULL != (check = getNextSector (line, this)) &&
			check->lightlevel < min)
		{
			min = check->lightlevel;
		}
	}
	return min;
}

//
// Find the highest point on the floor of the sector
//
fixed_t sector_t::FindHighestFloorPoint (vertex_t **v) const
{
	int i;
	line_t *line;
	fixed_t height = FIXED_MIN;
	fixed_t probeheight;
	vertex_t *spot = NULL;

	if ((floorplane.a | floorplane.b) == 0)
	{
		if (v != NULL)
		{
			if (linecount == 0) *v = &vertexes[0];
			else *v = lines[0]->v1;
		}
		return -floorplane.d;
	}

	for (i = 0; i < linecount; i++)
	{
		line = lines[i];
		probeheight = floorplane.ZatPoint (line->v1);
		if (probeheight > height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = floorplane.ZatPoint (line->v2);
		if (probeheight > height)
		{
			height = probeheight;
			spot = line->v2;
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}

//
// Find the lowest point on the ceiling of the sector
//
fixed_t sector_t::FindLowestCeilingPoint (vertex_t **v) const
{
	int i;
	line_t *line;
	fixed_t height = FIXED_MAX;
	fixed_t probeheight;
	vertex_t *spot = NULL;

	if ((ceilingplane.a | ceilingplane.b) == 0)
	{
		if (v != NULL)
		{
			if (linecount == 0) *v = &vertexes[0];
			else *v = lines[0]->v1;
		}
		return ceilingplane.d;
	}

	for (i = 0; i < linecount; i++)
	{
		line = lines[i];
		probeheight = ceilingplane.ZatPoint (line->v1);
		if (probeheight < height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = ceilingplane.ZatPoint (line->v2);
		if (probeheight < height)
		{
			height = probeheight;
			spot = line->v2;
		}
	}
	if (v != NULL)
		*v = spot;
	return height;
}


void sector_t::SetColor(int r, int g, int b, int desat)
{
	PalEntry color = PalEntry (r,g,b);
	ColorMap = GetSpecialLights (color, ColorMap->Fade, desat);
	P_RecalculateAttachedLights(this);
}

void sector_t::SetFade(int r, int g, int b)
{
	PalEntry fade = PalEntry (r,g,b);
	ColorMap = GetSpecialLights (ColorMap->Color, fade, ColorMap->Desaturate);
	P_RecalculateAttachedLights(this);
}

//===========================================================================
//
// sector_t :: ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the sector's defining
// lines that is nearest to (x,y).
//
//===========================================================================

void sector_t::ClosestPoint(fixed_t fx, fixed_t fy, fixed_t &ox, fixed_t &oy) const
{
	int i;
	double x = fx, y = fy;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;

	for (i = 0; i < linecount; ++i)
	{
		vertex_t *v1 = lines[i]->v1;
		vertex_t *v2 = lines[i]->v2;
		double a = v2->x - v1->x;
		double b = v2->y - v1->y;
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->x;
			iy = v1->y;
		}
		else
		{
			double num = (x - v1->x) * a + (y - v1->y) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->x;
				iy = v1->y;
			}
			else if (u >= 1)
			{
				ix = v2->x;
				iy = v2->y;
			}
			else
			{
				ix = v1->x + u * a;
				iy = v1->y + u * b;
			}
		}
		a = (ix - x);
		b = (iy - y);
		dist = a*a + b*b;
		if (dist < bestdist)
		{
			bestdist = dist;
			bestx = ix;
			besty = iy;
		}
	}
	ox = fixed_t(bestx);
	oy = fixed_t(besty);
}


bool sector_t::PlaneMoving(int pos)
{
	if (pos == floor)
		return (floordata != NULL || (planes[floor].Flags & PLANEF_BLOCKED));
	else
		return (ceilingdata != NULL || (planes[ceiling].Flags & PLANEF_BLOCKED));
}


int sector_t::GetFloorLight () const
{
	if (GetFlags(sector_t::floor) & PLANEF_ABSLIGHTING)
	{
		return GetPlaneLight(floor);
	}
	else
	{
		return ClampLight(lightlevel + GetPlaneLight(floor));
	}
}

int sector_t::GetCeilingLight () const
{
	if (GetFlags(ceiling) & PLANEF_ABSLIGHTING)
	{
		return GetPlaneLight(ceiling);
	}
	else
	{
		return ClampLight(lightlevel + GetPlaneLight(ceiling));
	}
}


ASkyViewpoint *sector_t::GetSkyBox(int which)
{
	if (SkyBoxes[which] != NULL) return SkyBoxes[which];
	if (MoreFlags & (SECF_NOFLOORSKYBOX << which)) return NULL;
	return level.DefaultSkybox;
}


sector_t *sector_t::GetHeightSec() const 
{
	if (heightsec == NULL)
	{
		return NULL;
	}
	if (heightsec->MoreFlags & SECF_IGNOREHEIGHTSEC)
	{
		return NULL;
	}
	if (e && e->XFloor.ffloors.Size())
	{
		// If any of these fake floors render their planes, ignore heightsec.
		for (unsigned i = e->XFloor.ffloors.Size(); i-- > 0; )
		{
			if ((e->XFloor.ffloors[i]->flags & (FF_EXISTS | FF_RENDERPLANES)) == (FF_EXISTS | FF_RENDERPLANES))
			{
				return NULL;
			}
		}
	}
	return heightsec;
}


void sector_t::GetSpecial(secspecial_t *spec)
{
	spec->special = special;
	spec->damageamount = damageamount;
	spec->damagetype = damagetype;
	spec->damageinterval = damageinterval;
	spec->leakydamage = leakydamage;
	spec->Flags = Flags & SECF_SPECIALFLAGS;
}

void sector_t::SetSpecial(const secspecial_t *spec)
{
	special = spec->special;
	damageamount = spec->damageamount;
	damagetype = spec->damagetype;
	damageinterval = spec->damageinterval;
	leakydamage = spec->leakydamage;
	Flags = (Flags & ~SECF_SPECIALFLAGS) | (spec->Flags & SECF_SPECIALFLAGS);
}

void sector_t::TransferSpecial(sector_t *model)
{
	special = model->special;
	damageamount = model->damageamount;
	damagetype = model->damagetype;
	damageinterval = model->damageinterval;
	leakydamage = model->leakydamage;
	Flags = (Flags&~SECF_SPECIALFLAGS) | (model->Flags & SECF_SPECIALFLAGS);
}

int sector_t::GetTerrain(int pos) const
{
	return terrainnum[pos] >= 0 ? terrainnum[pos] : TerrainTypes[GetTexture(pos)];
}

FArchive &operator<< (FArchive &arc, secspecial_t &p)
{
	if (SaveVersion < 4529)
	{
		int special;
		arc << special;
		sector_t sec;
		memset(&sec, 0, sizeof(sec));
		P_InitSectorSpecial(&sec, special, true);
		sec.GetSpecial(&p);
	}
	else
	{
		arc << p.special
			<< p.damageamount
			<< p.damagetype
			<< p.damageinterval
			<< p.leakydamage
			<< p.Flags;
	}
	return arc;
}



bool secplane_t::CopyPlaneIfValid (secplane_t *dest, const secplane_t *opp) const
{
	bool copy = false;

	// If the planes do not have matching slopes, then always copy them
	// because clipping would require creating new sectors.
	if (a != dest->a || b != dest->b || c != dest->c)
	{
		copy = true;
	}
	else if (opp->a != -dest->a || opp->b != -dest->b || opp->c != -dest->c)
	{
		if (d < dest->d)
		{
			copy = true;
		}
	}
	else if (d < dest->d && d > -opp->d)
	{
		copy = true;
	}

	if (copy)
	{
		*dest = *this;
	}

	return copy;
}

FArchive &operator<< (FArchive &arc, secplane_t &plane)
{
	arc << plane.a << plane.b << plane.c << plane.d;
	//if (plane.c != 0)
	{	// plane.c should always be non-0. Otherwise, the plane
		// would be perfectly vertical.
		plane.ic = DivScale32 (1, plane.c);
	}
	return arc;
}

//==========================================================================
//
// P_AlignFlat
//
//==========================================================================

bool P_AlignFlat (int linenum, int side, int fc)
{
	line_t *line = lines + linenum;
	sector_t *sec = side ? line->backsector : line->frontsector;

	if (!sec)
		return false;

	fixed_t x = line->v1->x;
	fixed_t y = line->v1->y;

	angle_t angle = R_PointToAngle2 (x, y, line->v2->x, line->v2->y);
	angle_t norm = (angle-ANGLE_90) >> ANGLETOFINESHIFT;

	fixed_t dist = -DMulScale16 (finecosine[norm], x, finesine[norm], y);

	if (side)
	{
		angle = angle + ANGLE_180;
		dist = -dist;
	}

	sec->SetBase(fc, dist & ((1<<(FRACBITS+8))-1), 0-angle);
	return true;
}

//==========================================================================
//
// P_BuildPolyBSP
//
//==========================================================================
static FNodeBuilder::FLevel PolyNodeLevel;
static FNodeBuilder PolyNodeBuilder(PolyNodeLevel);

void subsector_t::BuildPolyBSP()
{
	assert((BSP == NULL || BSP->bDirty) && "BSP computed more than once");

	// Set up level information for the node builder.
	PolyNodeLevel.Sides = sides;
	PolyNodeLevel.NumSides = numsides;
	PolyNodeLevel.Lines = lines;
	PolyNodeLevel.NumLines = numlines;

	// Feed segs to the nodebuilder and build the nodes.
	PolyNodeBuilder.Clear();
	PolyNodeBuilder.AddSegs(firstline, numlines);
	for (FPolyNode *pn = polys; pn != NULL; pn = pn->pnext)
	{
		PolyNodeBuilder.AddPolySegs(&pn->segs[0], (int)pn->segs.Size());
	}
	PolyNodeBuilder.BuildMini(false);
	if (BSP == NULL)
	{
		BSP = new FMiniBSP;
	}
	PolyNodeBuilder.ExtractMini(BSP);
	for (unsigned int i = 0; i < BSP->Subsectors.Size(); ++i)
	{
		BSP->Subsectors[i].sector = sector;
	}
}

//==========================================================================
//
//
//
//==========================================================================

CUSTOM_CVAR(Int, r_fakecontrast, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 1;
	else if (self > 2) self = 2;
}

//==========================================================================
//
//
//
//==========================================================================

int side_t::GetLightLevel (bool foggy, int baselight, bool is3dlight, int *pfakecontrast) const
{
	if (!is3dlight && (Flags & WALLF_ABSLIGHTING))
	{
		baselight = Light;
	}

	if (pfakecontrast != NULL)
	{
		*pfakecontrast = 0;
	}

	if (!foggy || level.flags3 & LEVEL3_FORCEFAKECONTRAST) // Don't do relative lighting in foggy sectors
	{
		if (!(Flags & WALLF_NOFAKECONTRAST) && r_fakecontrast != 0)
		{
			int rel;
			if (((level.flags2 & LEVEL2_SMOOTHLIGHTING) || (Flags & WALLF_SMOOTHLIGHTING) || r_fakecontrast == 2) &&
				linedef->dx != 0)
			{
				rel = xs_RoundToInt // OMG LEE KILLOUGH LIVES! :/
					(
						level.WallHorizLight
						+ fabs(atan(double(linedef->dy) / linedef->dx) / 1.57079)
						* (level.WallVertLight - level.WallHorizLight)
					);
			}
			else
			{
				rel = linedef->dx == 0 ? level.WallVertLight : 
					  linedef->dy == 0 ? level.WallHorizLight : 0;
			}
			if (pfakecontrast != NULL)
			{
				*pfakecontrast = rel;
			}
			else
			{
				baselight += rel;
			}
		}
	}
	if (!is3dlight && !(Flags & WALLF_ABSLIGHTING) && (!foggy || (Flags & WALLF_LIGHT_FOG)))
	{
		baselight += this->Light;
	}
	return baselight;
}

