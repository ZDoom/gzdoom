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

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/shaders/gl_ambientshader.h"

void FLinearDepthShader::Bind()
{
	bool multisample = (gl_multisample > 1);
	if (mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		mShader.reset(new FShaderProgram());
		mShader->Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(FShaderProgram::Fragment, "shaders/glsl/lineardepth.fp", multisample ? "#define MULTISAMPLE\n" : "", 330);
		mShader->SetFragDataLocation(0, "FragColor");
		mShader->Link("shaders/glsl/lineardepth");
		mShader->SetAttribLocation(0, "PositionInProjection");
		DepthTexture.Init(*mShader, "DepthTexture");
		ColorTexture.Init(*mShader, "ColorTexture");
		SampleIndex.Init(*mShader, "SampleIndex");
		LinearizeDepthA.Init(*mShader, "LinearizeDepthA");
		LinearizeDepthB.Init(*mShader, "LinearizeDepthB");
		InverseDepthRangeA.Init(*mShader, "InverseDepthRangeA");
		InverseDepthRangeB.Init(*mShader, "InverseDepthRangeB");
		Scale.Init(*mShader, "Scale");
		Offset.Init(*mShader, "Offset");
		mMultisample = multisample;
	}
	mShader->Bind();
}

void FSSAOShader::Bind()
{
	bool multisample = (gl_multisample > 1);
	if (mCurrentQuality != gl_ssao || mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		mShader.reset(new FShaderProgram());
		mShader->Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(FShaderProgram::Fragment, "shaders/glsl/ssao.fp", GetDefines(gl_ssao, multisample), 330);
		mShader->SetFragDataLocation(0, "FragColor");
		mShader->Link("shaders/glsl/ssao");
		mShader->SetAttribLocation(0, "PositionInProjection");
		DepthTexture.Init(*mShader, "DepthTexture");
		NormalTexture.Init(*mShader, "NormalTexture");
		RandomTexture.Init(*mShader, "RandomTexture");
		UVToViewA.Init(*mShader, "UVToViewA");
		UVToViewB.Init(*mShader, "UVToViewB");
		InvFullResolution.Init(*mShader, "InvFullResolution");
		NDotVBias.Init(*mShader, "NDotVBias");
		NegInvR2.Init(*mShader, "NegInvR2");
		RadiusToScreen.Init(*mShader, "RadiusToScreen");
		AOMultiplier.Init(*mShader, "AOMultiplier");
		AOStrength.Init(*mShader, "AOStrength");
		Scale.Init(*mShader, "Scale");
		Offset.Init(*mShader, "Offset");
		SampleIndex.Init(*mShader, "SampleIndex");
		mMultisample = multisample;
	}
	mShader->Bind();
}

FString FSSAOShader::GetDefines(int mode, bool multisample)
{
	// Must match quality values in FGLRenderBuffers::CreateAmbientOcclusion
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

	if (multisample)
		defines += "\n#define MULTISAMPLE\n";
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

void FSSAOCombineShader::Bind()
{
	bool multisample = (gl_multisample > 1);
	if (mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		mShader.reset(new FShaderProgram());
		mShader->Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(FShaderProgram::Fragment, "shaders/glsl/ssaocombine.fp", multisample ? "#define MULTISAMPLE\n" : "", 330);
		mShader->SetFragDataLocation(0, "FragColor");
		mShader->Link("shaders/glsl/ssaocombine");
		mShader->SetAttribLocation(0, "PositionInProjection");
		AODepthTexture.Init(*mShader, "AODepthTexture");
		SceneFogTexture.Init(*mShader, "SceneFogTexture");
		SampleCount.Init(*mShader, "SampleCount");
		Scale.Init(*mShader, "Scale");
		Offset.Init(*mShader, "Offset");
		mMultisample = multisample;
	}
	mShader->Bind();
}
