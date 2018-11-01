// 
//---------------------------------------------------------------------------
// 1D dynamic shadow maps (API independent part)
// Copyright(C) 2017 Magnus Norddahl
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

#include "hwrenderer/dynlights/hw_shadowmap.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/data/buffers.h"
#include "stats.h"
#include "g_levellocals.h"

/*
	The 1D shadow maps are stored in a 1024x1024 texture as float depth values (R32F).

	Each line in the texture is assigned to a single light. For example, to grab depth values for light 20
	the fragment shader (main.fp) needs to sample from row 20. That is, the V texture coordinate needs
	to be 20.5/1024.

    The texel row for each light is split into four parts. One for each direction, like a cube texture,
	but then only in 2D where this reduces itself to a square. When main.fp samples from the shadow map
	it first decides in which direction the fragment is (relative to the light), like cubemap sampling does
	for 3D, but once again just for the 2D case.
	
	Texels 0-255 is Y positive, 256-511 is X positive, 512-767 is Y negative and 768-1023 is X negative.

	Generating the shadow map itself is done by FShadowMap::Update(). The shadow map texture's FBO is
	bound and then a screen quad is drawn to make a fragment shader cover all texels. For each fragment
	it shoots a ray and collects the distance to what it hit.
	
	The shadowmap.fp shader knows which light and texel it is processing by mapping gl_FragCoord.y back
	to the light index, and it knows which direction to ray trace by looking at gl_FragCoord.x. For
	example, if gl_FragCoord.y is 20.5, then it knows its processing light 20, and if gl_FragCoord.x is
	127.5, then it knows we are shooting straight ahead for the Y positive direction.

	Ray testing is done by uploading two GPU storage buffers - one holding AABB tree nodes, and one with
	the line segments at the leaf nodes of the tree. The fragment shader then performs a test same way
	as on the CPU, except everything uses indexes as pointers are not allowed in GLSL.
*/

cycle_t IShadowMap::UpdateCycles;
int IShadowMap::LightsProcessed;
int IShadowMap::LightsShadowmapped;

ADD_STAT(shadowmap)
{
	FString out;
	out.Format("upload=%04.2f ms  lights=%d  shadowmapped=%d", IShadowMap::UpdateCycles.TimeMS(), IShadowMap::LightsProcessed, IShadowMap::LightsShadowmapped);
	return out;
}

CUSTOM_CVAR(Int, gl_shadowmap_quality, 512, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	switch (self)
	{
	case 128:
	case 256:
	case 512:
	case 1024:
		break;
	default:
		self = 128;
		break;
	}
}

CUSTOM_CVAR (Bool, gl_light_shadowmap, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
    if (!self)
    {
        // Unset any residual shadow map indices in the light actors.
        TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
        while (auto light = it.Next())
        {
            light->mShadowmapIndex = 1024;
        }
    }
}

bool IShadowMap::ShadowTest(ADynamicLight *light, const DVector3 &pos)
{
	if (light->shadowmapped && light->radius > 0.0 && IsEnabled() && mAABBTree)
		return mAABBTree->RayTest(light->Pos(), pos) >= 1.0f;
	else
		return true;
}

bool IShadowMap::IsEnabled() const
{
	return gl_light_shadowmap && (screen->hwcaps & RFL_SHADER_STORAGE_BUFFER);
}

void IShadowMap::CollectLights()
{
	if (mLights.Size() != 1024 * 4) mLights.Resize(1024 * 4);
	int lightindex = 0;

	// Todo: this should go through the blockmap in a spiral pattern around the player so that closer lights are preferred.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	while (auto light = it.Next())
	{
		LightsProcessed++;
		if (light->shadowmapped && light->IsActive() && lightindex < 1024 * 4)
		{
			LightsShadowmapped++;

			light->mShadowmapIndex = lightindex >> 2;

			mLights[lightindex] = (float)light->X();
			mLights[lightindex+1] = (float)light->Y();
			mLights[lightindex+2] = (float)light->Z();
			mLights[lightindex+3] = light->GetRadius();
			lightindex += 4;
		}
        else
        {
            light->mShadowmapIndex = 1024;
        }

	}

	for (; lightindex < 1024 * 4; lightindex++)
	{
		mLights[lightindex] = 0;
	}
}

bool IShadowMap::ValidateAABBTree()
{
	// Just comparing the level info is not enough. If two MAPINFO-less levels get played after each other, 
	// they can both refer to the same default level info.
	if (level.info != mLastLevel && (level.nodes.Size() != mLastNumNodes || level.segs.Size() != mLastNumSegs))
	{
		mAABBTree.reset();

		mLastLevel = level.info;
		mLastNumNodes = level.nodes.Size();
		mLastNumSegs = level.segs.Size();
	}

	if (mAABBTree)
		return true;

	mAABBTree.reset(new hwrenderer::LevelAABBTree());
	return false;
}

bool IShadowMap::PerformUpdate()
{
	UpdateCycles.Reset();

	LightsProcessed = 0;
	LightsShadowmapped = 0;

	if (IsEnabled())
	{
		UpdateCycles.Clock();
		UploadAABBTree();
		UploadLights();
		mLightList->BindBase();
		mNodesBuffer->BindBase();
		mLinesBuffer->BindBase();
		return true;
	}
	return false;
}

void IShadowMap::UploadLights()
{
	CollectLights();

	if (mLightList == nullptr)
		mLightList = screen->CreateDataBuffer(4, true);

	mLightList->SetData(sizeof(float) * mLights.Size(), &mLights[0]);
}


void IShadowMap::UploadAABBTree()
{
	if (!ValidateAABBTree())
	{
		mNodesBuffer = screen->CreateDataBuffer(2, true);
		mNodesBuffer->SetData(sizeof(hwrenderer::AABBTreeNode) * mAABBTree->nodes.Size(), &mAABBTree->nodes[0]);

		mLinesBuffer = screen->CreateDataBuffer(3, true);
		mLinesBuffer->SetData(sizeof(hwrenderer::AABBTreeLine) * mAABBTree->lines.Size(), &mAABBTree->lines[0]);
	}
}

IShadowMap::~IShadowMap()
{
	if (mLightList) delete mLightList;
	if (mNodesBuffer) delete mNodesBuffer;
	if (mLinesBuffer) delete mLinesBuffer;
}

