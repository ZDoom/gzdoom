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

void PostProcessShaderInstance::Run()
{
	if (!Program)
	{
		const char *lumpName = Desc->ShaderLumpName.GetChars();
		int lump = Wads.CheckNumForFullName(lumpName);
		if (lump == -1) I_FatalError("Unable to load '%s'", lumpName);
		FString code = Wads.ReadLump(lump).GetString().GetChars();

		Program.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330); // Hmm, should this use shader.shaderversion?
		Program.Compile(FShaderProgram::Fragment, lumpName, code, "", Desc->ShaderVersion);
		Program.SetFragDataLocation(0, "FragColor");
		Program.Link(Desc->ShaderLumpName.GetChars());
		Program.SetAttribLocation(0, "PositionInProjection");
		InputTexture.Init(Program, "InputTexture");
		CustomTexture.Init(Program, "CustomTexture");

		if (Desc->Texture)
		{
			HWTexture = new FHardwareTexture(Desc->Texture->GetWidth(), Desc->Texture->GetHeight(), false);
			HWTexture->CreateTexture((unsigned char*)Desc->Texture->GetPixelsBgra(), Desc->Texture->GetWidth(), Desc->Texture->GetHeight(), 0, false, 0, "CustomTexture");
		}
	}

	FGLDebug::PushGroup(Desc->ShaderLumpName.GetChars());

	FGLPostProcessState savedState;
	savedState.SaveTextureBindings(2);

	GLRenderer->mBuffers->BindNextFB();
	GLRenderer->mBuffers->BindCurrentTexture(0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	Program.Bind();

	TMap<FString, float>::Iterator it1f(Desc->Uniform1f);
	TMap<FString, float>::Pair *pair1f;
	while (it1f.NextPair(pair1f))
	{
		int location = glGetUniformLocation(Program, pair1f->Key.GetChars());
		if (location != -1)
			glUniform1f(location, pair1f->Value);
	}

	TMap<FString, int>::Iterator it1i(Desc->Uniform1i);
	TMap<FString, int>::Pair *pair1i;
	while (it1i.NextPair(pair1i))
	{
		int location = glGetUniformLocation(Program, pair1i->Key.GetChars());
		if (location != -1)
			glUniform1i(location, pair1i->Value);
	}

	InputTexture.Set(0);

	if (HWTexture)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, HWTexture->GetTextureHandle(0));
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glActiveTexture(GL_TEXTURE0);

		CustomTexture.Set(1);
	}
	GLRenderer->RenderScreenQuad();

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLRenderer->mBuffers->NextTexture();

	FGLDebug::PopGroup();
}
