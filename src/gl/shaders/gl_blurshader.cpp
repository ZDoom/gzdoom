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
/*
** gl_blurshader.cpp
** Gaussian blur shader
**
*/

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "gl/shaders/gl_blurshader.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderbuffers.h"

//==========================================================================
//
// Performs a vertical gaussian blur pass
//
//==========================================================================

void FBlurShader::BlurVertical(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height)
{
	Blur(renderer, blurAmount, sampleCount, inputTexture, outputFrameBuffer, width, height, true);
}

//==========================================================================
//
// Performs a horizontal gaussian blur pass
//
//==========================================================================

void FBlurShader::BlurHorizontal(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height)
{
	Blur(renderer, blurAmount, sampleCount, inputTexture, outputFrameBuffer, width, height, false);
}

//==========================================================================
//
// Helper for BlurVertical and BlurHorizontal. Executes the actual pass
//
//==========================================================================

void FBlurShader::Blur(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height, bool vertical)
{
	BlurSetup *setup = GetSetup(blurAmount, sampleCount);
	if (vertical)
		setup->VerticalShader->Bind();
	else
		setup->HorizontalShader->Bind();

	inputTexture.Bind(0);

	outputFrameBuffer.Bind();
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
