// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
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

#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "r_defs.h"
#include "r_sky.h"
#include "r_utility.h"
#include "p_maputl.h"
#include "doomdata.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hw_renderstate.h"
#include "hw_skydome.h"

//==========================================================================
//
// General purpose wall rendering function
// everything goes through here
//
//==========================================================================

void GLWall::RenderWall(HWDrawInfo *di, FRenderState &state, int textured)
{
	assert(vertcount > 0);
	state.SetLightIndex(dynlightindex);
	state.Draw(DT_TriangleFan, vertindex, vertcount);
	vertexcount += vertcount;
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderFogBoundary(HWDrawInfo *di, FRenderState &state)
{
	if (gl_fogmode && !di->isFullbrightScene())
	{
		int rel = rellight + getExtraLight();
		state.EnableDrawBufferAttachments(false);
		state.SetFog(lightlevel, rel, false, &Colormap, false);
		state.SetEffect(EFF_FOGBOUNDARY);
		state.AlphaFunc(Alpha_GEqual, 0.f);
		state.SetDepthBias(-1, -128);
		RenderWall(di, state, GLWall::RWF_BLANK);
		state.ClearDepthBias();
		state.SetEffect(EFF_NONE);
		state.EnableDrawBufferAttachments(true);
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::RenderMirrorSurface(HWDrawInfo *di, FRenderState &state)
{
	if (!TexMan.mirrorTexture.isValid()) return;

	state.SetDepthFunc(DF_LEqual);

	// we use texture coordinates and texture matrix to pass the normal stuff to the shader so that the default vertex buffer format can be used as is.
	state.EnableTextureMatrix(true);

	// Use sphere mapping for this
	state.SetEffect(EFF_SPHEREMAP);
	state.SetColor(lightlevel, 0, di->isFullbrightScene(), Colormap, 0.1f);
	state.SetFog(lightlevel, 0, di->isFullbrightScene(), &Colormap, true);
	state.SetRenderStyle(STYLE_Add);
	state.AlphaFunc(Alpha_Greater, 0);

	FMaterial * pat = FMaterial::ValidateTexture(TexMan.mirrorTexture, false, false);
	state.SetMaterial(pat, CLAMP_NONE, 0, -1);

	flags &= ~GLWall::GLWF_GLOW;
	RenderWall(di, state, GLWall::RWF_BLANK);

	state.EnableTextureMatrix(false);
	state.SetEffect(EFF_NONE);
	state.AlphaFunc(Alpha_GEqual, gl_mask_sprite_threshold);

	state.SetDepthFunc(DF_Less);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here, right after the wall they are on,
	// because the depth buffer won't get set by translucent items.
	if (seg->sidedef->AttachedDecals)
	{
		DrawDecalsForMirror(di, state, di->Decals[1]);
	}
	state.SetRenderStyle(STYLE_Translucent);
}

//==========================================================================
//
// 
//
//==========================================================================

static const uint8_t renderwalltotier[] =
{
	side_t::none,
	side_t::top,
	side_t::mid,
	side_t::mid,
	side_t::bottom,
	side_t::none,
	side_t::none,
	side_t::mid,
	side_t::none,
	side_t::mid,
};

void GLWall::RenderTexturedWall(HWDrawInfo *di, FRenderState &state, int rflags)
{
	int tmode = state.GetTextureMode();
	int rel = rellight + getExtraLight();

	if (flags & GLWall::GLWF_GLOW)
	{
		state.EnableGlow(true);
		state.SetGlowParams(topglowcolor, bottomglowcolor);
		state.SetGlowPlanes(frontsector->ceilingplane, frontsector->floorplane);
	}
	state.SetMaterial(gltexture, flags & 3, 0, -1);

	if (type == RENDERWALL_M2SNF)
	{
		if (flags & GLWall::GLWF_CLAMPY)
		{
			if (tmode == TM_NORMAL) state.SetTextureMode(TM_CLAMPY);
		}
		state.SetFog(255, 0, di->isFullbrightScene(), nullptr, false);
	}
	if (type != RENDERWALL_COLOR && seg->sidedef != nullptr)
	{
		auto side = seg->sidedef;
		auto tierndx = renderwalltotier[type];
		auto &tier = side->textures[tierndx];
		PalEntry color1 = side->GetSpecialColor(tierndx, side_t::walltop, frontsector);
		PalEntry color2 = side->GetSpecialColor(tierndx, side_t::wallbottom, frontsector);
		state.SetObjectColor(color1);
		state.SetObjectColor2(color2);
		state.SetAddColor(side->GetAdditiveColor(tierndx, frontsector));
		if (color1 != color2)
		{
			// Do gradient setup only if there actually is a gradient.

			state.EnableGradient(true);
			if ((tier.flags & side_t::part::ClampGradient) && backsector)
			{
				if (tierndx == side_t::top)
				{
					state.SetGradientPlanes(frontsector->ceilingplane, backsector->ceilingplane);
				}
				else if (tierndx == side_t::mid)
				{
					state.SetGradientPlanes(backsector->ceilingplane, backsector->floorplane);
				}
				else // side_t::bottom:
				{
					state.SetGradientPlanes(backsector->floorplane, frontsector->floorplane);
				}
			}
			else
			{
				state.SetGradientPlanes(frontsector->ceilingplane, frontsector->floorplane);
			}
		}
	}

	float absalpha = fabsf(alpha);
	if (lightlist == nullptr)
	{
		if (type != RENDERWALL_M2SNF) state.SetFog(lightlevel, rel, di->isFullbrightScene(), &Colormap, RenderStyle == STYLE_Add);
		state.SetColor(lightlevel, rel, di->isFullbrightScene(), Colormap, absalpha);
		RenderWall(di, state, rflags);
	}
	else
	{
		state.EnableSplit(true);

		for (unsigned i = 0; i < lightlist->Size(); i++)
		{
			secplane_t &lowplane = i == (*lightlist).Size() - 1 ? frontsector->floorplane : (*lightlist)[i + 1].plane;
			// this must use the exact same calculation method as GLWall::Process etc.
			float low1 = lowplane.ZatPoint(vertexes[0]);
			float low2 = lowplane.ZatPoint(vertexes[1]);

			if (low1 < ztop[0] || low2 < ztop[1])
			{
				int thisll = (*lightlist)[i].caster != nullptr ? hw_ClampLight(*(*lightlist)[i].p_lightlevel) : lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = Colormap.FadeColor;
				thiscm.FogDensity = Colormap.FogDensity;
				thiscm.CopyFrom3DLight(&(*lightlist)[i]);
				state.SetColor(thisll, rel, false, thiscm, absalpha);
				if (type != RENDERWALL_M2SNF) state.SetFog(thisll, rel, false, &thiscm, RenderStyle == STYLE_Add);
				state.SetSplitPlanes((*lightlist)[i].plane, lowplane);
				RenderWall(di, state, rflags);
			}
			if (low1 <= zbottom[0] && low2 <= zbottom[1]) break;
		}

		state.EnableSplit(false);
	}
	state.SetObjectColor(0xffffffff);
	state.SetObjectColor2(0);
	state.SetAddColor(0);
	state.SetTextureMode(tmode);
	state.EnableGlow(false);
	state.EnableGradient(false);
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTranslucentWall(HWDrawInfo *di, FRenderState &state)
{
	state.SetRenderStyle(RenderStyle);
	if (gltexture)
	{
		if (!gltexture->tex->GetTranslucency()) state.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
		else state.AlphaFunc(Alpha_GEqual, 0.f);
		RenderTexturedWall(di, state, GLWall::RWF_TEXTURED | GLWall::RWF_NOSPLIT);
	}
	else
	{
		state.AlphaFunc(Alpha_GEqual, 0.f);
		state.SetColor(lightlevel, 0, false, Colormap, fabsf(alpha));
		state.SetFog(lightlevel, 0, false, &Colormap, RenderStyle == STYLE_Add);
		state.EnableTexture(false);
		RenderWall(di, state, GLWall::RWF_NOSPLIT);
		state.EnableTexture(true);
	}
	state.SetRenderStyle(STYLE_Translucent);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DrawWall(HWDrawInfo *di, FRenderState &state, bool translucent)
{
	if (screen->BuffersArePersistent())
	{
		if (level.HasDynamicLights && !di->isFullbrightScene() && gltexture != nullptr)
		{
			SetupLights(di, lightdata);
		}
		MakeVertices(di, !!(flags & GLWall::GLWF_TRANSLUCENT));
	}

	state.SetNormal(glseg.Normal());
	if (!translucent)
	{
		RenderTexturedWall(di, state, GLWall::RWF_TEXTURED);
	}
	else
	{
		switch (type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface(di, state);
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary(di, state);
			break;

		default:
			RenderTranslucentWall(di, state);
			break;
		}
	}
}

//==========================================================================
//
// Collect lights for shader
//
//==========================================================================

void GLWall::SetupLights(HWDrawInfo *di, FDynLightData &lightdata)
{
	lightdata.Clear();

	if (RenderStyle == STYLE_Add && !level.lightadditivesurfaces) return;	// no lights on additively blended surfaces.

	// check for wall types which cannot have dynamic lights on them (portal types never get here so they don't need to be checked.)
	switch (type)
	{
	case RENDERWALL_FOGBOUNDARY:
	case RENDERWALL_MIRRORSURFACE:
	case RENDERWALL_COLOR:
		return;
	}

	float vtx[]={glseg.x1,zbottom[0],glseg.y1, glseg.x1,ztop[0],glseg.y1, glseg.x2,ztop[1],glseg.y2, glseg.x2,zbottom[1],glseg.y2};
	Plane p;


	auto normal = glseg.Normal();
	p.Set(normal, -normal.X * glseg.x1 - normal.Z * glseg.y1);

	FLightNode *node;
	if (seg->sidedef == NULL)
	{
		node = NULL;
	}
	else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
	{
		node = seg->sidedef->lighthead;
	}
	else if (sub)
	{
		// Polobject segs cannot be checked per sidedef so use the subsector instead.
		node = sub->section->lighthead;
	}
	else node = NULL;

	// Iterate through all dynamic lights which touch this wall and render them
	while (node)
	{
		if (!(node->lightsource->flags2&MF2_DORMANT))
		{
			iter_dlight++;

			DVector3 posrel = node->lightsource->PosRelative(seg->frontsector);
			float x = posrel.X;
			float y = posrel.Y;
			float z = posrel.Z;
			float dist = fabsf(p.DistToPoint(x, z, y));
			float radius = node->lightsource->GetRadius();
			float scale = 1.0f / ((2.f * radius) - dist);
			FVector3 fn, pos;

			if (radius > 0.f && dist < radius)
			{
				FVector3 nearPt, up, right;

				pos = { x, z, y };
				fn = p.Normal();

				fn.GetRightUp(right, up);

				FVector3 tmpVec = fn * dist;
				nearPt = pos + tmpVec;

				FVector3 t1;
				int outcnt[4]={0,0,0,0};
				texcoord tcs[4];

				// do a quick check whether the light touches this polygon
				for(int i=0;i<4;i++)
				{
					t1 = FVector3(&vtx[i*3]);
					FVector3 nearToVert = t1 - nearPt;
					tcs[i].u = ((nearToVert | right) * scale) + 0.5f;
					tcs[i].v = ((nearToVert | up) * scale) + 0.5f;

					if (tcs[i].u<0) outcnt[0]++;
					if (tcs[i].u>1) outcnt[1]++;
					if (tcs[i].v<0) outcnt[2]++;
					if (tcs[i].v>1) outcnt[3]++;

				}
				if (outcnt[0]!=4 && outcnt[1]!=4 && outcnt[2]!=4 && outcnt[3]!=4) 
				{
					draw_dlight += lightdata.GetLight(seg->frontsector->PortalGroup, p, node->lightsource, true);
				}
			}
		}
		node = node->nextLight;
	}
	dynlightindex = screen->mLights->UploadLights(lightdata);
}


const char GLWall::passflag[] = {
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

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::PutWall(HWDrawInfo *di, bool translucent)
{
	if (gltexture && gltexture->tex->GetTranslucency() && passflag[type] == 2)
	{
		translucent = true;
	}
	if (translucent)
	{
		flags |= GLWF_TRANSLUCENT;
		ViewDistance = (di->Viewpoint.Pos - (seg->linedef->v1->fPos() + seg->linedef->Delta() / 2)).XY().LengthSquared();
	}
	
	if (di->isFullbrightScene())
	{
		// light planes don't get drawn with fullbright rendering
		if (gltexture == NULL) return;
		Colormap.Clear();
	}
    
    if (di->isFullbrightScene() || (Colormap.LightColor.isWhite() && lightlevel == 255))
        flags &= ~GLWF_GLOW;
    
	if (!screen->BuffersArePersistent())
	{
		if (level.HasDynamicLights && !di->isFullbrightScene() && gltexture != nullptr)
		{
			SetupLights(di, lightdata);
		}
		MakeVertices(di, translucent);
	}


	bool solid;
	if (passflag[type] == 1) solid = true;
	else if (type == RENDERWALL_FFBLOCK) solid = gltexture && !gltexture->isMasked();
	else solid = false;
	if (solid) ProcessDecals(di);

	di->AddWall(this);

	lightlist = nullptr;
	// make sure that following parts of the same linedef do not get this one's vertex and lighting info.
	vertcount = 0;	
	dynlightindex = -1;
	flags &= ~GLWF_TRANSLUCENT;
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::PutPortal(HWDrawInfo *di, int ptype, int plane)
{
	auto pstate = screen->mPortalState;
	HWPortal * portal = nullptr;

	MakeVertices(di, false);
	switch (ptype)
	{
		// portals don't go into the draw list.
		// Instead they are added to the portal manager
	case PORTALTYPE_HORIZON:
		horizon = pstate->UniqueHorizons.Get(horizon);
		portal = di->FindPortal(horizon);
		if (!portal)
		{
			portal = new HWHorizonPortal(pstate, horizon, di->Viewpoint);
			di->Portals.Push(portal);
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_SKYBOX:
		portal = di->FindPortal(secportal);
		if (!portal)
		{
			// either a regular skybox or an Eternity-style horizon
			if (secportal->mType != PORTS_SKYVIEWPOINT) portal = new HWEEHorizonPortal(pstate, secportal);
			else
			{
				portal = new HWSkyboxPortal(pstate, secportal);
				di->Portals.Push(portal);
			}
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_SECTORSTACK:
		portal = di->FindPortal(this->portal);
		if (!portal)
		{
			portal = new HWSectorStackPortal(pstate, this->portal);
			di->Portals.Push(portal);
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_PLANEMIRROR:
		if (pstate->PlaneMirrorMode * planemirror->fC() <= 0)
		{
			planemirror = pstate->UniquePlaneMirrors.Get(planemirror);
			portal = di->FindPortal(planemirror);
			if (!portal)
			{
				portal = new HWPlaneMirrorPortal(pstate, planemirror);
				di->Portals.Push(portal);
			}
			portal->AddLine(this);
		}
		break;

	case PORTALTYPE_MIRROR:
		portal = di->FindPortal(seg->linedef);
		if (!portal)
		{
			portal = new HWMirrorPortal(pstate, seg->linedef);
			di->Portals.Push(portal);
		}
		portal->AddLine(this);
		if (gl_mirror_envmap)
		{
			// draw a reflective layer over the mirror
			di->AddMirrorSurface(this);
		}
		break;

	case PORTALTYPE_LINETOLINE:
		if (!lineportal)
			return;
		portal = di->FindPortal(lineportal);
		if (!portal)
		{
			line_t *otherside = lineportal->lines[0]->mDestination;
			if (otherside != nullptr && otherside->portalindex < level.linePortals.Size())
			{
				di->ProcessActorsInPortal(otherside->getPortal()->mGroup, di->in_area);
			}
			portal = new HWLineToLinePortal(pstate, lineportal);
			di->Portals.Push(portal);
		}
		portal->AddLine(this);
		break;

	case PORTALTYPE_SKY:
		sky = pstate->UniqueSkies.Get(sky);
		portal = di->FindPortal(sky);
		if (!portal)
		{
			portal = new HWSkyPortal(screen->mSkyData, pstate, sky);
			di->Portals.Push(portal);
		}
		portal->AddLine(this);
		break;
	}
	vertcount = 0;

	if (plane != -1 && portal)
	{
		portal->planesused |= (1<<plane);
	}
}

//==========================================================================
//
//	Sets 3D-floor lighting info
//
//==========================================================================

void GLWall::Put3DWall(HWDrawInfo *di, lightlist_t * lightlist, bool translucent)
{
	// only modify the light level if it doesn't originate from the seg's frontsector. This is to account for light transferring effects
	if (lightlist->p_lightlevel != &seg->sidedef->sector->lightlevel)
	{
		lightlevel = hw_ClampLight(*lightlist->p_lightlevel);
	}
	// relative light won't get changed here. It is constant across the entire wall.

	Colormap.CopyFrom3DLight(lightlist);
	PutWall(di, translucent);
}

//==========================================================================
//
//  Splits a wall vertically if a 3D-floor
//	creates different lighting across the wall
//
//==========================================================================

bool GLWall::SplitWallComplex(HWDrawInfo *di, sector_t * frontsector, bool translucent, float& maplightbottomleft, float& maplightbottomright)
{
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
			GLWall copyWall1 = *this, copyWall2 = *this;

			copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2 - glseg.x1);
			copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2 - glseg.y1);
			copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1] - ztop[0]);
			copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1] - zbottom[0]);
			copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright - glseg.fracleft);
			copyWall1.tcs[UPRGT].u = copyWall2.tcs[UPLFT].u = tcs[UPLFT].u + coeff * (tcs[UPRGT].u - tcs[UPLFT].u);
			copyWall1.tcs[UPRGT].v = copyWall2.tcs[UPLFT].v = tcs[UPLFT].v + coeff * (tcs[UPRGT].v - tcs[UPLFT].v);
			copyWall1.tcs[LORGT].u = copyWall2.tcs[LOLFT].u = tcs[LOLFT].u + coeff * (tcs[LORGT].u - tcs[LOLFT].u);
			copyWall1.tcs[LORGT].v = copyWall2.tcs[LOLFT].v = tcs[LOLFT].v + coeff * (tcs[LORGT].v - tcs[LOLFT].v);

			copyWall1.SplitWall(di, frontsector, translucent);
			copyWall2.SplitWall(di, frontsector, translucent);
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
			GLWall copyWall1 = *this, copyWall2 = *this;

			copyWall1.glseg.x2 = copyWall2.glseg.x1 = glseg.x1 + coeff * (glseg.x2 - glseg.x1);
			copyWall1.glseg.y2 = copyWall2.glseg.y1 = glseg.y1 + coeff * (glseg.y2 - glseg.y1);
			copyWall1.ztop[1] = copyWall2.ztop[0] = ztop[0] + coeff * (ztop[1] - ztop[0]);
			copyWall1.zbottom[1] = copyWall2.zbottom[0] = zbottom[0] + coeff * (zbottom[1] - zbottom[0]);
			copyWall1.glseg.fracright = copyWall2.glseg.fracleft = glseg.fracleft + coeff * (glseg.fracright - glseg.fracleft);
			copyWall1.tcs[UPRGT].u = copyWall2.tcs[UPLFT].u = tcs[UPLFT].u + coeff * (tcs[UPRGT].u - tcs[UPLFT].u);
			copyWall1.tcs[UPRGT].v = copyWall2.tcs[UPLFT].v = tcs[UPLFT].v + coeff * (tcs[UPRGT].v - tcs[UPLFT].v);
			copyWall1.tcs[LORGT].u = copyWall2.tcs[LOLFT].u = tcs[LOLFT].u + coeff * (tcs[LORGT].u - tcs[LOLFT].u);
			copyWall1.tcs[LORGT].v = copyWall2.tcs[LOLFT].v = tcs[LOLFT].v + coeff * (tcs[LORGT].v - tcs[LOLFT].v);

			copyWall1.SplitWall(di, frontsector, translucent);
			copyWall2.SplitWall(di, frontsector, translucent);
			return true;
		}
	}

	return false;
}

void GLWall::SplitWall(HWDrawInfo *di, sector_t * frontsector, bool translucent)
{
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
	if (seg->linedef->Index() == 1)
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
				if (!(screen->hwcaps & RFL_NO_CLIP_PLANES))
				{
					// Use hardware clipping if this cannot be done cleanly.
					this->lightlist = &lightlist;
					PutWall(di, translucent);

					goto out;
				}
				// crappy fallback if no clip planes available
				else if (SplitWallComplex(di, frontsector, translucent, maplightbottomleft, maplightbottomright))
				{
					goto out;
				}
			}

			// 3D floor is completely within this light
			if (maplightbottomleft<=zbottom[0] && maplightbottomright<=zbottom[1])
			{
				Put3DWall(di, &lightlist[i], translucent);
				goto out;
			}

			if (maplightbottomleft<=ztop[0] && maplightbottomright<=ztop[1] &&
				(maplightbottomleft!=ztop[0] || maplightbottomright!=ztop[1]))
			{
				GLWall copyWall1 = *this;

				copyWall1.flags |= GLWF_NOSPLITLOWER;
				flags |= GLWF_NOSPLITUPPER;
				ztop[0]=copyWall1.zbottom[0]=maplightbottomleft;
				ztop[1]=copyWall1.zbottom[1]=maplightbottomright;
				tcs[UPLFT].v=copyWall1.tcs[LOLFT].v=copyWall1.tcs[UPLFT].v+ 
					(maplightbottomleft-copyWall1.ztop[0])*(copyWall1.tcs[LOLFT].v-copyWall1.tcs[UPLFT].v)/(zbottom[0]-copyWall1.ztop[0]);
				tcs[UPRGT].v=copyWall1.tcs[LORGT].v=copyWall1.tcs[UPRGT].v+ 
					(maplightbottomright-copyWall1.ztop[1])*(copyWall1.tcs[LORGT].v-copyWall1.tcs[UPRGT].v)/(zbottom[1]-copyWall1.ztop[1]);
				copyWall1.Put3DWall(di, &lightlist[i], translucent);
			}
			if (ztop[0]==zbottom[0] && ztop[1]==zbottom[1]) 
			{
				//::SplitWall.Unclock();
				goto out;
			}
		}
	}

	Put3DWall(di, &lightlist[lightlist.Size()-1], translucent);

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
bool GLWall::DoHorizon(HWDrawInfo *di, seg_t * seg,sector_t * fs, vertex_t * v1,vertex_t * v2)
{
	GLHorizonInfo hi;
	lightlist_t * light;

	// ZDoom doesn't support slopes in a horizon sector so I won't either!
	ztop[1] = ztop[0] = fs->GetPlaneTexZ(sector_t::ceiling);
	zbottom[1] = zbottom[0] = fs->GetPlaneTexZ(sector_t::floor);

    auto vpz = di->Viewpoint.Pos.Z;
	if (vpz < fs->GetPlaneTexZ(sector_t::ceiling))
	{
		if (vpz > fs->GetPlaneTexZ(sector_t::floor))
			zbottom[1] = zbottom[0] = vpz;

		if (fs->GetTexture(sector_t::ceiling) == skyflatnum)
		{
			SkyPlane(di, fs, sector_t::ceiling, false);
		}
		else
		{
			hi.plane.GetFromSector(fs, sector_t::ceiling);
			hi.lightlevel = hw_ClampLight(fs->GetCeilingLight());
			hi.colormap = fs->Colormap;
			hi.specialcolor = fs->SpecialColors[sector_t::ceiling];

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->ceilingplane, true);

				if(!(fs->GetFlags(sector_t::ceiling)&PLANEF_ABSLIGHTING)) hi.lightlevel = hw_ClampLight(*light->p_lightlevel);
				hi.colormap.CopyLight(light->extra_colormap);
			}

			if (di->isFullbrightScene()) hi.colormap.Clear();
			horizon = &hi;
			PutPortal(di, PORTALTYPE_HORIZON, -1);
		}
		ztop[1] = ztop[0] = zbottom[0];
	} 

	if (vpz > fs->GetPlaneTexZ(sector_t::floor))
	{
		zbottom[1] = zbottom[0] = fs->GetPlaneTexZ(sector_t::floor);
		if (fs->GetTexture(sector_t::floor) == skyflatnum)
		{
			SkyPlane(di, fs, sector_t::floor, false);
		}
		else
		{
			hi.plane.GetFromSector(fs, sector_t::floor);
			hi.lightlevel = hw_ClampLight(fs->GetFloorLight());
			hi.colormap = fs->Colormap;
			hi.specialcolor = fs->SpecialColors[sector_t::floor];

			if (fs->e->XFloor.ffloors.Size())
			{
				light = P_GetPlaneLight(fs, &fs->floorplane, false);

				if(!(fs->GetFlags(sector_t::floor)&PLANEF_ABSLIGHTING)) hi.lightlevel = hw_ClampLight(*light->p_lightlevel);
				hi.colormap.CopyLight(light->extra_colormap);
			}

			if (di->isFullbrightScene()) hi.colormap.Clear();
			horizon = &hi;
			PutPortal(di, PORTALTYPE_HORIZON, -1);
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
		if (gltexture->tex->isHardwareCanvas()) normalize = true;
		else if (flags & GLWF_CLAMPY)
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

void GLWall::CheckTexturePosition(FTexCoordInfo *tci)
{
	float sub;

	if (gltexture->tex->isHardwareCanvas()) return;

	// clamp texture coordinates to a reasonable range.
	// Extremely large values can cause visual problems
	if (tci->mScale.Y > 0)
	{
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
			flags |= GLWF_CLAMPY;
		}
	}
	else
	{
		if (tcs[LOLFT].v < tcs[LORGT].v)
		{
			sub = float(xs_FloorToInt(tcs[LOLFT].v));
		}
		else
		{
			sub = float(xs_FloorToInt(tcs[LORGT].v));
		}
		tcs[UPLFT].v -= sub;
		tcs[UPRGT].v -= sub;
		tcs[LOLFT].v -= sub;
		tcs[LORGT].v -= sub;

		if ((tcs[LOLFT].v == 0.f && tcs[LORGT].v == 0.f && tcs[UPLFT].v <= 1.f && tcs[UPRGT].v <= 1.f) ||
			(tcs[LOLFT].v >= 0.f && tcs[LORGT].v >= 0.f && tcs[UPLFT].v == 1.f && tcs[UPRGT].v == 1.f))
		{
			flags |= GLWF_CLAMPY;
		}
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
			flags |= GLWF_CLAMPX;
		}
	}
}

