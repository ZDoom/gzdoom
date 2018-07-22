// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Christopher Bruns
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
** gl_3dRowshader.cpp
** Copy rendered texture to back buffer, possibly with gamma correction
** while interleaving rows from two independent viewpoint textures,
** representing the left-eye and right-eye views.
**
*/

#include "hw_present3dRowshader.h"

void FPresent3DCheckerShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		Init("shaders/glsl/present_checker3d.fp", "shaders/glsl/presentChecker3d");
	}
	mShader->Bind(q);
}

void FPresent3DColumnShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		Init("shaders/glsl/present_column3d.fp", "shaders/glsl/presentColumn3d");
	}
	mShader->Bind(q);
}

void FPresent3DRowShader::Bind(IRenderQueue *q)
{
	if (!mShader)
	{
		Init("shaders/glsl/present_row3d.fp", "shaders/glsl/presentRow3d");
	}
	mShader->Bind(q);
}
