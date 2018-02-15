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

#include "gl/system/gl_system.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "a_sharedglobal.h"
#include "g_levellocals.h"
#include "actor.h"
#include "actorinlines.h"
#include "gl/gl_functions.h"

#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_data.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/dynlights/gl_glow.h"
#include "gl/dynlights/gl_lightbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_clock.h"
#include "gl/utility/gl_templates.h"
#include "gl/renderer/gl_quaddrawer.h"

EXTERN_CVAR(Bool, gl_seamless)

//==========================================================================
//
// Collect lights for shader
//
//==========================================================================
FDynLightData lightdata;


void GLWall::SetupLights()
{
	if (RenderStyle == STYLE_Add && !glset.lightadditivesurfaces) return;	// no lights on additively blended surfaces.

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

	lightdata.Clear();
	p.Set(&glseg);

	/*
	if (!p.ValidNormal()) 
	{
		return;
	}
	*/
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
		node = sub->lighthead;
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
					gl_GetLight(seg->frontsector->PortalGroup, p, node->lightsource, true, lightdata);
				}
			}
		}
		node = node->nextLight;
	}

	dynlightindex = GLRenderer->mLights->UploadLights(lightdata);
}

//==========================================================================
//
// build the vertices for this wall
//
//==========================================================================

void GLWall::MakeVertices(bool nosplit)
{
	if (vertcount == 0)
	{
		bool split = (gl_seamless && !nosplit && seg->sidedef != NULL && !(seg->sidedef->Flags & WALLF_POLYOBJ) && !(flags & GLWF_NOSPLIT));

		FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();

		ptr->Set(glseg.x1, zbottom[0], glseg.y1, tcs[LOLFT].u, tcs[LOLFT].v);
		ptr++;
		if (split && glseg.fracleft == 0) SplitLeftEdge(ptr);
		ptr->Set(glseg.x1, ztop[0], glseg.y1, tcs[UPLFT].u, tcs[UPLFT].v);
		ptr++;
		if (split && !(flags & GLWF_NOSPLITUPPER)) SplitUpperEdge(ptr);
		ptr->Set(glseg.x2, ztop[1], glseg.y2, tcs[UPRGT].u, tcs[UPRGT].v);
		ptr++;
		if (split && glseg.fracright == 1) SplitRightEdge(ptr);
		ptr->Set(glseg.x2, zbottom[1], glseg.y2, tcs[LORGT].u, tcs[LORGT].v);
		ptr++;
		if (split && !(flags & GLWF_NOSPLITLOWER)) SplitLowerEdge(ptr);
		vertcount = GLRenderer->mVBO->GetCount(ptr, &vertindex);
	}
}


//==========================================================================
//
// General purpose wall rendering function
// everything goes through here
//
//==========================================================================

