//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2017 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Sector utility functions.
//
//-----------------------------------------------------------------------------

/* For code that originates from ZDoom the following applies:
**
**---------------------------------------------------------------------------
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

#include "p_spec.h"
#include "p_lnspec.h"
#include "c_cvars.h"
#include "doomstat.h"
#include "g_level.h"
#include "nodebuild.h"
#include "p_terrain.h"
#include "po_man.h"
#include "serializer.h"
#include "r_utility.h"
#include "a_sharedglobal.h"
#include "p_local.h"
#include "r_sky.h"
#include "r_data/colormaps.h"
#include "g_levellocals.h"
#include "vm.h"


// [RH]
// P_NextSpecialSector()
//
// Returns the next special sector attached to this sector
// with a certain special.
sector_t *sector_t::NextSpecialSector (int type, sector_t *nogood) const
{
	sector_t *tsec;
	for (auto ln : Lines)
	{
		if (NULL != (tsec = getNextSector (ln, this)) &&
			tsec != nogood &&
			tsec->special == type)
		{
			return tsec;
		}
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, NextSpecialSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(type);
	PARAM_POINTER(nogood, sector_t);
	ACTION_RETURN_POINTER(self->NextSpecialSector(type, nogood));
}

//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
double sector_t::FindLowestFloorSurrounding (vertex_t **v) const
{
	sector_t *other;
	double floor;
	double ofloor;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::floor);

	spot = Lines[0]->v1;
	floor = floorplane.ZatPoint(spot);

	for (auto check : Lines)
	{
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

DEFINE_ACTION_FUNCTION(_Sector, FindLowestFloorSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindLowestFloorSurrounding(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}
	
//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
double sector_t::FindHighestFloorSurrounding (vertex_t **v) const
{
	sector_t *other;
	double floor;
	double ofloor;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::floor);

	spot = Lines[0]->v1;
	floor = -FLT_MAX;

	for (auto check : Lines)
	{
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

DEFINE_ACTION_FUNCTION(_Sector, FindHighestFloorSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindHighestFloorSurrounding(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
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
double sector_t::FindNextHighestFloor (vertex_t **v) const
{
	double height;
	double heightdiff;
	double ofloor, floor;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::floor);

	spot = Lines[0]->v1;
	height = floorplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : Lines)
	{
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

DEFINE_ACTION_FUNCTION(_Sector, FindNextHighestFloor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindNextHighestFloor(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
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
double sector_t::FindNextLowestFloor (vertex_t **v) const
{
	double height;
	double heightdiff;
	double ofloor, floor;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::floor);

	spot = Lines[0]->v1;
	height = floorplane.ZatPoint (spot);
	heightdiff = FLT_MAX;

	for (auto check : Lines)
	{
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
			floor = floorplane.ZatPoint(check->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindNextLowestFloor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindNextLowestFloor(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
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
double sector_t::FindNextLowestCeiling (vertex_t **v) const
{
	double height;
	double heightdiff;
	double oceil, ceil;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::floor);

	spot = Lines[0]->v1;
	height = ceilingplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : Lines)
	{
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			ceil = ceilingplane.ZatPoint(check->v1);
			if (oceil < ceil && ceil - oceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = ceil - oceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
			ceil = ceilingplane.ZatPoint(check->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindNextLowestCeiling)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindNextLowestCeiling(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
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
double sector_t::FindNextHighestCeiling (vertex_t **v) const
{
	double height;
	double heightdiff;
	double oceil, ceil;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = Lines[0]->v1;
	height = ceilingplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : Lines)
	{
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			ceil = ceilingplane.ZatPoint(check->v1);
			if (oceil > ceil && oceil - ceil < heightdiff && !IsLinked(other, true))
			{
				heightdiff = oceil - ceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
			ceil = ceilingplane.ZatPoint(check->v2);
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


DEFINE_ACTION_FUNCTION(_Sector, FindNextHighestCeiling)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindNextHighestCeiling(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
double sector_t::FindLowestCeilingSurrounding (vertex_t **v) const
{
	double height;
	double oceil;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = Lines[0]->v1;
	height = FLT_MAX;

	for (auto check : Lines)
	{
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			if (oceil < height)
			{
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindLowestCeilingSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindLowestCeilingSurrounding(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
double sector_t::FindHighestCeilingSurrounding (vertex_t **v) const
{
	double height;
	double oceil;
	sector_t *other;
	vertex_t *spot;

	if (Lines.Size() == 0) return GetPlaneTexZ(sector_t::ceiling);

	spot = Lines[0]->v1;
	height = -FLT_MAX;

	for (auto check : Lines)
	{
		if (NULL != (other = getNextSector (check, this)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			if (oceil > height)
			{
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindHighestCeilingSurrounding)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindHighestCeilingSurrounding(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}


//
// P_FindShortestTextureAround()
//
// Passed a sector number, returns the shortest lower texture on a
// linedef bounding the sector.
//
// jff 02/03/98 Add routine to find shortest lower texture
//

static inline void CheckShortestTex (FTextureID texnum, double &minsize)
{
	if (texnum.isValid() || (texnum.isNull() && (i_compatflags & COMPATF_SHORTTEX)))
	{
		FTexture *tex = TexMan[texnum];
		if (tex != NULL)
		{
			double h = tex->GetScaledHeight();
			if (h < minsize)
			{
				minsize = h;
			}
		}
	}
}

double sector_t::FindShortestTextureAround () const
{
	double minsize = FLT_MAX;

	for (auto check : Lines)
	{
		if (check->flags & ML_TWOSIDED)
		{
			CheckShortestTex (check->sidedef[0]->GetTexture(side_t::bottom), minsize);
			CheckShortestTex (check->sidedef[1]->GetTexture(side_t::bottom), minsize);
		}
	}
	return minsize < FLT_MAX ? minsize : TexMan[0]->GetHeight();
}

DEFINE_ACTION_FUNCTION(_Sector, FindShortestTextureAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	double h = self->FindShortestTextureAround();
	ACTION_RETURN_FLOAT(h);
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
double sector_t::FindShortestUpperAround () const
{
	double minsize = FLT_MAX;

	for (auto check : Lines)
	{
		if (check->flags & ML_TWOSIDED)
		{
			CheckShortestTex (check->sidedef[0]->GetTexture(side_t::top), minsize);
			CheckShortestTex (check->sidedef[1]->GetTexture(side_t::top), minsize);
		}
	}
	return minsize < FLT_MAX ? minsize : TexMan[0]->GetHeight();
}

DEFINE_ACTION_FUNCTION(_Sector, FindShortestUpperAround)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	double h = self->FindShortestUpperAround();
	ACTION_RETURN_FLOAT(h);
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
sector_t *sector_t::FindModelFloorSector (double floordestheight) const
{
	sector_t *sec;

	for (auto check : Lines)
	{
		sec = getNextSector (check, this);
		if (sec != NULL &&
			(sec->floorplane.ZatPoint(check->v1) == floordestheight ||
			 sec->floorplane.ZatPoint(check->v2) == floordestheight))
		{
			return sec;
		}
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, FindModelFloorSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = self->FindModelFloorSector(fdh);
	ACTION_RETURN_POINTER(h);
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
sector_t *sector_t::FindModelCeilingSector (double floordestheight) const
{
	sector_t *sec;

	for (auto check : Lines)
	{
		sec = getNextSector (check, this);
		if (sec != NULL &&
			(sec->ceilingplane.ZatPoint(check->v1) == floordestheight ||
			 sec->ceilingplane.ZatPoint(check->v2) == floordestheight))
		{
			return sec;
		}
	}
	return NULL;
}

DEFINE_ACTION_FUNCTION(_Sector, FindModelCeilingSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(fdh);
	auto h = self->FindModelCeilingSector(fdh);
	ACTION_RETURN_POINTER(h);
}

//
// Find minimum light from an adjacent sector
//
int sector_t::FindMinSurroundingLight (int min) const
{
	sector_t*	check;
		
	for (auto line : Lines)
	{
		if (NULL != (check = getNextSector (line, this)) &&
			check->lightlevel < min)
		{
			min = check->lightlevel;
		}
	}
	return min;
}

DEFINE_ACTION_FUNCTION(_Sector, FindMinSurroundingLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(min);
	auto h = self->FindMinSurroundingLight(min);
	ACTION_RETURN_INT(h);
}

//
// Find the highest point on the floor of the sector
//
double sector_t::FindHighestFloorPoint (vertex_t **v) const
{
	double height = -FLT_MAX;
	double probeheight;
	vertex_t *spot = NULL;

	if (!floorplane.isSlope())
	{
		if (v != NULL)
		{
			if (Lines.Size() == 0) *v = &level.vertexes[0];
			else *v = Lines[0]->v1;
		}
		return -floorplane.fD();
	}

	for (auto line : Lines)
	{
		probeheight = floorplane.ZatPoint(line->v1);
		if (probeheight > height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = floorplane.ZatPoint(line->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindHighestFloorPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindHighestFloorPoint(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

//
// Find the lowest point on the ceiling of the sector
//
double sector_t::FindLowestCeilingPoint (vertex_t **v) const
{
	double height = FLT_MAX;
	double probeheight;
	vertex_t *spot = NULL;

	if (!ceilingplane.isSlope())
	{
		if (v != NULL)
		{
			if (Lines.Size() == 0) *v = &level.vertexes[0];
			else *v = Lines[0]->v1;
		}
		return ceilingplane.fD();
	}

	for (auto line : Lines)
	{
		probeheight = ceilingplane.ZatPoint(line->v1);
		if (probeheight < height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = ceilingplane.ZatPoint(line->v2);
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

DEFINE_ACTION_FUNCTION(_Sector, FindLowestCeilingPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	vertex_t *v;
	double h = self->FindLowestCeilingPoint(&v);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(v);
	return numret;
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetColor(int r, int g, int b, int desat)
{
	Colormap.LightColor = PalEntry(r, g, b);
	Colormap.Desaturation = desat;
	P_RecalculateAttachedLights(this);
}

DEFINE_ACTION_FUNCTION(_Sector, SetColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(color);
	PARAM_INT(desat);
	self->Colormap.LightColor.SetRGB(color);
	self->Colormap.Desaturation = desat;
	P_RecalculateAttachedLights(self);
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetFade(int r, int g, int b)
{
	Colormap.FadeColor = PalEntry (r,g,b);
	P_RecalculateAttachedLights(this);
}

DEFINE_ACTION_FUNCTION(_Sector, SetFade)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_COLOR(fade);
	self->Colormap.FadeColor.SetRGB(fade);
	P_RecalculateAttachedLights(self);
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetSpecialColor(int slot, int r, int g, int b)
{
	SpecialColors[slot] = PalEntry(255, r, g, b);
}

void sector_t::SetSpecialColor(int slot, PalEntry rgb)
{
	rgb.a = 255;
	SpecialColors[slot] = rgb;
}

DEFINE_ACTION_FUNCTION(_Sector, SetSpecialColor)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(num);
	PARAM_COLOR(color);
	if (num >= 0 && num < 5)
	{
		color.a = 255;
		self->SetSpecialColor(num, color);
	}
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetFogDensity(int dens)
{
	Colormap.FogDensity = dens;
}

DEFINE_ACTION_FUNCTION(_Sector, SetFogDensity)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(dens);
	self->Colormap.FogDensity = dens;
	return 0;
}

//===========================================================================
//
// sector_t :: ClosestPoint
//
// Given a point (x,y), returns the point (ox,oy) on the sector's defining
// lines that is nearest to (x,y).
//
//===========================================================================

void sector_t::ClosestPoint(const DVector2 &in, DVector2 &out) const
{
	double x = in.X, y = in.Y;
	double bestdist = HUGE_VAL;
	double bestx = 0, besty = 0;

	for (auto check : Lines)
	{
		vertex_t *v1 = check->v1;
		vertex_t *v2 = check->v2;
		double a = v2->fX() - v1->fX();
		double b = v2->fY() - v1->fY();
		double den = a*a + b*b;
		double ix, iy, dist;

		if (den == 0)
		{ // Line is actually a point!
			ix = v1->fX();
			iy = v1->fY();
		}
		else
		{
			double num = (x - v1->fX()) * a + (y - v1->fY()) * b;
			double u = num / den;
			if (u <= 0)
			{
				ix = v1->fX();
				iy = v1->fY();
			}
			else if (u >= 1)
			{
				ix = v2->fX();
				iy = v2->fY();
			}
			else
			{
				ix = v1->fX() + u * a;
				iy = v1->fY() + u * b;
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
	out = { bestx, besty };
}


//=====================================================================================
//
//
//=====================================================================================

bool sector_t::PlaneMoving(int pos)
{
	if (pos == floor)
		return (floordata != NULL || (planes[floor].Flags & PLANEF_BLOCKED));
	else
		return (ceilingdata != NULL || (planes[ceiling].Flags & PLANEF_BLOCKED));
}

DEFINE_ACTION_FUNCTION(_Sector, PlaneMoving)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_BOOL(self->PlaneMoving(pos));
}

//=====================================================================================
//
//
//=====================================================================================

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

DEFINE_ACTION_FUNCTION(_Sector, GetFloorLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetFloorLight());
}

//=====================================================================================
//
//
//=====================================================================================

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

DEFINE_ACTION_FUNCTION(_Sector, GetCeilingLight)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_INT(self->GetCeilingLight());
}

//=====================================================================================
//
//
//=====================================================================================

FSectorPortal *sector_t::ValidatePortal(int which)
{
	FSectorPortal *port = GetPortal(which);
	if (port->mType == PORTS_SKYVIEWPOINT && port->mSkybox == nullptr) return nullptr;				// A skybox without a viewpoint is just a regular sky.
	if (PortalBlocksView(which)) return nullptr;													// disabled or obstructed linked portal.
	if ((port->mFlags & PORTSF_SKYFLATONLY) && GetTexture(which) != skyflatnum) return nullptr;		// Skybox without skyflat texture
	return port;
}

//=====================================================================================
//
//
//=====================================================================================

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

DEFINE_ACTION_FUNCTION(_Sector, GetHeightSec)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	ACTION_RETURN_POINTER(self->GetHeightSec());
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::GetSpecial(secspecial_t *spec)
{
	spec->special = special;
	spec->damageamount = damageamount;
	spec->damagetype = damagetype;
	spec->damageinterval = damageinterval;
	spec->leakydamage = leakydamage;
	spec->Flags = Flags & SECF_SPECIALFLAGS;
}

DEFINE_ACTION_FUNCTION(_Sector, GetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	self->GetSpecial(spec);
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetSpecial(const secspecial_t *spec)
{
	special = spec->special;
	damageamount = spec->damageamount;
	damagetype = spec->damagetype;
	damageinterval = spec->damageinterval;
	leakydamage = spec->leakydamage;
	Flags = (Flags & ~SECF_SPECIALFLAGS) | (spec->Flags & SECF_SPECIALFLAGS);
}

DEFINE_ACTION_FUNCTION(_Sector, SetSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, secspecial_t);
	self->SetSpecial(spec);
	return 0;
}


//=====================================================================================
//
//
//=====================================================================================

void sector_t::TransferSpecial(sector_t *model)
{
	special = model->special;
	damageamount = model->damageamount;
	damagetype = model->damagetype;
	damageinterval = model->damageinterval;
	leakydamage = model->leakydamage;
	Flags = (Flags&~SECF_SPECIALFLAGS) | (model->Flags & SECF_SPECIALFLAGS);
}

DEFINE_ACTION_FUNCTION(_Sector, TransferSpecial)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_POINTER(spec, sector_t);
	self->TransferSpecial(spec);
	return 0;
}

//=====================================================================================
//
//
//=====================================================================================

int sector_t::GetTerrain(int pos) const
{
	return terrainnum[pos] >= 0 ? terrainnum[pos] : TerrainTypes[GetTexture(pos)];
}

DEFINE_ACTION_FUNCTION(_Sector, GetTerrain)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(pos);
	ACTION_RETURN_INT(self->GetTerrain(pos));
}

//=====================================================================================
//
//
//=====================================================================================

void sector_t::CheckPortalPlane(int plane)
{
	if (GetPortalType(plane) == PORTS_LINKEDPORTAL)
	{
		double portalh = GetPortalPlaneZ(plane);
		double planeh = GetPlaneTexZ(plane);
		int obstructed = PLANEF_OBSTRUCTED * (plane == sector_t::floor ? planeh > portalh : planeh < portalh);
		planes[plane].Flags = (planes[plane].Flags  & ~PLANEF_OBSTRUCTED) | obstructed;
	}
}

DEFINE_ACTION_FUNCTION(_Sector, CheckPortalPlane)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(plane);
	self->CheckPortalPlane(plane);
	return 0;
}

//===========================================================================
//
// Finds the highest ceiling at the given position, all portals considered
//
//===========================================================================

double sector_t::HighestCeilingAt(const DVector2 &p, sector_t **resultsec)
{
	sector_t *check = this;
	double planeheight = -FLT_MAX;
	DVector2 pos = p;

	// Continue until we find a blocking portal or a portal below where we actually are.
	while (!check->PortalBlocksMovement(ceiling) && planeheight < check->GetPortalPlaneZ(ceiling))
	{
		pos += check->GetPortalDisplacement(ceiling);
		planeheight = check->GetPortalPlaneZ(ceiling);
		check = P_PointInSector(pos);
	}
	if (resultsec) *resultsec = check;
	return check->ceilingplane.ZatPoint(pos);
}

DEFINE_ACTION_FUNCTION(_Sector, HighestCeilingAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	sector_t *s;
	double h = self->HighestCeilingAt(DVector2(x, y), &s);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(s);
	return numret;
}

//===========================================================================
//
// Finds the lowest floor at the given position, all portals considered
//
//===========================================================================

double sector_t::LowestFloorAt(const DVector2 &p, sector_t **resultsec)
{
	sector_t *check = this;
	double planeheight = FLT_MAX;
	DVector2 pos = p;

	// Continue until we find a blocking portal or a portal above where we actually are.
	while (!check->PortalBlocksMovement(floor) && planeheight > check->GetPortalPlaneZ(floor))
	{
		pos += check->GetPortalDisplacement(floor);
		planeheight = check->GetPortalPlaneZ(ceiling);
		check = P_PointInSector(pos);
	}
	if (resultsec) *resultsec = check;
	return check->floorplane.ZatPoint(pos);
}

DEFINE_ACTION_FUNCTION(_Sector, LowestFloorAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	sector_t *s;
	double h = self->LowestFloorAt(DVector2(x, y), &s);
	if (numret > 0) ret[0].SetFloat(h);
	if (numret > 1) ret[1].SetPointer(s);
	return numret;
}

//=====================================================================================
//
//
//=====================================================================================

double sector_t::NextHighestCeilingAt(double x, double y, double bottomz, double topz, int flags, sector_t **resultsec, F3DFloor **resultffloor)
{
	sector_t *sec = this;
	double planeheight = -FLT_MAX;

	while (true)
	{
		// Looking through planes from bottom to top
		double realceil = sec->ceilingplane.ZatPoint(x, y);
		for (int i = sec->e->XFloor.ffloors.Size() - 1; i >= 0; --i)
		{
			F3DFloor *rover = sec->e->XFloor.ffloors[i];
			if (!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			double ff_bottom = rover->bottom.plane->ZatPoint(x, y);
			double ff_top = rover->top.plane->ZatPoint(x, y);

			double delta1 = bottomz - (ff_bottom + ((ff_top - ff_bottom) / 2));
			double delta2 = topz - (ff_bottom + ((ff_top - ff_bottom) / 2));

			if (ff_bottom < realceil && fabs(delta1) > fabs(delta2))
			{ 
				if (resultsec) *resultsec = sec;
				if (resultffloor) *resultffloor = rover;
				return ff_bottom;
			}
		}
		if ((flags & FFCF_NOPORTALS) || sec->PortalBlocksMovement(ceiling) || planeheight >= sec->GetPortalPlaneZ(ceiling))
		{ // Use sector's ceiling
			if (resultffloor) *resultffloor = NULL;
			if (resultsec) *resultsec = sec;
			return realceil;
		}
		else
		{
			DVector2 pos = sec->GetPortalDisplacement(ceiling);
			x += pos.X;
			y += pos.Y;
			planeheight = sec->GetPortalPlaneZ(ceiling);
			sec = P_PointInSector(x, y);
		}
	}
}

DEFINE_ACTION_FUNCTION(_Sector, NextHighestCeilingAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(bottomz);
	PARAM_FLOAT(topz);
	PARAM_INT_DEF(flags);
	sector_t *resultsec;
	F3DFloor *resultff;
	double resultheight = self->NextHighestCeilingAt(x, y, bottomz, topz, flags, &resultsec, &resultff);

	if (numret > 2)
	{
		ret[2].SetPointer(resultff);
		numret = 3;
	}
	if (numret > 1)
	{
		ret[1].SetPointer(resultsec);
	}
	if (numret > 0)
	{
		ret[0].SetFloat(resultheight);
	}
	return numret;
}

//=====================================================================================
//
//
//=====================================================================================

double sector_t::NextLowestFloorAt(double x, double y, double z, int flags, double steph, sector_t **resultsec, F3DFloor **resultffloor)
{
	sector_t *sec = this;
	double planeheight = FLT_MAX;
	while (true)
	{
		// Looking through planes from top to bottom
		unsigned numff = sec->e->XFloor.ffloors.Size();
		double realfloor = sec->floorplane.ZatPoint(x, y);
		for (unsigned i = 0; i < numff; ++i)
		{
			F3DFloor *ff = sec->e->XFloor.ffloors[i];


			// either with feet above the 3D floor or feet with less than 'stepheight' map units inside
			if ((ff->flags & (FF_EXISTS | FF_SOLID)) == (FF_EXISTS | FF_SOLID))
			{
				double ffz = ff->top.plane->ZatPoint(x, y);
				double ffb = ff->bottom.plane->ZatPoint(x, y);

				if (ffz > realfloor && (z >= ffz || (!(flags & FFCF_3DRESTRICT) && (ffb < z && ffz < z + steph))))
				{ // This floor is beneath our feet.
					if (resultsec) *resultsec = sec;
					if (resultffloor) *resultffloor = ff;
					return ffz;
				}
			}
		}
		if ((flags & FFCF_NOPORTALS) || sec->PortalBlocksMovement(sector_t::floor) || planeheight <= sec->GetPortalPlaneZ(floor))
		{ // Use sector's floor
			if (resultffloor) *resultffloor = NULL;
			if (resultsec) *resultsec = sec;
			return realfloor;
		}
		else
		{
			DVector2 pos = sec->GetPortalDisplacement(floor);
			x += pos.X;
			y += pos.Y;
			planeheight = sec->GetPortalPlaneZ(floor);
			sec = P_PointInSector(x, y);
		}
	}
}

DEFINE_ACTION_FUNCTION(_Sector, NextLowestFloorAt)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	PARAM_INT_DEF(flags);
	PARAM_FLOAT_DEF(steph);
	sector_t *resultsec;
	F3DFloor *resultff;
	double resultheight = self->NextLowestFloorAt(x, y, z, flags, steph, &resultsec, &resultff);

	if (numret > 2)
	{
		ret[2].SetPointer(resultff);
		numret = 3;
	}
	if (numret > 1)
	{
		ret[1].SetPointer(resultsec);
	}
	if (numret > 0)
	{
		ret[0].SetFloat(resultheight);
	}
	return numret;
}


//===========================================================================
//
// 
//
//===========================================================================

 double sector_t::GetFriction(int plane, double *pMoveFac) const
{
	if (Flags & SECF_FRICTION) 
	{ 
		if (pMoveFac) *pMoveFac = movefactor;
		return friction; 
	}
	FTerrainDef *terrain = &Terrains[GetTerrain(plane)];
	if (terrain->Friction != 0)
	{
		if (pMoveFac) *pMoveFac = terrain->MoveFactor;
		return terrain->Friction;
	}
	else
	{
		if (pMoveFac) *pMoveFac = ORIG_FRICTION_FACTOR;
		return ORIG_FRICTION;
	}
}

 DEFINE_ACTION_FUNCTION(_Sector, GetFriction)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(plane);
	 double mf;
	 double h = self->GetFriction(plane, &mf);
	 if (numret > 0) ret[0].SetFloat(h);
	 if (numret > 1) ret[1].SetFloat(mf);
	 return numret;
 }

 //===========================================================================
 //
 // 
 //
 //===========================================================================

 void sector_t::RemoveForceField()
 {
	 for (auto line : Lines)
	 {
		 if (line->backsector != NULL && line->special == ForceField)
		 {
			 line->flags &= ~(ML_BLOCKING | ML_BLOCKEVERYTHING);
			 line->special = 0;
			 line->sidedef[0]->SetTexture(side_t::mid, FNullTextureID());
			 line->sidedef[1]->SetTexture(side_t::mid, FNullTextureID());
		 }
	 }
 }

 DEFINE_ACTION_FUNCTION(_Sector, RemoveForceField)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 self->RemoveForceField();
	 return 0;
 }

 //===========================================================================
 //
 // phares 3/12/98: End of friction effects
 //
 //===========================================================================

 void sector_t::AdjustFloorClip() const
 {
	 msecnode_t *node;

	 for (node = touching_thinglist; node; node = node->m_snext)
	 {
		 if (node->m_thing->flags2 & MF2_FLOORCLIP)
		 {
			 node->m_thing->AdjustFloorClip();
		 }
	 }
 }

 DEFINE_ACTION_FUNCTION(_Sector, AdjustFloorClip)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 self->AdjustFloorClip();
	 return 0;
 }


 //===========================================================================
 //
 //
 //
 //===========================================================================

 bool sector_t::TriggerSectorActions(AActor *thing, int activation)
 {
	 AActor *act = SecActTarget;
	 bool res = false;

	 while (act != nullptr)
	 {
		 AActor *next = act->tracer;

		 IFVIRTUALPTRNAME(act, "SectorAction", TriggerAction)
		 {
			 VMValue params[3] = { (DObject *)act, thing, activation };
			 VMReturn ret;
			 int didit;
			 ret.IntAt(&didit);
			 VMCall(func, params, 3, &ret, 1);

			 if (didit)
			 {
				 if (act->flags4 & MF4_STANDSTILL)
				 {
					 act->Destroy();
				 }
			 }
			 act = next;
			 res |= !!didit;
		 }
	 }
	 return res;
 }

 DEFINE_ACTION_FUNCTION(_Sector, TriggerSectorActions)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_OBJECT(thing, AActor);
	 PARAM_INT(activation);
	 ACTION_RETURN_BOOL(self->TriggerSectorActions(thing, activation));
 }

 //===========================================================================
 //
 //
 //
 //===========================================================================

 DEFINE_ACTION_FUNCTION(_Sector, PointInSector)
 {
	 PARAM_PROLOGUE;
	 PARAM_FLOAT(x);
	 PARAM_FLOAT(y);
	 ACTION_RETURN_POINTER(P_PointInSector(x, y));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, AddXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddXOffset(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXOffset(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXOffset(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, AddYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->AddXOffset(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL_DEF(addbase);
	 ACTION_RETURN_FLOAT(self->GetYOffset(pos, addbase));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetXScale(pos, o);
	 return 0;
 }

 
 DEFINE_ACTION_FUNCTION(_Sector, GetXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetXScale(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetYScale(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetYScale(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_ANGLE(o);
	 self->SetAngle(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetAngle)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_BOOL_DEF(addbase);
	 ACTION_RETURN_FLOAT(self->GetAngle(pos, addbase).Degrees);
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetBase)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_ANGLE(a);
	 self->SetBase(pos, o, a);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetAlpha(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetAlpha)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetAlpha(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetFlags(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetVisFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetVisFlags(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, ChangeFlags)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(a);
	 PARAM_INT(o);
	 self->ChangeFlags(pos, a, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 self->SetPlaneLight(pos, o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetPlaneLight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPlaneLight(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_INT(o);
	 PARAM_BOOL_DEF(adj);
	 self->SetTexture(pos, FSetTextureID(o), adj);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetTexture(pos).GetIndex());
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 PARAM_BOOL_DEF(dirty);
	 self->SetPlaneTexZ(pos, o, dirty);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetPlaneTexZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPlaneTexZ(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->SetLightLevel(o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, ChangeLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(o);
	 self->ChangeLightLevel(o);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetLightLevel)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_INT(self->GetLightLevel());
 }

 DEFINE_ACTION_FUNCTION(_Sector, ClearSpecial)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 self->ClearSpecial();
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, PortalBlocksView)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksView(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, PortalBlocksSight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSight(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, PortalBlocksMovement)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksMovement(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, PortalBlocksSound)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalBlocksSound(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, PortalIsLinked)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_BOOL(self->PortalIsLinked(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, ClearPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 self->ClearPortal(pos);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetPortalPlaneZ)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetPortalPlaneZ(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetPortalDisplacement)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_VEC2(self->GetPortalDisplacement(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetPortalType)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetPortalType(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetOppositePortalGroup)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetOppositePortalGroup(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, CenterFloor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterFloor());
 }

 DEFINE_ACTION_FUNCTION(_Sector, CenterCeiling)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 ACTION_RETURN_FLOAT(self->CenterCeiling());
 }

 DEFINE_ACTION_FUNCTION(_Sector, Index)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	unsigned ndx = self->Index();
	if (ndx >= level.sectors.Size())
	{
		// This qualifies as an array out of bounds exception. Normally it can only happen when a sector copy is concerned which scripts should not be able to create.
		ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Accessed invalid sector");
	}
	ACTION_RETURN_INT(ndx);
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetEnvironmentID)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(envnum);
	level.Zones[self->ZoneNumber].Environment = S_FindEnvironment(envnum);
	return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetEnvironment)
 {
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_STRING(env);
	level.Zones[self->ZoneNumber].Environment = S_FindEnvironment(env);
	return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_FLOAT(self->GetGlowHeight(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, GetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 ACTION_RETURN_INT(self->GetGlowColor(pos));
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetGlowHeight)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_FLOAT(o);
	 self->SetGlowHeight(pos, float(o));
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Sector, SetGlowColor)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	 PARAM_INT(pos);
	 PARAM_COLOR(o);
	 self->SetGlowColor(pos, o);
	 return 0;
 }

 //===========================================================================
 //
 //  line_t exports
 //
 //===========================================================================

 DEFINE_ACTION_FUNCTION(_Line, isLinePortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isLinePortal());
 }

 DEFINE_ACTION_FUNCTION(_Line, isVisualPortal)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_BOOL(self->isVisualPortal());
 }

 DEFINE_ACTION_FUNCTION(_Line, getPortalDestination)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_POINTER(self->getPortalDestination());
 }

 DEFINE_ACTION_FUNCTION(_Line, getPortalAlignment)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 ACTION_RETURN_INT(self->getPortalAlignment());
 }

 DEFINE_ACTION_FUNCTION(_Line, Index)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(line_t);
	 unsigned ndx = self->Index();
	 if (ndx >= level.lines.Size())
	 {
		 ThrowAbortException(X_ARRAY_OUT_OF_BOUNDS, "Accessed invalid line");
	 }
	 ACTION_RETURN_INT(ndx);
 }

 //===========================================================================
 //
 // 
 //
 //===========================================================================

 DEFINE_ACTION_FUNCTION(_Side, GetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_INT(self->GetTexture(which).GetIndex());
 }

 DEFINE_ACTION_FUNCTION(_Side, SetTexture)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_INT(tex);
	 self->SetTexture(which, FSetTextureID(tex));
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, SetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXOffset(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, AddTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureXOffset(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, GetTextureXOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXOffset(which));
 }

 DEFINE_ACTION_FUNCTION(_Side, SetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYOffset(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, AddTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->AddTextureYOffset(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, GetTextureYOffset)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYOffset(which));
 }

 DEFINE_ACTION_FUNCTION(_Side, SetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureXScale(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, MultiplyTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureXScale(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, GetTextureXScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureXScale(which));
 }

 DEFINE_ACTION_FUNCTION(_Side, SetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->SetTextureYScale(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, MultiplyTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 PARAM_FLOAT(ofs);
	 self->MultiplyTextureYScale(which, ofs);
	 return 0;
 }

 DEFINE_ACTION_FUNCTION(_Side, GetTextureYScale)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 PARAM_INT(which);
	 ACTION_RETURN_FLOAT(self->GetTextureYScale(which));
 }

 DEFINE_ACTION_FUNCTION(_Side, V1)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V1());
 }

 DEFINE_ACTION_FUNCTION(_Side, V2)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_POINTER(self->V2());
 }

 DEFINE_ACTION_FUNCTION(_Side, Index)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(side_t);
	 ACTION_RETURN_INT(self->Index());
 }

 DEFINE_ACTION_FUNCTION(_Vertex, Index)
 {
	 PARAM_SELF_STRUCT_PROLOGUE(vertex_t);
	 ACTION_RETURN_INT(self->Index());
 }


//===========================================================================
//
// 
//
//===========================================================================

 FSerializer &Serialize(FSerializer &arc, const char *key, secspecial_t &spec, secspecial_t *def)
 {
	 if (arc.BeginObject(key))
	 {
		 arc("special", spec.special)
			 ("damageamount", spec.damageamount)
			 ("damagetype", spec.damagetype)
			 ("damageinterval", spec.damageinterval)
			 ("leakydamage", spec.leakydamage)
			 ("flags", spec.Flags)
			 .EndObject();
	 }
	return arc;
}


//===========================================================================
//
// 
//
//===========================================================================

bool secplane_t::CopyPlaneIfValid (secplane_t *dest, const secplane_t *opp) const
{
	bool copy = false;

	// If the planes do not have matching slopes, then always copy them
	// because clipping would require creating new sectors.
	if (Normal() != dest->Normal())
	{
		copy = true;
	}
	else if (opp->Normal() != -dest->Normal())
	{
		if (fD() < dest->fD())
		{
			copy = true;
		}
	}
	else if (fD() < dest->fD() && fD() > -opp->fD())
	{
		copy = true;
	}

	if (copy)
	{
		*dest = *this;
	}

	return copy;
}

//==========================================================================
//
// P_AlignFlat
//
//==========================================================================

bool P_AlignFlat (int linenum, int side, int fc)
{
	line_t *line = &level.lines[linenum];
	sector_t *sec = side ? line->backsector : line->frontsector;

	if (!sec)
		return false;

	DAngle angle = line->Delta().Angle();
	DAngle norm = angle - 90;
	double dist = -(norm.Cos() * line->v1->fX() + norm.Sin() * line->v1->fY());

	if (side)
	{
		angle += 180.;
		dist = -dist;
	}

	sec->SetBase(fc, dist, -angle);
	return true;
}



//==========================================================================
//
// P_ReplaceTextures
//
//==========================================================================

void P_ReplaceTextures(const char *fromname, const char *toname, int flags)
{
	FTextureID picnum1, picnum2;

	if (fromname == nullptr)
		return;

	if ((flags ^ (NOT_BOTTOM | NOT_MIDDLE | NOT_TOP)) != 0)
	{
		picnum1 = TexMan.GetTexture(fromname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture(toname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);

		for (auto &side : level.sides)
		{
			for (int j = 0; j<3; j++)
			{
				static uint8_t bits[] = { NOT_TOP, NOT_MIDDLE, NOT_BOTTOM };
				if (!(flags & bits[j]) && side.GetTexture(j) == picnum1)
				{
					side.SetTexture(j, picnum2);
				}
			}
		}
	}
	if ((flags ^ (NOT_FLOOR | NOT_CEILING)) != 0)
	{
		picnum1 = TexMan.GetTexture(fromname, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTexture(toname, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);

		for (auto &sec : level.sectors)
		{
			if (!(flags & NOT_FLOOR) && sec.GetTexture(sector_t::floor) == picnum1)
				sec.SetTexture(sector_t::floor, picnum2);
			if (!(flags & NOT_CEILING) && sec.GetTexture(sector_t::ceiling) == picnum1)
				sec.SetTexture(sector_t::ceiling, picnum2);
		}
	}
}

DEFINE_ACTION_FUNCTION(_TexMan, ReplaceTextures)
{
	PARAM_PROLOGUE;
	PARAM_STRING(from);
	PARAM_STRING(to);
	PARAM_INT(flags);
	P_ReplaceTextures(from, to, flags);
	return 0;
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
	PolyNodeLevel.Sides = &level.sides[0];
	PolyNodeLevel.NumSides = level.sides.Size();
	PolyNodeLevel.Lines = &level.lines[0];
	PolyNodeLevel.NumLines = numlines; // is this correct???

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
			DVector2 delta = linedef->Delta();
			int rel;
			if (((level.flags2 & LEVEL2_SMOOTHLIGHTING) || (Flags & WALLF_SMOOTHLIGHTING) || r_fakecontrast == 2) &&
				delta.X != 0)
			{
				rel = xs_RoundToInt // OMG LEE KILLOUGH LIVES! :/
					(
						level.WallHorizLight
						+ fabs(atan(delta.Y / delta.X) / 1.57079)
						* (level.WallVertLight - level.WallHorizLight)
					);
			}
			else
			{
				rel = delta.X == 0 ? level.WallVertLight : 
					  delta.Y == 0 ? level.WallHorizLight : 0;
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

DEFINE_ACTION_FUNCTION(_Secplane, isSlope)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	ACTION_RETURN_BOOL(!self->normal.XY().isZero());
}

DEFINE_ACTION_FUNCTION(_Secplane, PointOnSide)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_INT(self->PointOnSide(DVector3(x, y, z)));
}

DEFINE_ACTION_FUNCTION(_Secplane, ZatPoint)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	ACTION_RETURN_FLOAT(self->ZatPoint(x, y));
}

DEFINE_ACTION_FUNCTION(_Secplane, ZatPointDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(d);
	ACTION_RETURN_FLOAT((d + self->normal.X*x + self->normal.Y*y) * self->negiC);
}

DEFINE_ACTION_FUNCTION(_Secplane, isEqual)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_POINTER(other, secplane_t);
	ACTION_RETURN_BOOL(*self == *other);
}

DEFINE_ACTION_FUNCTION(_Secplane, ChangeHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	self->ChangeHeight(hdiff);
	return 0;
}

DEFINE_ACTION_FUNCTION(_Secplane, GetChangedHeight)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(hdiff);
	ACTION_RETURN_FLOAT(self->GetChangedHeight(hdiff));
}

DEFINE_ACTION_FUNCTION(_Secplane, HeightDiff)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(oldd);
	if (numparam == 2)
	{
		ACTION_RETURN_FLOAT(self->HeightDiff(oldd));
	}
	else
	{
		PARAM_FLOAT(newd);
		ACTION_RETURN_FLOAT(self->HeightDiff(oldd, newd));
	}
}

DEFINE_ACTION_FUNCTION(_Secplane, PointToDist)
{
	PARAM_SELF_STRUCT_PROLOGUE(secplane_t);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	ACTION_RETURN_FLOAT(self->PointToDist(DVector2(x, y), z));
}


DEFINE_FIELD_X(Sector, sector_t, floorplane)
DEFINE_FIELD_X(Sector, sector_t, ceilingplane)
DEFINE_FIELD_X(Sector, sector_t, Colormap)
DEFINE_FIELD_X(Sector, sector_t, SpecialColors)
DEFINE_FIELD_X(Sector, sector_t, SoundTarget)
DEFINE_FIELD_X(Sector, sector_t, special)
DEFINE_FIELD_X(Sector, sector_t, lightlevel)
DEFINE_FIELD_X(Sector, sector_t, seqType)
DEFINE_FIELD_X(Sector, sector_t, sky)
DEFINE_FIELD_X(Sector, sector_t, SeqName)
DEFINE_FIELD_X(Sector, sector_t, centerspot)
DEFINE_FIELD_X(Sector, sector_t, validcount)
DEFINE_FIELD_X(Sector, sector_t, thinglist)
DEFINE_FIELD_X(Sector, sector_t, friction)
DEFINE_FIELD_X(Sector, sector_t, movefactor)
DEFINE_FIELD_X(Sector, sector_t, terrainnum)
DEFINE_FIELD_X(Sector, sector_t, floordata)
DEFINE_FIELD_X(Sector, sector_t, ceilingdata)
DEFINE_FIELD_X(Sector, sector_t, lightingdata)
DEFINE_FIELD_X(Sector, sector_t, interpolations)
DEFINE_FIELD_X(Sector, sector_t, soundtraversed)
DEFINE_FIELD_X(Sector, sector_t, stairlock)
DEFINE_FIELD_X(Sector, sector_t, prevsec)
DEFINE_FIELD_X(Sector, sector_t, nextsec)
DEFINE_FIELD_UNSIZED(Sector, sector_t, Lines)
DEFINE_FIELD_X(Sector, sector_t, heightsec)
DEFINE_FIELD_X(Sector, sector_t, bottommap)
DEFINE_FIELD_X(Sector, sector_t, midmap)
DEFINE_FIELD_X(Sector, sector_t, topmap)
DEFINE_FIELD_X(Sector, sector_t, touching_thinglist)
DEFINE_FIELD_X(Sector, sector_t, sectorportal_thinglist)
DEFINE_FIELD_X(Sector, sector_t, gravity)
DEFINE_FIELD_X(Sector, sector_t, damagetype)
DEFINE_FIELD_X(Sector, sector_t, damageamount)
DEFINE_FIELD_X(Sector, sector_t, damageinterval)
DEFINE_FIELD_X(Sector, sector_t, leakydamage)
DEFINE_FIELD_X(Sector, sector_t, ZoneNumber)
DEFINE_FIELD_X(Sector, sector_t, MoreFlags)
DEFINE_FIELD_X(Sector, sector_t, Flags)
DEFINE_FIELD_X(Sector, sector_t, SecActTarget)
DEFINE_FIELD_X(Sector, sector_t, Portals)
DEFINE_FIELD_X(Sector, sector_t, PortalGroup)
DEFINE_FIELD_X(Sector, sector_t, sectornum)

DEFINE_FIELD_X(Line, line_t, v1)
DEFINE_FIELD_X(Line, line_t, v2)
DEFINE_FIELD_X(Line, line_t, delta)
DEFINE_FIELD_X(Line, line_t, flags)
DEFINE_FIELD_X(Line, line_t, activation)
DEFINE_FIELD_X(Line, line_t, special)
DEFINE_FIELD_X(Line, line_t, args)
DEFINE_FIELD_X(Line, line_t, alpha)
DEFINE_FIELD_X(Line, line_t, sidedef)
DEFINE_FIELD_X(Line, line_t, bbox)
DEFINE_FIELD_X(Line, line_t, frontsector)
DEFINE_FIELD_X(Line, line_t, backsector)
DEFINE_FIELD_X(Line, line_t, validcount)
DEFINE_FIELD_X(Line, line_t, locknumber)
DEFINE_FIELD_X(Line, line_t, portalindex)
DEFINE_FIELD_X(Line, line_t, portaltransferred)

DEFINE_FIELD_X(Side, side_t, sector)
DEFINE_FIELD_X(Side, side_t, linedef)
DEFINE_FIELD_X(Side, side_t, Light)
DEFINE_FIELD_X(Side, side_t, Flags)

DEFINE_FIELD_X(Secplane, secplane_t, normal)
DEFINE_FIELD_X(Secplane, secplane_t, D)
DEFINE_FIELD_X(Secplane, secplane_t, negiC)

DEFINE_FIELD_X(Vertex, vertex_t, p)
