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

#include "v_video.h"
#include "hw_blurshader.h"

void FBlurShader::Bind(IRenderQueue *q, bool vertical)
{
	if (!mShader[vertical])
	{
		FString prolog = Uniforms[vertical].CreateDeclaration("Uniforms", UniformBlock::Desc());
		if (vertical)
			prolog += "#define BLUR_VERTICAL\n";
		else
			prolog += "#define BLUR_HORIZONTAL\n";

		mShader[vertical].reset(screen->CreateShaderProgram());
		mShader[vertical]->Compile(IShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader[vertical]->Compile(IShaderProgram::Fragment, "shaders/glsl/blur.fp", prolog, 330);
		mShader[vertical]->Link("shaders/glsl/blur");
		mShader[vertical]->SetUniformBufferLocation(POSTPROCESS_BINDINGPOINT, "Uniforms");
		Uniforms[vertical].Init();
	}

	mShader[vertical]->Bind(q);
}
