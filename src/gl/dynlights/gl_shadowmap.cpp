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
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/shaders/gl_shadowmapshader.h"
#include "r_state.h"

void FShadowMap::Clear()
{
	mLightBSP.Clear();
}

void FShadowMap::Update()
{
	TThinkerIterator<ADynamicLight> it(STAT_DLIGHT);
	while (true)
	{
		ADynamicLight *light = it.Next();
		if (!light) break;
	}

	FGLDebug::PushGroup("ShadowMap");
	FGLPostProcessState savedState;

	GLRenderer->mShadowMapShader->Bind();
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, mLightBSP.GetNodesBuffer());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mLightBSP.GetSegsBuffer());

	glViewport(0, 0, 1024, 1024);
	GLRenderer->RenderScreenQuad();

	const auto &viewport = GLRenderer->mScreenViewport;
	glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, 0);

	FGLDebug::PopGroup();
}
