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

#include "gl_load/gl_system.h"
#include "v_video.h"
#include "gl/shaders/gl_bloomshader.h"

void FBloomExtractShader::Bind()
{
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/bloomextract.fp", "", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/bloomextract");
		mShader.SetAttribLocation(0, "PositionInProjection");
		SceneTexture.Init(mShader, "SceneTexture");
		ExposureTexture.Init(mShader, "ExposureTexture");
		Scale.Init(mShader, "Scale");
		Offset.Init(mShader, "Offset");
	}
	mShader.Bind();
}

void FBloomCombineShader::Bind()
{
	if (!mShader)
	{
		mShader.Compile(FShaderProgram::Vertex, "shaders/glsl/screenquad.vp", "", 330);
		mShader.Compile(FShaderProgram::Fragment, "shaders/glsl/bloomcombine.fp", "", 330);
		mShader.SetFragDataLocation(0, "FragColor");
		mShader.Link("shaders/glsl/bloomcombine");
		mShader.SetAttribLocation(0, "PositionInProjection");
		BloomTexture.Init(mShader, "Bloom");
	}
	mShader.Bind();
}
