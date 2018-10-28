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
#include "hwrenderer/postprocessing/hw_shadowmapshader.h"
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

	PerformUpdate();

	FGLDebug::PushGroup("ShadowMap");
	FGLPostProcessState savedState;

	GLRenderer->mBuffers->BindShadowMapFB();

	GLRenderer->mShadowMapShader->Bind(NOQUEUE);
	GLRenderer->mShadowMapShader->Uniforms->ShadowmapQuality = gl_shadowmap_quality;
	GLRenderer->mShadowMapShader->Uniforms.Set();

	glViewport(0, 0, gl_shadowmap_quality, 1024);
	GLRenderer->RenderScreenQuad();

	const auto &viewport = screen->mScreenViewport;
	glViewport(viewport.left, viewport.top, viewport.width, viewport.height);

	GLRenderer->mBuffers->BindShadowMapTexture(16);

	FGLDebug::PopGroup();

	UpdateCycles.Unclock();
}

