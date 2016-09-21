/*
** gl_bloomshader.cpp
** Shaders used for screen space ambient occlusion
**
**---------------------------------------------------------------------------
** Copyright 2016 Magnus Norddahl
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

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
	if (!mShader)
	{
		const char *defines = R"(
			#define USE_RANDOM_TEXTURE
			#define RANDOM_TEXTURE_WIDTH 4.0
			#define NUM_DIRECTIONS 8.0
			#define NUM_STEPS 4.0
		)";

		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/ssao.fp", defines, 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/ssao");
		mShader.SetAttribLocation(0, "PositionInProjection");
		DepthTexture.Init(mShader, "DepthTexture");
		RandomTexture.Init(mShader, "RandomTexture");
		UVToViewA.Init(mShader, "UVToViewA");
		UVToViewB.Init(mShader, "UVToViewB");
		InvFullResolution.Init(mShader, "InvFullResolution");
		NDotVBias.Init(mShader, "NDotVBias");
		NegInvR2.Init(mShader, "NegInvR2");
		RadiusToScreen.Init(mShader, "RadiusToScreen");
		AOMultiplier.Init(mShader, "AOMultiplier");
		AOStrength.Init(mShader, "AOStrength");
	}
	mShader.Bind();
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
