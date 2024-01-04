//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2018 Christoph Oelckers
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
#include "g_levellocals.h"
#include "vm.h"
#include "texturemanager.h"

//==========================================================================
//
//
//
//==========================================================================

CUSTOM_CVAR(Int, r_fakecontrast, 1, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0) self = 1;
	else if (self > 2) self = 2;
}

// [RH]
// P_NextSpecialSector()
//
// Returns the next special sector attached to this sector
// with a certain special.

sector_t* P_NextSpecialSectorVC(sector_t* sec, int type)
{
	sector_t* tsec;
	for (auto ln : sec->Lines)
	{
		if (nullptr != (tsec = getNextSector(ln, sec)) &&
			tsec->validcount != validcount &&
			tsec->special == type)
		{
			return tsec;
		}
	}
	return nullptr;
}


sector_t *P_NextSpecialSector (sector_t* sec, int type, sector_t *nogood)
{
	sector_t *tsec;
	for (auto ln : sec->Lines)
	{
		if (nullptr != (tsec = getNextSector (ln, sec)) &&
			tsec != nogood &&
			tsec->special == type)
		{
			return tsec;
		}
	}
	return nullptr;
}

DEFINE_ACTION_FUNCTION_NATIVE(_Sector, NextSpecialSector, P_NextSpecialSector)
{
	PARAM_SELF_STRUCT_PROLOGUE(sector_t);
	PARAM_INT(type);
	PARAM_POINTER(nogood, sector_t);
	ACTION_RETURN_POINTER(P_NextSpecialSector(self, type, nogood));
}

//
// P_FindLowestFloorSurrounding()
// FIND LOWEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
double FindLowestFloorSurrounding (const sector_t *sector, vertex_t **v)
{
	sector_t *other;
	double floor;
	double ofloor;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::floor);

	spot = sector->Lines[0]->v1;
	floor = sector->floorplane.ZatPoint(spot);

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			if (ofloor < floor && ofloor < sector->floorplane.ZatPoint (check->v1))
			{
				floor = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			if (ofloor < floor && ofloor < sector->floorplane.ZatPoint (check->v2))
			{
				floor = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != nullptr)
		*v = spot;
	return floor;
}

//
// P_FindHighestFloorSurrounding()
// FIND HIGHEST FLOOR HEIGHT IN SURROUNDING SECTORS
//
double FindHighestFloorSurrounding (const sector_t *sector, vertex_t **v)
{
	sector_t *other;
	double floor;
	double ofloor;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::floor);

	spot = sector->Lines[0]->v1;
	floor = -FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
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
	if (v != nullptr)
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
double FindNextHighestFloor (const sector_t *sector, vertex_t **v)
{
	double height;
	double heightdiff;
	double ofloor, floor;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::floor);

	spot = sector->Lines[0]->v1;
	height = sector->floorplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			floor = sector->floorplane.ZatPoint (check->v1);
			if (ofloor > floor && ofloor - floor < heightdiff && !sector->IsLinked(other, false))
			{
				heightdiff = ofloor - floor;
				height = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			floor = sector->floorplane.ZatPoint (check->v2);
			if (ofloor > floor && ofloor - floor < heightdiff && !sector->IsLinked(other, false))
			{
				heightdiff = ofloor - floor;
				height = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != nullptr)
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
double FindNextLowestFloor (const sector_t *sector, vertex_t **v)
{
	double height;
	double heightdiff;
	double ofloor, floor;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::floor);

	spot = sector->Lines[0]->v1;
	height = sector->floorplane.ZatPoint (spot);
	heightdiff = FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
		{
			ofloor = other->floorplane.ZatPoint (check->v1);
			floor = sector->floorplane.ZatPoint (check->v1);
			if (ofloor < floor && floor - ofloor < heightdiff && !sector->IsLinked(other, false))
			{
				heightdiff = floor - ofloor;
				height = ofloor;
				spot = check->v1;
			}
			ofloor = other->floorplane.ZatPoint (check->v2);
			floor = sector->floorplane.ZatPoint(check->v2);
			if (ofloor < floor && floor - ofloor < heightdiff && !sector->IsLinked(other, false))
			{
				heightdiff = floor - ofloor;
				height = ofloor;
				spot = check->v2;
			}
		}
	}
	if (v != nullptr)
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
double FindNextLowestCeiling (const sector_t *sector, vertex_t **v)
{
	double height;
	double heightdiff;
	double oceil, ceil;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::floor);

	spot = sector->Lines[0]->v1;
	height = sector->ceilingplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			ceil = sector->ceilingplane.ZatPoint(check->v1);
			if (oceil < ceil && ceil - oceil < heightdiff && !sector->IsLinked(other, true))
			{
				heightdiff = ceil - oceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
			ceil = sector->ceilingplane.ZatPoint(check->v2);
			if (oceil < ceil && ceil - oceil < heightdiff && !sector->IsLinked(other, true))
			{
				heightdiff = ceil - oceil;
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != nullptr)
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
double FindNextHighestCeiling (const sector_t *sector, vertex_t **v)
{
	double height;
	double heightdiff;
	double oceil, ceil;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::ceiling);

	spot = sector->Lines[0]->v1;
	height = sector->ceilingplane.ZatPoint(spot);
	heightdiff = FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
		{
			oceil = other->ceilingplane.ZatPoint(check->v1);
			ceil = sector->ceilingplane.ZatPoint(check->v1);
			if (oceil > ceil && oceil - ceil < heightdiff && !sector->IsLinked(other, true))
			{
				heightdiff = oceil - ceil;
				height = oceil;
				spot = check->v1;
			}
			oceil = other->ceilingplane.ZatPoint(check->v2);
			ceil = sector->ceilingplane.ZatPoint(check->v2);
			if (oceil > ceil && oceil - ceil < heightdiff && !sector->IsLinked(other, true))
			{
				heightdiff = oceil - ceil;
				height = oceil;
				spot = check->v2;
			}
		}
	}
	if (v != nullptr)
		*v = spot;
	return height;
}


