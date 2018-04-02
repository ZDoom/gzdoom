// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_glow.cpp
** Glowing flats like Doomsday
**
**/

#include "w_wad.h"
#include "sc_man.h"
#include "v_video.h"
#include "r_defs.h"
#include "textures/textures.h"

#include "gl/dynlights/gl_glow.h"

//==========================================================================
//
// Checks whether a sprite should be affected by a glow
//
//==========================================================================
int gl_CheckSpriteGlow(sector_t *sector, int lightlevel, const DVector3 &pos)
{
	float bottomglowcolor[4];
	bottomglowcolor[3] = 0;
	auto c = sector->planes[sector_t::floor].GlowColor;
	if (c == 0)
	{
		FTexture *tex = TexMan[sector->GetTexture(sector_t::floor)];
		if (tex != NULL && tex->isGlowing())
		{
			if (!tex->bAutoGlowing) tex = TexMan(sector->GetTexture(sector_t::floor));
			if (tex->isGlowing())	// recheck the current animation frame.
			{
				tex->GetGlowColor(bottomglowcolor);
				bottomglowcolor[3] = (float)tex->GlowHeight;
			}
		}
	}
	else if (c != ~0u)
	{
		bottomglowcolor[0] = c.r / 255.f;
		bottomglowcolor[1] = c.g / 255.f;
		bottomglowcolor[2] = c.b / 255.f;
		bottomglowcolor[3] = sector->planes[sector_t::floor].GlowHeight;
	}

	if (bottomglowcolor[3]> 0)
	{
		double floordiff = pos.Z - sector->floorplane.ZatPoint(pos);
		if (floordiff < bottomglowcolor[3])
		{
			int maxlight = (255 + lightlevel) >> 1;
			double lightfrac = floordiff / bottomglowcolor[3];
			if (lightfrac < 0) lightfrac = 0;
			lightlevel = int(lightfrac*lightlevel + maxlight*(1 - lightfrac));
		}
	}
	return lightlevel;
}

//==========================================================================
//
// Checks whether a wall should glow
//
//==========================================================================
bool gl_GetWallGlow(sector_t *sector, float *topglowcolor, float *bottomglowcolor)
{
	bool ret = false;
	bottomglowcolor[3] = topglowcolor[3] = 0;
	auto c = sector->planes[sector_t::ceiling].GlowColor;
	if (c == 0)
	{
		FTexture *tex = TexMan[sector->GetTexture(sector_t::ceiling)];
		if (tex != NULL && tex->isGlowing())
		{
			if (!tex->bAutoGlowing) tex = TexMan(sector->GetTexture(sector_t::ceiling));
			if (tex->isGlowing())	// recheck the current animation frame.
			{
				ret = true;
				tex->GetGlowColor(topglowcolor);
				topglowcolor[3] = (float)tex->GlowHeight;
			}
		}
	}
	else if (c != ~0u)
	{
		topglowcolor[0] = c.r / 255.f;
		topglowcolor[1] = c.g / 255.f;
		topglowcolor[2] = c.b / 255.f;
		topglowcolor[3] = sector->planes[sector_t::ceiling].GlowHeight;
		ret = topglowcolor[3] > 0;
	}

	c = sector->planes[sector_t::floor].GlowColor;
	if (c == 0)
	{
		FTexture *tex = TexMan[sector->GetTexture(sector_t::floor)];
		if (tex != NULL && tex->isGlowing())
		{
			if (!tex->bAutoGlowing) tex = TexMan(sector->GetTexture(sector_t::floor));
			if (tex->isGlowing())	// recheck the current animation frame.
			{
				ret = true;
				tex->GetGlowColor(bottomglowcolor);
				bottomglowcolor[3] = (float)tex->GlowHeight;
			}
		}
	}
	else if (c != ~0u)
	{
		bottomglowcolor[0] = c.r / 255.f;
		bottomglowcolor[1] = c.g / 255.f;
		bottomglowcolor[2] = c.b / 255.f;
		bottomglowcolor[3] = sector->planes[sector_t::floor].GlowHeight;
		ret = bottomglowcolor[3] > 0;
	}
	return ret;
}

