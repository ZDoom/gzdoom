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
** gl_20.cpp
**
** Fallback code for ancient hardware
** This file collects everything larger that is only needed for
** OpenGL 2.0/no shader compatibility.
**
*/

#include "gl/system/gl_system.h"
#include "menu/menu.h"
#include "tarray.h"
#include "doomtype.h"
#include "m_argv.h"
#include "zstring.h"
#include "i_system.h"
#include "v_text.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "g_levellocals.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/utility/gl_geometric.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_scenedrawer.h"
#include "gl/data/gl_vertexbuffer.h"


CVAR(Bool, gl_lights_additive, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_legacy_mode, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOSET)

//==========================================================================
//
// Do some tinkering with the menus so that certain options only appear
// when they are actually valid.
//
//==========================================================================

void gl_PatchMenu()
{
	// Radial fog and Doom lighting are not available without full shader support.
	if (!gl_legacy_mode) return;

	FOptionValues **opt = OptionValues.CheckKey("LightingModes");
	if (opt != NULL) 
	{
		for(int i = (*opt)->mValues.Size()-1; i>=0; i--)
		{
			// Delete 'Doom' lighting mode
			if ((*opt)->mValues[i].Value == 2.0 || (*opt)->mValues[i].Value == 8.0)
			{
				(*opt)->mValues.Delete(i);
			}
		}
	}

	opt = OptionValues.CheckKey("FogMode");
	if (opt != NULL) 
	{
		for(int i = (*opt)->mValues.Size()-1; i>=0; i--)
		{
			// Delete 'Radial' fog mode
			if ((*opt)->mValues[i].Value == 2.0)
			{
				(*opt)->mValues.Delete(i);
			}
		}
	}

	// disable features that don't work without shaders.
	if (gl_lightmode == 2 || gl_lightmode == 8) gl_lightmode = 3;
	if (gl_fogmode == 2) gl_fogmode = 1;

	// remove more unsupported stuff like postprocessing options.
	// This cannot be done with a menu filter because the renderer gets initialized long after the menu is set up.
	DMenuDescriptor **desc = MenuDescriptors.CheckKey("OpenGLOptions");
	if (desc != nullptr && (*desc)->IsKindOf(RUNTIME_CLASS(DOptionMenuDescriptor)))
	{
		auto md = static_cast<DOptionMenuDescriptor*>(*desc);
		for (int i = md->mItems.Size() - 1; i >= 0; i--)
		{
			if (!stricmp(md->mItems[i]->mAction.GetChars(), "gl_multisample") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_tonemap") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_bloom") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_lens") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_ssao") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_ssao_portals") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_fxaa") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_paltonemap_powtable") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "vr_mode") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "vr_enable_quadbuffered") ||
				!stricmp(md->mItems[i]->mAction.GetChars(), "gl_paltonemap_reverselookup"))
			{
				md->mItems.Delete(i);
			}
		}
	}
}


//==========================================================================
//
//
//
//==========================================================================

void gl_SetTextureMode(int type)
{
	if (type == TM_MASK)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_OPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERSE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
	}
	else if (type == TM_INVERTOPAQUE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE0);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_ONE_MINUS_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE); 
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
	else // if (type == TM_MODULATE)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}

//==========================================================================
//
//
//
//==========================================================================

static int ffTextureMode;
static bool ffTextureEnabled;
static bool ffFogEnabled;
static PalEntry ffFogColor;
static int ffSpecialEffect;
static float ffFogDensity;
static bool currentTextureMatrixState;
static bool currentModelMatrixState;

