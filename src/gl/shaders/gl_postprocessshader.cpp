// 
//---------------------------------------------------------------------------
//
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
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "w_wad.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_debug.h"
#include "gl/system/gl_cvars.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_postprocessstate.h"
#include "gl/renderer/gl_renderbuffers.h"
#include "gl/shaders/gl_postprocessshader.h"
#include "textures/textures.h"
#include "textures/bitmap.h"

CVAR(Bool, gl_custompost, true, 0)

TArray<PostProcessShader> PostProcessShaders;

FCustomPostProcessShaders::FCustomPostProcessShaders()
{
	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		mShaders.push_back(std::unique_ptr<PostProcessShaderInstance>(new PostProcessShaderInstance(&PostProcessShaders[i])));
	}
}

FCustomPostProcessShaders::~FCustomPostProcessShaders()
{
}

void FCustomPostProcessShaders::Run(FString target)
{
	if (!gl_custompost)
		return;

	for (auto &shader : mShaders)
	{
		if (shader->Desc->Target == target)
		{
			shader->Run();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

PostProcessShaderInstance::~PostProcessShaderInstance()
{
	for (const auto &it : mTextureHandles)
		glDeleteTextures(1, (GLuint*)&it.second);
}

void PostProcessShaderInstance::Run()
{
	if (!IsShaderSupported())
		return;

	CompileShader();

	if (!Desc->Enabled)
		return;

	FGLDebug::PushGroup(Desc->ShaderLumpName.GetChars());

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(1 + Desc->Textures.CountUsed());

	GLRenderer->mBuffers->BindNextFB();
	GLRenderer->mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	mProgram.Bind();

	UpdateUniforms();
	BindTextures();

	mInputTexture.Set(0);

	GLRenderer->RenderScreenQuad();

	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLRenderer->mBuffers->NextTexture();

	FGLDebug::PopGroup();
}

bool PostProcessShaderInstance::IsShaderSupported()
{
	int activeShaderVersion = (int)round(gl.glslversion * 10) * 10;
	return activeShaderVersion >= Desc->ShaderVersion;
}

void PostProcessShaderInstance::CompileShader()
{
	if (mProgram)
		return;

	// Get the custom shader
	const char *lumpName = Desc->ShaderLumpName.GetChars();
	int lump = Wads.CheckNumForFullName(lumpName);
	if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
	FString code = Wads.ReadLump(lump).GetString().GetChars();

	// Build an uniform block to use be used as input
	// (this is technically not an uniform block, but it could be changed into that for Vulkan GLSL support)
	FString uniformBlock;
	TMap<FString, PostProcessUniformValue>::Iterator it(Desc->Uniforms);
	TMap<FString, PostProcessUniformValue>::Pair *pair;
	while (it.NextPair(pair))
	{
		FString type;
		FString name = pair->Key;

		switch (pair->Value.Type)
		{
		case PostProcessUniformType::Float: type = "float"; break;
		case PostProcessUniformType::Int: type = "int"; break;
		case PostProcessUniformType::Vec2: type = "vec2"; break;
		case PostProcessUniformType::Vec3: type = "vec3"; break;
		default: break;
		}

		if (!type.IsEmpty())
			uniformBlock.AppendFormat("uniform %s %s;\n", type.GetChars(), name.GetChars());
	}

	// Build the input textures
	FString uniformTextures;
	uniformTextures += "uniform sampler2D InputTexture;\n";

	TMap<FString, FString>::Iterator itTextures(Desc->Textures);
	TMap<FString, FString>::Pair *pairTextures;
	while (itTextures.NextPair(pairTextures))
	{
		uniformTextures.AppendFormat("uniform sampler2D %s;\n", pairTextures->Key.GetChars());
	}

	// Setup pipeline
	FString pipelineInOut;
	pipelineInOut += "in vec2 TexCoord;\n";
	pipelineInOut += "out vec4 FragColor;\n";

	FString prolog;
	prolog += uniformBlock;
	prolog += uniformTextures;
	prolog += pipelineInOut;

	mProgram.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", Desc->ShaderVersion);
	mProgram.Compile(FShaderProgram::Fragment, lumpName, code, prolog.GetChars(), Desc->ShaderVersion);
	mProgram.SetFragDataLocation(0, "FragColor");
	mProgram.Link(Desc->ShaderLumpName.GetChars());
	mProgram.SetAttribLocation(0, "PositionInProjection");
	mInputTexture.Init(mProgram, "InputTexture");
}

void PostProcessShaderInstance::UpdateUniforms()
{
	TMap<FString, PostProcessUniformValue>::Iterator it(Desc->Uniforms);
	TMap<FString, PostProcessUniformValue>::Pair *pair;
	while (it.NextPair(pair))
	{
		int location = glGetUniformLocation(mProgram, pair->Key.GetChars());
		if (location != -1)
		{
			switch (pair->Value.Type)
			{
			case PostProcessUniformType::Float:
				glUniform1f(location, (float)pair->Value.Values[0]);
				break;
			case PostProcessUniformType::Int:
				glUniform1i(location, (int)pair->Value.Values[0]);
				break;
			case PostProcessUniformType::Vec2:
				glUniform2f(location, (float)pair->Value.Values[0], (float)pair->Value.Values[1]);
				break;
			case PostProcessUniformType::Vec3:
				glUniform3f(location, (float)pair->Value.Values[0], (float)pair->Value.Values[1], (float)pair->Value.Values[2]);
				break;
			default:
				break;
			}
		}
	}
}

void PostProcessShaderInstance::BindTextures()
{
	int textureUnit = 1;
	TMap<FString, FString>::Iterator it(Desc->Textures);
	TMap<FString, FString>::Pair *pair;
	while (it.NextPair(pair))
	{
		int location = glGetUniformLocation(mProgram, pair->Key.GetChars());
		if (location == -1)
			continue;

		FString name = pair->Value;
		FTexture *tex = TexMan(TexMan.CheckForTexture(name, ETextureType::Any));
		if (tex && tex->UseType != ETextureType::Null)
		{
			glUniform1i(location, textureUnit);

			glActiveTexture(GL_TEXTURE0 + 1);
			auto it = mTextureHandles.find(tex);
			if (it == mTextureHandles.end())
			{
				FBitmap bitmap;
				bitmap.Create(tex->GetWidth(), tex->GetHeight());
				tex->CopyTrueColorPixels(&bitmap, 0, 0);

				GLuint handle = 0;
				glGenTextures(1, &handle);
				glBindTexture(GL_TEXTURE_2D, handle);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex->GetWidth(), tex->GetHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap.GetPixels());
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				mTextureHandles[tex] = handle;
			}
			else
			{
				glBindTexture(GL_TEXTURE_2D, it->second);
			}

			textureUnit++;
		}
	}
}
