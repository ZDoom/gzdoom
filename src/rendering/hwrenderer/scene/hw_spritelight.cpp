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

#include "c_dispatch.h"
#include "a_dynlight.h" 
#include "p_local.h"
#include "p_effect.h"
#include "g_level.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "hw_dynlightdata.h"
#include "hw_shadowmap.h"
#include "hwrenderer/scene/hw_drawinfo.h"
#include "hwrenderer/scene/hw_drawstructs.h"
#include "models.h"
#include <cmath>	// needed for std::floor on mac

template<class T>
T smoothstep(const T edge0, const T edge1, const T x)
{
	auto t = clamp((x - edge0) / (edge1 - edge0), 0.0, 1.0);
	return t * t * (3.0 - 2.0 * t);
}

LightProbe* FindLightProbe(FLevelLocals* level, float x, float y, float z)
{
	LightProbe* foundprobe = nullptr;
	if (level->LightProbes.Size() > 0)
	{
#if 1
		double rcpCellSize = 1.0 / level->LPCellSize;
		int gridCenterX = (int)std::floor(x * rcpCellSize) - level->LPMinX;
		int gridCenterY = (int)std::floor(y * rcpCellSize) - level->LPMinY;
		int gridWidth = level->LPWidth;
		int gridHeight = level->LPHeight;
		float lastdist = 0.0f;
		for (int gridY = gridCenterY - 1; gridY <= gridCenterY + 1; gridY++)
		{
			for (int gridX = gridCenterX - 1; gridX <= gridCenterX + 1; gridX++)
			{
				if (gridX >= 0 && gridY >= 0 && gridX < gridWidth && gridY < gridHeight)
				{
					const LightProbeCell& cell = level->LPCells[gridX + (size_t)gridY * gridWidth];
					for (int i = 0; i < cell.NumProbes; i++)
					{
						LightProbe* probe = cell.FirstProbe + i;
						float dx = probe->X - x;
						float dy = probe->Y - y;
						float dz = probe->Z - z;
						float dist = dx * dx + dy * dy + dz * dz;
						if (!foundprobe || dist < lastdist)
						{
							foundprobe = probe;
							lastdist = dist;
						}
					}
				}
			}
		}
#else
		float lastdist = 0.0f;
		for (unsigned int i = 0; i < level->LightProbes.Size(); i++)
		{
			LightProbe *probe = &level->LightProbes[i];
			float dx = probe->X - x;
			float dy = probe->Y - y;
			float dz = probe->Z - z;
			float dist = dx * dx + dy * dy + dz * dz;
			if (i == 0 || dist < lastdist)
			{
				foundprobe = probe;
				lastdist = dist;
			}
		}
#endif
	}
	return foundprobe;
}

//==========================================================================
//
// Sets a single light value from all dynamic lights affecting the specified location
//
//==========================================================================

