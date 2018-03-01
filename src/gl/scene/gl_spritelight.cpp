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
*/

#include "gl/system/gl_system.h"
#include "c_dispatch.h"
#include "p_local.h"
#include "p_effect.h"
#include "vectors.h"
#include "gl/gl_functions.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "actorinlines.h"

#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_lightdata.h"
#include "gl/data/gl_data.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/scene/gl_portal.h"
#include "gl/shaders/gl_shader.h"
#include "gl/textures/gl_material.h"
#include "gl/dynlights/gl_lightbuffer.h"

FDynLightData modellightdata;
int modellightindex = -1;

template<class T>
T smoothstep(const T edge0, const T edge1, const T x)
{
	auto t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

//==========================================================================
//
// Sets a single light value from all dynamic lights affecting the specified location
//
//==========================================================================

void gl_SetDynSpriteLight(AActor *self, float x, float y, float z, subsector_t * subsec)
{
	ADynamicLight *light;
	float frac, lr, lg, lb;
	float radius;
	float out[3] = { 0.0f, 0.0f, 0.0f };
	
	// Go through both light lists
	FLightNode * node = subsec->lighthead;
	while (node)
	{
		light=node->lightsource;
		if (light->visibletoplayer && !(light->flags2&MF2_DORMANT) && (!(light->lightflags&LF_DONTLIGHTSELF) || light->target != self || !self) && !(light->lightflags&LF_DONTLIGHTACTORS))
		{
			float dist;
			FVector3 L;

			// This is a performance critical section of code where we cannot afford to let the compiler decide whether to inline the function or not.
			// This will do the calculations explicitly rather than calling one of AActor's utility functions.
			if (Displacements.size > 0)
			{
				int fromgroup = light->Sector->PortalGroup;
				int togroup = subsec->sector->PortalGroup;
				if (fromgroup == togroup || fromgroup == 0 || togroup == 0) goto direct;

				DVector2 offset = Displacements.getOffset(fromgroup, togroup);
				L = FVector3(x - light->X() - offset.X, y - light->Y() - offset.Y, z - light->Z());
			}
			else
			{
			direct:
				L = FVector3(x - light->X(), y - light->Y(), z - light->Z());
			}

			dist = L.LengthSquared();
			radius = light->GetRadius();

			if (dist < radius * radius)
			{
				dist = sqrtf(dist);	// only calculate the square root if we really need it.

				frac = 1.0f - (dist / radius);

				if (light->IsSpot())
				{
					L *= -1.0f / dist;
					DAngle negPitch = -light->Angles.Pitch;
					double xyLen = negPitch.Cos();
					double spotDirX = -light->Angles.Yaw.Cos() * xyLen;
					double spotDirY = -light->Angles.Yaw.Sin() * xyLen;
					double spotDirZ = -negPitch.Sin();
					double cosDir = L.X * spotDirX + L.Y * spotDirY + L.Z * spotDirZ;
					frac *= (float)smoothstep(light->SpotOuterAngle.Cos(), light->SpotInnerAngle.Cos(), cosDir);
				}

				if (frac > 0 && GLRenderer->mShadowMap.ShadowTest(light, { x, y, z }))
				{
					lr = light->GetRed() / 255.0f;
					lg = light->GetGreen() / 255.0f;
					lb = light->GetBlue() / 255.0f;
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
				}
			}
		}
		node = node->nextLight;
	}
	gl_RenderState.SetDynLight(out[0], out[1], out[2]);
	modellightindex = -1;
}

void gl_SetDynSpriteLight(AActor *thing, particle_t *particle)
{
	if (thing != NULL)
	{
		gl_SetDynSpriteLight(thing, thing->X(), thing->Y(), thing->Center(), thing->subsector);
	}
	else if (particle != NULL)
	{
		gl_SetDynSpriteLight(NULL, particle->Pos.X, particle->Pos.Y, particle->Pos.Z, particle->subsector);
	}
}

// Check if circle potentially intersects with node AABB
static bool CheckBBoxCircle(float *bbox, float x, float y, float radiusSquared)
{
	float centerX = (bbox[BOXRIGHT] + bbox[BOXLEFT]) * 0.5f;
	float centerY = (bbox[BOXBOTTOM] + bbox[BOXTOP]) * 0.5f;
	float extentX = (bbox[BOXRIGHT] - bbox[BOXLEFT]) * 0.5f;
	float extentY = (bbox[BOXBOTTOM] - bbox[BOXTOP]) * 0.5f;
	float aabbRadiusSquared = extentX * extentX + extentY * extentY;
	x -= centerX;
	y -= centerY;
	float dist = x * x + y * y;
	return dist <= radiusSquared + aabbRadiusSquared;
}

template<typename Callback>
void BSPNodeWalkCircle(void *node, float x, float y, float radiusSquared, const Callback &callback)
{
	while (!((size_t)node & 1))
	{
		node_t *bsp = (node_t *)node;

		if (CheckBBoxCircle(bsp->bbox[0], x, y, radiusSquared))
			BSPNodeWalkCircle(bsp->children[0], x, y, radiusSquared, callback);

		if (!CheckBBoxCircle(bsp->bbox[1], x, y, radiusSquared))
			return;

		node = bsp->children[1];
	}

	subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
	callback(sub);
}

template<typename Callback>
void BSPWalkCircle(float x, float y, float radiusSquared, const Callback &callback)
{
	if (level.nodes.Size() == 0)
		callback(&level.subsectors[0]);
	else
		BSPNodeWalkCircle(level.HeadNode(), x, y, radiusSquared, callback);
}

int gl_SetDynModelLight(AActor *self, int dynlightindex)
{
	// For deferred light mode this function gets called twice. First time for list upload, and second for draw.
	if (gl.lightmethod == LM_DEFERRED && dynlightindex != -1)
	{
		gl_RenderState.SetDynLight(0, 0, 0);
		modellightindex = dynlightindex;
		return dynlightindex;
	}

	// Legacy render path gets the old flat model light
	if (gl.lightmethod == LM_LEGACY)
	{
		gl_SetDynSpriteLight(self, nullptr);
		return -1;
	}

	modellightdata.Clear();

	if (self)
	{
		static std::vector<ADynamicLight*> addedLights; // static so that we build up a reserve (memory allocations stop)

		addedLights.clear();

		float x = self->X();
		float y = self->Y();
		float z = self->Center();
		float radiusSquared = self->renderradius * self->renderradius;

		BSPWalkCircle(x, y, radiusSquared, [&](subsector_t *subsector) // Iterate through all subsectors potentially touched by actor
		{
			FLightNode * node = subsector->lighthead;
			while (node) // check all lights touching a subsector
			{
				ADynamicLight *light = node->lightsource;
				if (light->visibletoplayer && !(light->flags2&MF2_DORMANT) && (!(light->lightflags&LF_DONTLIGHTSELF) || light->target != self) && !(light->lightflags&LF_DONTLIGHTACTORS))
				{
					int group = subsector->sector->PortalGroup;
					DVector3 pos = light->PosRelative(group);
					float radius = light->GetRadius() + self->renderradius;
					double dx = pos.X - x;
					double dy = pos.Y - y;
					double dz = pos.Z - z;
					double distSquared = dx * dx + dy * dy + dz * dz;
					if (distSquared < radius * radius) // Light and actor touches
					{
						if (std::find(addedLights.begin(), addedLights.end(), light) == addedLights.end()) // Check if we already added this light from a different subsector
						{
							gl_AddLightToList(group, light, modellightdata);
							addedLights.push_back(light);
						}
					}
				}
				node = node->nextLight;
			}
		});
	}

	dynlightindex = GLRenderer->mLights->UploadLights(modellightdata);

	if (gl.lightmethod != LM_DEFERRED)
	{
		gl_RenderState.SetDynLight(0, 0, 0);
		modellightindex = dynlightindex;
	}
	return dynlightindex;
}
