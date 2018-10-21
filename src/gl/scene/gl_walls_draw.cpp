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

#include "gl_load/gl_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"

#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// General purpose wall rendering function
// everything goes through here
//
//==========================================================================

void FDrawInfo::RenderWall(GLWall *wall, FRenderState &state, int textured)
{
	assert(wall->vertcount > 0);
	state.SetLightIndex(wall->dynlightindex);
	Draw(DT_TriangleFan, gl_RenderState, wall->vertindex, wall->vertcount);
	vertexcount += wall->vertcount;
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::RenderFogBoundary(GLWall *wall, FRenderState &state)
{
	if (gl_fogmode && !isFullbrightScene())
	{
		int rel = wall->rellight + getExtraLight();
		EnableDrawBufferAttachments(false);
		state.SetFog(wall->lightlevel, rel, false, &wall->Colormap, false);
		state.SetEffect(EFF_FOGBOUNDARY);
		state.AlphaFunc(Alpha_GEqual, 0.f);
		state.SetDepthBias(-1, -128);
		RenderWall(wall, state, GLWall::RWF_BLANK);
		state.ClearDepthBias();
		state.SetEffect(EFF_NONE);
		EnableDrawBufferAttachments(true);
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::RenderMirrorSurface(GLWall *wall, FRenderState &state)
{
	if (!TexMan.mirrorTexture.isValid()) return;

	// we use texture coordinates and texture matrix to pass the normal stuff to the shader so that the default vertex buffer format can be used as is.
	state.EnableTextureMatrix(true);

	// Use sphere mapping for this
	state.SetEffect(EFF_SPHEREMAP);

	state.SetColor(wall->lightlevel, 0, isFullbrightScene(), wall->Colormap ,0.1f);
	state.SetFog(wall->lightlevel, 0, isFullbrightScene(), &wall->Colormap, true);
	state.SetRenderStyle(STYLE_Add);
	state.AlphaFunc(Alpha_Greater,0);
	glDepthFunc(GL_LEQUAL);

	FMaterial * pat=FMaterial::ValidateTexture(TexMan.mirrorTexture, false, false);
	gl_RenderState.ApplyMaterial(pat, CLAMP_NONE, 0, -1);

	wall->flags &= ~GLWall::GLWF_GLOW;
	RenderWall(wall, state, GLWall::RWF_BLANK);

	state.EnableTextureMatrix(false);
	state.SetEffect(EFF_NONE);

	// Restore the defaults for the translucent pass
	state.AlphaFunc(Alpha_GEqual, gl_mask_sprite_threshold);
	glDepthFunc(GL_LESS);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here, right after the wall they are on,
	// because the depth buffer won't get set by translucent items.
	if (wall->seg->sidedef->AttachedDecals)
	{
		wall->DrawDecalsForMirror(this, gl_RenderState, decals[1]);
	}
	state.SetRenderStyle(STYLE_Translucent);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::RenderTexturedWall(GLWall *wall, FRenderState &state, int rflags)
{
	int tmode = state.GetTextureMode();
	int rel = wall->rellight + getExtraLight();

	if (wall->flags & GLWall::GLWF_GLOW)
	{
		state.EnableGlow(true);
		state.SetGlowParams(wall->topglowcolor, wall->bottomglowcolor);
	}
	state.SetGlowPlanes(wall->topplane, wall->bottomplane);
	state.SetMaterial(wall->gltexture, wall->flags & 3, 0, -1);

	if (wall->type == RENDERWALL_M2SNF)
	{
		if (wall->flags & GLWall::GLWF_CLAMPY)
		{
			if (tmode == TM_NORMAL) state.SetTextureMode(TM_CLAMPY);
		}
		SetFog(255, 0, nullptr, false);
	}
	state.SetObjectColor(wall->seg->frontsector->SpecialColors[sector_t::walltop] | 0xff000000);
	state.SetObjectColor2(wall->seg->frontsector->SpecialColors[sector_t::wallbottom] | 0xff000000);

	float absalpha = fabsf(wall->alpha);
	if (wall->lightlist == nullptr)
	{
		if (wall->type != RENDERWALL_M2SNF) SetFog(wall->lightlevel, rel, &wall->Colormap, wall->RenderStyle == STYLE_Add);
		SetColor(wall->lightlevel, rel, wall->Colormap, absalpha);
		RenderWall(wall, state, rflags);
	}
	else
	{
		state.EnableSplit(true);

		for (unsigned i = 0; i < wall->lightlist->Size(); i++)
		{
			secplane_t &lowplane = i == (*wall->lightlist).Size() - 1 ? wall->bottomplane : (*wall->lightlist)[i + 1].plane;
			// this must use the exact same calculation method as GLWall::Process etc.
			float low1 = lowplane.ZatPoint(wall->vertexes[0]);
			float low2 = lowplane.ZatPoint(wall->vertexes[1]);

			if (low1 < wall->ztop[0] || low2 < wall->ztop[1])
			{
				int thisll = (*wall->lightlist)[i].caster != NULL ? hw_ClampLight(*(*wall->lightlist)[i].p_lightlevel) : wall->lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = wall->Colormap.FadeColor;
				thiscm.FogDensity = wall->Colormap.FogDensity;
				thiscm.CopyFrom3DLight(&(*wall->lightlist)[i]);
				state.SetColor(thisll, rel, false, thiscm, absalpha);
				if (wall->type != RENDERWALL_M2SNF) state.SetFog(thisll, rel, false, &thiscm, wall->RenderStyle == STYLE_Add);
				state.SetSplitPlanes((*wall->lightlist)[i].plane, lowplane);
				RenderWall(wall, state, rflags);
			}
			if (low1 <= wall->zbottom[0] && low2 <= wall->zbottom[1]) break;
		}

		state.EnableSplit(false);
	}
	state.SetObjectColor(0xffffffff);
	state.SetObjectColor2(0);
	state.SetTextureMode(tmode);
	state.EnableGlow(false);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::RenderTranslucentWall(GLWall *wall, FRenderState &state)
{
	state.SetRenderStyle(wall->RenderStyle);
	if (wall->gltexture)
	{
		if (!wall->gltexture->tex->GetTranslucency()) state.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
		else state.AlphaFunc(Alpha_GEqual, 0.f);
		RenderTexturedWall(wall, state, GLWall::RWF_TEXTURED | GLWall::RWF_NOSPLIT);
	}
	else
	{
		state.AlphaFunc(Alpha_GEqual, 0.f);
		state.SetColor(wall->lightlevel, 0, false, wall->Colormap, fabsf(wall->alpha));
		state.SetFog(wall->lightlevel, 0, false, &wall->Colormap, wall->RenderStyle == STYLE_Add);
		state.EnableTexture(false);
		RenderWall(wall, state, GLWall::RWF_NOSPLIT);
		state.EnableTexture(true);
	}
	state.SetRenderStyle(STYLE_Translucent);
}

//==========================================================================
//
// 
//
//==========================================================================
void FDrawInfo::DrawWall(GLWall *wall, int pass)
{
	FRenderState &state = gl_RenderState;
	if (screen->BuffersArePersistent())
	{
		if (level.HasDynamicLights && !isFullbrightScene() && wall->gltexture != nullptr)
		{
			wall->SetupLights(this, lightdata);
		}
		wall->MakeVertices(this, !!(wall->flags & GLWall::GLWF_TRANSLUCENT));
	}

	gl_RenderState.SetNormal(wall->glseg.Normal());
	switch (pass)
	{
	case GLPASS_ALL:
		RenderTexturedWall(wall, state, GLWall::RWF_TEXTURED);
		break;

	case GLPASS_TRANSLUCENT:

		switch (wall->type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface(wall, state);
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary(wall, state);
			break;

		default:
			RenderTranslucentWall(wall, state);
			break;
		}
		break;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddWall(GLWall *wall)
{
	if (wall->flags & GLWall::GLWF_TRANSLUCENT)
	{
		auto newwall = drawlists[GLDL_TRANSLUCENT].NewWall();
		*newwall = *wall;
	}
	else
	{
		bool masked = GLWall::passflag[wall->type] == 1 ? false : (wall->gltexture && wall->gltexture->isMasked());
		int list;

		if ((wall->flags & GLWall::GLWF_SKYHACK && wall->type == RENDERWALL_M2S))
		{
			list = GLDL_MASKEDWALLSOFS;
		}
		else
		{
			list = masked ? GLDL_MASKEDWALLS : GLDL_PLAINWALLS;
		}
		auto newwall = drawlists[list].NewWall();
		*newwall = *wall;
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddMirrorSurface(GLWall *w)
{
	w->type = RENDERWALL_MIRRORSURFACE;
	auto newwall = drawlists[GLDL_TRANSLUCENTBORDER].NewWall();
	*newwall = *w;

	// Invalidate vertices to allow setting of texture coordinates
	newwall->vertcount = 0;

	FVector3 v = newwall->glseg.Normal();
	auto tcs = newwall->tcs;
	tcs[GLWall::LOLFT].u = tcs[GLWall::LORGT].u = tcs[GLWall::UPLFT].u = tcs[GLWall::UPRGT].u = v.X;
	tcs[GLWall::LOLFT].v = tcs[GLWall::LORGT].v = tcs[GLWall::UPLFT].v = tcs[GLWall::UPRGT].v = v.Z;
	newwall->MakeVertices(this, false);
	newwall->ProcessDecals(this);
}

//==========================================================================
//
// 
//
//==========================================================================

void FDrawInfo::AddPortal(GLWall *wall, int ptype)
{
	auto &pstate = GLRenderer->mPortalState;
	IPortal * portal;

	wall->MakeVertices(this, false);
	switch (ptype)
	{
		// portals don't go into the draw list.
		// Instead they are added to the portal manager
	case PORTALTYPE_HORIZON:
		wall->horizon = pstate.UniqueHorizons.Get(wall->horizon);
		portal = FindPortal(wall->horizon);
		if (!portal)
		{
			portal = new GLHorizonPortal(&pstate, wall->horizon, Viewpoint);
			Portals.Push(portal);
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SKYBOX:
		portal = FindPortal(wall->secportal);
		if (!portal)
		{
			// either a regular skybox or an Eternity-style horizon
			if (wall->secportal->mType != PORTS_SKYVIEWPOINT) portal = new GLEEHorizonPortal(&pstate, wall->secportal);
			else
			{
				portal = new GLScenePortal(&pstate, new HWSkyboxPortal(wall->secportal));
				Portals.Push(portal);
			}
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SECTORSTACK:
		portal = FindPortal(wall->portal);
		if (!portal)
		{
			portal = new GLScenePortal(&pstate, new HWSectorStackPortal(wall->portal));
			Portals.Push(portal);
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_PLANEMIRROR:
		if (pstate.PlaneMirrorMode * wall->planemirror->fC() <= 0)
		{
			//@sync-portal
			wall->planemirror = pstate.UniquePlaneMirrors.Get(wall->planemirror);
			portal = FindPortal(wall->planemirror);
			if (!portal)
			{
				portal = new GLScenePortal(&pstate, new HWPlaneMirrorPortal(wall->planemirror));
				Portals.Push(portal);
			}
			portal->AddLine(wall);
		}
		break;

	case PORTALTYPE_MIRROR:
		portal = FindPortal(wall->seg->linedef);
		if (!portal)
		{
			portal = new GLScenePortal(&pstate, new HWMirrorPortal(wall->seg->linedef));
			Portals.Push(portal);
		}
		portal->AddLine(wall);
		if (gl_mirror_envmap)
		{
			// draw a reflective layer over the mirror
			AddMirrorSurface(wall);
		}
		break;

	case PORTALTYPE_LINETOLINE:
		portal = FindPortal(wall->lineportal);
		if (!portal)
		{
			line_t *otherside = wall->lineportal->lines[0]->mDestination;
			if (otherside != nullptr && otherside->portalindex < level.linePortals.Size())
			{
				ProcessActorsInPortal(otherside->getPortal()->mGroup, in_area);
			}
			portal = new GLScenePortal(&pstate, new HWLineToLinePortal(wall->lineportal));
			Portals.Push(portal);
		}
		portal->AddLine(wall);
		break;

	case PORTALTYPE_SKY:
		wall->sky = pstate.UniqueSkies.Get(wall->sky);
		portal = FindPortal(wall->sky);
		if (!portal) 
		{
			portal = new GLSkyPortal(&pstate, wall->sky);
			Portals.Push(portal);
		}
		portal->AddLine(wall);
		break;
	}
	wall->vertcount = 0;
}

