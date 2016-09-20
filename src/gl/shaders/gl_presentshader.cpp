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

#include "gl/system/gl_system.h"
#include "files.h"
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_presentshader.h"

void FPresentShader::Bind()
{
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquadscale.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/present.fp", "", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/present");
		mShader.SetAttribLocation(0, "PositionInProjection");
		mShader.SetAttribLocation(1, "UV");
		InputTexture.Init(mShader, "InputTexture");
		InvGamma.Init(mShader, "InvGamma");
		Contrast.Init(mShader, "Contrast");
		Brightness.Init(mShader, "Brightness");
		Scale.Init(mShader, "UVScale");
	}
	mShader.Bind();
}
