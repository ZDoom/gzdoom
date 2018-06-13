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
** gl_tonemapshader.cpp
** Converts a HDR texture to 0-1 range by applying a tonemap operator
**
*/

#include "v_video.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl/shaders/gl_tonemapshader.h"

void FTonemapShader::Bind(IRenderQueue *q)
{
	auto &shader = mShader[gl_tonemap];
	if (!shader)
	{
		auto prolog = GetDefines(gl_tonemap);

		shader.reset(screen->CreateShaderProgram());
		shader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader->Compile(IShaderProgram::Fragment, "shaders/glsl/tonemap.fp", prolog, 330);
		shader->Link("shaders/glsl/tonemap");
	}
	shader->Bind(q);
}

bool FTonemapShader::IsPaletteMode()
{
	return gl_tonemap == Palette;
}

const char *FTonemapShader::GetDefines(int mode)
{
	switch (mode)
	{
	default:
	case Linear:     return "#define LINEAR\n";
	case Reinhard:   return "#define REINHARD\n";
	case HejlDawson: return "#define HEJLDAWSON\n";
	case Uncharted2: return "#define UNCHARTED2\n";
	case Palette:    return "#define PALETTE\n";
	}
}

void FExposureExtractShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/exposureextract.fp", prolog, 330);
		mShader->Link("shaders/glsl/exposureextract");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
	}
	mShader->Bind(q);
}

void FExposureAverageShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 400);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/exposureaverage.fp", "", 400);
		mShader->Link("shaders/glsl/exposureaverage");
	}
	mShader->Bind(q);
}

void FExposureCombineShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/exposurecombine.fp", prolog, 330);
		mShader->Link("shaders/glsl/exposurecombine");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
	}
	mShader->Bind(q);
}