void FRenderState::ApplyFixedFunction()
{
	int thistm = mTextureMode == TM_MODULATE && mTempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode;
	if (thistm != ffTextureMode)
	{
		ffTextureMode = thistm;
		if (ffTextureMode == TM_CLAMPY) ffTextureMode = TM_MODULATE;	// this cannot be replicated. Too bad if it creates visual artifacts
		gl_SetTextureMode(ffTextureMode);
	}
	if (mTextureEnabled != ffTextureEnabled)
	{
		if ((ffTextureEnabled = mTextureEnabled)) glEnable(GL_TEXTURE_2D);
		else glDisable(GL_TEXTURE_2D);
	}
	if (mFogEnabled != ffFogEnabled)
	{
		if ((ffFogEnabled = mFogEnabled))
		{
			glEnable(GL_FOG);
		}
		else glDisable(GL_FOG);
	}
	if (mFogEnabled)
	{
		if (ffFogColor != mFogColor)
		{
			ffFogColor = mFogColor;
			GLfloat FogColor[4] = { mFogColor.r / 255.0f,mFogColor.g / 255.0f,mFogColor.b / 255.0f,0.0f };
			glFogfv(GL_FOG_COLOR, FogColor);
		}
		if (ffFogDensity != mLightParms[2])
		{
			glFogf(GL_FOG_DENSITY, mLightParms[2] * -0.6931471f);	// = 1/log(2)
			ffFogDensity = mLightParms[2];
		}
	}
	if (mSpecialEffect != ffSpecialEffect)
	{
		switch (ffSpecialEffect)
		{
		case EFF_SPHEREMAP:
			glDisable(GL_TEXTURE_GEN_T);
			glDisable(GL_TEXTURE_GEN_S);

		default:
			break;
		}
		switch (mSpecialEffect)
		{
		case EFF_SPHEREMAP:
			// Use sphere mapping for this
			glEnable(GL_TEXTURE_GEN_T);
			glEnable(GL_TEXTURE_GEN_S);
			glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			break;

		default:
			break;
		}
		ffSpecialEffect = mSpecialEffect;
	}

	FStateVec4 col = mColor;

	col.vec[0] += mDynColor.vec[0];
	col.vec[1] += mDynColor.vec[1];
	col.vec[2] += mDynColor.vec[2];
	col.vec[0] = clamp(col.vec[0], 0.f, 1.f);

	col.vec[0] = clamp(col.vec[0], 0.f, 1.f);
	col.vec[1] = clamp(col.vec[1], 0.f, 1.f);
	col.vec[2] = clamp(col.vec[2], 0.f, 1.f);
	col.vec[3] = clamp(col.vec[3], 0.f, 1.f);

	col.vec[0] *= (mObjectColor.r / 255.f);
	col.vec[1] *= (mObjectColor.g / 255.f);
	col.vec[2] *= (mObjectColor.b / 255.f);
	col.vec[3] *= (mObjectColor.a / 255.f);
	glColor4fv(col.vec);

	glEnable(GL_BLEND);
	if (mAlphaThreshold > 0)
	{
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, mAlphaThreshold * col.vec[3]);
	}
	else
	{
		glDisable(GL_ALPHA_TEST);
	}

	if (mTextureMatrixEnabled)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf(mTextureMatrix.get());
		currentTextureMatrixState = true;
	}
	else if (currentTextureMatrixState)
	{
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		currentTextureMatrixState = false;
	}

	if (mModelMatrixEnabled)
	{
		VSMatrix mult = mViewMatrix;
		mult.multMatrix(mModelMatrix);
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(mult.get());
		currentModelMatrixState = true;
	}
	else if (currentModelMatrixState)
	{
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(mViewMatrix.get());
		currentModelMatrixState = false;
	}


}

//==========================================================================
//
//
//
//==========================================================================

void gl_FillScreen();

