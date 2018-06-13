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
** gl_presentshader.cpp
** Copy rendered texture to back buffer, possibly with gamma correction
**
*/

#include "v_video.h"
#include "hw_presentshader.h"

void FPresentShaderBase::Init(const char * vtx_shader_name, const char * program_name)
{
	FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());

	mShader.reset(screen->CreateShaderProgram());
	mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquadscale.vp", prolog, 330);
	mShader->Compile(IShaderProgram::Fragment, vtx_shader_name, prolog, 330);
	mShader->Link(program_name);
	mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
	Uniforms.Init();
}

void FPresentShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		Init("shaders/glsl/present.fp", "shaders/glsl/present");
	}
	mShader->Bind(q);
}
