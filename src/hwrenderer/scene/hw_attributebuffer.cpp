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

#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/utility/hw_lighting.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "p_local.h"
#include "r_sky.h"

//==========================================================================
//
//
//
//==========================================================================
void AttributeBufferData::CreateDefaultEntries(std::function<void(AttributeBufferData&)> callback)
{
    
    // Default settings for elements without any attributes (e.g. stencil planes)
    SetDefaults();
    uTextureMode = TM_MODULATE;
    callback(*this);
    
    // 2D settings, one for each of the existing texture modes. These will cover 99.9% of all 2D draw operations,
    // that is all except special colormaps, color overlays or desturation which are all extremely rare.

    uFogEnabled = 3;
    uLightIsAttr = true;
    uLightIndex = -1;
    uAlphaThreshold = -0.01f;

    for(int i=0;i<TM_COUNT;i++)
    {
        uTextureMode = TM_MODULATE+i;
        callback(*this);
    }
    
    // And one with a flipping texture matrix for camera textures
    uTextureMode = TM_OPAQUE;
    uTexMatrixIndex = 1;
    callback(*this);
}

//==========================================================================
//
// set current light color
//
//==========================================================================

void AttributeBufferData::SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon)
{
	if (fullbright)
	{
		SetColorAlpha(0xffffff, alpha, 0);
		SetSoftLightLevel(255);
	}
	else
	{
		int hwlightlevel = hw_CalcLightLevel(sectorlightlevel, rellight, weapon);
		PalEntry pe = hw_CalcLightColor(hwlightlevel, cm.LightColor, cm.BlendFactor);
		SetColorAlpha(pe, alpha, cm.Desaturation);
		SetSoftLightLevel(hw_ClampLight(sectorlightlevel + rellight));
	}
}

//==========================================================================
//
// Lighting stuff 
//
//==========================================================================

void AttributeBufferData::SetShaderLight(float level, float olight)
{
	const float MAXDIST = 256.f;
	const float THRESHOLD = 96.f;
	const float FACTOR = 0.75f;

	if (level > 0)
	{
		float lightdist, lightfactor;

		if (olight < THRESHOLD)
		{
			lightdist = (MAXDIST / 2) + (olight * MAXDIST / THRESHOLD / 2);
			olight = THRESHOLD;
		}
		else lightdist = MAXDIST;

		lightfactor = 1.f + ((olight / level) - 1.f) * FACTOR;
		if (lightfactor == 1.f) lightdist = 0.f;	// save some code in the shader
		SetLightParms(lightfactor, lightdist);
	}
	else
	{
		SetLightParms(1.f, 0.f);
	}
}


//==========================================================================
//
// Sets the fog for the current polygon
//
//==========================================================================

void AttributeBufferData::SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive)
{
	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity = 70;
		fogcolor = 0x808080;
	}
	else if (cmap != NULL && !fullbright)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = hw_GetFogDensity(lightlevel, fogcolor, cmap->FogDensity);
		fogcolor.a = 0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	//if (GLRenderer->mPortalState.inskybox) fogdensity += fogdensity / 2;


	// no fog in enhanced vision modes!
	if (fogdensity == 0 || gl_fogmode == 0)
	{
		uFogEnabled = 0;
		SetFog(0, 0);
	}
	else
	{
		if (level.lightmode == 2 && fogcolor == 0)
		{
			float light = hw_CalcLightLevel(lightlevel, rellight, false);
			SetShaderLight(light, lightlevel);
		}
		else
		{
			SetLightParms(1.f, 0.f);
		}

		// For additive rendering using the regular fog color here would mean applying it twice
		// so always use black
		if (isadditive)
		{
			fogcolor = 0;
		}

		uFogEnabled = fogcolor == 0 ? gl_fogmode : -gl_fogmode;

		SetFog(fogcolor, fogdensity);

		// Korshun: fullbright fog like in software renderer.
		if (level.lightmode == 8 && level.brightfog && fogdensity != 0 && fogcolor != 0)
		{
			SetSoftLightLevel(255);
		}
	}
}

