// 
//---------------------------------------------------------------------------
// 1D dynamic shadow maps (OpenGL dependent part)
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

#include "gl_load/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/dynlights/gl_shadowmap.h"
#include "gl/system/gl_debug.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/shaders/gl_shadowmapshader.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "stats.h"

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

	const auto &viewport = screen->mScreenViewport;
	glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);

	GLRenderer->mBuffers->BindShadowMapTexture(16);

	FGLDebug::PopGroup();

	UpdateCycles.Unclock();
}

void FShadowMap::UploadLights()
{
	CollectLights();
	
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
	if (!ValidateAABBTree())
	{
		int oldBinding = 0;
		glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING, &oldBinding);

		glGenBuffers(1, (GLuint*)&mNodesBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, mNodesBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(hwrenderer::AABBTreeNode) * mAABBTree->nodes.Size(), &mAABBTree->nodes[0], GL_STATIC_DRAW);

		glGenBuffers(1, (GLuint*)&mLinesBuffer);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, mLinesBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(hwrenderer::AABBTreeLine) * mAABBTree->lines.Size(), &mAABBTree->lines[0], GL_STATIC_DRAW);

		glBindBuffer(GL_SHADER_STORAGE_BUFFER, oldBinding);
	}
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
}
