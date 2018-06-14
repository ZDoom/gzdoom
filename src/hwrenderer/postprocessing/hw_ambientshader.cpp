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

#include "v_video.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hw_postprocess_cvars.h"
#include "hw_ambientshader.h"

void FAmbientShader::Bind(IRenderQueue *q)
{
	mShader->Bind(q);
}

void FLinearDepthShader::Validate()
{
	bool multisample = (gl_multisample > 1);
	if (mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());
		if (multisample) prolog += "#define MULTISAMPLE\n";

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/lineardepth.fp", prolog, 330);
		mShader->Link("shaders/glsl/lineardepth");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
		mMultisample = multisample;
	}
}

void FSSAOShader::Validate()
{
	bool multisample = (gl_multisample > 1);
	if (mCurrentQuality != gl_ssao || mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());
		prolog += GetDefines(gl_ssao, multisample);

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/ssao.fp", prolog, 330);
		mShader->Link("shaders/glsl/ssao");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
		mMultisample = multisample;
	}
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

void FDepthBlurShader::Validate()
{
	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());
		if (mVertical)
			prolog += "#define BLUR_VERTICAL\n";
		else
			prolog += "#define BLUR_HORIZONTAL\n";

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/depthblur.fp", prolog, 330);
		mShader->Link("shaders/glsl/depthblur");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
	}
}

void FSSAOCombineShader::Validate()
{
	bool multisample = (gl_multisample > 1);
	if (mMultisample != multisample)
		mShader.reset();

	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());
		if (multisample)
			prolog += "#define MULTISAMPLE\n";

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/ssaocombine.fp", prolog, 330);
		mShader->Link("shaders/glsl/ssaocombine");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
		mMultisample = multisample;
	}
}



FAmbientPass::FAmbientPass()
{
	mLinearDepthShader.reset(new FLinearDepthShader());
	mDepthBlurShader[0].reset(new FDepthBlurShader(false));
	mDepthBlurShader[1].reset(new FDepthBlurShader(true));
	mSSAOShader.reset(new FSSAOShader());
	mSSAOCombineShader.reset(new FSSAOCombineShader());
}


void FAmbientPass::Setup(float proj5, float aspect)
{
	float bias = gl_ssao_bias;
	float aoRadius = gl_ssao_radius;
	const float blurAmount = gl_ssao_blur;
	float aoStrength = gl_ssao_strength;

	//float tanHalfFovy = tan(fovy * (M_PI / 360.0f));
	float tanHalfFovy = 1.0f / proj5;
	float invFocalLenX = tanHalfFovy * aspect;
	float invFocalLenY = tanHalfFovy;
	float nDotVBias = clamp(bias, 0.0f, 1.0f);
	float r2 = aoRadius * aoRadius;

	float blurSharpness = 1.0f / blurAmount;

	const auto &mSceneViewport = screen->mSceneViewport;
	const auto &mScreenViewport = screen->mScreenViewport;

	float sceneScaleX = mSceneViewport.width / (float)mScreenViewport.width;
	float sceneScaleY = mSceneViewport.height / (float)mScreenViewport.height;
	float sceneOffsetX = mSceneViewport.left / (float)mScreenViewport.width;
	float sceneOffsetY = mSceneViewport.top / (float)mScreenViewport.height;

	mLinearDepthShader->Validate();
	if (gl_multisample > 1) mLinearDepthShader->Uniforms->SampleIndex = 0;
	mLinearDepthShader->Uniforms->LinearizeDepthA = 1.0f / screen->GetZFar() - 1.0f / screen->GetZNear();
	mLinearDepthShader->Uniforms->LinearizeDepthB = MAX(1.0f / screen->GetZNear(), 1.e-8f);
	mLinearDepthShader->Uniforms->InverseDepthRangeA = 1.0f;
	mLinearDepthShader->Uniforms->InverseDepthRangeB = 0.0f;
	mLinearDepthShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mLinearDepthShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mLinearDepthShader->Uniforms.Set();

	mSSAOShader->Validate();
	if (gl_multisample > 1) mSSAOShader->Uniforms->SampleIndex = 0;
	mSSAOShader->Uniforms->UVToViewA = { 2.0f * invFocalLenX, 2.0f * invFocalLenY };
	mSSAOShader->Uniforms->UVToViewB = { -invFocalLenX, -invFocalLenY };
	mSSAOShader->Uniforms->InvFullResolution = { 1.0f / AmbientWidth, 1.0f / AmbientHeight };
	mSSAOShader->Uniforms->NDotVBias = nDotVBias;
	mSSAOShader->Uniforms->NegInvR2 = -1.0f / r2;
	mSSAOShader->Uniforms->RadiusToScreen = aoRadius * 0.5f / tanHalfFovy * AmbientHeight;
	mSSAOShader->Uniforms->AOMultiplier = 1.0f / (1.0f - nDotVBias);
	mSSAOShader->Uniforms->AOStrength = aoStrength;
	mSSAOShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mSSAOShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mSSAOShader->Uniforms.Set();

	// Blur SSAO texture
	if (gl_ssao_debug < 2)
	{
		mDepthBlurShader[false]->Validate();
		mDepthBlurShader[false]->Uniforms->BlurSharpness = blurSharpness;
		mDepthBlurShader[false]->Uniforms->InvFullResolution = { 1.0f / AmbientWidth, 1.0f / AmbientHeight };
		mDepthBlurShader[false]->Uniforms.Set();

		mDepthBlurShader[true]->Validate();
		mDepthBlurShader[true]->Uniforms->BlurSharpness = blurSharpness;
		mDepthBlurShader[true]->Uniforms->InvFullResolution = { 1.0f / AmbientWidth, 1.0f / AmbientHeight };
		mDepthBlurShader[true]->Uniforms->PowExponent = gl_ssao_exponent;
		mDepthBlurShader[true]->Uniforms.Set();
	}

	mSSAOCombineShader->Validate();
	if (gl_multisample > 1) mSSAOCombineShader->Uniforms->SampleCount = gl_multisample;
	mSSAOCombineShader->Uniforms->Scale = { sceneScaleX, sceneScaleY };
	mSSAOCombineShader->Uniforms->Offset = { sceneOffsetX, sceneOffsetY };
	mSSAOCombineShader->Uniforms.Set();
}