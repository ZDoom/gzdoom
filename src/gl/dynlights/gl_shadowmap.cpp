// 
//---------------------------------------------------------------------------
// 1D dynamic shadow maps
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

#include "gl/system/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/dynlights/gl_shadowmap.h"
#include "gl/dynlights/gl_dynlight.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_debug.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/shaders/gl_shadowmapshader.h"
#include "r_state.h"
#include "g_levellocals.h"
#include "stats.h"

/*
	The 1D shadow maps are stored in a 1024x1024 texture as float depth values (R32F).

	Each line in the texture is assigned to a single light. For example, to grab depth values for light 20
	the fragment shader (main.fp) needs to sample from row 20. That is, the V texture coordinate needs
	to be 20.5/1024.

	mLightToShadowmap is a hash map storing which line each ADynamicLight is assigned to. The public
	ShadowMapIndex function allows the main rendering to find the index and upload that along with the
	normal light data. From there, the main.fp shader can sample from the shadow map texture, which
	is currently always bound to texture unit 16.

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

namespace
{
	cycle_t UpdateCycles;
	int LightsProcessed;
	int LightsShadowmapped;
}

ADD_STAT(shadowmap)
{
	FString out;
	out.Format("upload=%04.2f ms  lights=%d  shadowmapped=%d", UpdateCycles.TimeMS(), LightsProcessed, LightsShadowmapped);
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

void FShadowMap::Update()
{
	UpdateCycles.Reset();
	LightsProcessed = 0;
	LightsShadowmapped = 0;

	if (!IsEnabled())
		return;

	UpdateCycles.Clock();

	UploadAABBTree();
	UploadLights();

	FGLDebug::PushGroup("ShadowMap");
	FGLPostProcessState savedState;

	GLRenderer->mBuffers->BindShadowMapFB();

	GLRenderer->mShadowMapShader->Bind();
	GLRenderer->mShadowMapShader->ShadowmapQuality.Set(gl_shadowmap_quality);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, mLightList);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mNodesBuffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mLinesBuffer);

	glViewport(0, 0, gl_shadowmap_quality, 1024);
	GLRenderer->RenderScreenQuad();

	const auto &viewport = GLRenderer->mScreenViewport;
	glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);

	GLRenderer->mBuffers->BindShadowMapTexture(16);

	FGLDebug::PopGroup();

	UpdateCycles.Unclock();
}

bool FShadowMap::ShadowTest(ADynamicLight *light, const DVector3 &pos)
{
	if (light->shadowmapped && light->radius > 0.0 && IsEnabled() && mAABBTree)
		return mAABBTree->RayTest(light->Pos(), pos) >= 1.0f;
	else
		return true;
}

bool FShadowMap::IsEnabled() const
{
	return gl_light_shadowmap && !!(gl.flags & RFL_SHADER_STORAGE_BUFFER);
}

int FShadowMap::ShadowMapIndex(ADynamicLight *light)
{
	if (IsEnabled())
	{
		auto val = mLightToShadowmap.CheckKey(light);
		if (val != nullptr) return *val;
	}
	return 1024;
}

void FShadowMap::UploadLights()
{
	if (mLights.Size() != 1024 * 4) mLights.Resize(1024 * 4);
	int lightindex = 0;
	mLightToShadowmap.Clear(mLightToShadowmap.CountUsed() * 2); // To do: allow clearing a TMap while building up a reserve

	// Todo: this should go through the blockmap in a spiral pattern around the player so that closer lights are preferred.
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	while (auto light = it.Next())
	{
		LightsProcessed++;
		if (light->shadowmapped)
		{
			LightsShadowmapped++;

			mLightToShadowmap[light] = lightindex >> 2;

			mLights[lightindex] = light->X();
			mLights[lightindex+1] = light->Y();
			mLights[lightindex+2] = light->Z();
			mLights[lightindex+3] = light->GetRadius();
			lightindex += 4;

			if (lightindex == 1024*4) // Only 1024 lights for now
				break;
		}

	}

	for (; lightindex < 1024 * 4; lightindex++)
	{
		mLights[lightindex] = 0;
	}

	if (mLightList == 0)
		glGenBuffers(1, (GLuint*)&mLightList);

	int oldBinding = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldBinding);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLightList);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(float) * mLights.Size(), &mLights[0], GL_STATIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldBinding);
}

void FShadowMap::UploadAABBTree()
{
	// Just comparing the level info is not enough. If two MAPINFO-less levels get played after each other, 
	// they can both refer to the same default level info.
	if (level.info != mLastLevel && (level.nodes.Size() != mLastNumNodes || level.segs.Size() != mLastNumSegs))
		Clear();

	if (mAABBTree)
		return;

	mAABBTree.reset(new LevelAABBTree());

	int oldBinding = 0;
	glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldBinding);

	glGenBuffers(1, (GLuint*)&mNodesBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mNodesBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(AABBTreeNode) * mAABBTree->nodes.Size(), &mAABBTree->nodes[0], GL_STATIC_DRAW);

	glGenBuffers(1, (GLuint*)&mLinesBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLinesBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(AABBTreeLine) * mAABBTree->lines.Size(), &mAABBTree->lines[0], GL_STATIC_DRAW);

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldBinding);
}

void FShadowMap::Clear()
{
	if (mLightList != 0)
	{
		glDeleteBuffers(1, (GLuint*)&mLightList);
		mLightList = 0;
	}

	if (mNodesBuffer != 0)
	{
		glDeleteBuffers(1, (GLuint*)&mNodesBuffer);
		mNodesBuffer = 0;
	}

	if (mLinesBuffer != 0)
	{
		glDeleteBuffers(1, (GLuint*)&mLinesBuffer);
		mLinesBuffer = 0;
	}

	mAABBTree.reset();

	mLastLevel = level.info;
	mLastNumNodes = level.nodes.Size();
	mLastNumSegs = level.segs.Size();
}
