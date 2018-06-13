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
** gl_lensshader.cpp
** Lens distortion with chromatic aberration shader
**
*/

#include "v_video.h"
#include "gl/shaders/gl_lensshader.h"

void FLensShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		FString prolog = Uniforms.CreateDeclaration("Uniforms", UniformBlock::Desc());

		mShader.reset(screen->CreateShaderProgram());
		mShader->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader->Compile(IShaderProgram::Fragment, "shaders/glsl/lensdistortion.fp", prolog, 330);
		mShader->Link("shaders/glsl/lensdistortion");
		mShader->SetUniformBufferLocation(Uniforms.BindingPoint(), "Uniforms");
		Uniforms.Init();
	}
	mShader->Bind(q);
}
