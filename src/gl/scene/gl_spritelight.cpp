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
	return changed;
}



//==========================================================================
//
// Sets the light for a sprite - takes dynamic lights into account
//
//==========================================================================

static void gl_SetSpriteLight(AActor *self, fixed_t x, fixed_t y, fixed_t z, subsector_t * subsec, 
                              int lightlevel, int rellight, FColormap * cm, float alpha, bool weapon)
{
	float r,g,b;
	float result[4]; // Korshun.

	gl_GetLightColor(lightlevel, rellight, cm, &r, &g, &b, weapon);
	glColor4f(r, g, b, alpha);

	if (gl_GetSpriteLight(self, x, y, z, subsec, cm ? cm->colormap : 0, result))
	{
		gl_RenderState.SetDynLight(result[0], result[1], result[2]);

		if (glset.lightmode == 8)
		{
			gl_RenderState.SetSoftLightLevel(gl_ClampLight(lightlevel + rellight) / 255.f);
		}
	}
}

void gl_SetSpriteLight(AActor * thing, int lightlevel, int rellight, FColormap * cm, float alpha, bool weapon)
{ 
	subsector_t * subsec = thing->subsector;
	gl_SetSpriteLight(thing, thing->x, thing->y, thing->z+(thing->height>>1), subsec, lightlevel, rellight, cm, alpha, weapon);
}

void gl_SetSpriteLight(particle_t * thing, int lightlevel, int rellight, FColormap *cm, float alpha)
{ 
	gl_SetSpriteLight(NULL, thing->x, thing->y, thing->z, thing->subsector, lightlevel, rellight, cm, alpha, false);
}

//==========================================================================
//
// Sets render state to draw the given render style
//
//==========================================================================

void gl_SetSpriteLighting(FRenderStyle style, AActor *thing, int lightlevel, int rellight, FColormap *cm, 
						  float alpha, bool fullbright, bool weapon)
{
	FColormap internal_cm;

	if (gl_light_sprites && gl_lights && GLRenderer->mLightCount && !fullbright)
	{
		gl_SetSpriteLight(thing, lightlevel, rellight, cm, alpha, weapon);
	}
	else
	{
		gl_SetColor(lightlevel, rellight, cm, alpha, weapon);
	}
	gl_RenderState.AlphaFunc(GL_GEQUAL,alpha*gl_mask_sprite_threshold);
}

