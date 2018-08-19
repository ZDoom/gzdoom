// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2001-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_fakeflat.cpp
** Fake flat functions to render stacked sectors
**
*/

#include "p_lnspec.h"
#include "p_local.h"
#include "g_levellocals.h"
#include "a_sharedglobal.h"
#include "d_player.h"
#include "r_sky.h"
#include "hw_fakeflat.h"
#include "hw_drawinfo.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "r_utility.h"


//==========================================================================
//
// Check whether the player can look beyond this line
//
//==========================================================================

bool hw_CheckClip(side_t * sidedef, sector_t * frontsector, sector_t * backsector)
{
	line_t *linedef = sidedef->linedef;
	double bs_floorheight1;
	double bs_floorheight2;
	double bs_ceilingheight1;
	double bs_ceilingheight2;
	double fs_floorheight1;
	double fs_floorheight2;
	double fs_ceilingheight1;
	double fs_ceilingheight2;

	// Mirrors and horizons always block the view
	//if (linedef->special==Line_Mirror || linedef->special==Line_Horizon) return true;

	// Lines with portals must never block.
	// Portals which require the sky flat are excluded here, because for them the special sky semantics apply.
	if (!(frontsector->GetPortal(sector_t::ceiling)->mFlags & PORTSF_SKYFLATONLY) ||
		!(frontsector->GetPortal(sector_t::floor)->mFlags & PORTSF_SKYFLATONLY) ||
		!(backsector->GetPortal(sector_t::ceiling)->mFlags & PORTSF_SKYFLATONLY) ||
		!(backsector->GetPortal(sector_t::floor)->mFlags & PORTSF_SKYFLATONLY))
	{
		return false;
	}

	// on large levels this distinction can save some time
	// That's a lot of avoided multiplications if there's a lot to see!

	if (frontsector->ceilingplane.isSlope())
	{
		fs_ceilingheight1 = frontsector->ceilingplane.ZatPoint(linedef->v1);
		fs_ceilingheight2 = frontsector->ceilingplane.ZatPoint(linedef->v2);
	}
	else
	{
		fs_ceilingheight2 = fs_ceilingheight1 = frontsector->ceilingplane.fD();
	}

	if (frontsector->floorplane.isSlope())
	{
		fs_floorheight1 = frontsector->floorplane.ZatPoint(linedef->v1);
		fs_floorheight2 = frontsector->floorplane.ZatPoint(linedef->v2);
	}
	else
	{
		fs_floorheight2 = fs_floorheight1 = -frontsector->floorplane.fD();
	}

	if (backsector->ceilingplane.isSlope())
	{
		bs_ceilingheight1 = backsector->ceilingplane.ZatPoint(linedef->v1);
		bs_ceilingheight2 = backsector->ceilingplane.ZatPoint(linedef->v2);
	}
	else
	{
		bs_ceilingheight2 = bs_ceilingheight1 = backsector->ceilingplane.fD();
	}

	if (backsector->floorplane.isSlope())
	{
		bs_floorheight1 = backsector->floorplane.ZatPoint(linedef->v1);
		bs_floorheight2 = backsector->floorplane.ZatPoint(linedef->v2);
	}
	else
	{
		bs_floorheight2 = bs_floorheight1 = -backsector->floorplane.fD();
	}

	// now check for closed sectors!
	if (bs_ceilingheight1 <= fs_floorheight1 && bs_ceilingheight2 <= fs_floorheight2)
	{
		FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
		if (!tex || tex->UseType == ETextureType::Null) return false;
		if (backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
			frontsector->GetTexture(sector_t::ceiling) == skyflatnum) return false;
		return true;
	}

	if (fs_ceilingheight1 <= bs_floorheight1 && fs_ceilingheight2 <= bs_floorheight2)
	{
		FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
		if (!tex || tex->UseType == ETextureType::Null) return false;

		// properly render skies (consider door "open" if both floors are sky):
		if (backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
			frontsector->GetTexture(sector_t::ceiling) == skyflatnum) return false;
		return true;
	}

	if (bs_ceilingheight1 <= bs_floorheight1 && bs_ceilingheight2 <= bs_floorheight2)
	{
		// preserve a kind of transparent door/lift special effect:
		if (bs_ceilingheight1 < fs_ceilingheight1 || bs_ceilingheight2 < fs_ceilingheight2)
		{
			FTexture * tex = TexMan(sidedef->GetTexture(side_t::top));
			if (!tex || tex->UseType == ETextureType::Null) return false;
		}
		if (bs_floorheight1 > fs_floorheight1 || bs_floorheight2 > fs_floorheight2)
		{
			FTexture * tex = TexMan(sidedef->GetTexture(side_t::bottom));
			if (!tex || tex->UseType == ETextureType::Null) return false;
		}
		if (backsector->GetTexture(sector_t::ceiling) == skyflatnum &&
			frontsector->GetTexture(sector_t::ceiling) == skyflatnum) return false;
		if (backsector->GetTexture(sector_t::floor) == skyflatnum && frontsector->GetTexture(sector_t::floor)
			== skyflatnum) return false;
		return true;
	}

	return false;
}

//==========================================================================
//
// check for levels with exposed lower areas. May only be called if actual
// state is not known yet!
//
//==========================================================================

area_t hw_CheckViewArea(vertex_t *v1, vertex_t *v2, sector_t *frontsector, sector_t *backsector)
{
	if (backsector->GetHeightSec() && !frontsector->GetHeightSec())
	{
		sector_t * s = backsector->heightsec;

		double cz1 = frontsector->ceilingplane.ZatPoint(v1);
		double cz2 = frontsector->ceilingplane.ZatPoint(v2);
		double fz1 = s->floorplane.ZatPoint(v1);
		double fz2 = s->floorplane.ZatPoint(v2);

		// allow some tolerance in case slopes are involved
		if (cz1 <= fz1 + 1. / 100 && cz2 <= fz2 + 1. / 100)
			return area_below;
		else
			return area_normal;
	}
	return area_default;
}


