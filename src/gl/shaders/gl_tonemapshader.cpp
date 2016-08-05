/*
** gl_tonemapshader.cpp
** Converts a HDR texture to 0-1 range by applying a tonemap operator
**
**---------------------------------------------------------------------------
** Copyright 2016 Magnus Norddahl
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
** 5. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
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
#include "gl/shaders/gl_tonemapshader.h"

void FTonemapShader::Bind()
{
	auto &shader = mShader[gl_tonemap];
	if (!shader)
	{
		shader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		shader.Compile(FShaderProgram::Fragment, "shaders/glsl/tonemap.fp", GetDefines(gl_tonemap), 330);
		shader.SetFragDataLocation(0, "FragColor");
		shader.Link("shaders/glsl/tonemap");
		shader.SetAttribLocation(0, "PositionInProjection");
		SceneTexture.Init(shader, "InputTexture");
		Exposure.Init(shader, "ExposureAdjustment");
	}
	shader.Bind();
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
	}
}
