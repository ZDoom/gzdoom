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
** gl_dynlight1.cpp
** dynamic light application
**
**/

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

CVAR (Bool, gl_attachedlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_lights_checkside, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_sprites, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_particles, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);
CVAR (Bool, gl_light_shadowmap, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

//==========================================================================
//
// Sets up the parameters to render one dynamic light onto one plane
//
//==========================================================================
bool gl_GetLight(int group, Plane & p, ADynamicLight * light, bool checkside, FDynLightData &ldata)
{
	int i = 0;

	DVector3 pos = light->PosRelative(group);
	
	float dist = fabsf(p.DistToPoint(pos.X, pos.Z, pos.Y));
	float radius = (light->GetRadius());
	
	if (radius <= 0.f) return false;
	if (dist > radius) return false;
	if (checkside && gl_lights_checkside && p.PointOnSide(pos.X, pos.Z, pos.Y))
	{
		return false;
	}


	float cs;
	if (light->IsAdditive()) 
	{
		cs = 0.2f;
		i = 2;
	}
	else 
	{
		cs = 1.0f;
	}

	float r = light->GetRed() / 255.0f * cs;
	float g = light->GetGreen() / 255.0f * cs;
	float b = light->GetBlue() / 255.0f * cs;

	if (light->IsSubtractive())
	{
		Vector v;
		
		v.Set(r, g, b);
		r = v.Length() - r;
		g = v.Length() - g;
		b = v.Length() - b;
		i = 1;
	}

	// Store attenuate flag in the sign bit of the float.
	float shadowIndex = GLRenderer->mShadowMap.ShadowMapIndex(light) + 1.0f;
	if (!!(light->flags4 & MF4_ATTENUATE))
		shadowIndex = -shadowIndex;

	float *data = &ldata.arrays[i][ldata.arrays[i].Reserve(8)];
	data[0] = pos.X;
	data[1] = pos.Z;
	data[2] = pos.Y;
	data[3] = radius;
	data[4] = r;
	data[5] = g;
	data[6] = b;
	data[7] = shadowIndex;
	return true;
}

