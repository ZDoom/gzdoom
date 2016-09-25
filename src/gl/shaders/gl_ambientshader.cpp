// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Magnus Norddahl
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
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_ambientshader.h"

void FLinearDepthShader::Bind(bool multisample)
{
	auto &shader = mShader[multisample];
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/lineardepth.fp", multisample ? "#define MULTISAMPLE\n" : "", 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/lineardepth");
		shader.SetAttribLocation(0, "PositionInProjection");
		DepthTexture[multisample].Init(shader, "DepthTexture");
		ColorTexture[multisample].Init(shader, "ColorTexture");
		SampleCount[multisample].Init(shader, "SampleCount");
		LinearizeDepthA[multisample].Init(shader, "LinearizeDepthA");
		LinearizeDepthB[multisample].Init(shader, "LinearizeDepthB");
		InverseDepthRangeA[multisample].Init(shader, "InverseDepthRangeA");
		InverseDepthRangeB[multisample].Init(shader, "InverseDepthRangeB");
		Scale[multisample].Init(shader, "Scale");
		Offset[multisample].Init(shader, "Offset");
	}
	shader.Bind();
}

void FSSAOShader::Bind()
{
	auto &shader = mShader[gl_ssao];
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/ssao.fp", GetDefines(gl_ssao), 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/ssao");
		shader.SetAttribLocation(0, "PositionInProjection");
		DepthTexture.Init(shader, "DepthTexture");
		RandomTexture.Init(shader, "RandomTexture");
		UVToViewA.Init(shader, "UVToViewA");
		UVToViewB.Init(shader, "UVToViewB");
		InvFullResolution.Init(shader, "InvFullResolution");
		NDotVBias.Init(shader, "NDotVBias");
		NegInvR2.Init(shader, "NegInvR2");
		RadiusToScreen.Init(shader, "RadiusToScreen");
		AOMultiplier.Init(shader, "AOMultiplier");
		AOStrength.Init(shader, "AOStrength");
	}
	shader.Bind();
}

FString FSSAOShader::GetDefines(int mode)
{
	int numDirections, numSteps;
	switch (gl_ssao)
	{
	default:
	case LowQuality:    numDirections = 2; numSteps = 4; break;
	case MediumQuality: numDirections = 4; numSteps = 4; break;
	case HighQuality:   numDirections = 8; numSteps = 4; break;
	}

	FString defines;
	defines.Format(R"(
		#define USE_RANDOM_TEXTURE
		#define RANDOM_TEXTURE_WIDTH 4.0
		#define NUM_DIRECTIONS %d.0
		#define NUM_STEPS %d.0
	)", numDirections, numSteps);
	return defines;
}

void FDepthBlurShader::Bind(bool vertical)
{
	auto &shader = mShader[vertical];
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/depthblur.fp", vertical ? "#define BLUR_VERTICAL\n" : "#define BLUR_HORIZONTAL\n", 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/depthblur");
		shader.SetAttribLocation(0, "PositionInProjection");
		AODepthTexture[vertical].Init(shader, "AODepthTexture");
		BlurSharpness[vertical].Init(shader, "BlurSharpness");
		InvFullResolution[vertical].Init(shader, "InvFullResolution");
		PowExponent[vertical].Init(shader, "PowExponent");
	}
	shader.Bind();
}

void FSSAOCombineShader::Bind(bool multisample)
{
	auto &shader = mShader[multisample];
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/ssaocombine.fp", multisample ? "#define MULTISAMPLE\n" : "", 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/ssaocombine");
		shader.SetAttribLocation(0, "PositionInProjection");
		AODepthTexture[multisample].Init(shader, "AODepthTexture");
		SceneDataTexture[multisample].Init(shader, "SceneDataTexture");
		SampleCount[multisample].Init(shader, "SampleCount");
		Scale[multisample].Init(shader, "Scale");
		Offset[multisample].Init(shader, "Offset");
	}
	shader.Bind();
}
