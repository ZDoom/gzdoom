// 
//---------------------------------------------------------------------------
// 1D dynamic shadow maps (API independent part)
// Copyright(C) 2017 Magnus Norddahl
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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

#include "hw_shadowmap.h"
#include "hw_cvars.h"
#include "hw_dynlightdata.h"
#include "buffers.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "v_video.h"

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

cycle_t ShadowMap::UpdateCycles;
int ShadowMap::LightsProcessed;
int ShadowMap::LightsShadowmapped;

CVAR(Bool, gl_light_shadowmap, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, gl_light_raytrace, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

ADD_STAT(shadowmap)
{
	FString out;
	out.Format("upload=%04.2f ms  lights=%d  shadowmapped=%d", ShadowMap::UpdateCycles.TimeMS(), ShadowMap::LightsProcessed, ShadowMap::LightsShadowmapped);
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

bool ShadowMap::ShadowTest(const DVector3 &lpos, const DVector3 &pos)
{
	if (mAABBTree && gl_light_shadowmap)
		return mAABBTree->RayTest(lpos, pos) >= 1.0f;
	else
		return true;
}

void ShadowMap::PerformUpdate()
{
	UpdateCycles.Reset();

	LightsProcessed = 0;
	LightsShadowmapped = 0;

	// CollectLights will be null if the calling code decides that shadowmaps are not needed.
	if (CollectLights != nullptr)
	{
		UpdateCycles.Clock();

		mLights.Resize(1024 * 4);
		CollectLights();

		fb->SetShadowMaps(mLights, mAABBTree, mNewTree);
		mNewTree = false;

		UpdateCycles.Unclock();
	}
}

ShadowMap::~ShadowMap()
{
}

