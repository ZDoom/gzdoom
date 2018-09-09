// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
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
** gl_light.cpp
** Light level / fog management / dynamic lights
**
**/

#include "gl_load/gl_system.h"
#include "gl_load/gl_interface.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"
#include "gl/scene/gl_portal.h"
#include "p_local.h"
#include "r_sky.h"




//==========================================================================
//
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be)
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,  };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1 };

	int srcblend = blendstyles[style.SrcAlpha%STYLEALPHA_MAX];
	int dstblend = blendstyles[style.DestAlpha%STYLEALPHA_MAX];
	int blendequation = renderops[style.BlendOp&15];
	int texturemode = drawopaque? TM_OPAQUE : TM_MODULATE;

	if (style.Flags & STYLEF_RedIsAlpha)
	{
		texturemode = TM_REDTOALPHA;
	}
	else if (style.Flags & STYLEF_ColorIsFixed)
	{
		texturemode = TM_MASK;
	}
	else if (style.Flags & STYLEF_InvertSource)
	{
		texturemode = TM_INVERSE;
	}

	if (blendequation == -1)
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	if (allowcolorblending && srcblend == GL_SRC_ALPHA && dstblend == GL_ONE && blendequation == GL_FUNC_ADD)
	{
		srcblend = GL_SRC_COLOR;
	}

	*tm = texturemode;
	*be = blendequation;
	*sb = srcblend;
	*db = dstblend;
}


//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon)
{ 
	if (fullbright)
	{
		gl_RenderState.SetColorAlpha(0xffffff, alpha, 0);
		gl_RenderState.SetSoftLightLevel(255);
	}
	else
	{
		int hwlightlevel = hw_CalcLightLevel(sectorlightlevel, rellight, weapon, cm.BlendFactor);
		PalEntry pe = hw_CalcLightColor(hwlightlevel, cm.LightColor, cm.BlendFactor);
		gl_RenderState.SetColorAlpha(pe, alpha, cm.Desaturation);
		gl_RenderState.SetSoftLightLevel(hw_ClampLight(sectorlightlevel + rellight), cm.BlendFactor);
	}
}

//==========================================================================
//
// Lighting stuff 
//
//==========================================================================

void gl_SetShaderLight(float level, float olight)
{
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 0.75f;

	if (level > 0)
	{
		float lightdist, lightfactor;
			
		if (olight < THRESHOLD)
		{
			lightdist = (MAXDIST/2) + (olight * MAXDIST / THRESHOLD / 2);
			olight = THRESHOLD;
		}
		else lightdist = MAXDIST;

		lightfactor = 1.f + ((olight/level) - 1.f) * FACTOR;
		if (lightfactor == 1.f) lightdist = 0.f;	// save some code in the shader
		gl_RenderState.SetLightParms(lightfactor, lightdist);
	}
	else
	{
		gl_RenderState.SetLightParms(1.f, 0.f);
	}
}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void gl_SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive)
{
	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else if (cmap != NULL && !fullbright)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = hw_GetFogDensity(lightlevel, fogcolor, cmap->FogDensity, cmap->BlendFactor);
		fogcolor.a=0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (GLRenderer->mPortalState.inskybox) fogdensity+=fogdensity/2;


	// no fog in enhanced vision modes!
	if (fogdensity==0 || gl_fogmode == 0)
	{
		gl_RenderState.EnableFog(false);
		gl_RenderState.SetFog(0,0);
	}
	else
	{
		if ((level.lightmode == 2 || (level.lightmode == 8 && cmap->BlendFactor > 0)) && fogcolor == 0)
		{
			float light = hw_CalcLightLevel(lightlevel, rellight, false, cmap->BlendFactor);
			gl_SetShaderLight(light, lightlevel);
		}
		else
		{
			gl_RenderState.SetLightParms(1.f, 0.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor=0;
		}

		gl_RenderState.EnableFog(true);
		gl_RenderState.SetFog(fogcolor, fogdensity);

		// Korshun: fullbright fog like in software renderer.
		if (level.lightmode == 8 && cmap->BlendFactor == 0 && level.brightfog && fogdensity != 0 && fogcolor != 0)
		{
			gl_RenderState.SetSoftLightLevel(255);
		}
	}
}
