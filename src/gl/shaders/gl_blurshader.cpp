/*
** gl_blurshader.cpp
** Gaussian blur shader
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
#include "gl/shaders/gl_blurshader.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/renderer/gl_renderer.h"

//==========================================================================
//
// Performs a vertical gaussian blur pass
//
//==========================================================================

void FBlurShader::BlurVertical(FGLRenderer *renderer, float blurAmount, int sampleCount, GLuint inputTexture, GLuint outputFrameBuffer, int width, int height)
{
	Blur(renderer, blurAmount, sampleCount, inputTexture, outputFrameBuffer, width, height, true);
}

//==========================================================================
//
// Performs a horizontal gaussian blur pass
//
//==========================================================================

void FBlurShader::BlurHorizontal(FGLRenderer *renderer, float blurAmount, int sampleCount, GLuint inputTexture, GLuint outputFrameBuffer, int width, int height)
{
	Blur(renderer, blurAmount, sampleCount, inputTexture, outputFrameBuffer, width, height, false);
}

//==========================================================================
//
// Helper for BlurVertical and BlurHorizontal. Executes the actual pass
//
//==========================================================================

void FBlurShader::Blur(FGLRenderer *renderer, float blurAmount, int sampleCount, GLuint inputTexture, GLuint outputFrameBuffer, int width, int height, bool vertical)
{
	BlurSetup *setup = GetSetup(blurAmount, sampleCount);
	if (vertical)
		setup->VerticalShader->Bind();
	else
		setup->HorizontalShader->Bind();

	if (gl.glslversion < 1.3)
	{
		if (vertical)
		{
			setup->VerticalScaleX.Set(1.0f / width);
			setup->VerticalScaleY.Set(1.0f / height);
		}
		else
		{
			setup->HorizontalScaleX.Set(1.0f / width);
			setup->HorizontalScaleY.Set(1.0f / height);
		}
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, inputTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindFramebuffer(GL_FRAMEBUFFER, outputFrameBuffer);
	glViewport(0, 0, width, height);
	glDisable(GL_BLEND);

	renderer->RenderScreenQuad();
}

//==========================================================================
//
// Compiles the blur shaders needed for the specified blur amount and
// kernel size
//
//==========================================================================

FBlurShader::BlurSetup *FBlurShader::GetSetup(float blurAmount, int sampleCount)
{
	for (size_t mBlurSetupIndex = 0; mBlurSetupIndex < mBlurSetups.Size(); mBlurSetupIndex++)
	{
		if (mBlurSetups[mBlurSetupIndex].blurAmount == blurAmount && mBlurSetups[mBlurSetupIndex].sampleCount == sampleCount)
		{
			return &mBlurSetups[mBlurSetupIndex];
		}
	}

	BlurSetup blurSetup(blurAmount, sampleCount);

	FString vertexCode = VertexShaderCode();
	FString horizontalCode = FragmentShaderCode(blurAmount, sampleCount, false);
	FString verticalCode = FragmentShaderCode(blurAmount, sampleCount, true);

	blurSetup.VerticalShader = std::make_shared<FShaderProgram>();
	blurSetup.VerticalShader->Compile(FShaderProgram::Vertex, "vertical blur vertex shader", vertexCode, "", 330);
	blurSetup.VerticalShader->Compile(FShaderProgram::Fragment, "vertical blur fragment shader", verticalCode, "", 330);
	blurSetup.VerticalShader->SetFragDataLocation(0, "FragColor");
	blurSetup.VerticalShader->SetAttribLocation(0, "PositionInProjection");
	blurSetup.VerticalShader->Link("vertical blur");
	blurSetup.VerticalShader->Bind();
	glUniform1i(glGetUniformLocation(*blurSetup.VerticalShader.get(), "SourceTexture"), 0);

	blurSetup.HorizontalShader = std::make_shared<FShaderProgram>();
	blurSetup.HorizontalShader->Compile(FShaderProgram::Vertex, "horizontal blur vertex shader", vertexCode, "", 330);
	blurSetup.HorizontalShader->Compile(FShaderProgram::Fragment, "horizontal blur fragment shader", horizontalCode, "", 330);
	blurSetup.HorizontalShader->SetFragDataLocation(0, "FragColor");
	blurSetup.HorizontalShader->SetAttribLocation(0, "PositionInProjection");
	blurSetup.HorizontalShader->Link("horizontal blur");
	blurSetup.HorizontalShader->Bind();
	glUniform1i(glGetUniformLocation(*blurSetup.HorizontalShader.get(), "SourceTexture"), 0);
	
	if (gl.glslversion < 1.3)
	{
		blurSetup.VerticalScaleX.Init(*blurSetup.VerticalShader.get(), "ScaleX");
		blurSetup.VerticalScaleY.Init(*blurSetup.VerticalShader.get(), "ScaleY");
		blurSetup.HorizontalScaleX.Init(*blurSetup.HorizontalShader.get(), "ScaleX");
		blurSetup.HorizontalScaleY.Init(*blurSetup.HorizontalShader.get(), "ScaleY");
	}

	mBlurSetups.Push(blurSetup);

	return &mBlurSetups[mBlurSetups.Size() - 1];
}

//==========================================================================
//
// The vertex shader GLSL code
//
//==========================================================================

FString FBlurShader::VertexShaderCode()
{
	return R"(
		in vec4 PositionInProjection;
		out vec2 TexCoord;

		void main()
		{
			gl_Position = PositionInProjection;
			TexCoord = (gl_Position.xy + 1.0) * 0.5;
		}
	)";
}

//==========================================================================
//
// Generates the fragment shader GLSL code for a specific blur setup
//
//==========================================================================

FString FBlurShader::FragmentShaderCode(float blurAmount, int sampleCount, bool vertical)
{
	TArray<float> sampleWeights;
	TArray<int> sampleOffsets;
	ComputeBlurSamples(sampleCount, blurAmount, sampleWeights, sampleOffsets);

	const char *fragmentShader =
		R"(
		in vec2 TexCoord;
		uniform sampler2D SourceTexture;
		out vec4 FragColor;
		#if __VERSION__ < 130
		uniform float ScaleX;
		uniform float ScaleY;
		vec4 textureOffset(sampler2D s, vec2 texCoord, ivec2 offset)
		{
			return texture2D(s, texCoord + vec2(ScaleX * float(offset.x), ScaleY * float(offset.y)));
		}
		#endif
		void main()
		{
			FragColor = %s;
		}
		)";

	FString loopCode;
	for (int i = 0; i < sampleCount; i++)
	{
		if (i > 0)
			loopCode += " + ";

		if (vertical)
			loopCode.AppendFormat("\r\n\t\t\ttextureOffset(SourceTexture, TexCoord, ivec2(0, %d)) * %f", sampleOffsets[i], (double)sampleWeights[i]);
		else
			loopCode.AppendFormat("\r\n\t\t\ttextureOffset(SourceTexture, TexCoord, ivec2(%d, 0)) * %f", sampleOffsets[i], (double)sampleWeights[i]);
	}

	FString code;
	code.Format(fragmentShader, loopCode.GetChars());
	return code;
}

//==========================================================================
//
// Calculates the sample weight for a specific offset in the kernel
//
//==========================================================================

float FBlurShader::ComputeGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / sqrtf(2 * (float)M_PI * theta)) * expf(-(n * n) / (2.0f * theta * theta)));
}

//==========================================================================
//
// Calculates the sample weights and offsets
//
//==========================================================================

void FBlurShader::ComputeBlurSamples(int sampleCount, float blurAmount, TArray<float> &sampleWeights, TArray<int> &sampleOffsets)
{
	sampleWeights.Resize(sampleCount);
	sampleOffsets.Resize(sampleCount);

	sampleWeights[0] = ComputeGaussian(0, blurAmount);
	sampleOffsets[0] = 0;

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;
		sampleOffsets[i * 2 + 1] = i + 1;
		sampleOffsets[i * 2 + 2] = -i - 1;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}
