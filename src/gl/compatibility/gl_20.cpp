/*
** gl_20.cpp
**
** Fallback code for ancient hardware
** This file collects everything larger that is only needed for
** OpenGL 2.0/no shader compatibility.
**
**---------------------------------------------------------------------------
** Copyright 2005-2016 Christoph Oelckers
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
** 4. Full disclosure of the entire project's source code, except for third
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
#include "menu/menu.h"
#include "tarray.h"
#include "doomtype.h"
#include "m_argv.h"
#include "zstring.h"
#include "version.h"
#include "i_system.h"
#include "v_text.h"
#include "r_utility.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/utility/gl_geometric.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"


//==========================================================================
//
// Do some tinkering with the menus so that certain options only appear
// when they are actually valid.
//
//==========================================================================

void gl_PatchMenu()
{
	if (gl.glslversion == 0)
	{
		// Radial fog and Doom lighting are not available in SM < 4 cards
		// The way they are implemented does not work well on older hardware.

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
	}
}


//==========================================================================
//
//
//
//==========================================================================

void gl_SetTextureMode(int type)
{
	static float white[] = {1.f,1.f,1.f,1.f};

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
	if (mTextureMode != ffTextureMode)
	{
		ffTextureMode = mTextureMode;
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
		FSpecialColormap *scm = &SpecialColormaps[gl_fixedcolormap - CM_FIRSTSPECIALCOLORMAP];
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
bool gl_SetupLight(int group, Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right,
	float & scale, int desaturation, bool checkside, bool forceadditive)
{
	Vector fn, pos;

	DVector3 lpos = light->PosRelative(group);

	float dist = fabsf(p.DistToPoint(lpos.X, lpos.Z, lpos.Y));
	float radius = (light->GetRadius() * gl_lights_size);

	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(lpos.X, lpos.Z, lpos.Y))
	{
		return false;
	}
	if (light->owned && light->target != NULL && !light->target->IsVisibleToPlayer())
	{
		return false;
	}

	scale = 1.0f / ((2.f * radius) - dist);

	// project light position onto plane (find closest point on plane)


	pos.Set(lpos.X, lpos.Z, lpos.Y);
	fn = p.Normal();
	fn.GetRightUp(right, up);

#ifdef _MSC_VER
	nearPt = pos + fn * dist;
#else
	Vector tmpVec = fn * dist;
	nearPt = pos + tmpVec;
#endif

	float cs = 1.0f - (dist / radius);
	if (gl_lights_additive || light->flags4&MF4_ADDITIVE || forceadditive) cs *= 0.2f;	// otherwise the light gets too strong.
	float r = light->GetRed() / 255.0f * cs * gl_lights_intensity;
	float g = light->GetGreen() / 255.0f * cs * gl_lights_intensity;
	float b = light->GetBlue() / 255.0f * cs * gl_lights_intensity;

	if (light->IsSubtractive())
	{
		Vector v;

		gl_RenderState.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
		v.Set(r, g, b);
		r = v.Length() - r;
		g = v.Length() - g;
		b = v.Length() - b;
	}
	else
	{
		gl_RenderState.BlendEquation(GL_FUNC_ADD);
	}
	if (desaturation > 0 && gl.glslversion > 0)	// no-shader excluded because no desaturated textures.
	{
		float gray = (r * 77 + g * 143 + b * 37) / 257;

		r = (r*(32 - desaturation) + gray*desaturation) / 32;
		g = (g*(32 - desaturation) + gray*desaturation) / 32;
		b = (b*(32 - desaturation) + gray*desaturation) / 32;
	}
	glColor3f(r, g, b);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

bool gl_SetupLightTexture()
{
	if (GLRenderer->gllight == NULL) return false;
	FMaterial * pat = FMaterial::ValidateTexture(GLRenderer->gllight, false);
	pat->Bind(CLAMP_XY, 0);
	return true;
}

//==========================================================================
//
//
//
//==========================================================================

void FGLRenderer::RenderMultipassStuff()
{
	return;
	// First pass: empty background with sector light only

	// Part 1: solid geometry. This is set up so that there are no transparent parts

	// remove any remaining texture bindings and shaders whick may get in the way.
	gl_RenderState.EnableTexture(false);
	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.Apply();
	gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_BASE);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_BASE);

	// Part 2: masked geometry. This is set up so that only pixels with alpha>0.5 will show
	// This creates a blank surface that only fills the nontransparent parts of the texture
	gl_RenderState.EnableTexture(true);
	gl_RenderState.SetTextureMode(TM_MASK);
	gl_RenderState.EnableBrightmap(true);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_BRIGHT].DrawWalls(GLPASS_BASE_MASKED);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_BASE_MASKED);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_BRIGHT].DrawFlats(GLPASS_BASE_MASKED);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_BASE_MASKED);

	// Part 3: The base of fogged surfaces, including the texture
	gl_RenderState.EnableBrightmap(false);
	gl_RenderState.SetTextureMode(TM_MODULATE);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_FOG].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_WALLS_FOGMASKED].DrawWalls(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_FOG].DrawFlats(GLPASS_PLAIN);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_FOGMASKED].DrawFlats(GLPASS_PLAIN);

	// second pass: draw lights
	glDepthMask(false);
	if (mLightCount && !gl_fixedcolormap)
	{
		if (gl_SetupLightTexture())
		{
			gl_RenderState.BlendFunc(GL_ONE, GL_ONE);
			glDepthFunc(GL_EQUAL);
			if (glset.lightmode == 8) gl_RenderState.SetSoftLightLevel(255);
			gl_drawinfo->dldrawlists[GLLDL_WALLS_PLAIN].DrawWalls(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_WALLS_BRIGHT].DrawWalls(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_LIGHTTEX);
			gl_drawinfo->dldrawlists[GLLDL_FLATS_BRIGHT].DrawFlats(GLPASS_LIGHTTEX);
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
	gl_drawinfo->dldrawlists[GLLDL_WALLS_BRIGHT].DrawWalls(GLPASS_TEXONLY);
	gl_drawinfo->dldrawlists[GLLDL_FLATS_BRIGHT].DrawFlats(GLPASS_TEXONLY);
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
		gl_drawinfo->dldrawlists[GLLDL_WALLS_BRIGHT].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_MASKED].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_PLAIN].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_BRIGHT].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_MASKED].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_FOG].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_WALLS_FOGMASKED].DrawWalls(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_FOG].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
		gl_drawinfo->dldrawlists[GLLDL_FLATS_FOGMASKED].DrawFlats(GLPASS_LIGHTTEX_ADDITIVE);
	}
	else gl_lights = false;
}

