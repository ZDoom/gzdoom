/*
** gl_dynlight1.cpp
** dynamic light application
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
#include "vectors.h"
#include "gl/gl_functions.h"
#include "g_level.h"

#include "gl/system/gl_interface.h"
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
// Light related CVARs
//
//==========================================================================

CUSTOM_CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self) gl_RecreateAllAttachedLights();
	else gl_DeleteAllAttachedLights();
}

CUSTOM_CVAR (Bool, gl_dynlight_shader, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self && (gl.maxuniforms < 1024 || gl.shadermodel < 4)) self = false;
}

CVAR (Bool, gl_attachedlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_intensity, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Float, gl_lights_size, 1.0f, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CUSTOM_CVAR (Bool, gl_lights_additive, false,  CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	gl_DeleteAllAttachedLights();
	gl_RecreateAllAttachedLights();
}

//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================
bool gl_GetLight(Plane & p, ADynamicLight * light,
				 int desaturation, bool checkside, bool forceadditive, FDynLightData &ldata)
{
	Vector fn, pos;
	int i = 0;

    float x = FIXED2FLOAT(light->x);
	float y = FIXED2FLOAT(light->y);
	float z = FIXED2FLOAT(light->z);
	
	float dist = fabsf(p.DistToPoint(x, z, y));
	float radius = (light->GetRadius() * gl_lights_size);
	
	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(x, z, y))
	{
		return false;
	}


	float cs;
	if (gl_lights_additive || light->flags4&MF4_ADDITIVE || forceadditive) 
	{
		cs = 0.2f;
		i = 2;
	}
	else 
	{
		cs = 1.0f;
	}

	float r = light->GetRed() / 255.0f * cs * gl_lights_intensity;
	float g = light->GetGreen() / 255.0f * cs * gl_lights_intensity;
	float b = light->GetBlue() / 255.0f * cs * gl_lights_intensity;

	if (light->IsSubtractive())
	{
		Vector v;
		
		v.Set(r, g, b);
		r = v.Length() - r;
		g = v.Length() - g;
		b = v.Length() - b;
		i = 1;
	}

	if (desaturation>0)
	{
		float gray=(r*77 + g*143 + b*37)/257;

		r= (r*(32-desaturation)+ gray*desaturation)/32;
		g= (g*(32-desaturation)+ gray*desaturation)/32;
		b= (b*(32-desaturation)+ gray*desaturation)/32;
	}
	float *data = &ldata.arrays[i][ldata.arrays[i].Reserve(8)];
	data[0] = x;
	data[1] = z;
	data[2] = y;
	data[3] = radius;
	data[4] = r;
	data[5] = g;
	data[6] = b;
	data[7] = 0;
	return true;
}




//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================
bool gl_SetupLight(Plane & p, ADynamicLight * light, Vector & nearPt, Vector & up, Vector & right, 
				   float & scale, int desaturation, bool checkside, bool forceadditive)
{
	Vector fn, pos;

    float x = FIXED2FLOAT(light->x);
	float y = FIXED2FLOAT(light->y);
	float z = FIXED2FLOAT(light->z);
	
	float dist = fabsf(p.DistToPoint(x, z, y));
	float radius = (light->GetRadius() * gl_lights_size);
	
	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(x, z, y))
	{
		return false;
	}
	if (light->owned && light->target != NULL && !light->target->IsVisibleToPlayer())
	{
		return false;
	}

	scale = 1.0f / ((2.f * radius) - dist);

	// project light position onto plane (find closest point on plane)


	pos.Set(x,z,y);
	fn=p.Normal();
	fn.GetRightUp(right, up);

#ifdef _MSC_VER
	nearPt = pos + fn * dist;
#else
	Vector tmpVec = fn * dist;
	nearPt = pos + tmpVec;
#endif

	float cs = 1.0f - (dist / radius);
	if (gl_lights_additive || light->flags4&MF4_ADDITIVE || forceadditive) cs*=0.2f;	// otherwise the light gets too strong.
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
	if (desaturation>0)
	{
		float gray=(r*77 + g*143 + b*37)/257;

		r= (r*(32-desaturation)+ gray*desaturation)/32;
		g= (g*(32-desaturation)+ gray*desaturation)/32;
		b= (b*(32-desaturation)+ gray*desaturation)/32;
	}
	glColor3f(r,g,b);
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
	FMaterial * pat = FMaterial::ValidateTexture(GLRenderer->gllight);
	pat->BindPatch(CM_DEFAULT, 0);
	return true;
}


//==========================================================================
//
//
//
//==========================================================================

inline fixed_t P_AproxDistance3(fixed_t dx, fixed_t dy, fixed_t dz)
{
	return P_AproxDistance(P_AproxDistance(dx,dy),dz);
}

