/*
** gl_light.cpp
** Light level / fog management / dynamic lights
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
#include "gl/system/gl_interface.h"
#include "gl/system/gl_cvars.h"
#include "gl/data/gl_data.h"
#include "gl/renderer/gl_colormap.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"
#include "gl/scene/gl_portal.h"
#include "c_dispatch.h"
#include "p_local.h"
#include "g_level.h"
#include "r_sky.h"

// externally settable lighting properties
static float distfogtable[2][256];	// light to fog conversion table for black fog
static PalEntry outsidefogcolor;
int fogdensity;
int outsidefogdensity;
int skyfog;

CUSTOM_CVAR (Int, gl_light_ambient, 20, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	// ambient of 0 does not work correctly because light level 0 is special.
	if (self < 1) self = 1;
}

CVAR(Int, gl_weaponlight, 8, CVAR_ARCHIVE);
CUSTOM_CVAR(Bool, gl_enhanced_nightvision, true, CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	// The fixed colormap state needs to be reset because if this happens when
	// a shader is set to CM_LITE or CM_TORCH it won't register the change in behavior caused by this CVAR.
	if (GLRenderer != NULL && GLRenderer->mShaderManager != NULL)
	{
		GLRenderer->mShaderManager->ResetFixedColormap();
	}
}
CVAR(Bool, gl_brightfog, false, CVAR_ARCHIVE);



//==========================================================================
//
// Sets up the fog tables
//
//==========================================================================

CUSTOM_CVAR (Int, gl_distfog, 70, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	for (int i=0;i<256;i++)
	{

		if (i<164)
		{
			distfogtable[0][i]= (gl_distfog>>1) + (gl_distfog)*(164-i)/164;
		}
		else if (i<230)
		{											    
			distfogtable[0][i]= (gl_distfog>>1) - (gl_distfog>>1)*(i-164)/(230-164);
		}
		else distfogtable[0][i]=0;

		if (i<128)
		{
			distfogtable[1][i]= 6.f + (gl_distfog>>1) + (gl_distfog)*(128-i)/48;
		}
		else if (i<216)
		{											    
			distfogtable[1][i]= (216.f-i) / ((216.f-128.f)) * gl_distfog / 10;
		}
		else distfogtable[1][i]=0;
	}
}

CUSTOM_CVAR(Int,gl_fogmode,1,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	if (self>2) self=2;
	if (self<0) self=0;
}

CUSTOM_CVAR(Int, gl_lightmode, 3 ,CVAR_ARCHIVE|CVAR_NOINITCALL)
{
	int newself = self;
	if (newself > 4) newself=8;	// use 8 for software lighting to avoid conflicts with the bit mask
	if (newself < 0) newself=0;
	if (self != newself) self = newself;
	glset.lightmode = newself;
}




//==========================================================================
//
// Sets render state to draw the given render style
// includes: Texture mode, blend equation and blend mode
//
//==========================================================================

void gl_GetRenderStyle(FRenderStyle style, bool drawopaque, bool allowcolorblending,
					   int *tm, int *sb, int *db, int *be)
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1 };

	int srcblend = blendstyles[style.SrcAlpha&3];
	int dstblend = blendstyles[style.DestAlpha&3];
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
		// The only place where InvertSource is used is for inverted sprites with the infrared powerup.
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
// Set fog parameters for the level
//
//==========================================================================
void gl_SetFogParams(int _fogdensity, PalEntry _outsidefogcolor, int _outsidefogdensity, int _skyfog)
{
	fogdensity=_fogdensity;
	outsidefogcolor=_outsidefogcolor;
	outsidefogdensity=_outsidefogdensity;
	skyfog=_skyfog;

	outsidefogdensity>>=1;
	fogdensity>>=1;
}


//==========================================================================
//
// Get current light level
//
//==========================================================================

int gl_CalcLightLevel(int lightlevel, int rellight, bool weapon)
{
	int light;

	if (lightlevel == 0) return 0;

	if ((glset.lightmode & 2) && lightlevel < 192 && !weapon) 
	{
		light = xs_CRoundToInt(192.f - (192-lightlevel)* 1.95f);
	}
	else
	{
		light=lightlevel;
	}

	if (light<gl_light_ambient && glset.lightmode != 8)		// ambient clipping only if not using software lighting model.
	{
		light = gl_light_ambient;
		if (rellight<0) rellight>>=1;
	}
	return clamp(light+rellight, 0, 255);
}

//==========================================================================
//
// Get current light color
//
//==========================================================================

static PalEntry gl_CalcLightColor(int light, PalEntry pe, int blendfactor)
{
	int r,g,b;

	if (glset.lightmode == 8)
	{
		return pe;
	}
	else if (blendfactor == 0)
	{
		r = pe.r * light / 255;
		g = pe.g * light / 255;
		b = pe.b * light / 255;
	}
	else
	{
		// This is what Legacy does with colored light in 3D volumes. No, it doesn't really make sense...
		// It also doesn't translate well to software style lighting.
		int mixlight = light * (255 - blendfactor);

		r = (mixlight + pe.r * blendfactor) / 255;
		g = (mixlight + pe.g * blendfactor) / 255;
		b = (mixlight + pe.b * blendfactor) / 255;
	}
	return PalEntry(255, BYTE(r), BYTE(g), BYTE(b));
}

//==========================================================================
//
// set current light color
//
//==========================================================================
void gl_SetColor(int sectorlightlevel, int rellight, const FColormap &cm, float alpha, bool weapon)
{ 
	if (gl_fixedcolormap != CM_DEFAULT)
	{
		gl_RenderState.SetColorAlpha(0xffffff, alpha, 0);
		gl_RenderState.SetSoftLightLevel(255);
	}
	else
	{
		int hwlightlevel = gl_CalcLightLevel(sectorlightlevel, rellight, weapon);
		PalEntry pe = gl_CalcLightColor(hwlightlevel, cm.LightColor, cm.blendfactor);
		gl_RenderState.SetColorAlpha(pe, alpha, cm.desaturation);
		gl_RenderState.SetSoftLightLevel(gl_ClampLight(sectorlightlevel + rellight));
	}
}

//==========================================================================
//
// calculates the current fog density
//
//	Rules for fog:
//
//  1. If bit 4 of gl_lightmode is set always use the level's fog density. 
//     This is what Legacy's GL render does.
//	2. black fog means no fog and always uses the distfogtable based on the level's fog density setting
//	3. If outside fog is defined and the current fog color is the same as the outside fog
//	   the engine always uses the outside fog density to make the fog uniform across the level.
//	   If the outside fog's density is undefined it uses the level's fog density and if that is
//	   not defined it uses a default of 70.
//	4. If a global fog density is specified it is being used for all fog on the level
//	5. If none of the above apply fog density is based on the light level as for the software renderer.
//
//==========================================================================

float gl_GetFogDensity(int lightlevel, PalEntry fogcolor)
{
	float density;

	if (glset.lightmode&4)
	{
		// uses approximations of Legacy's default settings.
		density = fogdensity? fogdensity : 18;
	}
	else if ((fogcolor.d & 0xffffff) == 0)
	{
		// case 1: black fog
		if (glset.lightmode != 8)
		{
			density=distfogtable[glset.lightmode!=0][gl_ClampLight(lightlevel)];
		}
		else
		{
			density = 0;
		}
	}
	else if (outsidefogdensity != 0 && outsidefogcolor.a!=0xff && (fogcolor.d & 0xffffff) == (outsidefogcolor.d & 0xffffff))
	{
		// case 2. outsidefogdensity has already been set as needed
		density=outsidefogdensity;
	}
	else  if (fogdensity!=0)
	{
		// case 3: level has fog density set
		density=fogdensity;
	}
	else if (lightlevel < 248)
	{
		// case 4: use light level
		density=clamp<int>(255-lightlevel,30,255);
	}
	else 
	{
		density = 0.f;
	}
	return density;
}


//==========================================================================
//
// Check fog by current lighting info
//
//==========================================================================

bool gl_CheckFog(FColormap *cm, int lightlevel)
{
	// Check for fog boundaries. This needs a few more checks for the sectors
	bool frontfog;

	PalEntry fogcolor = cm->FadeColor;

	if ((fogcolor.d & 0xffffff) == 0)
	{
		frontfog = false;
	}
	else if (outsidefogdensity != 0 && outsidefogcolor.a!=0xff && (fogcolor.d & 0xffffff) == (outsidefogcolor.d & 0xffffff))
	{
		frontfog = true;
	}
	else  if (fogdensity!=0 || (glset.lightmode & 4))
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
// Check if the current linedef is a candidate for a fog boundary
//
// Requirements for a fog boundary:
// - front sector has no fog
// - back sector has fog
// - at least one of both does not have a sky ceiling.
//
//==========================================================================

bool gl_CheckFog(sector_t *frontsector, sector_t *backsector)
{
	if (gl_fixedcolormap) return false;
	if (frontsector == backsector) return false;	// there can't be a boundary if both sides are in the same sector.

	// Check for fog boundaries. This needs a few more checks for the sectors

	PalEntry fogcolor = frontsector->ColorMap->Fade;

	if ((fogcolor.d & 0xffffff) == 0)
	{
		return false;
	}
	else if (outsidefogdensity != 0 && outsidefogcolor.a!=0xff && (fogcolor.d & 0xffffff) == (outsidefogcolor.d & 0xffffff))
	{
	}
	else  if (fogdensity!=0 || (glset.lightmode & 4))
	{
		// case 3: level has fog density set
	}
	else 
	{
		// case 4: use light level
		if (frontsector->lightlevel >= 248) return false;
	}

	fogcolor = backsector->ColorMap->Fade;

	if ((fogcolor.d & 0xffffff) == 0)
	{
	}
	else if (outsidefogdensity != 0 && outsidefogcolor.a!=0xff && (fogcolor.d & 0xffffff) == (outsidefogcolor.d & 0xffffff))
	{
		return false;
	}
	else  if (fogdensity!=0 || (glset.lightmode & 4))
	{
		// case 3: level has fog density set
		return false;
	}
	else 
	{
		// case 4: use light level
		if (backsector->lightlevel < 248) return false;
	}

	// in all other cases this might create more problems than it solves.
	return ((frontsector->GetTexture(sector_t::ceiling)!=skyflatnum || 
			 backsector->GetTexture(sector_t::ceiling)!=skyflatnum));
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

void gl_SetFog(int lightlevel, int rellight, const FColormap *cmap, bool isadditive)
{
	PalEntry fogcolor;
	float fogdensity;

	if (level.flags&LEVEL_HASFADETABLE)
	{
		fogdensity=70;
		fogcolor=0x808080;
	}
	else if (cmap != NULL && gl_fixedcolormap == 0)
	{
		fogcolor = cmap->FadeColor;
		fogdensity = gl_GetFogDensity(lightlevel, fogcolor);
		fogcolor.a=0;
	}
	else
	{
		fogcolor = 0;
		fogdensity = 0;
	}

	// Make fog a little denser when inside a skybox
	if (GLPortal::inskybox) fogdensity+=fogdensity/2;


	// no fog in enhanced vision modes!
	if (fogdensity==0 || gl_fogmode == 0)
	{
		gl_RenderState.EnableFog(false);
		gl_RenderState.SetFog(0,0);
	}
	else
	{
		if (glset.lightmode == 2 && fogcolor == 0)
		{
			float light = gl_CalcLightLevel(lightlevel, rellight, false);
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
		if (glset.lightmode == 8 && glset.brightfog && fogdensity != 0 && fogcolor != 0)
		{
			gl_RenderState.SetSoftLightLevel(255);
		}
	}
}


//==========================================================================
//
// For testing sky fog sheets
//
//==========================================================================
CCMD(skyfog)
{
	if (argv.argc()>1)
	{
		skyfog=strtol(argv[1],NULL,0);
	}
}