//==========================================================================
//
// This is mostly like R_FakeFlat but with a few alterations necessitated
// by hardware rendering
//
//==========================================================================
sector_t * hw_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back)
{
	return sec;
	if (!sec->GetHeightSec() || sec->heightsec==sec) 
	{
		// check for backsectors with the ceiling lower than the floor. These will create
		// visual glitches because upper amd lower textures overlap.
		if (back && (sec->MoreFlags & SECMF_OVERLAPPING))
		{
			*dest = *sec;
			dest->ceilingplane = sec->floorplane;
			dest->ceilingplane.FlipVert();
			dest->planes[sector_t::ceiling].TexZ = dest->planes[sector_t::floor].TexZ;
			dest->ClearPortal(sector_t::ceiling);
			dest->ClearPortal(sector_t::floor);
			return dest;
		}
		return sec;
	}

#ifdef _DEBUG
	if (sec->sectornum==560)
	{
		int a = 0;
	}
#endif

	if (in_area==area_above)
	{
		if (sec->heightsec->MoreFlags&SECMF_FAKEFLOORONLY /*|| sec->GetTexture(sector_t::ceiling)==skyflatnum*/) in_area=area_normal;
	}

	int diffTex = (sec->heightsec->MoreFlags & SECMF_CLIPFAKEPLANES);
	sector_t * s = sec->heightsec;
	
	memcpy(dest, sec, sizeof(sector_t));

	// Replace floor and ceiling height with control sector's heights.
	if (diffTex)
	{
		if (s->floorplane.CopyPlaneIfValid (&dest->floorplane, &sec->ceilingplane, false))
		{
			dest->CopySecPlaneInfo(sector_t::floor, s, sector_t::floor, sec, sector_t::vbo_fakefloor);
			dest->CopyTextureInfo(sector_t::floor, s, sector_t::floor);
		}
		else if (s->MoreFlags & SECMF_FAKEFLOORONLY)
		{
			if (in_area==area_below)
			{
				dest->CopyColors(s);
				if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
				{
					dest->lightlevel  = s->lightlevel;
					dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
					dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
					dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
					dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
				}
				return dest;
			}
			return sec;
		}
	}
	else
	{
		dest->CopySecPlaneInfo(sector_t::floor, s, sector_t::floor, sec, sector_t::vbo_fakefloor);
	}

	if (!(s->MoreFlags&SECMF_FAKEFLOORONLY))
	{
		if (diffTex)
		{
			// This clips to the actual floor plane and inverts the texture rules.
			if (s->ceilingplane.CopyPlaneIfValid (&dest->ceilingplane, &sec->floorplane, false))
			{
				dest->CopySecPlaneInfo(sector_t::ceiling, s, sector_t::ceiling, sec, sector_t::vbo_fakeceiling);
				dest->CopyTextureInfo(sector_t::ceiling, s, sector_t::ceiling);
			}
		}
		else
		{
			dest->CopySecPlaneInfo(sector_t::ceiling, s, sector_t::ceiling, sec, sector_t::vbo_fakeceiling);
		}
	}

	if (in_area==area_below)
	{
		dest->CopyColors(s);
		dest->CopySecPlaneInfo(sector_t::floor, sec, sector_t::floor, sec, sector_t::floor);
		dest->CopySecPlaneInfo(sector_t::ceiling, s, sector_t::floor, sec, sector_t::vbo_fakefloor);
		dest->ClearPortal(sector_t::ceiling);

		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->lightlevel  = s->lightlevel;
		}

		if (!back)
		{
			dest->CopyTextureInfo(sector_t::floor, diffTex? sec : s, sector_t::floor);
			
			if (s->GetTexture(sector_t::ceiling) == skyflatnum) 
			{
				dest->CopyTextureInfo(sector_t::ceiling, dest, sector_t::floor);
			} 
			else 
			{
				dest->CopyTextureInfo(sector_t::ceiling, s, diffTex? sector_t::floor : sector_t::ceiling);
			}
			
			if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
			{
				dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
				dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
				dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
				dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
			}
		}
	}
	else if (in_area == area_above)
	{
		dest->CopyColors(s);
		dest->CopySecPlaneInfo(sector_t::ceiling, sec, sector_t::ceiling, sec, sector_t::ceiling);
		dest->CopySecPlaneInfo(sector_t::floor, s, sector_t::ceiling, sec, sector_t::vbo_fakeceiling);
		dest->ClearPortal(sector_t::floor);

		if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
		{
			dest->lightlevel  = s->lightlevel;
		}

		if (!back)
		{
			dest->CopyTextureInfo(sector_t::ceiling, diffTex? sec : s, sector_t::ceiling);
			
			if (s->GetTexture(sector_t::floor) != skyflatnum)
			{
				dest->CopyTextureInfo(sector_t::floor, s, sector_t::floor);
			}
			else
			{
				dest->CopyTextureInfo(sector_t::floor, s, sector_t::ceiling);
			}
			
			if (!(s->MoreFlags & SECMF_NOFAKELIGHT))
			{
				dest->lightlevel  = s->lightlevel;
				dest->SetPlaneLight(sector_t::floor, s->GetPlaneLight(sector_t::floor));
				dest->SetPlaneLight(sector_t::ceiling, s->GetPlaneLight(sector_t::ceiling));
				dest->ChangeFlags(sector_t::floor, -1, s->GetFlags(sector_t::floor));
				dest->ChangeFlags(sector_t::ceiling, -1, s->GetFlags(sector_t::ceiling));
			}
		}
	}
	return dest;
}