//==========================================================================
//
//  Handle one sided walls, upper and lower texture
//
//==========================================================================
void GLWall::DoTexture(HWDrawInfo *di, int _type,seg_t * seg, int peg,
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
	uint8_t savedflags = flags;

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
		PutPortal(di, PORTALTYPE_MIRROR, -1);
	}
	else
	{

		CheckTexturePosition(&tci);

		// Add this wall to the render list
		sector_t * sec = sub ? sub->sector : seg->frontsector;
		if (sec->e->XFloor.lightlist.Size()==0 || di->isFullbrightScene()) PutWall(di, false);
		else SplitWall(di, sec, false);
	}

	glseg = glsave;
	flags = savedflags;
}


//==========================================================================
//
// 
//
//==========================================================================

void GLWall::DoMidTexture(HWDrawInfo *di, seg_t * seg, bool drawfogboundary,
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
	float rowoffset = 0;

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
		rowoffset = tci.RowOffset(seg->sidedef->GetTextureYOffset(side_t::mid));
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
		FTexture * tex = TexMan.GetTexture(seg->sidedef->GetTexture(side_t::top), true);
		if (!tex || !tex->isValid())
		{
			if (front->GetTexture(sector_t::ceiling) == skyflatnum &&
				back->GetTexture(sector_t::ceiling) == skyflatnum && !wrap)
			{
				// intra-sky lines do not clip the texture at all if there's no upper texture.
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
		tex = TexMan.GetTexture(seg->sidedef->GetTexture(side_t::bottom), true);
		if (!tex || !tex->isValid())
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
			flags |= GLWF_CLAMPX;
		}
		else
		{
			flags &= ~GLWF_CLAMPX;
		}
		if (!wrap)
		{
			flags |= GLWF_CLAMPY;
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
		PutWall(di, true);
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
			translucent =alpha < 1. || (gltexture && gltexture->tex->GetTranslucency());
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

				GLWall split = *this;

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
				if (front->e->XFloor.lightlist.Size()==0 || di->isFullbrightScene()) split.PutWall(di, translucent);
				else split.SplitWall(di, front, translucent);

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
			if (front->e->XFloor.lightlist.Size()==0 || di->isFullbrightScene()) PutWall(di, translucent);
			else SplitWall(di, front, translucent);
		}
		alpha=1.0f;
	}
	// restore some values that have been altered in this function
	glseg=glsave;
	flags&=~(GLWF_CLAMPX|GLWF_CLAMPY|GLWF_NOSPLITUPPER|GLWF_NOSPLITLOWER);
	RenderStyle = STYLE_Normal;
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::BuildFFBlock(HWDrawInfo *di, seg_t * seg, F3DFloor * rover,
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
		if (!di->isFullbrightScene())
		{
			// this may not yet be done
			light = P_GetPlaneLight(rover->target, rover->top.plane, true);
			Colormap.Clear();
			Colormap.LightColor = light->extra_colormap.FadeColor;
			// the fog plane defines the light level, not the front sector
			lightlevel = hw_ClampLight(*light->p_lightlevel);
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
		CheckTexturePosition(&tci);
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

	if (sec->e->XFloor.lightlist.Size() == 0 || di->isFullbrightScene()) PutWall(di, translucent);
	else SplitWall(di, sec, translucent);

	alpha = 1.0f;
	lightlevel = savelight;
	Colormap = savecolor;
	flags &= ~GLWF_CLAMPY;
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
void GLWall::InverseFloors(HWDrawInfo *di, seg_t * seg, sector_t * frontsector,
	float topleft, float topright,
	float bottomleft, float bottomright)
{
	TArray<F3DFloor *> & frontffloors = frontsector->e->XFloor.ffloors;

	for (unsigned int i = 0; i < frontffloors.Size(); i++)
	{
		F3DFloor * rover = frontffloors[i];
		if (rover->flags & FF_THISINSIDE) continue;	// only relevant for software rendering.
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

		BuildFFBlock(di, seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
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
void GLWall::ClipFFloors(HWDrawInfo *di, seg_t * seg, F3DFloor * ffloor, sector_t * frontsector,
	float topleft, float topright,
	float bottomleft, float bottomright)
{
	TArray<F3DFloor *> & frontffloors = frontsector->e->XFloor.ffloors;

	const unsigned int flags = ffloor->flags & (FF_SWIMMABLE | FF_TRANSLUCENT);

	for (unsigned int i = 0; i < frontffloors.Size(); i++)
	{
		F3DFloor * rover = frontffloors[i];
		if (rover->flags & FF_THISINSIDE) continue;	// only relevant for software rendering.
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
			BuildFFBlock(di, seg, ffloor, topleft, topright, ff_topleft, ff_topright);
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
	BuildFFBlock(di, seg, ffloor, topleft, topright, bottomleft, bottomright);
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::DoFFloorBlocks(HWDrawInfo *di, seg_t * seg, sector_t * frontsector, sector_t * backsector,
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
		if (rover->flags & FF_THISINSIDE) continue;	// only relevant for software rendering.
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
			InverseFloors(di, seg, frontsector, topleft, topright, ff_topleft, ff_topright);

		// if translucent or liquid clip away adjoining parts of the same type of FFloors on the other side
		if (rover->flags&(FF_SWIMMABLE | FF_TRANSLUCENT))
			ClipFFloors(di, seg, rover, frontsector, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);
		else
			BuildFFBlock(di, seg, rover, ff_topleft, ff_topright, ff_bottomleft, ff_bottomright);

		topleft = ff_bottomleft;
		topright = ff_bottomright;
		renderedsomething = true;
		if (topleft <= bottomleft && topright <= bottomright) return;
	}

	// draw all inverse floors below the lowest one
	if (frontsector->e->XFloor.ffloors.Size() > 0)
	{
		if (topleft > bottomleft || topright > bottomright)
			InverseFloors(di, seg, frontsector, topleft, topright, bottomleft, bottomright);
	}
}
	
//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Process(HWDrawInfo *di, seg_t *seg, sector_t * frontsector, sector_t * backsector)
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
	if (seg->linedef->Index() == 14454)
	{
		int a = 0;
	}
#endif

	// note: we always have a valid sidedef and linedef reference when getting here.

	this->seg = seg;
	this->frontsector = frontsector;
	this->backsector = backsector;
	vertindex = 0;
	vertcount = 0;

	if ((seg->sidedef->Flags & WALLF_POLYOBJ) && seg->backsector)
	{
		// Textures on 2-sided polyobjects are aligned to the actual seg's sectors
		segfront = realfront = seg->frontsector;
		segback = realback = seg->backsector;
	}
	else
	{
		// Need these for aligning the textures
		realfront = &level.sectors[frontsector->sectornum];
		realback = backsector ? &level.sectors[backsector->sectornum] : NULL;
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
			if (v1->dirty) v1->RecalcVertexHeights();
			if (v2->dirty) v2->RecalcVertexHeights();
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
		flags |= GLWF_NOSPLITUPPER | GLWF_NOSPLITLOWER;	// seg-splitting not needed for single segs.
	}


	vertexes[0] = v1;
	vertexes[1] = v2;

	glseg.x1 = v1->fX();
	glseg.y1 = v1->fY();
	glseg.x2 = v2->fX();
	glseg.y2 = v2->fY();
	Colormap = frontsector->Colormap;
	flags = 0;
	dynlightindex = -1;
	lightlist = NULL;

	int rel = 0;
	int orglightlevel = hw_ClampLight(frontsector->lightlevel);
	bool foggy = (!Colormap.FadeColor.isBlack() || level.flags&LEVEL_HASFADETABLE);	// fog disables fake contrast
	lightlevel = hw_ClampLight(seg->sidedef->GetLightLevel(foggy, orglightlevel, false, &rel));
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


	if (frontsector->GetWallGlow(topglowcolor, bottomglowcolor)) flags |= GLWF_GLOW;

	zfloor[0] = ffh1 = segfront->floorplane.ZatPoint(v1);
	zfloor[1] = ffh2 = segfront->floorplane.ZatPoint(v2);
	zceil[0] = fch1 = segfront->ceilingplane.ZatPoint(v1);
	zceil[1] = fch2 = segfront->ceilingplane.ZatPoint(v2);

	if (seg->linedef->special == Line_Horizon)
	{
		SkyNormal(di, frontsector, v1, v2);
		DoHorizon(di, seg, frontsector, v1, v2);
		return;
	}

	//return;
	// [GZ] 3D middle textures are necessarily two-sided, even if they lack the explicit two-sided flag
	if (!backsector || !(seg->linedef->flags&(ML_TWOSIDED | ML_3DMIDTEX))) // one sided
	{
		// sector's sky
		SkyNormal(di, frontsector, v1, v2);

		if (seg->linedef->isVisualPortal())
		{
			lineportal = seg->linedef->getPortal()->mGroup;
			ztop[0] = zceil[0];
			ztop[1] = zceil[1];
			zbottom[0] = zfloor[0];
			zbottom[1] = zfloor[1];
			PutPortal(di, PORTALTYPE_LINETOLINE, -1);
		}
		else if (seg->linedef->GetTransferredPortal())
		{
			SkyLine(di, frontsector, seg->linedef);
		}
		else
		{
			// normal texture
			gltexture = FMaterial::ValidateTexture(seg->sidedef->GetTexture(side_t::mid), false, true);
			if (gltexture)
			{
				DoTexture(di, RENDERWALL_M1S, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
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

		SkyTop(di, seg, frontsector, backsector, v1, v2);
		SkyBottom(di, seg, frontsector, backsector, v1, v2);

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
					DoTexture(di, RENDERWALL_TOP, seg, (seg->linedef->flags & (ML_DONTPEGTOP)) == 0,
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
							DoTexture(di, RENDERWALL_TOP, seg, (seg->linedef->flags & (ML_DONTPEGTOP)) == 0,
								crefz, realback->GetPlaneTexZ(sector_t::ceiling),
								fch1, fch2, bch1a, bch2a, 0);
						}
					}
					else
					{
						// skip processing if the back is a malformed subsector
						if (seg->PartnerSeg != NULL && !(seg->PartnerSeg->Subsector->hacked & 4))
						{
							di->AddUpperMissingTexture(seg->sidedef, sub, bch1a);
						}
					}
				}
			}
		}


		/* mid texture */
		bool isportal = seg->linedef->isVisualPortal() && seg->sidedef == seg->linedef->sidedef[0];
		sector_t *backsec = isportal? seg->linedef->getPortalDestination()->frontsector : backsector;

		bool drawfogboundary = !di->isFullbrightScene() && hw_CheckFog(frontsector, backsec);
		FTexture *tex = TexMan.GetTexture(seg->sidedef->GetTexture(side_t::mid), true);
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
			DoMidTexture(di, seg, drawfogboundary, frontsector, backsector, realfront, realback,
				fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
		}

		if (isportal)
		{
			lineportal = seg->linedef->getPortal()->mGroup;
			ztop[0] = bch1;
			ztop[1] = bch2;
			zbottom[0] = bfh1;
			zbottom[1] = bfh2;
			PutPortal(di, PORTALTYPE_LINETOLINE, -1);
		}
		else if (backsector->e->XFloor.ffloors.Size() || frontsector->e->XFloor.ffloors.Size())
		{
			DoFFloorBlocks(di, seg, frontsector, backsector, fch1, fch2, ffh1, ffh2, bch1, bch2, bfh1, bfh2);
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
				DoTexture(di, RENDERWALL_BOTTOM, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
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
						DoTexture(di, RENDERWALL_BOTTOM, seg, (seg->linedef->flags & ML_DONTPEGBOTTOM) > 0,
							realback->GetPlaneTexZ(sector_t::floor), frefz,
							bfh1, bfh2, ffh1, ffh2, frefz - crefz);
					}
				}
				else if (backsector->GetTexture(sector_t::floor) != skyflatnum)
				{
					// skip processing if the back is a malformed subsector
					if (seg->PartnerSeg != NULL && !(seg->PartnerSeg->Subsector->hacked & 4))
					{
						di->AddLowerMissingTexture(seg->sidedef, sub, bfh1);
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
void GLWall::ProcessLowerMiniseg(HWDrawInfo *di, seg_t *seg, sector_t * frontsector, sector_t * backsector)
{
	if (frontsector->GetTexture(sector_t::floor) == skyflatnum) return;
	lightlist = NULL;

	float ffh = frontsector->GetPlaneTexZ(sector_t::floor);
	float bfh = backsector->GetPlaneTexZ(sector_t::floor);

	vertindex = 0;
	vertcount = 0;
	if (bfh > ffh)
	{
		this->seg = seg;
		this->frontsector = frontsector;
		this->backsector = backsector;
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
		lightlevel = hw_ClampLight(frontsector->lightlevel);
		rellight = 0;

		alpha = 1.0f;
		RenderStyle = STYLE_Normal;
		Colormap = frontsector->Colormap;

		if (frontsector->GetWallGlow(topglowcolor, bottomglowcolor)) flags |= GLWF_GLOW;
		dynlightindex = -1;

		zfloor[0] = zfloor[1] = ffh;

		gltexture = FMaterial::ValidateTexture(frontsector->GetTexture(sector_t::floor), false, true);

		if (gltexture)
		{
			FTexCoordInfo tci;
			type = RENDERWALL_BOTTOM;
			gltexture->GetTexCoordInfo(&tci, 1.f, 1.f);
			SetWallCoordinates(seg, &tci, bfh, bfh, bfh, ffh, ffh, 0);
			PutWall(di, false);
		}
	}
}
