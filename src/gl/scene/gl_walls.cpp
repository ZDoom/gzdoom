/*
** gl_wall.cpp
** Wall rendering preparation
**
**---------------------------------------------------------------------------
** Copyright 2000-2005 Christoph Oelckers
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
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "templates.h"
#include "vectors.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "p_maputl.h"
#include "doomdata.h"
#include "portal.h"

#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_templates.h"
#include "gl/shaders/gl_shader.h"


//==========================================================================
//
// Checks whether a wall should glow
//
//==========================================================================
void GLWall::CheckGlowing()
{
	bottomglowcolor[3] = topglowcolor[3] = 0;
	if (!gl_isFullbright(Colormap.LightColor, lightlevel))
	{
		FTexture *tex = TexMan[topflat];
		if (tex != NULL && tex->isGlowing())
		{
			flags |= GLWall::GLWF_GLOW;
			tex->GetGlowColor(topglowcolor);
			topglowcolor[3] = tex->gl_info.GlowHeight;
		}

		tex = TexMan[bottomflat];
		if (tex != NULL && tex->isGlowing())
		{
			flags |= GLWall::GLWF_GLOW;
			tex->GetGlowColor(bottomglowcolor);
			bottomglowcolor[3] = tex->gl_info.GlowHeight;
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::PutWall(bool translucent)
{
	int list;

	static char passflag[] = {
		0,		//RENDERWALL_NONE,             
		1,		//RENDERWALL_TOP,              // unmasked
		1,		//RENDERWALL_M1S,              // unmasked
		2,		//RENDERWALL_M2S,              // depends on render and texture settings
		1,		//RENDERWALL_BOTTOM,           // unmasked
		3,		//RENDERWALL_FOGBOUNDARY,      // translucent
		1,		//RENDERWALL_MIRRORSURFACE,    // only created from PORTALTYPE_MIRROR
		2,		//RENDERWALL_M2SNF,            // depends on render and texture settings, no fog, used on mid texture lines with a fog boundary.
		3,		//RENDERWALL_COLOR,            // translucent
		2,		//RENDERWALL_FFBLOCK           // depends on render and texture settings
	};


	if (gltexture && gltexture->GetTransparent() && passflag[type] == 2)
	{
		translucent = true;
	}

	if (gl_fixedcolormap)
	{
		// light planes don't get drawn with fullbright rendering
		if (gltexture == NULL) return;
		Colormap.Clear();
	}

	CheckGlowing();

	if (translucent) // translucent walls
	{
		ViewDistance = (ViewPos - (seg->linedef->v1->fPos() + seg->linedef->Delta() / 2)).XY().LengthSquared();
		gl_drawinfo->drawlists[GLDL_TRANSLUCENT].AddWall(this);
	}
	else
	{
		if (gl.lightmethod == LM_SOFTWARE && !translucent)
		{
			// This is not yet ready.
			if (PutWallCompat(passflag[type])) return;
		}

		bool masked;

		masked = passflag[type] == 1 ? false : (gltexture && gltexture->isMasked());

		if ((flags&GLWF_SKYHACK && type == RENDERWALL_M2S))
		{
			list = GLDL_MASKEDWALLSOFS;
		}
		else
		{
			list = masked ? GLDL_MASKEDWALLS : GLDL_PLAINWALLS;
		}
		gl_drawinfo->drawlists[list].AddWall(this);

	}
	lightlist = NULL;
}

void GLWall::PutPortal(int ptype)
{
	GLPortal * portal;

	switch (ptype)
	{
	// portals don't go into the draw list.
	// Instead they are added to the portal manager
	case PORTALTYPE_HORIZON:
		horizon=UniqueHorizons.Get(horizon);
		portal=GLPortal::FindPortal(horizon);
		if (!portal) portal=new GLHorizonPortal(horizon);
		portal->AddLine(this);
		break;

	case PORTALTYPE_SKYBOX:
		portal = GLPortal::FindPortal(secportal);
		if (!portal)
		{
			// either a regular skybox or an Eternity-style horizon
			if (secportal->mType != PORTS_SKYVIEWPOINT) portal = new GLEEHorizonPortal(secportal);
			else portal = new GLSkyboxPortal(secportal);
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_SECTORSTACK:
		portal = this->portal->GetRenderState();
		portal->AddLine(this);
		break;

	case PORTALTYPE_PLANEMIRROR:
		if (GLPortal::PlaneMirrorMode * planemirror->fC() <=0)
		{
			//@sync-portal
			planemirror=UniquePlaneMirrors.Get(planemirror);
			portal=GLPortal::FindPortal(planemirror);
			if (!portal) portal=new GLPlaneMirrorPortal(planemirror);
			portal->AddLine(this);
		}
		break;

	case PORTALTYPE_MIRROR:
		portal=GLPortal::FindPortal(seg->linedef);
		if (!portal) portal=new GLMirrorPortal(seg->linedef);
		portal->AddLine(this);
		if (gl_mirror_envmap) 
		{
			// draw a reflective layer over the mirror
			type=RENDERWALL_MIRRORSURFACE;
			gl_drawinfo->drawlists[GLDL_TRANSLUCENTBORDER].AddWall(this);
		}
		break;

	case PORTALTYPE_LINETOLINE:
		portal=GLPortal::FindPortal(lineportal);
		if (!portal)
		{
			line_t *otherside = lineportal->lines[0]->mDestination;
			if (otherside != NULL && otherside->portalindex < linePortals.Size())
			{
				gl_RenderActorsInPortal(linePortalToGL[otherside->portalindex]);
			}
			portal = new GLLineToLinePortal(lineportal);
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_SKY:
		portal=GLPortal::FindPortal(sky);
		if (!portal) portal=new GLSkyPortal(sky);
		portal->AddLine(this);
		break;
	}
}
//==========================================================================
//
//	Sets 3D-floor lighting info
//
//==========================================================================

void GLWall::Put3DWall(lightlist_t * lightlist, bool translucent)
{
	// only modify the light level if it doesn't originate from the seg's frontsector. This is to account for light transferring effects
	if (lightlist->p_lightlevel != &seg->sidedef->sector->lightlevel)
	{
		lightlevel = gl_ClampLight(*lightlist->p_lightlevel);
	}
	// relative light won't get changed here. It is constant across the entire wall.

	Colormap.CopyFrom3DLight(lightlist);
	PutWall(translucent);
}

//==========================================================================
//
//  Splits a wall vertically if a 3D-floor
//	creates different lighting across the wall
//
//==========================================================================

bool GLWall::SplitWallComplex(sector_t * frontsector, bool translucent, float& maplightbottomleft, float& maplightbottomright)
{
	GLWall copyWall1, copyWall2;

	// check for an intersection with the upper plane
	if ((maplightbottomleft<ztop[0] && maplightbottomright>ztop[1]) ||
		(maplightbottomleft>ztop[0] && maplightbottomright<ztop[1]))
	{
		float clen = MAX<float>(fabsf(glseg.x2 - glseg.x1), fabsf(glseg.y2 - glseg.y1));

		float dch = ztop[1] - ztop[0];
		float dfh = maplightbottomright - maplightbottomleft;
		float coeff = (ztop[0] - maplightbottomleft) / (dfh - dch);

		// check for inaccuracies - let's be a little generous here!
		if (coeff*clen<.1f)
		{
			maplightbottomleft = ztop[0];
		}
		else if (coeff*clen>clen - .1f)
		{
			maplightbottomright = ztop[1];
		}
		else
		{
			// split the wall in two at the intersection and recursively split both halves
			copyWall1 = copyWall2 = *this;

			copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2 - glseg.x1);
			copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2 - glseg.y1);
			copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1] - ztop[0]);
			copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1] - zbottom[0]);
			copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright - glseg.fracleft);
			copyWall1.tcs[UPRGT].u = copyWall2.tcs[UPLFT].u = tcs[UPLFT].u + coeff * (tcs[UPRGT].u - tcs[UPLFT].u);
			copyWall1.tcs[UPRGT].v = copyWall2.tcs[UPLFT].v = tcs[UPLFT].v + coeff * (tcs[UPRGT].v - tcs[UPLFT].v);
			copyWall1.tcs[LORGT].u = copyWall2.tcs[LOLFT].u = tcs[LOLFT].u + coeff * (tcs[LORGT].u - tcs[LOLFT].u);
			copyWall1.tcs[LORGT].v = copyWall2.tcs[LOLFT].v = tcs[LOLFT].v + coeff * (tcs[LORGT].v - tcs[LOLFT].v);

			copyWall1.SplitWall(frontsector, translucent);
			copyWall2.SplitWall(frontsector, translucent);
			return true;
		}
	}

	// check for an intersection with the lower plane
	if ((maplightbottomleft<zbottom[0] && maplightbottomright>zbottom[1]) ||
		(maplightbottomleft>zbottom[0] && maplightbottomright<zbottom[1]))
	{
		float clen = MAX<float>(fabsf(glseg.x2 - glseg.x1), fabsf(glseg.y2 - glseg.y1));

		float dch = zbottom[1] - zbottom[0];
		float dfh = maplightbottomright - maplightbottomleft;
		float coeff = (zbottom[0] - maplightbottomleft) / (dfh - dch);

		// check for inaccuracies - let's be a little generous here because there's
		// some conversions between floats and fixed_t's involved
		if (coeff*clen<.1f)
		{
			maplightbottomleft = zbottom[0];
		}
		else if (coeff*clen>clen - .1f)
		{
			maplightbottomright = zbottom[1];
		}
		else
		{
			// split the wall in two at the intersection and recursively split both halves
			copyWall1 = copyWall2 = *this;

			copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2 - glseg.x1);
			copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2 - glseg.y1);
			copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1] - ztop[0]);
			copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1] - zbottom[0]);
			copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright - glseg.fracleft);
			copyWall1.tcs[UPRGT].u = copyWall2.tcs[UPLFT].u = tcs[UPLFT].u + coeff * (tcs[UPRGT].u - tcs[UPLFT].u);
			copyWall1.tcs[UPRGT].v = copyWall2.tcs[UPLFT].v = tcs[UPLFT].v + coeff * (tcs[UPRGT].v - tcs[UPLFT].v);
			copyWall1.tcs[LORGT].u = copyWall2.tcs[LOLFT].u = tcs[LOLFT].u + coeff * (tcs[LORGT].u - tcs[LOLFT].u);
			copyWall1.tcs[LORGT].v = copyWall2.tcs[LOLFT].v = tcs[LOLFT].v + coeff * (tcs[LORGT].v - tcs[LOLFT].v);

			copyWall1.SplitWall(frontsector, translucent);
			copyWall2.SplitWall(frontsector, translucent);
			return true;
		}
	}

	return false;
}

void GLWall::SplitWall(sector_t * frontsector, bool translucent)
{
	GLWall copyWall1;
	float maplightbottomleft;
	float maplightbottomright;
	unsigned int i;
	int origlight = lightlevel;
	FColormap origcm=Colormap;

	TArray<lightlist_t> & lightlist=frontsector->e->XFloor.lightlist;

	if (glseg.x1==glseg.x2 && glseg.y1==glseg.y2)
	{
		return;
	}
	//::SplitWall.Clock();

#ifdef _DEBUG
	if (seg->linedef-lines==1)
	{
		int a = 0;
	}
#endif

	if (lightlist.Size()>1)
	{
		for(i=0;i<lightlist.Size()-1;i++)
		{
			if (i<lightlist.Size()-1) 
			{
				secplane_t &p = lightlist[i+1].plane;
				maplightbottomleft = p.ZatPoint(glseg.x1,glseg.y1);
				maplightbottomright= p.ZatPoint(glseg.x2,glseg.y2);
			}
			else 
			{
				maplightbottomright = maplightbottomleft = -32000;
			}

			// The light is completely above the wall!
			if (maplightbottomleft>=ztop[0] && maplightbottomright>=ztop[1])
			{
				continue;
			}

			// check for an intersection with the upper and lower planes of the wall segment
			if ((maplightbottomleft<ztop[0] && maplightbottomright>ztop[1]) ||
				(maplightbottomleft > ztop[0] && maplightbottomright < ztop[1]) ||
				(maplightbottomleft<zbottom[0] && maplightbottomright>zbottom[1]) ||
				(maplightbottomleft > zbottom[0] && maplightbottomright < zbottom[1]))
			{
				if (!(gl.flags & RFL_NO_CLIP_PLANES))
				{
					// Use hardware clipping if this cannot be done cleanly.
					this->lightlist = &lightlist;
					PutWall(translucent);

					goto out;
				}
				// crappy fallback if no clip planes available
				else if (SplitWallComplex(frontsector, translucent, maplightbottomleft, maplightbottomright))
				{
					goto out;
				}
			}

			// 3D floor is completely within this light
			if (maplightbottomleft<=zbottom[0] && maplightbottomright<=zbottom[1])
			{
				Put3DWall(&lightlist[i], translucent);
				goto out;
			}

			if (maplightbottomleft<=ztop[0] && maplightbottomright<=ztop[1] &&
				(maplightbottomleft!=ztop[0] || maplightbottomright!=ztop[1]))
			{
				copyWall1=*this;

				copyWall1.flags |= GLWF_NOSPLITLOWER;
				flags |= GLWF_NOSPLITUPPER;
				ztop[0]=copyWall1.zbottom[0]=maplightbottomleft;
				ztop[1]=copyWall1.zbottom[1]=maplightbottomright;
				tcs[UPLFT].v=copyWall1.tcs[LOLFT].v=copyWall1.tcs[UPLFT].v+ 
					(maplightbottomleft-copyWall1.ztop[0])*(copyWall1.tcs[LOLFT].v-copyWall1.tcs[UPLFT].v)/(zbottom[0]-copyWall1.ztop[0]);
				tcs[UPRGT].v=copyWall1.tcs[LORGT].v=copyWall1.tcs[UPRGT].v+ 
					(maplightbottomright-copyWall1.ztop[1])*(copyWall1.tcs[LORGT].v-copyWall1.tcs[UPRGT].v)/(zbottom[1]-copyWall1.ztop[1]);
				copyWall1.Put3DWall(&lightlist[i], translucent);
			}
			if (ztop[0]==zbottom[0] && ztop[1]==zbottom[1]) 
			{
				//::SplitWall.Unclock();
				goto out;
			}
		}
	}

	Put3DWall(&lightlist[lightlist.Size()-1], translucent);

out:
	lightlevel=origlight;
	Colormap=origcm;
	flags &= ~GLWF_NOSPLITUPPER;
	this->lightlist = NULL;
	//::SplitWall.Unclock();
}


//==========================================================================
//
// 
//
//==========================================================================
bool GLWall::DoHorizon(seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2)
{
	GLHorizonInfo hi;
	lightlist_t * light;

	// ZDoom doesn't support slopes in a horizon sector so I won't either!
	ztop[1] = ztop[0] = fs->GetPlaneTexZ(sector_t::ceiling);
	zbottom[1] = zbottom[0] = fs->GetPlaneTexZ(sector_t::floor);

	if (ViewPos.Z < fs->GetPlaneTexZ(sector_t::ceiling))
	{
		if (ViewPos.Z > fs->GetPlaneTexZ(sector_t::floor))
			zbottom[1] = zbottom[0] = ViewPos.Z;

		if (fs->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			SkyPlane(fs, sector_t::ceiling, false);
		}
		else
		{
			hi.plane.GetFromSector(fs, true);
			hi.lightlevel = gl_ClampLight(fs->GetCeilingLight());
			hi.colormap = fs->ColorMap;

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->ceilingplane, true);

				if(!(fs->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING)) hi.lightlevel = gl_ClampLight(*light->p_lightlevel);
				hi.colormap.LightColor = (light->extra_colormap)->Color;
			}

			if (gl_fixedcolormap) hi.colormap.Clear();
			horizon = &hi;
			PutPortal(PORTALTYPE_HORIZON);
		}
		ztop[1] = ztop[0] = zbottom[0];
	} 

	if (ViewPos.Z > fs->GetPlaneTexZ(sector_t::floor))
	{
		zbottom[1] = zbottom[0] = fs->GetPlaneTexZ(sector_t::floor);
		if (fs->GetTexture(sector_t::floor) == skyflatnum)
		{
			SkyPlane(fs, sector_t::floor, false);
		}
		else
		{
			hi.plane.GetFromSector(fs, false);
			hi.lightlevel = gl_ClampLight(fs->GetFloorLight());
			hi.colormap = fs->ColorMap;

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->floorplane, false);

				if(!(fs->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING)) hi.lightlevel = gl_ClampLight(*light->p_lightlevel);
				hi.colormap.LightColor = (light->extra_colormap)->Color;
			}

			if (gl_fixedcolormap) hi.colormap.Clear();
			horizon = &hi;
			PutPortal(PORTALTYPE_HORIZON);
		}
	}
	return true;
}

//==========================================================================
//
// 
//
//==========================================================================
bool GLWall::SetWallCoordinates(seg_t * seg, FTexCoordInfo *tci, float texturetop,
	float topleft, float topright, float bottomleft, float bottomright, float t_ofs)
{
	//
	//
	// set up texture coordinate stuff
	//
	// 
	float l_ul;
	float texlength;

	if (gltexture)
	{
		float length = seg->sidedef ? seg->sidedef->TexelLength : Dist2(glseg.x1, glseg.y1, glseg.x2, glseg.y2);

		l_ul = tci->FloatToTexU(tci->TextureOffset(t_ofs));
		texlength = tci->FloatToTexU(length);
	}
	else
	{
		tci = NULL;
		l_ul = 0;
		texlength = 0;
	}


	//
	//
	// set up coordinates for the left side of the polygon
	//
	// check left side for intersections
	if (topleft >= bottomleft)
	{
		// normal case
		ztop[0] = topleft;
		zbottom[0] = bottomleft;

		if (tci)
		{
			tcs[UPLFT].v = tci->FloatToTexV(-ztop[0] + texturetop);
			tcs[LOLFT].v = tci->FloatToTexV(-zbottom[0] + texturetop);
		}
	}
	else
	{
		// ceiling below floor - clip to the visible part of the wall
		float dch = topright - topleft;
		float dfh = bottomright - bottomleft;
		float inter_x = (bottomleft - topleft) / (dch - dfh);

		float inter_y = topleft + inter_x * dch;

		glseg.x1 = glseg.x1 + inter_x * (glseg.x2 - glseg.x1);
		glseg.y1 = glseg.y1 + inter_x * (glseg.y2 - glseg.y1);
		glseg.fracleft = inter_x;

		zbottom[0] = ztop[0] = inter_y;

		if (tci)
		{
			tcs[LOLFT].v = tcs[UPLFT].v = tci->FloatToTexV(-ztop[0] + texturetop);
		}
	}

	//
	//
	// set up coordinates for the right side of the polygon
	//
	// check left side for intersections
	if (topright >= bottomright)
	{
		// normal case
		ztop[1] = topright;
		zbottom[1] = bottomright;

		if (tci)
		{
			tcs[UPRGT].v = tci->FloatToTexV(-ztop[1] + texturetop);
			tcs[LORGT].v = tci->FloatToTexV(-zbottom[1] + texturetop);
		}
	}
	else
	{
		// ceiling below floor - clip to the visible part of the wall
		float dch = topright - topleft;
		float dfh = bottomright - bottomleft;
		float inter_x = (bottomleft - topleft) / (dch - dfh);

		float inter_y = topleft + inter_x * dch;

		glseg.x2 = glseg.x1 + inter_x * (glseg.x2 - glseg.x1);
		glseg.y2 = glseg.y1 + inter_x * (glseg.y2 - glseg.y1);
		glseg.fracright = inter_x;

		zbottom[1] = ztop[1] = inter_y;
		if (tci)
		{
			tcs[LORGT].v = tcs[UPRGT].v = tci->FloatToTexV(-ztop[1] + texturetop);
		}
	}

	tcs[UPLFT].u = tcs[LOLFT].u = l_ul + texlength * glseg.fracleft;
	tcs[UPRGT].u = tcs[LORGT].u = l_ul + texlength * glseg.fracright;

	if (gltexture != NULL)
	{
		bool normalize = false;
		if (gltexture->tex->bHasCanvas) normalize = true;
		else if (flags & GLT_CLAMPY)
		{
			// for negative scales we can get negative coordinates here.
			normalize = (tcs[UPLFT].v > tcs[LOLFT].v || tcs[UPRGT].v > tcs[LORGT].v);
		}
		if (normalize)
		{
			// we have to shift the y-coordinate from [-1..0] to [0..1] when using texture clamping with a negative scale
			tcs[UPLFT].v += 1.f;
			tcs[UPRGT].v += 1.f;
			tcs[LOLFT].v += 1.f;
			tcs[LORGT].v += 1.f;
		}
	}

	return true;
}

//==========================================================================
//
// Do some tweaks with the texture coordinates to reduce visual glitches
//
//==========================================================================

void GLWall::CheckTexturePosition()
{
	float sub;

	if (gltexture->tex->bHasCanvas) return;

	// clamp texture coordinates to a reasonable range.
	// Extremely large values can cause visual problems
	if (tcs[UPLFT].v < tcs[UPRGT].v)
	{
		sub = float(xs_FloorToInt(tcs[UPLFT].v));
	}
	else
	{
		sub = float(xs_FloorToInt(tcs[UPRGT].v));
	}
	tcs[UPLFT].v -= sub;
	tcs[UPRGT].v -= sub;
	tcs[LOLFT].v -= sub;
	tcs[LORGT].v -= sub;

	if ((tcs[UPLFT].v == 0.f && tcs[UPRGT].v == 0.f && tcs[LOLFT].v <= 1.f && tcs[LORGT].v <= 1.f) ||
		(tcs[UPLFT].v >= 0.f && tcs[UPRGT].v >= 0.f && tcs[LOLFT].v == 1.f && tcs[LORGT].v == 1.f))
	{
		flags |= GLT_CLAMPY;
	}

	// Check if this is marked as a skybox and if so, do the same for x.
	// This intentionally only tests the seg's frontsector.
	if (seg->frontsector->special == GLSector_Skybox)
	{
		sub = (float)xs_FloorToInt(tcs[UPLFT].u);
		tcs[UPLFT].u -= sub;
		tcs[UPRGT].u -= sub;
		tcs[LOLFT].u -= sub;
		tcs[LORGT].u -= sub;
		if ((tcs[UPLFT].u == 0.f && tcs[LOLFT].u == 0.f && tcs[UPRGT].u <= 1.f && tcs[LORGT].u <= 1.f) ||
			(tcs[UPLFT].u >= 0.f && tcs[LOLFT].u >= 0.f && tcs[UPRGT].u == 1.f && tcs[LORGT].u == 1.f))
		{
			flags |= GLT_CLAMPX;
		}
	}
}

//==========================================================================
//
//  Handle one sided walls, upper and lower texture
//
//==========================================================================
void GLWall::DoTexture(int _type,seg_t * seg, int peg,
					   float ceilingrefheight,float floorrefheight,
					   float topleft,float topright,
					   float bottomleft,float bottomright,
					   float v_offset)
{
	if (topleft<=bottomleft && topright<=bottomright) return;

	// The Vertex values can be destroyed in this function and must be restored aferward!
	GLSeg glsave=glseg;
	float flh=ceilingrefheight-floorrefheight;
	int texpos;
	BYTE savedflags = flags;

	switch (_type)
	{
	case RENDERWALL_TOP:
		texpos = side_t::top;
		break;
	case RENDERWALL_BOTTOM:
		texpos = side_t::bottom;
		break;
	default:
		texpos = side_t::mid;
		break;
	}

	FTexCoordInfo tci;

	gltexture->GetTexCoordInfo(&tci, seg->sidedef, texpos);

	type = _type;

	float floatceilingref = ceilingrefheight + tci.RowOffset(seg->sidedef->GetTextureYOffset(texpos));
	if (peg) floatceilingref += tci.mRenderHeight - flh - v_offset;

	if (!SetWallCoordinates(seg, &tci, floatceilingref, topleft, topright, bottomleft, bottomright, 
							seg->sidedef->GetTextureXOffset(texpos))) return;

	if (seg->linedef->special == Line_Mirror && _type == RENDERWALL_M1S && gl_mirrors)
	{
		PutPortal(PORTALTYPE_MIRROR);
	}
	else
	{

		CheckTexturePosition();

		// Add this wall to the render list
		sector_t * sec = sub ? sub->sector : seg->frontsector;
		if (sec->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) PutWall(false);
		else SplitWall(sec, false);
	}

	glseg = glsave;
	flags = savedflags;
}


//==========================================================================
//
// 
//
//==========================================================================

void GLWall::DoMidTexture(seg_t * seg, bool drawfogboundary, 
						  sector_t * front, sector_t * back,
						  sector_t * realfront, sector_t * realback,
						  float fch1, float fch2, float ffh1, float ffh2,
						  float bch1, float bch2, float bfh1, float bfh2)
								
{
	FTexCoordInfo tci;
	float topleft,bottomleft,topright,bottomright;
	GLSeg glsave=glseg;
	float texturetop, texturebottom;
	bool wrap = (seg->linedef->flags&ML_WRAP_MIDTEX) || (seg->sidedef->Flags&WALLF_WRAP_MIDTEX);
	bool mirrory = false;

	//
	//
	// Get the base coordinates for the texture
	//
	//
	if (gltexture)
	{
		// Align the texture to the ORIGINAL sector's height!!
		// At this point slopes don't matter because they don't affect the texture's z-position

		gltexture->GetTexCoordInfo(&tci, seg->sidedef, side_t::mid);
		if (tci.mRenderHeight < 0)
		{
			mirrory = true;
			tci.mRenderHeight = -tci.mRenderHeight;
			tci.mScale.Y = -tci.mScale.Y;
		}
		float rowoffset = tci.RowOffset(seg->sidedef->GetTextureYOffset(side_t::mid));
		if ((seg->linedef->flags & ML_DONTPEGBOTTOM) >0)
		{
			texturebottom = MAX(realfront->GetPlaneTexZ(sector_t::floor), realback->GetPlaneTexZ(sector_t::floor)) + rowoffset;
			texturetop = texturebottom + tci.mRenderHeight;
		}
		else
		{
			texturetop = MIN(realfront->GetPlaneTexZ(sector_t::ceiling), realback->GetPlaneTexZ(sector_t::ceiling)) + rowoffset;
			texturebottom = texturetop - tci.mRenderHeight;
		}
	}
	else texturetop=texturebottom=0;

	//
	//
	// Depending on missing textures and possible plane intersections
	// decide which planes to use for the polygon
	//
	//
	if (realfront!=realback || drawfogboundary || wrap || realfront->GetHeightSec())
	{
		//
		//
		// Set up the top
		//
		//
		FTexture * tex = TexMan(seg->sidedef->GetTexture(side_t::top));
		if (!tex || tex->UseType==FTexture::TEX_Null)
		{
			if (front->GetTexture(sector_t::ceiling) == skyflatnum &&
				back->GetTexture(sector_t::ceiling) == skyflatnum)
			{
				// intra-sky lines do not clip the texture at all if there's no upper texture
				topleft = topright = texturetop;
			}
			else
			{
				// texture is missing - use the higher plane
				topleft = MAX(bch1,fch1);
				topright = MAX(bch2,fch2);
			}
		}
		else if ((bch1>fch1 || bch2>fch2) && 
				 (seg->frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || seg->backsector->GetTexture(sector_t::ceiling)==skyflatnum)) 
				 // (!((bch1<=fch1 && bch2<=fch2) || (bch1>=fch1 && bch2>=fch2)))
		{
			// Use the higher plane and let the geometry clip the extruding part
			topleft = bch1;
			topright = bch2;
		}
		else
		{
			// But not if there can be visual artifacts.
			topleft = MIN(bch1,fch1);
			topright = MIN(bch2,fch2);
		}
		
		//
		//
		// Set up the bottom
		//
		//
		tex = TexMan(seg->sidedef->GetTexture(side_t::bottom));
		if (!tex || tex->UseType==FTexture::TEX_Null)
		{
			// texture is missing - use the lower plane
			bottomleft = MIN(bfh1,ffh1);
			bottomright = MIN(bfh2,ffh2);
		}
		else if (bfh1<ffh1 || bfh2<ffh2) // (!((bfh1<=ffh1 && bfh2<=ffh2) || (bfh1>=ffh1 && bfh2>=ffh2)))
		{
			// the floor planes intersect. Use the backsector's floor for drawing so that
			// drawing the front sector's plane clips the polygon automatically.
			bottomleft = bfh1;
			bottomright = bfh2;
		}
		else
		{
			// normal case - use the higher plane
			bottomleft = MAX(bfh1,ffh1);
			bottomright = MAX(bfh2,ffh2);
		}
		
		//
		//
		// if we don't need a fog sheet let's clip away some unnecessary parts of the polygon
		//
		//
		if (!drawfogboundary && !wrap)
		{
			if (texturetop<topleft && texturetop<topright) topleft=topright=texturetop;
			if (texturebottom>bottomleft && texturebottom>bottomright) bottomleft=bottomright=texturebottom;
		}
	}
	else
	{
		//
		//
		// if both sides of the line are in the same sector and the sector
		// doesn't have a Transfer_Heights special don't clip to the planes
		// Clipping to the planes is not necessary and can even produce
		// unwanted side effects.
		//
		//
		topleft=topright=texturetop;
		bottomleft=bottomright=texturebottom;
	}

	// nothing visible - skip the rest
	if (topleft<=bottomleft && topright<=bottomright) return;


	//
	//
	// set up texture coordinate stuff
	//
	// 
	float t_ofs = seg->sidedef->GetTextureXOffset(side_t::mid);

	if (gltexture)
	{
		// First adjust the texture offset so that the left edge of the linedef is inside the range [0..1].
		float texwidth = tci.TextureAdjustWidth();

		if (t_ofs >= 0)
		{
			float div = t_ofs / texwidth;
			t_ofs = (div - xs_FloorToInt(div)) * texwidth;
		}
		else
		{
			float div = (-t_ofs) / texwidth;
			t_ofs = texwidth - (div - xs_FloorToInt(div)) * texwidth;
		}


		// Now check whether the linedef is completely within the texture range of [0..1].
		// If so we should use horizontal texture clamping to prevent filtering artifacts
		// at the edges.

		float textureoffset = tci.TextureOffset(t_ofs);
		int righttex = int(textureoffset) + seg->sidedef->TexelLength;
		
		if ((textureoffset == 0 && righttex <= tci.mRenderWidth) ||
			(textureoffset >= 0 && righttex == tci.mRenderWidth))
		{
			flags |= GLT_CLAMPX;
		}
		else
		{
			flags &= ~GLT_CLAMPX;
		}
		if (!wrap)
		{
			flags |= GLT_CLAMPY;
		}
	}
	if (mirrory)
	{
		tci.mRenderHeight = -tci.mRenderHeight;
		tci.mScale.Y = -tci.mScale.Y;
	}
	SetWallCoordinates(seg, &tci, texturetop, topleft, topright, bottomleft, bottomright, t_ofs);

	//
	//
	// draw fog sheet if required
	//
	// 
	if (drawfogboundary)
	{
		flags |= GLWF_NOSPLITUPPER|GLWF_NOSPLITLOWER;
		type=RENDERWALL_FOGBOUNDARY;
		FMaterial *savetex = gltexture;
		gltexture = NULL;
		PutWall(true);
		if (!savetex) 
		{
			flags &= ~(GLWF_NOSPLITUPPER|GLWF_NOSPLITLOWER);
			return;
		}
		gltexture = savetex;
		type=RENDERWALL_M2SNF;
	}
	else type=RENDERWALL_M2S;

	//
	//
	// set up alpha blending
	//
	// 
	if (seg->linedef->alpha != 0)
	{
		bool translucent = false;

		switch (seg->linedef->flags& ML_ADDTRANS)//TRANSBITS)
		{
		case 0:
			RenderStyle=STYLE_Translucent;
			alpha = seg->linedef->alpha;
			translucent =alpha < 1. || (gltexture && gltexture->GetTransparent());
			break;

		case ML_ADDTRANS:
			RenderStyle=STYLE_Add;
			alpha = seg->linedef->alpha;
			translucent=true;
			break;
		}

		//
		//
		// for textures with large empty areas only the visible parts are drawn.
		// If these textures come too close to the camera they severely affect performance
		// if stacked closely together
		// Recognizing vertical gaps is rather simple and well worth the effort.
		//
		//
		FloatRect *splitrect;
		int v = gltexture->GetAreas(&splitrect);
		if (seg->frontsector == seg->backsector) flags |= GLWF_NOSPLIT;	// we don't need to do vertex splits if a line has both sides in the same sector
		if (v>0 && !drawfogboundary && !(seg->linedef->flags&ML_WRAP_MIDTEX))
		{
			// split the poly!
			GLWall split;
			int i,t=0;
			float v_factor=(zbottom[0]-ztop[0])/(tcs[LOLFT].v-tcs[UPLFT].v);
			// only split the vertical area of the polygon that does not contain slopes.
			float splittopv = MAX(tcs[UPLFT].v, tcs[UPRGT].v);
			float splitbotv = MIN(tcs[LOLFT].v, tcs[LORGT].v);

			// this is split vertically into sections.
			for(i=0;i<v;i++)
			{
				// the current segment is below the bottom line of the splittable area
				// (IOW. the whole wall has been done)
				if (splitrect[i].top>=splitbotv) break;

				float splitbot=splitrect[i].top+splitrect[i].height;

				// the current segment is above the top line of the splittable area
				if (splitbot<=splittopv) continue;

				split=*this;

				// the top line of the current segment is inside the splittable area
				// use the splitrect's top as top of this segment
				// if not use the top of the remaining polygon
				if (splitrect[i].top>splittopv)
				{
					split.ztop[0]=split.ztop[1]= ztop[0]+v_factor*(splitrect[i].top-tcs[UPLFT].v);
					split.tcs[UPLFT].v=split.tcs[UPRGT].v=splitrect[i].top;
				}
				// the bottom line of the current segment is inside the splittable area
				// use the splitrect's bottom as bottom of this segment
				// if not use the bottom of the remaining polygon
				if (splitbot<=splitbotv)
				{
					split.zbottom[0]=split.zbottom[1]=ztop[0]+v_factor*(splitbot-tcs[UPLFT].v);
					split.tcs[LOLFT].v=split.tcs[LORGT].v=splitbot;
				}
				//
				//
				// Draw the stuff
				//
				//
				if (realfront->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) split.PutWall(translucent);
				else split.SplitWall(realfront, translucent);

				t=1;
			}
			render_texsplit+=t;
		}
		else
		{
			//
			//
			// Draw the stuff without splitting
			//
			//
			if (realfront->e->XFloor.lightlist.Size()==0 || gl_fixedcolormap) PutWall(translucent);
			else SplitWall(realfront, translucent);
		}
		alpha=1.0f;
	}
	// restore some values that have been altered in this function
	glseg=glsave;
	flags&=~(GLT_CLAMPX|GLT_CLAMPY|GLWF_NOSPLITUPPER|GLWF_NOSPLITLOWER);
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::BuildFFBlock(seg_t * seg, F3DFloor * rover,
	float ff_topleft, float ff_topright,
	float ff_bottomleft, float ff_bottomright)
{
	side_t * mastersd = rover->master->sidedef[0];
	float to;
	lightlist_t * light;
	bool translucent;
	int savelight = lightlevel;
	FColormap savecolor = Colormap;
	float ul;
	float texlength;
	FTexCoordInfo tci;

	if (rover->flags&FF_FOG)
	{
		if (!gl_fixedcolormap)
		{
			// this may not yet be done
			light = P_GetPlaneLight(rover->target, rover->top.plane, true);
			Colormap.Clear();
			Colormap.LightColor = (light->extra_colormap)->Fade;
			// the fog plane defines the light level, not the front sector
			lightlevel = gl_ClampLight(*light->p_lightlevel);
			gltexture = NULL;
			type = RENDERWALL_FFBLOCK;
		}
		else return;
	}
	else
	{

		if (rover->flags&FF_UPPERTEXTURE)
		{
			gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::top), false, true);
			if (!gltexture) return;
			gltexture->GetTexCoordInfo(&tci, seg->sidedef, side_t::top);
		}
		else if (rover->flags&FF_LOWERTEXTURE)
		{
			gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::bottom), false, true);
			if (!gltexture) return;
			gltexture->GetTexCoordInfo(&tci, seg->sidedef, side_t::bottom);
		}
		else
		{
			gltexture = FMaterial::ValidateTexture(mastersd->GetTexture(side_t::mid), false, true);
			if (!gltexture) return;
			gltexture->GetTexCoordInfo(&tci, mastersd, side_t::mid);
		}

		to = (rover->flags&(FF_UPPERTEXTURE | FF_LOWERTEXTURE)) ? 0 : tci.TextureOffset(mastersd->GetTextureXOffset(side_t::mid));

		ul = tci.FloatToTexU(to + tci.TextureOffset(seg->sidedef->GetTextureXOffset(side_t::mid)));

		texlength = tci.FloatToTexU(seg->sidedef->TexelLength);

		tcs[UPLFT].u = tcs[LOLFT].u = ul + texlength * glseg.fracleft;
		tcs[UPRGT].u = tcs[LORGT].u = ul + texlength * glseg.fracright;

		float rowoffset = tci.RowOffset(seg->sidedef->GetTextureYOffset(side_t::mid));
		to = (rover->flags&(FF_UPPERTEXTURE | FF_LOWERTEXTURE)) ?
			0.f : tci.RowOffset(mastersd->GetTextureYOffset(side_t::mid));

		to += rowoffset + rover->top.model->GetPlaneTexZ(rover->top.isceiling);

		tcs[UPLFT].v = tci.FloatToTexV(to - ff_topleft);
		tcs[UPRGT].v = tci.FloatToTexV(to - ff_topright);
		tcs[LOLFT].v = tci.FloatToTexV(to - ff_bottomleft);
		tcs[LORGT].v = tci.FloatToTexV(to - ff_bottomright);
		type = RENDERWALL_FFBLOCK;
		CheckTexturePosition();
	}

	ztop[0] = ff_topleft;
	ztop[1] = ff_topright;
	zbottom[0] = ff_bottomleft;//-0.001f;
	zbottom[1] = ff_bottomright;

	if (rover->flags&(FF_TRANSLUCENT | FF_ADDITIVETRANS | FF_FOG))
	{
		alpha = rover->alpha / 255.0f;
		RenderStyle = (rover->flags&FF_ADDITIVETRANS) ? STYLE_Add : STYLE_Translucent;
		translucent = true;
		type = gltexture ? RENDERWALL_M2S : RENDERWALL_COLOR;
	}
	else
	{
		alpha = 1.0f;
		RenderStyle = STYLE_Normal;
		translucent = false;
	}

	sector_t * sec = sub ? sub->sector : seg->frontsector;

	if (sec->e->XFloor.lightlist.Size() == 0 || gl_fixedcolormap) PutWall(translucent);
	else SplitWall(sec, translucent);

	alpha = 1.0f;
	lightlevel = savelight;
	Colormap = savecolor;
	flags &= ~GLT_CLAMPY;
}


//==========================================================================
//
// 
//
//==========================================================================

__forceinline void GLWall::GetPlanePos(F3DFloor::planeref *planeref, float &left, float &right)
{
	left=planeref->plane->ZatPoint(vertexes[0]);
	right=planeref->plane->ZatPoint(vertexes[1]);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::InverseFloors(seg_t * seg, sector_t * frontsector,
	float topleft, float topright,
	float bottomleft, float bottomright)
{
	TArray<F3DFloor *> & frontffloors = frontsector->e->XFloor.ffloors;

	for (unsigned int i = 0; i < frontffloors.Size(); i++)
	{
		F3DFloor * rover = frontffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES)) continue;
		if (!(rover->flags&(FF_INVERTSIDES | FF_ALLSIDES))) continue;

		float ff_topleft;
		float ff_topright;
		float ff_bottomleft;
		float ff_bottomright;

		GetPlanePos(&rover->top, ff_topleft, ff_topright);
		GetPlanePos(&rover->bottom, ff_bottomleft, ff_bottomright);

		// above ceiling
		if (ff_bottomleft > topleft && ff_bottomright > topright) continue;

		if (ff_topleft > topleft && ff_topright > topright)
		{
			// the new section overlaps with the previous one - clip it!
			ff_topleft = topleft;
			ff_topright = topright;
		}
		if (ff_bottomleft < bottomleft && ff_bottomright < bottomright)
		{
			ff_bottomleft = bottomleft;
			ff_bottomright = bottomright;
		}
		if (ff_topleft < ff_bottomleft || ff_topright < ff_bottomright) continue;

		BuildFFBlock(seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
		topleft = ff_bottomleft;
		topright = ff_bottomright;

		if (topleft <= bottomleft && topright <= bottomright) return;
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::ClipFFloors(seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
	float topleft, float topright,
	float bottomleft, float bottomright)
{
	TArray<F3DFloor *> & frontffloors = frontsector->e->XFloor.ffloors;

	int flags = ffloor->flags & (FF_SWIMMABLE | FF_TRANSLUCENT);

	for (unsigned int i = 0; i < frontffloors.Size(); i++)
	{
		F3DFloor * rover = frontffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES)) continue;
		if ((rover->flags&(FF_SWIMMABLE | FF_TRANSLUCENT)) != flags) continue;

		float ff_topleft;
		float ff_topright;
		float ff_bottomleft;
		float ff_bottomright;

		GetPlanePos(&rover->top, ff_topleft, ff_topright);

		// we are completely below the bottom so unless there are some
		// (unsupported) intersections there won't be any more floors that
		// could clip this one.
		if (ff_topleft < bottomleft && ff_topright < bottomright) goto done;

		GetPlanePos(&rover->bottom, ff_bottomleft, ff_bottomright);
		// above top line?
		if (ff_bottomleft > topleft && ff_bottomright > topright) continue;

		// overlapping the top line
		if (ff_topleft >= topleft && ff_topright >= topright)
		{
			// overlapping with the entire range
			if (ff_bottomleft <= bottomleft && ff_bottomright <= bottomright) return;
			else if (ff_bottomleft > bottomleft && ff_bottomright > bottomright)
			{
				topleft = ff_bottomleft;
				topright = ff_bottomright;
			}
			else
			{
				// an intersecting case.
				// proper handling requires splitting but
				// I don't need this right now.
			}
		}
		else if (ff_topleft <= topleft && ff_topright <= topright)
		{
			BuildFFBlock(seg, ffloor, topleft, topright, ff_topleft, ff_topright);
			if (ff_bottomleft <= bottomleft && ff_bottomright <= bottomright) return;
			topleft = ff_bottomleft;
			topright = ff_bottomright;
		}
		else
		{
			// an intersecting case.
			// proper handling requires splitting but
			// I don't need this right now.
		}
	}

done:
	// if the program reaches here there is one block left to draw
	BuildFFBlock(seg, ffloor, topleft, topright, bottomleft, bottomright);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DoFFloorBlocks(seg_t * seg, sector_t * frontsector, sector_t * backsector,
	float fch1, float fch2, float ffh1, float ffh2,
	float bch1, float bch2, float bfh1, float bfh2)

{
	TArray<F3DFloor *> & backffloors = backsector->e->XFloor.ffloors;
	float topleft, topright, bottomleft, bottomright;
	bool renderedsomething = false;

	// if the ceilings intersect use the backsector's height because this sector's ceiling will
	// obstruct the redundant parts.

	if (fch1 < bch1 && fch2 < bch2)
	{
		topleft = fch1;
		topright = fch2;
	}
	else
	{
		topleft = bch1;
		topright = bch2;
	}

	if (ffh1 > bfh1 && ffh2 > bfh2)
	{
		bottomleft = ffh1;
		bottomright = ffh2;
	}
	else
	{
		bottomleft = bfh1;
		bottomright = bfh2;
	}

	for (unsigned int i = 0; i < backffloors.Size(); i++)
	{
		F3DFloor * rover = backffloors[i];
		if (!(rover->flags&FF_EXISTS)) continue;
		if (!(rover->flags&FF_RENDERSIDES) || (rover->flags&FF_INVERTSIDES)) continue;

		float ff_topleft;
		float ff_topright;
		float ff_bottomleft;
		float ff_bottomright;

		GetPlanePos(&rover->top, ff_topleft, ff_topright);
		GetPlanePos(&rover->bottom, ff_bottomleft, ff_bottomright);

		// completely above ceiling
		if (ff_bottomleft > topleft && ff_bottomright > topright && !renderedsomething) continue;

		if (ff_topleft > topleft && ff_topright > topright)
		{
			// the new section overlaps with the previous one - clip it!
			ff_topleft = topleft;
			ff_topright = topright;
		}

		// do all inverse floors above the current one it there is a gap between the
		// last 3D floor and this one.
		if (topleft > ff_topleft && topright > ff_topright)
			InverseFloors(seg, frontsector, topleft, topright, ff_topleft, ff_topright);

		// if translucent or liquid clip away adjoining parts of the same type of FFloors on the other side
		if (rover->flags&(FF_SWIMMABLE | FF_TRANSLUCENT))
			ClipFFloors(seg, rover, frontsector, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
		else
			BuildFFBlock(seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);

		topleft = ff_bottomleft;
		topright = ff_bottomright;
		renderedsomething = true;
		if (topleft <= bottomleft && topright <= bottomright) return;
	}

	// draw all inverse floors below the lowest one
	if (frontsector->e->XFloor.ffloors.Size() > 0)
	{
		if (topleft > bottomleft || topright > bottomright)
			InverseFloors(seg, frontsector, topleft, topright, bottomleft, bottomright);
	}
}
	
//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Process(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	vertex_t * v1, *v2;
	float fch1;
	float ffh1;
	float fch2;
	float ffh2;
	float frefz, crefz;
	sector_t * realfront;
	sector_t * realback;
	sector_t * segfront;
	sector_t * segback;

#ifdef _DEBUG
	if (seg->linedef - lines < 4)
	{
		int a = 0;
	}
#endif

	// note: we always have a valid sidedef and linedef reference when getting here.

	this->seg = seg;

	if ((seg->sidedef->Flags & WALLF_POLYOBJ) && seg->backsector)
	{
		// Textures on 2-sided polyobjects are aligned to the actual seg's sectors
		segfront = realfront = seg->frontsector;
		segback = realback = seg->backsector;
	}
	else
	{
		// Need these for aligning the textures
		realfront = &sectors[frontsector->sectornum];
		realback = backsector ? &sectors[backsector->sectornum] : NULL;
		segfront = frontsector;
		segback = backsector;
	}
	frefz = realfront->GetPlaneTexZ(sector_t::floor);
	crefz = realfront->GetPlaneTexZ(sector_t::ceiling);

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1 = seg->linedef->v1;
		v2 = seg->linedef->v2;
	}
	else
	{
		v1 = seg->linedef->v2;
		v2 = seg->linedef->v1;
	}

	if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
	{
		glseg.fracleft = 0;
		glseg.fracright = 1;
		if (gl_seamless)
		{
			if (v1->dirty) gl_RecalcVertexHeights(v1);
			if (v2->dirty) gl_RecalcVertexHeights(v2);
		}
	}
	else	// polyobjects must be rendered per seg.
	{
		if (fabs(v1->fX() - v2->fX()) > fabs(v1->fY() - v2->fY()))
		{
			glseg.fracleft = (seg->v1->fX() - v1->fX()) / (v2->fX() - v1->fX());
			glseg.fracright = (seg->v2->fX() - v1->fX()) / float(v2->fX() - v1->fX());
		}
		else
		{
			glseg.fracleft = (seg->v1->fY() - v1->fY()) / (v2->fY() - v1->fY());
			glseg.fracright = (seg->v2->fY() - v1->fY()) / (v2->fY() - v1->fY());
		}
		v1 = seg->v1;
		v2 = seg->v2;
	}


	vertexes[0] = v1;
	vertexes[1] = v2;

	glseg.x1 = v1->fX();
	glseg.y1 = v1->fY();
	glseg.x2 = v2->fX();
	glseg.y2 = v2->fY();
	Colormap = frontsector->ColorMap;
	flags = 0;
	dynlightindex = -1;
	lightlist = NULL;

	int rel = 0;
	int orglightlevel = gl_ClampLight(frontsector->lightlevel);
	bool foggy = (!gl_isBlack(Colormap.FadeColor) || level.flags&LEVEL_HASFADETABLE);	// fog disables fake contrast
	lightlevel = gl_ClampLight(seg->sidedef->GetLightLevel(foggy, orglightlevel, false, &rel));
	if (orglightlevel >= 253)			// with the software renderer fake contrast won't be visible above this.
	{
		rellight = 0;
	}
	else if (lightlevel - rel > 256)	// the brighter part of fake contrast will be clamped so also clamp the darker part by the same amount for better looks
	{
		rellight = 256 - lightlevel + rel;
	}
	else
	{
		rellight = rel;
	}

	alpha = 1.0f;
	RenderStyle = STYLE_Normal;
	gltexture = NULL;

	topflat = frontsector->GetTexture(sector_t::ceiling);	// for glowing textures. These must be saved because
	bottomflat = frontsector->GetTexture(sector_t::floor);	// the sector passed here might be a temporary copy.
	topplane = frontsector->ceilingplane;
	bottomplane = frontsector->floorplane;

	zfloor[0] = ffh1 = segfront->floorplane.ZatPoint(v1);
	zfloor[1] = ffh2 = segfront->floorplane.ZatPoint(v2);
	zceil[0] = fch1 = segfront->ceilingplane.ZatPoint(v1);
	zceil[1] = fch2 = segfront->ceilingplane.ZatPoint(v2);

	if (seg->linedef->special == Line_Horizon)
	{
		SkyNormal(frontsector, v1, v2);
		DoHorizon(seg, frontsector, v1, v2);
		return;
	}

	//return;
	// [GZ] 3D middle textures are necessarily two-sided, even if they lack the explicit two-sided flag
	if (!backsector || !(seg->linedef->flags&(ML_TWOSIDED | ML_3DMIDTEX))) // one sided
	{
		// sector's sky
		SkyNormal(frontsector, v1, v2);

		if (seg->linedef->isVisualPortal())
		{
			lineportal = linePortalToGL[seg->linedef->portalindex];
			ztop[0] = zceil[0];
			ztop[1] = zceil[1];
			zbottom[0] = zfloor[0];
			zbottom[1] = zfloor[1];
			PutPortal(PORTALTYPE_LINETOLINE);
		}
		else if (seg->linedef->portaltransferred > 0)
		{
			SkyLine(frontsector, seg->linedef);
		}
		else
		{
			// normal texture
			gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::mid), false, true);
			if (gltexture)
			{
				DoTexture(RENDERWALL_M1S, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
					crefz, frefz,	// must come from the original!
					fch1, fch2, ffh1, ffh2, 0);
			}
		}
	}
	else // two sided
	{
		float bfh1 = segback->floorplane.ZatPoint(v1);
		float bfh2 = segback->floorplane.ZatPoint(v2);
		float bch1 = segback->ceilingplane.ZatPoint(v1);
		float bch2 = segback->ceilingplane.ZatPoint(v2);

		SkyTop(seg, frontsector, backsector, v1, v2);
		SkyBottom(seg, frontsector, backsector, v1, v2);

		// upper texture
		if (frontsector->GetTexture(sector_t::ceiling) != skyflatnum || backsector->GetTexture(sector_t::ceiling) != skyflatnum)
		{
			float bch1a = bch1, bch2a = bch2;
			if (frontsector->GetTexture(sector_t::floor) != skyflatnum || backsector->GetTexture(sector_t::floor) != skyflatnum)
			{
				// the back sector's floor obstructs part of this wall				
				if (ffh1 > bch1 && ffh2 > bch2)
				{
					bch2a = ffh2;
					bch1a = ffh1;
				}
			}

			if (bch1a < fch1 || bch2a < fch2)
			{
				gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::top), false, true);
				if (gltexture)
				{
					DoTexture(RENDERWALL_TOP, seg, (seg->linedef->flags & (ML_DONTPEGTOP)) == 0,
						crefz, realback->GetPlaneTexZ(sector_t::ceiling),
						fch1, fch2, bch1a, bch2a, 0);
				}
				else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
				{
					if ((frontsector->ceilingplane.isSlope() || backsector->ceilingplane.isSlope()) &&
						frontsector->GetTexture(sector_t::ceiling) != skyflatnum &&
						backsector->GetTexture(sector_t::ceiling) != skyflatnum)
					{
						gltexture = FMaterial::ValidateTexture(frontsector->GetTexture(sector_t::ceiling), false, true);
						if (gltexture)
						{
							DoTexture(RENDERWALL_TOP, seg, (seg->linedef->flags & (ML_DONTPEGTOP)) == 0,
								crefz, realback->GetPlaneTexZ(sector_t::ceiling),
								fch1, fch2, bch1a, bch2a, 0);
						}
					}
					else
					{
						// skip processing if the back is a malformed subsector
						if (seg->PartnerSeg != NULL && !(seg->PartnerSeg->Subsector->hacked & 4))
						{
							gl_drawinfo->AddUpperMissingTexture(seg->sidedef, sub, bch1a);
						}
					}
				}
			}
		}


		/* mid texture */
		bool drawfogboundary = gl_CheckFog(frontsector, backsector);
		FTexture *tex = TexMan(seg->sidedef->GetTexture(side_t::mid));
		if (tex != NULL)
		{
			if (i_compatflags & COMPATF_MASKEDMIDTEX)
			{
				tex = tex->GetRawTexture();
			}
			gltexture = FMaterial::ValidateTexture(tex, false);
		}
		else gltexture = NULL;

		if (gltexture || drawfogboundary)
		{
			DoMidTexture(seg, drawfogboundary, frontsector, backsector, realfront, realback,
				fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}

		if (seg->linedef->isVisualPortal() && seg->sidedef == seg->linedef->sidedef[0])
		{
			lineportal = linePortalToGL[seg->linedef->portalindex];
			ztop[0] = bch1;
			ztop[1] = bch2;
			zbottom[0] = bfh1;
			zbottom[1] = bfh2;
			PutPortal(PORTALTYPE_LINETOLINE);
		}
		else if (backsector->e->XFloor.ffloors.Size() || frontsector->e->XFloor.ffloors.Size())
		{
			DoFFloorBlocks(seg, frontsector, backsector, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}

		/* bottom texture */
		// the back sector's ceiling obstructs part of this wall (specially important for sky sectors)
		if (fch1 < bfh1 && fch2 < bfh2)
		{
			bfh1 = fch1;
			bfh2 = fch2;
		}

		if (bfh1 > ffh1 || bfh2 > ffh2)
		{
			gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::bottom), false, true);
			if (gltexture)
			{
				DoTexture(RENDERWALL_BOTTOM, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
					realback->GetPlaneTexZ(sector_t::floor), frefz,
					bfh1, bfh2, ffh1, ffh2,
					frontsector->GetTexture(sector_t::ceiling) == skyflatnum && backsector->GetTexture(sector_t::ceiling) == skyflatnum ?
					frefz - realback->GetPlaneTexZ(sector_t::ceiling) :
					frefz - crefz);
			}
			else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
			{
				if ((frontsector->floorplane.isSlope() || backsector->floorplane.isSlope()) &&
					frontsector->GetTexture(sector_t::floor) != skyflatnum &&
					backsector->GetTexture(sector_t::floor) != skyflatnum)
				{
					// render it anyway with the sector's floor texture. With a background sky
					// there are ugly holes otherwise and slopes are simply not precise enough
					// to mach in any case.
					gltexture = FMaterial::ValidateTexture(frontsector->GetTexture(sector_t::floor), false, true);
					if (gltexture)
					{
						DoTexture(RENDERWALL_BOTTOM, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
							realback->GetPlaneTexZ(sector_t::floor), frefz,
							bfh1, bfh2, ffh1, ffh2, frefz - crefz);
					}
				}
				else if (backsector->GetTexture(sector_t::floor) != skyflatnum)
				{
					// skip processing if the back is a malformed subsector
					if (seg->PartnerSeg != NULL && !(seg->PartnerSeg->Subsector->hacked & 4))
					{
						gl_drawinfo->AddLowerMissingTexture(seg->sidedef, sub, bfh1);
					}
				}
			}
		}
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::ProcessLowerMiniseg(seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	if (frontsector->GetTexture(sector_t::floor) == skyflatnum) return;
	lightlist = NULL;

	float ffh = frontsector->GetPlaneTexZ(sector_t::floor);
	float bfh = backsector->GetPlaneTexZ(sector_t::floor);
	if (bfh > ffh)
	{
		this->seg = seg;
		this->sub = NULL;

		vertex_t * v1 = seg->v1;
		vertex_t * v2 = seg->v2;
		vertexes[0] = v1;
		vertexes[1] = v2;

		glseg.x1 = v1->fX();
		glseg.y1 = v1->fY();
		glseg.x2 = v2->fX();
		glseg.y2 = v2->fY();
		glseg.fracleft = 0;
		glseg.fracright = 1;

		flags = 0;

		// can't do fake contrast without a sidedef
		lightlevel = gl_ClampLight(frontsector->lightlevel);
		rellight = 0;

		alpha = 1.0f;
		RenderStyle = STYLE_Normal;
		Colormap = frontsector->ColorMap;

		topflat = frontsector->GetTexture(sector_t::ceiling);	// for glowing textures
		bottomflat = frontsector->GetTexture(sector_t::floor);
		topplane = frontsector->ceilingplane;
		bottomplane = frontsector->floorplane;
		dynlightindex = -1;

		zfloor[0] = zfloor[1] = ffh;

		gltexture = FMaterial::ValidateTexture(frontsector->GetTexture(sector_t::floor), false, true);

		if (gltexture)
		{
			FTexCoordInfo tci;
			type = RENDERWALL_BOTTOM;
			gltexture->GetTexCoordInfo(&tci, 1.f, 1.f);
			SetWallCoordinates(seg, &tci, bfh, bfh, bfh, ffh, ffh, 0);
			PutWall(false);
		}
	}
}