void GLWall::RenderWall(int textured)
{
	gl_RenderState.Apply();
	gl_RenderState.ApplyLightIndex(dynlightindex);
	if (gl.buffermethod != BM_DEFERRED)
	{
		MakeVertices(!!(textured&RWF_NOSPLIT));
	}
	else if (vertcount == 0)
	{
		// This should never happen but in case it actually does, use the quad drawer as fallback (without edge splitting.)
		// This way it at least gets drawn.
		FQuadDrawer qd;
		qd.Set(0, glseg.x1, zbottom[0], glseg.y1, tcs[LOLFT].u, tcs[LOLFT].v);
		qd.Set(1, glseg.x1, ztop[0], glseg.y1, tcs[UPLFT].u, tcs[UPLFT].v);
		qd.Set(2, glseg.x2, ztop[1], glseg.y2, tcs[UPRGT].u, tcs[UPRGT].v);
		qd.Set(3, glseg.x2, zbottom[1], glseg.y2, tcs[LORGT].u, tcs[LORGT].v);
		qd.Render(GL_TRIANGLE_FAN);
		vertexcount += 4;
		return;
	}
	GLRenderer->mVBO->RenderArray(GL_TRIANGLE_FAN, vertindex, vertcount);
	vertexcount += vertcount;
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderFogBoundary()
{
	if (gl_fogmode && mDrawer->FixedColormap == 0)
	{
		if (!gl.legacyMode)
		{
			int rel = rellight + getExtraLight();
			mDrawer->SetFog(lightlevel, rel, &Colormap, false);
			gl_RenderState.EnableDrawBuffers(1);
			gl_RenderState.SetEffect(EFF_FOGBOUNDARY);
			gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(-1.0f, -128.0f);
			RenderWall(RWF_BLANK);
			glPolygonOffset(0.0f, 0.0f);
			glDisable(GL_POLYGON_OFFSET_FILL);
			gl_RenderState.SetEffect(EFF_NONE);
			gl_RenderState.EnableDrawBuffers(gl_RenderState.GetPassDrawBufferCount());
		}
		else
		{
			RenderFogBoundaryCompat();
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
void GLWall::RenderMirrorSurface()
{
	if (!GLRenderer->mirrorTexture.isValid()) return;

	// For the sphere map effect we need a normal of the mirror surface,
	FVector3 v = glseg.Normal();

	if (!gl.legacyMode)
	{
		// we use texture coordinates and texture matrix to pass the normal stuff to the shader so that the default vertex buffer format can be used as is.
		tcs[LOLFT].u = tcs[LORGT].u = tcs[UPLFT].u = tcs[UPRGT].u = v.X;
		tcs[LOLFT].v = tcs[LORGT].v = tcs[UPLFT].v = tcs[UPRGT].v = v.Z;

		gl_RenderState.EnableTextureMatrix(true);
		gl_RenderState.mTextureMatrix.computeNormalMatrix(gl_RenderState.mViewMatrix);
	}
	else
	{
		glNormal3fv(&v[0]);
	}

	// Use sphere mapping for this
	gl_RenderState.SetEffect(EFF_SPHEREMAP);

	mDrawer->SetColor(lightlevel, 0, Colormap ,0.1f);
	mDrawer->SetFog(lightlevel, 0, &Colormap, true);
	gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);
	gl_RenderState.AlphaFunc(GL_GREATER,0);
	glDepthFunc(GL_LEQUAL);

	FMaterial * pat=FMaterial::ValidateTexture(GLRenderer->mirrorTexture, false, false);
	gl_RenderState.SetMaterial(pat, CLAMP_NONE, 0, -1, false);

	flags &= ~GLWF_GLOW;
	RenderWall(RWF_BLANK);

	gl_RenderState.EnableTextureMatrix(false);
	gl_RenderState.SetEffect(EFF_NONE);

	// Restore the defaults for the translucent pass
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_sprite_threshold);
	glDepthFunc(GL_LESS);

	// This is drawn in the translucent pass which is done after the decal pass
	// As a result the decals have to be drawn here.
	if (seg->sidedef->AttachedDecals)
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(-1.0f, -128.0f);
		glDepthMask(false);
		DoDrawDecals();
		glDepthMask(true);
		glPolygonOffset(0.0f, 0.0f);
		glDisable(GL_POLYGON_OFFSET_FILL);
		gl_RenderState.SetTextureMode(TM_MODULATE);
		gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTextured(int rflags)
{
	int tmode = gl_RenderState.GetTextureMode();
	int rel = rellight + getExtraLight();

	if (flags & GLWF_GLOW)
	{
		gl_RenderState.EnableGlow(true);
		gl_RenderState.SetGlowParams(topglowcolor, bottomglowcolor);
	}
	gl_RenderState.SetGlowPlanes(topplane, bottomplane);
	gl_RenderState.SetMaterial(gltexture, flags & 3, 0, -1, false);

	if (type == RENDERWALL_M2SNF)
	{
		if (flags & GLT_CLAMPY)
		{
			if (tmode == TM_MODULATE) gl_RenderState.SetTextureMode(TM_CLAMPY);
		}
		mDrawer->SetFog(255, 0, NULL, false);
	}
	gl_RenderState.SetObjectColor(seg->frontsector->SpecialColors[sector_t::walltop] | 0xff000000);
	gl_RenderState.SetObjectColor2(seg->frontsector->SpecialColors[sector_t::wallbottom] | 0xff000000);

	float absalpha = fabsf(alpha);
	if (lightlist == NULL)
	{
		if (type != RENDERWALL_M2SNF) mDrawer->SetFog(lightlevel, rel, &Colormap, RenderStyle == STYLE_Add);
		mDrawer->SetColor(lightlevel, rel, Colormap, absalpha);
		RenderWall(rflags);
	}
	else
	{
		gl_RenderState.EnableSplit(true);

		for (unsigned i = 0; i < lightlist->Size(); i++)
		{
			secplane_t &lowplane = i == (*lightlist).Size() - 1 ? bottomplane : (*lightlist)[i + 1].plane;
			// this must use the exact same calculation method as GLWall::Process etc.
			float low1 = lowplane.ZatPoint(vertexes[0]);
			float low2 = lowplane.ZatPoint(vertexes[1]);

			if (low1 < ztop[0] || low2 < ztop[1])
			{
				int thisll = (*lightlist)[i].caster != NULL ? gl_ClampLight(*(*lightlist)[i].p_lightlevel) : lightlevel;
				FColormap thiscm;
				thiscm.FadeColor = Colormap.FadeColor;
				thiscm.FogDensity = Colormap.FogDensity;
				thiscm.CopyFrom3DLight(&(*lightlist)[i]);
				mDrawer->SetColor(thisll, rel, thiscm, absalpha);
				if (type != RENDERWALL_M2SNF) mDrawer->SetFog(thisll, rel, &thiscm, RenderStyle == STYLE_Add);
				gl_RenderState.SetSplitPlanes((*lightlist)[i].plane, lowplane);
				RenderWall(rflags);
			}
			if (low1 <= zbottom[0] && low2 <= zbottom[1]) break;
		}

		gl_RenderState.EnableSplit(false);
	}
	gl_RenderState.SetObjectColor(0xffffffff);
	gl_RenderState.SetObjectColor2(0);
	gl_RenderState.SetTextureMode(tmode);
	gl_RenderState.EnableGlow(false);
}

//==========================================================================
//
// 
//
//==========================================================================

void GLWall::RenderTranslucentWall()
{
	if (gltexture)
	{
		if (mDrawer->FixedColormap == CM_DEFAULT && gl_lights && gl.lightmethod == LM_DIRECT)
		{
			SetupLights();
		}
		if (!gltexture->GetTransparent()) gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
		else gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
		if (RenderStyle == STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA,GL_ONE);
		RenderTextured(RWF_TEXTURED | RWF_NOSPLIT);
		if (RenderStyle == STYLE_Add) gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else
	{
		gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
		mDrawer->SetColor(lightlevel, 0, Colormap, fabsf(alpha));
		mDrawer->SetFog(lightlevel, 0, &Colormap, RenderStyle == STYLE_Add);
		gl_RenderState.EnableTexture(false);
		RenderWall(RWF_NOSPLIT);
		gl_RenderState.EnableTexture(true);
	}
}

//==========================================================================
//
// 
//
//==========================================================================
void GLWall::Draw(int pass)
{
	gl_RenderState.SetNormal(glseg.Normal());
	switch (pass)
	{
	case GLPASS_LIGHTSONLY:
		SetupLights();
		break;

	case GLPASS_ALL:
		SetupLights();
		// fall through
	case GLPASS_PLAIN:
		RenderTextured(RWF_TEXTURED);
		break;

	case GLPASS_TRANSLUCENT:

		switch (type)
		{
		case RENDERWALL_MIRRORSURFACE:
			RenderMirrorSurface();
			break;

		case RENDERWALL_FOGBOUNDARY:
			RenderFogBoundary();
			break;

		default:
			RenderTranslucentWall();
			break;
		}
		break;

	case GLPASS_LIGHTTEX:
	case GLPASS_LIGHTTEX_ADDITIVE:
	case GLPASS_LIGHTTEX_FOGGY:
		RenderLightsCompat(pass);
		break;

	case GLPASS_TEXONLY:
		gl_RenderState.SetMaterial(gltexture, flags & 3, 0, -1, false);
		RenderWall(RWF_TEXTURED);
		break;
	}
}
