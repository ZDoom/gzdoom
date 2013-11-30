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
#include "c_dispatch.h"
#include "p_local.h"
#include "p_effect.h"
#include "vectors.h"
#include "gl/gl_functions.h"
#include "g_level.h"

#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"


//==========================================================================
//
// Gets the light for a sprite - takes dynamic lights into account
//
//==========================================================================

bool gl_GetSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, int desaturation, float * out, line_t *line, int side)
{
	ADynamicLight *light;
	float frac, lr, lg, lb;
	float radius;
	bool changed = false;
	
	out[0]=out[1]=out[2]=0;

	for(int j=0;j<2;j++)
	{
		// Go through both light lists
		FLightNode * node = subsec->lighthead[j];
		while (node)
		{
			light=node->lightsource;
			if (!light->owned || light->target == NULL || light->target->IsVisibleToPlayer())
			{
				if (!(light->flags2&MF2_DORMANT) &&
					(!(light->flags4&MF4_DONTLIGHTSELF) || light->target != self))
				{
					float dist = FVector3(FIXED2FLOAT(x - light->x), FIXED2FLOAT(y - light->y), FIXED2FLOAT(z - light->z)).Length();
					radius = light->GetRadius() * gl_lights_size;

					if (dist < radius)
					{
						frac = 1.0f - (dist / radius);

						if (frac > 0)
						{
							if (line != NULL)
							{
								if (P_PointOnLineSide(light->x, light->y, line) != side)
								{
									node = node->nextLight;
									continue;
								}
							}
							lr = light->GetRed() / 255.0f * gl_lights_intensity;
							lg = light->GetGreen() / 255.0f * gl_lights_intensity;
							lb = light->GetBlue() / 255.0f * gl_lights_intensity;
							if (light->IsSubtractive())
							{
								float bright = FVector3(lr, lg, lb).Length();
								FVector3 lightColor(lr, lg, lb);
								lr = (bright - lr) * -1;
								lg = (bright - lg) * -1;
								lb = (bright - lb) * -1;
							}

							out[0] += lr * frac;
							out[1] += lg * frac;
							out[2] += lb * frac;
							changed = true;
						}
					}
				}
			}
			node = node->nextLight;
		}
	}

	// Desaturate dynamic lighting if applicable
	if (desaturation>0 && desaturation<=CM_DESAT31)
	{
		float gray=(out[0]*77 + out[1]*143 + out[2]*37)/257;

		out[0]= (out[0]*(31-desaturation)+ gray*desaturation)/31;
		out[1]= (out[1]*(31-desaturation)+ gray*desaturation)/31;
		out[2]= (out[2]*(31-desaturation)+ gray*desaturation)/31;
	}
	return changed;
}



//==========================================================================
//
// Sets the light for a sprite - takes dynamic lights into account
//
//==========================================================================

static int gl_SetSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, 
                              int lightlevel, int rellight, FColormap * cm, float alpha, 
							  PalEntry ThingColor, bool weapon)
{
	float r,g,b;
	float result[4]; // Korshun.

	gl_GetLightColor(lightlevel, rellight, cm, &r, &g, &b, weapon);
	bool res = gl_GetSpriteLight(self, x, y, z, subsec, cm? cm->colormap : 0, result);
	if (!res || glset.lightmode == 8)
	{
		r *= ThingColor.r/255.f;
		g *= ThingColor.g/255.f;
		b *= ThingColor.b/255.f;
		glColor4f(r, g, b, alpha);
		if (glset.lightmode == 8) 
		{
			glVertexAttrib1f(VATTR_LIGHTLEVEL, gl_CalcLightLevel(lightlevel, rellight, weapon) / 255.0f); // Korshun.
			gl_RenderState.SetDynLight(result[0], result[1], result[2]);
		}
		return lightlevel;
	}
	else
	{
		// Note: Due to subtractive lights the values can easily become negative so we have to clamp both
		// at the low and top end of the range!
		r = clamp<float>(result[0]+r, 0, 1.0f);
		g = clamp<float>(result[1]+g, 0, 1.0f);
		b = clamp<float>(result[2]+b, 0, 1.0f);

		float dlightlevel = r*77 + g*143 + b*35;

		r *= ThingColor.r/255.f;
		g *= ThingColor.g/255.f;
		b *= ThingColor.b/255.f;

		glColor4f(r, g, b, alpha);		

		if (dlightlevel == 0) return 0;

		if (glset.lightmode&2 && dlightlevel<192.f) 
		{
			return xs_CRoundToInt(192.f - (192.f - dlightlevel) / 1.95f);
		}
		else
		{
			return xs_CRoundToInt(dlightlevel);
		}
	}
}

int gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm,
					   float alpha, PalEntry ThingColor, bool weapon)
{ 
	subsector_t * subsec = thing->subsector;
	return gl_SetSpriteLight(thing, thing->x, thing->y, thing->z+(thing->height>>1), subsec, 
					  lightlevel, rellight, cm, alpha, ThingColor, weapon);
}

int gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha, PalEntry ThingColor)
{ 
	return gl_SetSpriteLight(NULL, thing->x, thing->y, thing->z, thing->subsector, lightlevel, rellight, 
					  cm, alpha, ThingColor, false);
}

//==========================================================================
//
// Modifies the color values depending on the render style
//
//==========================================================================

void gl_GetSpriteLighting(FRenderStyle style, AActor *thing, FColormap *cm, PalEntry &ThingColor)
{
	if (style.Flags & STYLEF_RedIsAlpha)
	{
		cm->colormap = CM_SHADE;
	}
	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{
			ThingColor = PalEntry(thing->fillcolor).InverseColor();
		}
		else
		{
			ThingColor = thing->fillcolor;
		}
	}

	// This doesn't work like in the software renderer.
	if (style.Flags & STYLEF_InvertSource)
	{
		int gray = (cm->LightColor.r*77 + cm->LightColor.r*143 + cm->LightColor.r*36)>>8;
		cm->LightColor.r = cm->LightColor.g = cm->LightColor.b = gray;
	}
}


//==========================================================================
//
// Sets render state to draw the given render style
//
//==========================================================================

int gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  PalEntry ThingColor, float alpha, bool fullbright, bool weapon)
{
	FColormap internal_cm;

	if (style.Flags & STYLEF_RedIsAlpha)
	{
		cm->colormap = CM_SHADE;
	}
	if (style.Flags & STYLEF_ColorIsFixed)
	{
		if (style.Flags & STYLEF_InvertSource)
		{
			ThingColor = PalEntry(thing->fillcolor).InverseColor();
		}
		else
		{
			ThingColor = thing->fillcolor;
		}
		gl_ModifyColor(ThingColor.r, ThingColor.g, ThingColor.b, cm->colormap);
	}

	// This doesn't work like in the software renderer.
	if (style.Flags & STYLEF_InvertSource)
	{
		internal_cm = *cm;
		cm = &internal_cm;

		int gray = (internal_cm.LightColor.r*77 + internal_cm.LightColor.r*143 + internal_cm.LightColor.r*36)>>8;
		cm->LightColor.r = cm->LightColor.g = cm->LightColor.b = gray;
	}

	if (style.BlendOp == STYLEOP_Shadow)
	{
		glColor4f(0.2f * ThingColor.r / 255.f, 0.2f * ThingColor.g / 255.f, 
					0.2f * ThingColor.b / 255.f, (alpha = 0.33f));
	}
	else
	{
		if (gl_light_sprites && gl_lights && GLRenderer->mLightCount && !fullbright)
		{
			lightlevel = gl_SetSpriteLight(thing, lightlevel, rellight, cm, alpha, ThingColor, weapon);
		}
		else
		{
			gl_SetColor(lightlevel, rellight, cm, alpha, ThingColor, weapon);
		}
	}
	gl_RenderState.AlphaFunc(GL_GEQUAL,alpha*gl_mask_sprite_threshold);
	return lightlevel;
}