void FRenderState::DrawColormapOverlay()
{
	float r, g, b;
	if (mColormapState > CM_DEFAULT && mColormapState < CM_MAXCOLORMAP)
	{
		FSpecialColormap *scm = &SpecialColormaps[mColormapState - CM_FIRSTSPECIALCOLORMAP];
		float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
			scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

		if (m[0] < 0 && m[1] < 0 && m[2] < 0)
		{
			gl_RenderState.SetColor(1, 1, 1, 1);
			gl_RenderState.BlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
			gl_FillScreen();

			r = scm->ColorizeStart[0];
			g = scm->ColorizeStart[1];
			b = scm->ColorizeStart[2];
		}
		else
		{
			r = scm->ColorizeEnd[0];
			g = scm->ColorizeEnd[1];
			b = scm->ColorizeEnd[2];
		}
	}
	else if (mColormapState == CM_LITE)
	{
		if (gl_enhanced_nightvision)
		{
			r = 0.375f, g = 1.0f, b = 0.375f;
		}
		else
		{
			return;
		}
	}
	else if (mColormapState >= CM_TORCH)
	{
		int flicker = mColormapState - CM_TORCH;
		r = (0.8f + (7 - flicker) / 70.0f);
		if (r > 1.0f) r = 1.0f;
		b = g = r;
		if (gl_enhanced_nightvision) b = g * 0.75f;
	}
	else return;

	gl_RenderState.SetColor(r, g, b, 1.f);
	gl_RenderState.BlendFunc(GL_DST_COLOR, GL_ZERO);
	gl_FillScreen();
	gl_RenderState.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================

bool gl_SetupLight(int group, Plane & p, ADynamicLight * light, FVector3 & nearPt, FVector3 & up, FVector3 & right,
	float & scale, bool checkside, bool additive)
{
	FVector3 fn, pos;

	DVector3 lpos = light->PosRelative(group);

	float dist = fabsf(p.DistToPoint(lpos.X, lpos.Z, lpos.Y));
	float radius = light->GetRadius();

	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(lpos.X, lpos.Z, lpos.Y))
	{
		return false;
	}
	if (!light->visibletoplayer)
	{
		return false;
	}

	scale = 1.0f / ((2.f * radius) - dist);

	// project light position onto plane (find closest point on plane)


	pos = { (float)lpos.X, (float)lpos.Z, (float)lpos.Y };
	fn = p.Normal();
	fn.GetRightUp(right, up);

	FVector3 tmpVec = fn * dist;
	nearPt = pos + tmpVec;

	float cs = 1.0f - (dist / radius);
	if (additive) cs *= 0.2f;	// otherwise the light gets too strong.
	float r = light->GetRed() / 255.0f * cs;
	float g = light->GetGreen() / 255.0f * cs;
	float b = light->GetBlue() / 255.0f * cs;

	if (light->IsSubtractive())
	{
		gl_RenderState.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		float length = float(FVector3(r, g, b).Length());
		r = length - r;
		g = length - g;
		b = length - b;
	}
	else
	{
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
	}
	gl_RenderState.SetColor(r, g, b);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool gl_SetupLightTexture()
{
	if (!GLRenderer->glLight.isValid()) return false;
	FMaterial * pat = FMaterial::ValidateTexture(GLRenderer->glLight, false, false);
	gl_RenderState.SetMaterial(pat, CLAMP_XY_NOMIP, 0, -1, false);
	return true;
}

//==========================================================================
//
// Check fog in current sector for placing into the proper draw list.
//
//==========================================================================

static bool gl_CheckFog(FColormap *cm, int lightlevel)
{
	bool frontfog;

	PalEntry fogcolor = cm->FadeColor;

	if ((fogcolor.d & 0xffffff) == 0)
	{
		frontfog = false;
	}
	else if (level.outsidefogdensity != 0 && APART(level.info->outsidefog) != 0xff && (fogcolor.d & 0xffffff) == (level.info->outsidefog & 0xffffff))
	{
		frontfog = true;
	}
	else  if (level.fogdensity != 0 || (glset.lightmode & 4) || cm->FogDensity > 0)
	{
		// case 3: level has fog density set
		frontfog = true;
	}
	else
	{
		// case 4: use light level
		frontfog = lightlevel < 248;
	}
	return frontfog;
}

//==========================================================================
//
//
//
//==========================================================================

bool GLWall::PutWallCompat(int passflag)
{
	static int list_indices[2][2] =
	{ { GLLDL_WALLS_PLAIN, GLLDL_WALLS_FOG },{ GLLDL_WALLS_MASKED, GLLDL_WALLS_FOGMASKED } };

	// are lights possible?
	if (mDrawer->FixedColormap != CM_DEFAULT || !gl_lights || seg->sidedef == nullptr || type == RENDERWALL_M2SNF || !gltexture) return false;

	// multipassing these is problematic.
	if ((flags&GLWF_SKYHACK && type == RENDERWALL_M2S)) return false;

	// Any lights affecting this wall?
	if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
	{
		if (seg->sidedef->lighthead == nullptr) return false;
	}
	else if (sub)
	{
		if (sub->lighthead == nullptr) return false;
	}

	bool foggy = gl_CheckFog(&Colormap, lightlevel) || (level.flags&LEVEL_HASFADETABLE) || gl_lights_additive;
	bool masked = passflag == 2 && gltexture->isMasked();

	int list = list_indices[masked][foggy];
	gl_drawinfo->dldrawlists[list].AddWall(this);
	return true;

}

//==========================================================================
//
//
//
//==========================================================================

bool GLFlat::PutFlatCompat(bool fog)
{
	// are lights possible?
	if (mDrawer->FixedColormap != CM_DEFAULT || !gl_lights || !gltexture || renderstyle != STYLE_Translucent || alpha < 1.f - FLT_EPSILON || sector->lighthead == NULL) return false;

	static int list_indices[2][2] =
	{ { GLLDL_FLATS_PLAIN, GLLDL_FLATS_FOG },{ GLLDL_FLATS_MASKED, GLLDL_FLATS_FOGMASKED } };

	bool masked = gltexture->isMasked() && ((renderflags&SSRF_RENDER3DPLANES) || stack);
	bool foggy = gl_CheckFog(&Colormap, lightlevel) || (level.flags&LEVEL_HASFADETABLE) || gl_lights_additive;

	
	int list = list_indices[masked][foggy];
	gl_drawinfo->dldrawlists[list].AddFlat(this);
	return true;
}


//==========================================================================
//
// Fog boundary without any shader support
//
//==========================================================================

void GLWall::RenderFogBoundaryCompat()
{
	// without shaders some approximation is needed. This won't look as good
	// as the shader version but it's an acceptable compromise.
	float fogdensity = gl_GetFogDensity(lightlevel, Colormap.FadeColor, Colormap.FogDensity);

	float dist1 = Dist2(r_viewpoint.Pos.X, r_viewpoint.Pos.Y, glseg.x1, glseg.y1);
	float dist2 = Dist2(r_viewpoint.Pos.X, r_viewpoint.Pos.Y, glseg.x2, glseg.y2);

	// these values were determined by trial and error and are scale dependent!
	float fogd1 = (0.95f - exp(-fogdensity*dist1 / 62500.f)) * 1.05f;
	float fogd2 = (0.95f - exp(-fogdensity*dist2 / 62500.f)) * 1.05f;

	float fc[4] = { Colormap.FadeColor.r / 255.0f,Colormap.FadeColor.g / 255.0f,Colormap.FadeColor.b / 255.0f,fogd2 };

	gl_RenderState.EnableTexture(false);
	gl_RenderState.EnableFog(false);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0);
	gl_RenderState.Apply();
	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(-1.0f, -128.0f);
	glDepthFunc(GL_LEQUAL);
	glColor4f(fc[0], fc[1], fc[2], fogd1);
	glBegin(GL_TRIANGLE_FAN);
	glTexCoord2f(tcs[LOLFT].u, tcs[LOLFT].v);
	glVertex3f(glseg.x1, zbottom[0], glseg.y1);
	glTexCoord2f(tcs[UPLFT].u, tcs[UPLFT].v);
	glVertex3f(glseg.x1, ztop[0], glseg.y1);
	glColor4f(fc[0], fc[1], fc[2], fogd2);
	glTexCoord2f(tcs[UPRGT].u, tcs[UPRGT].v);
	glVertex3f(glseg.x2, ztop[1], glseg.y2);
	glTexCoord2f(tcs[LORGT].u, tcs[LORGT].v);
	glVertex3f(glseg.x2, zbottom[1], glseg.y2);
	glEnd();
	glDepthFunc(GL_LESS);
	glPolygonOffset(0.0f, 0.0f);
	glDisable(GL_POLYGON_OFFSET_FILL);
	gl_RenderState.EnableFog(true);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.5f);
	gl_RenderState.EnableTexture(true);
}