void HWDrawInfo::GetDynSpriteLight(AActor *self, float x, float y, float z, FLightNode *node, int portalgroup, float *out)
{
	FDynamicLight *light;
	float frac, lr, lg, lb;
	float radius;
	
	out[0] = out[1] = out[2] = 0.f;

	LightProbe* probe = FindLightProbe(Level, x, y, z);
	if (probe)
	{
		out[0] = probe->Red;
		out[1] = probe->Green;
		out[2] = probe->Blue;
	}

	// Go through both light lists
	while (node)
	{
		light=node->lightsource;
		if (light->ShouldLightActor(self))
		{
			float dist;
			FVector3 L;

			// This is a performance critical section of code where we cannot afford to let the compiler decide whether to inline the function or not.
			// This will do the calculations explicitly rather than calling one of AActor's utility functions.
			if (Level->Displacements.size > 0)
			{
				int fromgroup = light->Sector->PortalGroup;
				int togroup = portalgroup;
				if (fromgroup == togroup || fromgroup == 0 || togroup == 0) goto direct;

				DVector2 offset = Level->Displacements.getOffset(fromgroup, togroup);
				L = FVector3(x - (float)(light->X() + offset.X), y - (float)(light->Y() + offset.Y), z - (float)light->Z());
			}
			else
			{
			direct:
				L = FVector3(x - (float)light->X(), y - (float)light->Y(), z - (float)light->Z());
			}

			dist = (float)L.LengthSquared();
			radius = light->GetRadius();

			if (dist < radius * radius)
			{
				dist = sqrtf(dist);	// only calculate the square root if we really need it.

				frac = 1.0f - (dist / radius);

				if (light->IsSpot())
				{
					L *= -1.0f / dist;
					DAngle negPitch = -*light->pPitch;
					DAngle Angle = light->target->Angles.Yaw;
					double xyLen = negPitch.Cos();
					double spotDirX = -Angle.Cos() * xyLen;
					double spotDirY = -Angle.Sin() * xyLen;
					double spotDirZ = -negPitch.Sin();
					double cosDir = L.X * spotDirX + L.Y * spotDirY + L.Z * spotDirZ;
					frac *= (float)smoothstep(light->pSpotOuterAngle->Cos(), light->pSpotInnerAngle->Cos(), cosDir);
				}

				if (frac > 0 && (!light->shadowmapped || (light->GetRadius() > 0 && screen->mShadowMap.ShadowTest(light->Pos, { x, y, z }))))
				{
					lr = light->GetRed() / 255.0f;
					lg = light->GetGreen() / 255.0f;
					lb = light->GetBlue() / 255.0f;

					if (light->target)
					{
						float alpha = (float)light->target->Alpha;
						lr *= alpha;
						lg *= alpha;
						lb *= alpha;
					}

					if (light->IsSubtractive())
					{
						float bright = (float)FVector3(lr, lg, lb).Length();
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
}

void HWDrawInfo::GetDynSpriteLight(AActor *thing, particle_t *particle, float *out)
{
	if (thing != NULL)
	{
		GetDynSpriteLight(thing, (float)thing->X(), (float)thing->Y(), (float)thing->Center(), thing->section->lighthead, thing->Sector->PortalGroup, out);
	}
	else if (particle != NULL)
	{
		GetDynSpriteLight(NULL, (float)particle->Pos.X, (float)particle->Pos.Y, (float)particle->Pos.Z, particle->subsector->section->lighthead, particle->subsector->sector->PortalGroup, out);
	}
}

// static so that we build up a reserve (memory allocations stop)
// For multithread processing each worker thread needs its own copy, though.
static thread_local TArray<FDynamicLight*> addedLightsArray; 

void hw_GetDynModelLight(AActor *self, FDynLightData &modellightdata)
{
	modellightdata.Clear();

	if (self)
	{
		auto &addedLights = addedLightsArray;	// avoid going through the thread local storage for each use.

		addedLights.Clear();

		float x = (float)self->X();
		float y = (float)self->Y();
		float z = (float)self->Center();
		float actorradius = (float)self->RenderRadius();
		float radiusSquared = actorradius * actorradius;
		dl_validcount++;

		BSPWalkCircle(self->Level, x, y, radiusSquared, [&](subsector_t *subsector) // Iterate through all subsectors potentially touched by actor
		{
			auto section = subsector->section;
			if (section->validcount == dl_validcount) return;	// already done from a previous subsector.
			FLightNode * node = section->lighthead;
			while (node) // check all lights touching a subsector
			{
				FDynamicLight *light = node->lightsource;
				if (light->ShouldLightActor(self))
				{
					int group = subsector->sector->PortalGroup;
					DVector3 pos = light->PosRelative(group);
					float radius = (float)(light->GetRadius() + actorradius);
					double dx = pos.X - x;
					double dy = pos.Y - y;
					double dz = pos.Z - z;
					double distSquared = dx * dx + dy * dy + dz * dz;
					if (distSquared < radius * radius) // Light and actor touches
					{
						if (std::find(addedLights.begin(), addedLights.end(), light) == addedLights.end()) // Check if we already added this light from a different subsector
						{
							AddLightToList(modellightdata, group, light, true);
							addedLights.Push(light);
						}
					}
				}
				node = node->nextLight;
			}
		});
	}
}
