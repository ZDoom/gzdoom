//
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Alexey Lysiuk
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

//
// Fast approXimate Anti-Aliasing (FXAA) post-processing
//

#include "gl_load/gl_system.h"
#include "gl/shaders/gl_fxaashader.h"

EXTERN_CVAR(Int, gl_fxaa)

void FFXAALumaShader::Bind()
{
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/fxaa.fp", "#define FXAA_LUMA_PASS\n", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/fxaa");
		mShader.SetAttribLocation(0, "PositionInProjection");
		InputTexture.Init(mShader, "InputTexture");
	}

	mShader.Bind();
}

static int GetMaxVersion()
{
	return gl.glslversion >= 4.f ? 400 : 330;
}

static FString GetDefines()
{
	int quality;

	switch (gl_fxaa)
	{
	default:
	case FFXAAShader::Low:     quality = 10; break;
	case FFXAAShader::Medium:  quality = 12; break;
	case FFXAAShader::High:    quality = 29; break;
	case FFXAAShader::Extreme: quality = 39; break;
	}

	const int gatherAlpha = GetMaxVersion() >= 400 ? 1 : 0;

	// TODO: enable FXAA_GATHER4_ALPHA on OpenGL earlier than 4.0
	// when GL_ARB_gpu_shader5/GL_NV_gpu_shader5 extensions are supported

	FString result;
	result.Format(
		"#define FXAA_QUALITY__PRESET %i\n"
		"#define FXAA_GATHER4_ALPHA %i\n",
		quality, gatherAlpha);

	return result;
}

void FFXAAShader::Bind()
{
	assert(gl_fxaa > 0 && gl_fxaa < Count);
	FShaderProgram &shader = mShaders[gl_fxaa];

	if (!shader)
	{
		const FString defines = GetDefines();
		const int maxVersion = GetMaxVersion();

		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/fxaa.fp", defines, maxVersion);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/fxaa");
		shader.SetAttribLocation(0, "PositionInProjection");
		InputTexture.Init(shader, "InputTexture");
		ReciprocalResolution.Init(shader, "ReciprocalResolution");
	}

	shader.Bind();
}