//==========================================================================
//
// Flats 
//
//==========================================================================

void GLFlat::DrawSubsectorLights(subsector_t * sub, int pass)
{
	Plane p;
	FVector3 nearPt, up, right, t1;
	float scale;

	FLightNode * node = sub->lighthead;
	while (node)
	{
		ADynamicLight * light = node->lightsource;

		if (light->flags2&MF2_DORMANT ||
			(pass == GLPASS_LIGHTTEX && light->IsAdditive()) ||
			(pass == GLPASS_LIGHTTEX_ADDITIVE && !light->IsAdditive()))
		{
			node = node->nextLight;
			continue;
		}

		// we must do the side check here because gl_SetupLight needs the correct plane orientation
		// which we don't have for Legacy-style 3D-floors
		double planeh = plane.plane.ZatPoint(light);
		if (gl_lights_checkside && ((planeh<light->Z() && ceiling) || (planeh>light->Z() && !ceiling)))
		{
			node = node->nextLight;
			continue;
		}

		p.Set(plane.plane);
		if (!gl_SetupLight(sub->sector->PortalGroup, p, light, nearPt, up, right, scale, false, pass != GLPASS_LIGHTTEX))
		{
			node = node->nextLight;
			continue;
		}
		gl_RenderState.Apply();

		FFlatVertex *ptr = GLRenderer->mVBO->GetBuffer();
		for (unsigned int k = 0; k < sub->numlines; k++)
		{
			vertex_t *vt = sub->firstline[k].v1;
			ptr->x = vt->fX();
			ptr->z = plane.plane.ZatPoint(vt) + dz;
			ptr->y = vt->fY();
			t1 = { ptr->x, ptr->z, ptr->y };
			FVector3 nearToVert = t1 - nearPt;

			ptr->u = ((nearToVert | right) * scale) + 0.5f;
			ptr->v = ((nearToVert | up) * scale) + 0.5f;
			ptr++;
		}
		GLRenderer->mVBO->RenderCurrent(ptr, GL_TRIANGLE_FAN);
		node = node->nextLight;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void GLFlat::DrawLightsCompat(int pass)
{
	gl_RenderState.Apply();
	// Draw the subsectors belonging to this sector
	for (int i = 0; i<sector->subsectorcount; i++)
	{
		subsector_t * sub = sector->subsectors[i];
		if (gl_drawinfo->ss_renderflags[sub->Index()] & renderflags)
		{
			DrawSubsectorLights(sub, pass);
		}
	}

	// Draw the subsectors assigned to it due to missing textures
	if (!(renderflags&SSRF_RENDER3DPLANES))
	{
		gl_subsectorrendernode * node = (renderflags&SSRF_RENDERFLOOR) ?
			gl_drawinfo->GetOtherFloorPlanes(sector->sectornum) :
			gl_drawinfo->GetOtherCeilingPlanes(sector->sectornum);

		while (node)
		{
			DrawSubsectorLights(node->sub, pass);
			node = node->next;
		}
	}
}


//==========================================================================
//
// Sets up the texture coordinates for one light to be rendered
//
//==========================================================================
bool GLWall::PrepareLight(ADynamicLight * light, int pass)
{
	float vtx[] = { glseg.x1,zbottom[0],glseg.y1, glseg.x1,ztop[0],glseg.y1, glseg.x2,ztop[1],glseg.y2, glseg.x2,zbottom[1],glseg.y2 };
	Plane p;
	FVector3 nearPt, up, right;
	float scale;

	p.Set(&glseg);

	if (!p.ValidNormal())
	{
		return false;
	}

	if (!gl_SetupLight(seg->frontsector->PortalGroup, p, light, nearPt, up, right, scale, true, pass != GLPASS_LIGHTTEX))
	{
		return false;
	}

	FVector3 t1;
	int outcnt[4] = { 0,0,0,0 };

	for (int i = 0; i<4; i++)
	{
		t1 = &vtx[i * 3];
		FVector3 nearToVert = t1 - nearPt;
		tcs[i].u = ((nearToVert | right) * scale) + 0.5f;
		tcs[i].v = ((nearToVert | up) * scale) + 0.5f;

		// quick check whether the light touches this polygon
		if (tcs[i].u<0) outcnt[0]++;
		if (tcs[i].u>1) outcnt[1]++;
		if (tcs[i].v<0) outcnt[2]++;
		if (tcs[i].v>1) outcnt[3]++;

	}
	// The light doesn't touch this polygon
	if (outcnt[0] == 4 || outcnt[1] == 4 || outcnt[2] == 4 || outcnt[3] == 4) return false;

	draw_dlight++;
	return true;
}


void GLWall::RenderLightsCompat(int pass)
{
	FLightNode * node;

	// black fog is diminishing light and should affect lights less than the rest!
	if (pass == GLPASS_LIGHTTEX) mDrawer->SetFog((255 + lightlevel) >> 1, 0, NULL, false);
	else mDrawer->SetFog(lightlevel, 0, &Colormap, true);

	if (seg->sidedef == NULL)
	{
		return;
	}
	else if (!(seg->sidedef->Flags & WALLF_POLYOBJ))
	{
		// Iterate through all dynamic lights which touch this wall and render them
		node = seg->sidedef->lighthead;
	}
	else if (sub)
	{
		// To avoid constant rechecking for polyobjects use the subsector's lightlist instead
		node = sub->lighthead;
	}
	else
	{
		return;
	}

	texcoord save[4];
	memcpy(save, tcs, sizeof(tcs));
	while (node)
	{
		ADynamicLight * light = node->lightsource;

		if (light->flags2&MF2_DORMANT ||
			(pass == GLPASS_LIGHTTEX && light->IsAdditive()) ||
			(pass == GLPASS_LIGHTTEX_ADDITIVE && !light->IsAdditive()))
		{
			node = node->nextLight;
			continue;
		}
		if (PrepareLight(light, pass))
		{
			vertcount = 0;
			RenderWall(RWF_TEXTURED);
		}
		node = node->nextLight;
	}
	memcpy(tcs, save, sizeof(tcs));
	vertcount = 0;
}

//==========================================================================
//
//
//
//==========================================================================

void GLSceneDrawer::RenderMultipassStuff()
{
	// First pass: empty background with sector light only

	// Part 1: solid geometry. This is set up so that there are no transparent parts

	// remove any remaining texture bindings and shaders whick may get in the way.
	gl_RenderState.EnableTexture(false);
	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.Apply();
	gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_PLAIN);

	// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
	// This creates a blank surface that only fills the nontransparent parts of the texture
	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetTextureMode(TM_MASK);
	gl_RenderState.EnableBrightmap(true);
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_PLAIN);

	// Part 3: The base of fogged surfaces, including the texture
	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_FOG].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_FOG].DrawFlats(GLPASS_PLAIN);
	gl_RenderState.AlphaFunc(GL_GEQUAL, gl_mask_threshold);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_FOGMASKED].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_FOGMASKED].DrawFlats(GLPASS_PLAIN);

	// second pass: draw lights
	glDepthMask(false);
	if (GLRenderer->mLightCount && !FixedColormap)
	{
		if (gl_SetupLightTexture())
		{
			gl_RenderState.BlendFunc(GL_ONE, GL_ONE);
			glDepthFunc(GL_EQUAL);
			if (glset.lightmode == 8) gl_RenderState.SetSoftLightLevel(255);
			gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_LIGHTTEX);
			gl_RenderState.BlendEquation(GL_FUNC_ADD);
		}
		else gl_lights = false;
	}

	// third pass: modulated texture
	gl_RenderState.SetColor(0xffffffff);
	gl_RenderState.BlendFunc(GL_DST_COLOR, GL_ZERO);
	gl_RenderState.EnableFog(false);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0);
	glDepthFunc(GL_LEQUAL);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_TEXONLY);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_TEXONLY);
	gl_RenderState.AlphaFunc(GL_GREATER, gl_mask_threshold);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_TEXONLY);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_TEXONLY);

	// fourth pass: additive lights
	gl_RenderState.EnableFog(true);
	gl_RenderState.BlendFunc(GL_ONE, GL_ONE);
	glDepthFunc(GL_EQUAL);
	if (gl_SetupLightTexture())
	{
		gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_FOG].DrawWalls(GLPASS_LIGHTTEX_FOGGY);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_FOGMASKED].DrawWalls(GLPASS_LIGHTTEX_FOGGY);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_FOG].DrawFlats(GLPASS_LIGHTTEX_FOGGY);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_FOGMASKED].DrawFlats(GLPASS_LIGHTTEX_FOGGY);
	}
	else gl_lights = false;

	glDepthFunc(GL_LESS);
	gl_RenderState.AlphaFunc(GL_GEQUAL, 0.f);
	gl_RenderState.EnableFog(true);
	gl_RenderState.BlendFunc(GL_ONE, GL_ZERO);
	glDepthMask(true);

}

