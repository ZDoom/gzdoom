/*
** gl_sky.cpp
** Sky preparation code.
**
**---------------------------------------------------------------------------
** Copyright 2002-2005 Christoph Oelckers
** All rights reserved.
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
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
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

#include "gl/system/gl_system.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "r_sky.h"
#include "r_state.h"
#include "r_utility.h"
#include "doomdata.h"
#include "portal.h"
#include "gl/gl_functions.h"

#include "gl/data/gl_data.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_convert.h"

CVAR(Bool,gl_noskyboxes, false, 0)
extern int skyfog;

enum
{
	NoSkyDraw = 89
};


//==========================================================================
//
//  Set up the skyinfo struct
//
//==========================================================================

void GLSkyInfo::init(int sky1, PalEntry FadeColor)
{
	memset(this, 0, sizeof(*this));
	if ((sky1 & PL_SKYFLAT) && (sky1 & (PL_SKYFLAT - 1)))
	{
		const line_t *l = &lines[(sky1&(PL_SKYFLAT - 1)) - 1];
		const side_t *s = l->sidedef[0];
		int pos;

		if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
		{
			pos = side_t::bottom;
		}
		else
		{
			pos = side_t::top;
		}

		FTextureID texno = s->GetTexture(pos);
		texture[0] = FMaterial::ValidateTexture(texno, false, true);
		if (!texture[0] || texture[0]->tex->UseType == FTexture::TEX_Null) goto normalsky;
		skytexno1 = texno;
		x_offset[0] = ANGLE_TO_FLOAT(s->GetTextureXOffset(pos));
		y_offset = FIXED2FLOAT(s->GetTextureYOffset(pos));
		mirrored = !l->args[2];
	}
	else
	{
	normalsky:
		if (level.flags&LEVEL_DOUBLESKY)
		{
			texture[1] = FMaterial::ValidateTexture(sky1texture, false, true);
			x_offset[1] = GLRenderer->mSky1Pos;
			doublesky = true;
		}

		if ((level.flags&LEVEL_SWAPSKIES || (sky1 == PL_SKYFLAT) || (level.flags&LEVEL_DOUBLESKY)) &&
			sky2texture != sky1texture)	// If both skies are equal use the scroll offset of the first!
		{
			texture[0] = FMaterial::ValidateTexture(sky2texture, false, true);
			skytexno1 = sky2texture;
			sky2 = true;
			x_offset[0] = GLRenderer->mSky2Pos;
		}
		else if (!doublesky)
		{
			texture[0] = FMaterial::ValidateTexture(sky1texture, false, true);
			skytexno1 = sky1texture;
			x_offset[0] = GLRenderer->mSky1Pos;
		}
	}
	if (skyfog > 0)
	{
		fadecolor = FadeColor;
		fadecolor.a = 0;
	}
	else fadecolor = 0;

}


//==========================================================================
//
//  Calculate sky texture for ceiling or floor
//
//==========================================================================

void GLWall::SkyPlane(sector_t *sector, int plane, bool allowreflect)
{
	int ptype = -1;
	FPortal *portal = sector->portals[plane];
	if (portal != NULL)
	{
		if (sector->PortalBlocksView(plane)) return;

		if (GLPortal::instack[1 - plane]) return;
		ptype = PORTALTYPE_SECTORSTACK;
		this->portal = portal;
	}
	else
	{
		ASkyViewpoint * skyboxx = sector->GetSkyBox(plane);
		if (sector->GetTexture(plane) == skyflatnum || (skyboxx != NULL && skyboxx->bAlways))
		{
			GLSkyInfo skyinfo;

			// JUSTHIT is used as an indicator that a skybox is in use.
			// This is to avoid recursion

			if (!gl_noskyboxes && skyboxx && GLRenderer->mViewActor != skyboxx && !(skyboxx->flags&MF_JUSTHIT))
			{
				ptype = PORTALTYPE_SKYBOX;
				skybox = skyboxx;
			}
			else
			{
				skyinfo.init(sector->sky, Colormap.FadeColor);
				ptype = PORTALTYPE_SKY;
				sky = UniqueSkies.Get(&skyinfo);
			}
		}
		else if (allowreflect && sector->GetReflect(plane) > 0)
		{
			if ((plane == sector_t::ceiling && viewz > sector->ceilingplane.d) ||
				(plane == sector_t::floor && viewz < -sector->floorplane.d)) return;
			ptype = PORTALTYPE_PLANEMIRROR;
			planemirror = plane == sector_t::ceiling ? &sector->ceilingplane : &sector->floorplane;
		}
	}
	if (ptype != -1)
	{
		PutPortal(ptype);
	}
}


//==========================================================================
//
//  Calculate sky texture for a line
//
//==========================================================================

void GLWall::SkyLine(sector_t *fs, line_t *line)
{
	ASkyViewpoint * skyboxx = line->skybox;
	GLSkyInfo skyinfo;
	int ptype;

	// JUSTHIT is used as an indicator that a skybox is in use.
	// This is to avoid recursion

	if (!gl_noskyboxes && skyboxx && GLRenderer->mViewActor != skyboxx && !(skyboxx->flags&MF_JUSTHIT))
	{
		ptype = PORTALTYPE_SKYBOX;
		skybox = skyboxx;
	}
	else
	{
		skyinfo.init(fs->sky, Colormap.FadeColor);
		ptype = PORTALTYPE_SKY;
		sky = UniqueSkies.Get(&skyinfo);
	}
	PutPortal(ptype);
}


//==========================================================================
//
//  Skies on one sided walls
//
//==========================================================================

void GLWall::SkyNormal(sector_t * fs,vertex_t * v1,vertex_t * v2)
{
	ztop[0]=ztop[1]=32768.0f;
	zbottom[0]=zceil[0];
	zbottom[1]=zceil[1];
	SkyPlane(fs, sector_t::ceiling, true);

	if (seg->linedef->skybox != NULL)
	{
		ztop[0] = zceil[0];
		ztop[1] = zceil[1];
		zbottom[0] = zfloor[0];
		zbottom[1] = zfloor[1];
		SkyLine(fs, seg->linedef);
	}

	ztop[0]=zfloor[0];
	ztop[1]=zfloor[1];
	zbottom[0]=zbottom[1]=-32768.0f;
	SkyPlane(fs, sector_t::floor, true);
}

//==========================================================================
//
//  Upper Skies on two sided walls
//
//==========================================================================

void GLWall::SkyTop(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2)
{
	if (fs->GetTexture(sector_t::ceiling)==skyflatnum)
	{
		if (bs->special == NoSkyDraw) return;
		if (bs->GetTexture(sector_t::ceiling)==skyflatnum) 
		{
			// if the back sector is closed the sky must be drawn!
			if (bs->ceilingplane.ZatPoint(v1) > bs->floorplane.ZatPoint(v1) ||
				bs->ceilingplane.ZatPoint(v2) > bs->floorplane.ZatPoint(v2) || bs->transdoor)
					return;

			// one more check for some ugly transparent door hacks
			if (bs->floorplane.a==0 && bs->floorplane.b==0 && fs->floorplane.a==0 && fs->floorplane.b==0)
			{
				if (bs->GetPlaneTexZ(sector_t::floor)==fs->GetPlaneTexZ(sector_t::floor)+FRACUNIT)
				{
					FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
					if (!tex || tex->UseType==FTexture::TEX_Null) return;

					// very, very, very ugly special case (See Icarus MAP14)
					// It is VERY important that this is only done for a floor height difference of 1
					// or it will cause glitches elsewhere.
					tex = TexMan(seg->sidedef->GetTexture(side_t::mid));
					if (tex != NULL && !(seg->linedef->flags & ML_DONTPEGTOP) &&
						seg->sidedef->GetTextureYOffset(side_t::mid) > 0)
					{
						ztop[0]=ztop[1]=32768.0f;
						zbottom[0]=zbottom[1]= 
							FIXED2FLOAT(bs->ceilingplane.ZatPoint(v2) + seg->sidedef->GetTextureYOffset(side_t::mid));
						SkyPlane(fs, sector_t::ceiling, false);
						return;
					}
				}
			}
		}

		ztop[0]=ztop[1]=32768.0f;

		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::top));
		if (bs->GetTexture(sector_t::ceiling) != skyflatnum)

		{
			zbottom[0]=zceil[0];
			zbottom[1]=zceil[1];
		}
		else
		{
			zbottom[0]=FIXED2FLOAT(bs->ceilingplane.ZatPoint(v1));
			zbottom[1]=FIXED2FLOAT(bs->ceilingplane.ZatPoint(v2));
			flags|=GLWF_SKYHACK;	// mid textures on such lines need special treatment!
		}
	}
	else 
	{
		float frontreflect = fs->GetReflect(sector_t::ceiling);
		if (frontreflect > 0)
		{
			float backreflect = bs->GetReflect(sector_t::ceiling);
			if (backreflect > 0 && bs->ceilingplane.d == fs->ceilingplane.d)
			{
				// Don't add intra-portal line to the portal.
				return;
			}
		}
		else
		{
			FPortal *pfront = fs->portals[sector_t::ceiling];
			FPortal *pback = bs->portals[sector_t::ceiling];
			if (pfront == NULL || fs->PortalBlocksView(sector_t::ceiling)) return;
			if (pfront == pback && !bs->PortalBlocksView(sector_t::ceiling)) return;
		}

		// stacked sectors
		fixed_t fsc1=fs->ceilingplane.ZatPoint(v1);
		fixed_t fsc2=fs->ceilingplane.ZatPoint(v2);

		ztop[0]=ztop[1]=32768.0f;
		zbottom[0]=FIXED2FLOAT(fsc1);
		zbottom[1]=FIXED2FLOAT(fsc2);
	}

	SkyPlane(fs, sector_t::ceiling, true);
}


//==========================================================================
//
//  Lower Skies on two sided walls
//
//==========================================================================

void GLWall::SkyBottom(seg_t * seg,sector_t * fs,sector_t * bs,vertex_t * v1,vertex_t * v2)
{
	if (fs->GetTexture(sector_t::floor)==skyflatnum)
	{
		if (bs->special == NoSkyDraw) return;
		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
		
		// For lower skies the normal logic only applies to walls with no lower texture!
		if (tex->UseType==FTexture::TEX_Null)
		{
			if (bs->GetTexture(sector_t::floor)==skyflatnum)
			{
				// if the back sector is closed the sky must be drawn!
				if (bs->ceilingplane.ZatPoint(v1) > bs->floorplane.ZatPoint(v1) ||
					bs->ceilingplane.ZatPoint(v2) > bs->floorplane.ZatPoint(v2))
						return;

			}
			else
			{
				// Special hack for Vrack2b
				if (bs->floorplane.ZatPoint(FIXED2FLOAT(viewx), FIXED2FLOAT(viewy)) > FIXED2FLOAT(viewz)) return;
			}
		}
		zbottom[0]=zbottom[1]=-32768.0f;

		if ((tex && tex->UseType!=FTexture::TEX_Null) || bs->GetTexture(sector_t::floor)!=skyflatnum)
		{
			ztop[0]=zfloor[0];
			ztop[1]=zfloor[1];
		}
		else
		{
			ztop[0]=FIXED2FLOAT(bs->floorplane.ZatPoint(v1));
			ztop[1]=FIXED2FLOAT(bs->floorplane.ZatPoint(v2));
			flags|=GLWF_SKYHACK;	// mid textures on such lines need special treatment!
		}
	}
	else 
	{
		float frontreflect = fs->GetReflect(sector_t::floor);
		if (frontreflect > 0)
		{
			float backreflect = bs->GetReflect(sector_t::floor);
			if (backreflect > 0 && bs->floorplane.d == fs->floorplane.d)
			{
				// Don't add intra-portal line to the portal.
				return;
			}
		}
		else
		{
			FPortal *pfront = fs->portals[sector_t::floor];
			FPortal *pback = bs->portals[sector_t::floor];
			if (pfront == NULL || fs->PortalBlocksView(sector_t::floor)) return;
			if (pfront == pback && !bs->PortalBlocksView(sector_t::floor)) return;
		}

		// stacked sectors
		fixed_t fsc1=fs->floorplane.ZatPoint(v1);
		fixed_t fsc2=fs->floorplane.ZatPoint(v2);

		zbottom[0]=zbottom[1]=-32768.0f;
		ztop[0]=FIXED2FLOAT(fsc1);
		ztop[1]=FIXED2FLOAT(fsc2);
	}

	SkyPlane(fs, sector_t::floor, true);
}