//
// FIND LOWEST CEILING IN THE SURROUNDING SECTORS
//
double FindLowestCeilingSurrounding (const sector_t *sector, vertex_t **v)
{
	double height;
	double oceil;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::ceiling);

	spot = sector->Lines[0]->v1;
	height = FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
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
	if (v != nullptr)
		*v = spot;
	return height;
}


//
// FIND HIGHEST CEILING IN THE SURROUNDING SECTORS
//
double FindHighestCeilingSurrounding (const sector_t *sector, vertex_t **v)
{
	double height;
	double oceil;
	sector_t *other;
	vertex_t *spot;

	if (sector->Lines.Size() == 0) return sector->GetPlaneTexZ(sector_t::ceiling);

	spot = sector->Lines[0]->v1;
	height = -FLT_MAX;

	for (auto check : sector->Lines)
	{
		if (NULL != (other = getNextSector (check, sector)))
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
	if (v != nullptr)
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

static inline void CheckShortestTex (FLevelLocals *Level, FTextureID texnum, double &minsize)
{
	if (texnum.isValid() || (texnum.isNull() && (Level->i_compatflags & COMPATF_SHORTTEX)))
	{
		auto tex = TexMan.GetGameTexture(texnum);
		if (tex != NULL)
		{
			double h = tex->GetDisplayHeight();
			if (h < minsize)
			{
				minsize = h;
			}
		}
	}
}

double FindShortestTextureAround (sector_t *sec)
{
	double minsize = FLT_MAX;

	for (auto check : sec->Lines)
	{
		if (check->flags & ML_TWOSIDED)
		{
			CheckShortestTex (sec->Level, check->sidedef[0]->GetTexture(side_t::bottom), minsize);
			CheckShortestTex (sec->Level, check->sidedef[1]->GetTexture(side_t::bottom), minsize);
		}
	}
	return minsize < FLT_MAX ? minsize : TexMan.GameByIndex(0)->GetDisplayHeight();
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
double FindShortestUpperAround (sector_t *sec)
{
	double minsize = FLT_MAX;

	for (auto check : sec->Lines)
	{
		if (check->flags & ML_TWOSIDED)
		{
			CheckShortestTex (sec->Level, check->sidedef[0]->GetTexture(side_t::top), minsize);
			CheckShortestTex (sec->Level, check->sidedef[1]->GetTexture(side_t::top), minsize);
		}
	}
	return minsize < FLT_MAX ? minsize : TexMan.GameByIndex(0)->GetDisplayHeight();
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
sector_t *FindModelFloorSector (sector_t *sect, double floordestheight)
{
	sector_t *sec;

	for (auto check : sect->Lines)
	{
		sec = getNextSector (check, sect);
		if (sec != NULL &&
			(sec->floorplane.ZatPoint(check->v1) == floordestheight ||
			 sec->floorplane.ZatPoint(check->v2) == floordestheight))
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
sector_t *FindModelCeilingSector (sector_t *sect, double floordestheight)
{
	sector_t *sec;

	for (auto check : sect->Lines)
	{
		sec = getNextSector (check, sect);
		if (sec != NULL &&
			(sec->ceilingplane.ZatPoint(check->v1) == floordestheight ||
			 sec->ceilingplane.ZatPoint(check->v2) == floordestheight))
		{
			return sec;
		}
	}
	return NULL;
}

//
// Find minimum light from an adjacent sector
//
int FindMinSurroundingLight (const sector_t *sector, int min)
{
	sector_t*	check;
		
	for (auto line : sector->Lines)
	{
		if (NULL != (check = getNextSector (line, sector)) &&
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
double FindHighestFloorPoint (const sector_t *sector, vertex_t **v)
{
	double height = -FLT_MAX;
	double probeheight;
	vertex_t *spot = NULL;

	if (!sector->floorplane.isSlope())
	{
		if (v != NULL)
		{
			if (sector->Lines.Size() == 0) *v = &sector->Level->vertexes[0];
			else *v = sector->Lines[0]->v1;
		}
		return -sector->floorplane.fD();
	}

	for (auto line : sector->Lines)
	{
		probeheight = sector->floorplane.ZatPoint(line->v1);
		if (probeheight > height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = sector->floorplane.ZatPoint(line->v2);
		if (probeheight > height)
		{
			height = probeheight;
			spot = line->v2;
		}
	}
	if (v != nullptr)
		*v = spot;
	return height;
}


//
// Find the lowest point on the ceiling of the sector
//
double FindLowestCeilingPoint (const sector_t *sector, vertex_t **v)
{
	double height = FLT_MAX;
	double probeheight;
	vertex_t *spot = nullptr;

	if (!sector->ceilingplane.isSlope())
	{
		if (v != nullptr)
		{
			if (sector->Lines.Size() == 0) *v = &sector->Level->vertexes[0];
			else *v = sector->Lines[0]->v1;
		}
		return sector->ceilingplane.fD();
	}

	for (auto line : sector->Lines)
	{
		probeheight = sector->ceilingplane.ZatPoint(line->v1);
		if (probeheight < height)
		{
			height = probeheight;
			spot = line->v1;
		}
		probeheight = sector->ceilingplane.ZatPoint(line->v2);
		if (probeheight < height)
		{
			height = probeheight;
			spot = line->v2;
		}
	}
	if (v != nullptr)
		*v = spot;
	return height;
}


//=====================================================================================
//
// 'color' is intentionally an int here
//
//=====================================================================================

void SetColor(sector_t *sector, int color, int desat)
{
	sector->Colormap.LightColor = color;
	sector->Colormap.Desaturation = desat;
	P_RecalculateAttachedLights(sector);
}

//=====================================================================================
//
// 'color' is intentionally an int here
//
//=====================================================================================

void SetFade(sector_t *sector, int color)
{
	sector->Colormap.FadeColor = color;
	P_RecalculateAttachedLights(sector);
}

//=====================================================================================
//
//
//=====================================================================================

//=====================================================================================
//
//
//=====================================================================================

void sector_t::SetFogDensity(int dens)
{
	Colormap.FogDensity = dens;
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

int PlaneMoving(sector_t *sector, int pos)
{
	if (pos == sector_t::floor)
		return (sector->floordata != nullptr || (sector->planes[sector_t::floor].Flags & PLANEF_BLOCKED));
	else
		return (sector->ceilingdata != nullptr || (sector->planes[sector_t::ceiling].Flags & PLANEF_BLOCKED));
}

//=====================================================================================
//
//
//=====================================================================================

int GetFloorLight(const sector_t *sector)
{
	if (sector->GetFlags(sector_t::floor) & PLANEF_ABSLIGHTING)
	{
		return sector->GetPlaneLight(sector_t::floor);
	}
	else
	{
		return sector->ClampLight(sector->lightlevel + sector->GetPlaneLight(sector_t::floor));
	}
}

//=====================================================================================
//
//
//=====================================================================================

int GetCeilingLight(const sector_t *sector)
{
	if (sector->GetFlags(sector_t::ceiling) & PLANEF_ABSLIGHTING)
	{
		return sector->GetPlaneLight(sector_t::ceiling);
	}
	else
	{
		return sector->ClampLight(sector->lightlevel + sector->GetPlaneLight(sector_t::ceiling));
	}
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

void GetSpecial(sector_t *sector, secspecial_t *spec)
{
	spec->special = sector->special;
	spec->damageamount = sector->damageamount;
	spec->damagetype = sector->damagetype;
	spec->damageinterval = sector->damageinterval;
	spec->leakydamage = sector->leakydamage;
	spec->Flags = sector->Flags & SECF_SPECIALFLAGS;
}

//=====================================================================================
//
//
//=====================================================================================

void SetSpecial(sector_t *sector, const secspecial_t *spec)
{
	sector->special = spec->special;
	sector->damageamount = spec->damageamount;
	sector->damagetype = spec->damagetype;
	sector->damageinterval = spec->damageinterval;
	sector->leakydamage = spec->leakydamage;
	sector->Flags = (sector->Flags & ~SECF_SPECIALFLAGS) | (spec->Flags & SECF_SPECIALFLAGS);
}

//=====================================================================================
//
//
//=====================================================================================

void TransferSpecial(sector_t *sector, sector_t *model)
{
	sector->special = model->special;
	sector->damageamount = model->damageamount;
	sector->damagetype = model->damagetype;
	sector->damageinterval = model->damageinterval;
	sector->leakydamage = model->leakydamage;
	sector->Flags = (sector->Flags&~SECF_SPECIALFLAGS) | (model->Flags & SECF_SPECIALFLAGS);
}

//=====================================================================================
//
//
//=====================================================================================

int GetTerrain(const sector_t *sector, int pos)
{
	return sector->terrainnum[pos] >= 0 ? sector->terrainnum[pos] : TerrainTypes[sector->GetTexture(pos)];
}

FTerrainDef *GetFloorTerrain_S(const sector_t* sec, int pos)
{
	return &Terrains[GetTerrain(sec, pos)];
}

//=====================================================================================
//
//
//=====================================================================================

void CheckPortalPlane(sector_t *sector, int plane)
{
	if (sector->GetPortalType(plane) == PORTS_LINKEDPORTAL)
	{
		double portalh = sector->GetPortalPlaneZ(plane);
		double planeh = sector->GetPlaneTexZ(plane);
		int obstructed = PLANEF_OBSTRUCTED * (plane == sector_t::floor ? planeh > portalh : planeh < portalh);
		sector->planes[plane].Flags = (sector->planes[plane].Flags  & ~PLANEF_OBSTRUCTED) | obstructed;
	}
}

//===========================================================================
//
// Finds the highest ceiling at the given position, all portals considered
//
//===========================================================================

double HighestCeilingAt(sector_t *check, double x, double y, sector_t **resultsec)
{
	double planeheight = -FLT_MAX;
	DVector2 pos(x, y);

	// Continue until we find a blocking portal or a portal below where we actually are.
	while (!check->PortalBlocksMovement(sector_t::ceiling) && planeheight < check->GetPortalPlaneZ(sector_t::ceiling))
	{
		pos += check->GetPortalDisplacement(sector_t::ceiling);
		planeheight = check->GetPortalPlaneZ(sector_t::ceiling);
		check = check->Level->PointInSector(pos);
	}
	if (resultsec) *resultsec = check;
	return check->ceilingplane.ZatPoint(pos);
}

//===========================================================================
//
// Finds the lowest floor at the given position, all portals considered
//
//===========================================================================

double LowestFloorAt(sector_t *check, double x, double y, sector_t **resultsec)
{
	double planeheight = FLT_MAX;
	DVector2 pos(x, y);

	// Continue until we find a blocking portal or a portal above where we actually are.
	while (!check->PortalBlocksMovement(sector_t::floor) && planeheight > check->GetPortalPlaneZ(sector_t::floor))
	{
		pos += check->GetPortalDisplacement(sector_t::floor);
		planeheight = check->GetPortalPlaneZ(sector_t::ceiling);
		check = check->Level->PointInSector(pos);
	}
	if (resultsec) *resultsec = check;
	return check->floorplane.ZatPoint(pos);
}

//=====================================================================================
//
//
//=====================================================================================

double NextHighestCeilingAt(sector_t *sec, double x, double y, double bottomz, double topz, int flags, sector_t **resultsec, F3DFloor **resultffloor)
{
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
		if ((flags & FFCF_NOPORTALS) || sec->PortalBlocksMovement(sector_t::ceiling) || planeheight >= sec->GetPortalPlaneZ(sector_t::ceiling))
		{ // Use sector's ceiling
			if (resultffloor) *resultffloor = NULL;
			if (resultsec) *resultsec = sec;
			return realceil;
		}
		else
		{
			DVector2 pos = sec->GetPortalDisplacement(sector_t::ceiling);
			x += pos.X;
			y += pos.Y;
			planeheight = sec->GetPortalPlaneZ(sector_t::ceiling);
			sec = sec->Level->PointInSector(x, y);
		}
	}
}

//=====================================================================================
//
//
//=====================================================================================

double NextLowestFloorAt(sector_t *sec, double x, double y, double z, int flags, double steph, sector_t **resultsec, F3DFloor **resultffloor)
{
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
		if ((flags & FFCF_NOPORTALS) || sec->PortalBlocksMovement(sector_t::floor) || planeheight <= sec->GetPortalPlaneZ(sector_t::floor))
		{ // Use sector's floor
			if (resultffloor) *resultffloor = NULL;
			if (resultsec) *resultsec = sec;
			return realfloor;
		}
		else
		{
			DVector2 pos = sec->GetPortalDisplacement(sector_t::floor);
			x += pos.X;
			y += pos.Y;
			planeheight = sec->GetPortalPlaneZ(sector_t::floor);
			sec = sec->Level->PointInSector(x, y);
		}
	}
}

//===========================================================================
//
// 
//
//===========================================================================

double GetFriction(const sector_t *self, int plane, double *pMoveFac)
{
	if (self->Flags & SECF_FRICTION) 
	{ 
		if (pMoveFac) *pMoveFac = self->movefactor;
		return self->friction;
	}
	FTerrainDef *terrain = &Terrains[self->GetTerrain(plane)];
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

 //===========================================================================
 //
 // 
 //
 //===========================================================================

 void RemoveForceField(sector_t *sector)
 {
	 for (auto line : sector->Lines)
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

 //===========================================================================
 //
 // phares 3/12/98: End of friction effects
 //
 //===========================================================================

 void AdjustFloorClip(const sector_t *sector)
 {
	 msecnode_t *node;

	 for (node = sector->touching_thinglist; node; node = node->m_snext)
	 {
		 if (node->m_thing->flags2 & MF2_FLOORCLIP)
		 {
			 node->m_thing->AdjustFloorClip();
		 }
	 }
 }

 //==========================================================================
 //
 // Checks whether a sprite should be affected by a glow
 //
 //==========================================================================

 int sector_t::CheckSpriteGlow(int lightlevel, const DVector3 &pos)
 {
	 float bottomglowcolor[4];
	 bottomglowcolor[3] = 0;
	 auto c = planes[sector_t::floor].GlowColor;
	 if (c == 0)
	 {
		 auto tex = TexMan.GetGameTexture(GetTexture(sector_t::floor));
		 if (tex != NULL && tex->isGlowing())
		 {
			 if (!tex->isAutoGlowing()) tex = TexMan.GetGameTexture(GetTexture(sector_t::floor), true);
			 if (tex->isGlowing())	// recheck the current animation frame.
			 {
				 tex->GetGlowColor(bottomglowcolor);
				 bottomglowcolor[3] = (float)tex->GetGlowHeight();
			 }
		 }
	 }
	 else if (c != ~0u)
	 {
		 bottomglowcolor[0] = c.r / 255.f;
		 bottomglowcolor[1] = c.g / 255.f;
		 bottomglowcolor[2] = c.b / 255.f;
		 bottomglowcolor[3] = planes[sector_t::floor].GlowHeight;
	 }

	 if (bottomglowcolor[3]> 0)
	 {
		 double floordiff = pos.Z - floorplane.ZatPoint(pos);
		 if (floordiff < bottomglowcolor[3])
		 {
			 int maxlight = (255 + lightlevel) >> 1;
			 double lightfrac = floordiff / bottomglowcolor[3];
			 if (lightfrac < 0) lightfrac = 0;
			 lightlevel = int(lightfrac*lightlevel + maxlight * (1 - lightfrac));
		 }
	 }
	 return lightlevel;
 }

 //==========================================================================
 //
 // Checks whether a wall should glow
 //
 //==========================================================================
 bool sector_t::GetWallGlow(float *topglowcolor, float *bottomglowcolor)
 {
	 bool ret = false;
	 bottomglowcolor[3] = topglowcolor[3] = 0;
	 auto c = planes[sector_t::ceiling].GlowColor;
	 if (c == 0)
	 {
		 auto tex = TexMan.GetGameTexture(GetTexture(sector_t::ceiling));
		 if (tex != NULL && tex->isGlowing())
		 {
			 if (!tex->isAutoGlowing()) tex = TexMan.GetGameTexture(GetTexture(sector_t::ceiling), true);
			 if (tex->isGlowing())	// recheck the current animation frame.
			 {
				 ret = true;
				 tex->GetGlowColor(topglowcolor);
				 topglowcolor[3] = (float)tex->GetGlowHeight();
			 }
		 }
	 }
	 else if (c != ~0u)
	 {
		 topglowcolor[0] = c.r / 255.f;
		 topglowcolor[1] = c.g / 255.f;
		 topglowcolor[2] = c.b / 255.f;
		 topglowcolor[3] = planes[sector_t::ceiling].GlowHeight;
		 ret = topglowcolor[3] > 0;
	 }

	 c = planes[sector_t::floor].GlowColor;
	 if (c == 0)
	 {
		 auto tex = TexMan.GetGameTexture(GetTexture(sector_t::floor));
		 if (tex != NULL && tex->isGlowing())
		 {
			 if (!tex->isAutoGlowing()) tex = TexMan.GetGameTexture(GetTexture(sector_t::floor), true);
			 if (tex->isGlowing())	// recheck the current animation frame.
			 {
				 ret = true;
				 tex->GetGlowColor(bottomglowcolor);
				 bottomglowcolor[3] = (float)tex->GetGlowHeight();
			 }
		 }
	 }
	 else if (c != ~0u)
	 {
		 bottomglowcolor[0] = c.r / 255.f;
		 bottomglowcolor[1] = c.g / 255.f;
		 bottomglowcolor[2] = c.b / 255.f;
		 bottomglowcolor[3] = planes[sector_t::floor].GlowHeight;
		 ret = bottomglowcolor[3] > 0;
	 }
	 return ret;
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

 //===========================================================================
 //
 // checks if the floor is higher than the ceiling and sets a flag
 // This condition needs to be tested by the hardware renderer,
 // so always having its state available in a flag allows for easier optimization.
 //
 //===========================================================================

 void sector_t::CheckOverlap()
 {
	 if (planes[sector_t::floor].TexZ > planes[sector_t::ceiling].TexZ && !floorplane.isSlope() && !ceilingplane.isSlope())
	 {
		 MoreFlags |= SECMF_OVERLAPPING;
	 }
	 else
	 {
		 MoreFlags &= ~SECMF_OVERLAPPING;
	 }
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

bool FLevelLocals::AlignFlat (int linenum, int side, int fc)
{
	line_t *line = &lines[linenum];
	sector_t *sec = side ? line->backsector : line->frontsector;

	if (!sec)
		return false;

	DAngle angle = line->Delta().Angle();
	DAngle norm = angle - DAngle::fromDeg(90.);
	double dist = -(norm.Cos() * line->v1->fX() + norm.Sin() * line->v1->fY());

	if (side)
	{
		angle += DAngle::fromDeg(180.);
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

void FLevelLocals::ReplaceTextures(const char *fromname, const char *toname, int flags)
{
	FTextureID picnum1, picnum2;

	if (fromname == nullptr)
		return;

	if ((flags ^ (NOT_BOTTOM | NOT_MIDDLE | NOT_TOP)) != 0)
	{
		picnum1 = TexMan.GetTextureID(fromname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTextureID(toname, ETextureType::Wall, FTextureManager::TEXMAN_Overridable);

		for (auto &side : sides)
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
		picnum1 = TexMan.GetTextureID(fromname, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);
		picnum2 = TexMan.GetTextureID(toname, ETextureType::Flat, FTextureManager::TEXMAN_Overridable);

		for (auto &sec : sectors)
		{
			if (!(flags & NOT_FLOOR) && sec.GetTexture(sector_t::floor) == picnum1)
				sec.SetTexture(sector_t::floor, picnum2);
			if (!(flags & NOT_CEILING) && sec.GetTexture(sector_t::ceiling) == picnum1)
				sec.SetTexture(sector_t::ceiling, picnum2);
		}
	}
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
	PolyNodeLevel.Sides = &sector->Level->sides[0];
	PolyNodeLevel.NumSides = sector->Level->sides.Size();
	PolyNodeLevel.Lines = &sector->Level->lines[0];
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
		BSP->Subsectors[i].section = section;
	}
	for (unsigned i = 0; i < BSP->Segs.Size(); i++)
	{
		BSP->Segs[i].Subsector = this;
		BSP->Segs[i].PartnerSeg = nullptr;
	}

}

//===========================================================================
//
//
//
//===========================================================================

void line_t::AdjustLine()
{
	setDelta(v2->fX() - v1->fX(), v2->fY() - v1->fY());

	if (v1->fX() < v2->fX())
	{
		bbox[BOXLEFT] = v1->fX();
		bbox[BOXRIGHT] = v2->fX();
	}
	else
	{
		bbox[BOXLEFT] = v2->fX();
		bbox[BOXRIGHT] = v1->fX();
	}

	if (v1->fY() < v2->fY())
	{
		bbox[BOXBOTTOM] = v1->fY();
		bbox[BOXTOP] = v2->fY();
	}
	else
	{
		bbox[BOXBOTTOM] = v2->fY();
		bbox[BOXTOP] = v1->fY();
	}
}

//==========================================================================
//
//
//
//==========================================================================

int side_t::GetLightLevel (bool foggy, int baselight, int which, bool is3dlight, int *pfakecontrast) const
{
	if (!is3dlight)
	{
		if (Flags & (WALLF_ABSLIGHTING_TIER << which))
		{
			baselight = TierLights[which];
		}
		else if (Flags & WALLF_ABSLIGHTING)
		{
			baselight = Light + TierLights[which];
		}
	}

	if (pfakecontrast != NULL)
	{
		*pfakecontrast = 0;
	}

	if (!foggy || sector->Level->flags3 & LEVEL3_FORCEFAKECONTRAST) // Don't do relative lighting in foggy sectors
	{
		if (!(Flags & WALLF_NOFAKECONTRAST) && r_fakecontrast != 0)
		{
			DVector2 delta = linedef->Delta();
			int rel;
			if (((sector->Level->flags2 & LEVEL2_SMOOTHLIGHTING) || (Flags & WALLF_SMOOTHLIGHTING) || r_fakecontrast == 2) &&
				delta.X != 0)
			{
				rel = xs_RoundToInt // OMG LEE KILLOUGH LIVES! :/
					(
						sector->Level->WallHorizLight
						+ fabs(atan(delta.Y / delta.X) / 1.57079)
						* (sector->Level->WallVertLight - sector->Level->WallHorizLight)
					);
			}
			else
			{
				rel = delta.X == 0 ? sector->Level->WallVertLight : 
					  delta.Y == 0 ? sector->Level->WallHorizLight : 0;
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
	if (!is3dlight && !(Flags & WALLF_ABSLIGHTING) && !(Flags & (WALLF_ABSLIGHTING_TIER << which)) && (!foggy || (Flags & WALLF_LIGHT_FOG)))
	{
		baselight += this->Light + this->TierLights[which];
	}
	return baselight;
}

//==========================================================================
//
// Recalculate all heights affecting this vertex.
//
//==========================================================================

void vertex_t::RecalcVertexHeights()
{
	int i, j, k;
	float height;

	numheights = 0;
	for (i = 0; i < numsectors; i++)
	{
		for (j = 0; j<2; j++)
		{
			if (j == 0) height = (float)sectors[i]->ceilingplane.ZatPoint(this);
			else height = (float)sectors[i]->floorplane.ZatPoint(this);

			for (k = 0; k < numheights; k++)
			{
				if (height == heightlist[k]) break;
				if (height < heightlist[k])
				{
					memmove(&heightlist[k + 1], &heightlist[k], sizeof(float) * (numheights - k));
					heightlist[k] = height;
					numheights++;
					break;
				}
			}
			if (k == numheights) heightlist[numheights++] = height;
		}
	}
	if (numheights <= 2) numheights = 0;	// is not in need of any special attention
	dirty = false;
}

