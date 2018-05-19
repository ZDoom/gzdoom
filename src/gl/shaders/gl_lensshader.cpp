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

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "gl/shaders/gl_lensshader.h"

void FLensShader::Bind()
{
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/lensdistortion.fp", "", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/lensdistortion");
		mShader.SetAttribLocation(0, "PositionInProjection");
		InputTexture.Init(mShader, "InputTexture");
		AspectRatio.Init(mShader, "Aspect");
		Scale.Init(mShader, "Scale");
		LensDistortionCoefficient.Init(mShader, "k");
		CubicDistortionValue.Init(mShader, "kcube");
	}
	mShader.Bind();
}
