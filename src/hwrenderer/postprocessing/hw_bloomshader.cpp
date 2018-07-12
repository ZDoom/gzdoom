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
** gl_bloomshader.cpp
** Shaders used to do bloom
**
*/

#include "v_video.h"
#include "hw_bloomshader.h"

void FBloomExtractShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/bloomextract.fp", prolog, 330);
		mShader->Link("shaders/glsl/bloomextract");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
	}
	mShader->Bind(q);
}

void FBloomCombineShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/bloomcombine.fp", "", 330);
		mShader->Link("shaders/glsl/bloomcombine");
	}
	mShader->Bind(q);
}
