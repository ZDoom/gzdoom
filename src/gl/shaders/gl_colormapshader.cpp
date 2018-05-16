// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Christoph Oelckers
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
** gl_colormapshader.cpp
** Applies a fullscreen colormap to the scene
**
*/

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "gl/shaders/gl_colormapshader.h"

void FColormapShader::Bind()
{
	auto &shader = mShader;
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/colormap.fp", "", 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/colormap");
		shader.SetAttribLocation(0, "PositionInProjection");
		MapStart.Init(shader, "uFixedColormapStart");
		MapRange.Init(shader, "uFixedColormapRange");
	}
	shader.Bind();
}

