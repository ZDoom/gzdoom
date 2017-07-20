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
#include "m_swap.h"
#include "v_video.h"
#include "gl/gl_functions.h"
#include "vectors.h"
#include "gl/system/gl_interface.h"
#include "gl/system/gl_framebuffer.h"
#include "gl/system/gl_cvars.h"
#include "gl/shaders/gl_presentshader.h"

void FPresentShaderBase::Init(const char * vtx_shader_name, const char * program_name)
{
	mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquadscale.vp", "", 330);
	mShader.Compile(FShaderProgram::Fragment, vtx_shader_name, "", 330);
	mShader.SetFragDataLocation(0, "FragColor");
	mShader.Link(program_name);
	mShader.SetAttribLocation(0, "PositionInProjection");
	mShader.SetAttribLocation(1, "UV");
	InvGamma.Init(mShader, "InvGamma");
	Contrast.Init(mShader, "Contrast");
	Brightness.Init(mShader, "Brightness");
	Saturation.Init(mShader, "Saturation");
	GrayFormula.Init(mShader, "GrayFormula");
	Scale.Init(mShader, "UVScale");
}

void FPresentShader::Bind()
{
	if (!mShader)
	{
		Init("shaders/glsl/present.fp", "shaders/glsl/present");
		InputTexture.Init(mShader, "InputTexture");
	}
	mShader.Bind();
}
