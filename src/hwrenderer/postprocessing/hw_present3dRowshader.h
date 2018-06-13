// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
** gl_present3dRowshader.h
** Final composition and present shader for row-interleaved stereoscopic 3D mode
**
*/

#ifndef GL_PRESENT3DROWSHADER_H_
#define GL_PRESENT3DROWSHADER_H_

#include "hw_shaderprogram.h"
#include "hw_presentshader.h"

class FPresent3DCheckerShader : public FPresentShaderBase
{
public:
	void Bind(IRenderQueue *q) override;
};

class FPresent3DColumnShader : public FPresentShaderBase
{
public:
	void Bind(IRenderQueue *q) override;
};

class FPresent3DRowShader : public FPresentShaderBase
{
public:
	void Bind(IRenderQueue *q) override;
};

// GL_PRESENT3DROWSHADER_H_
#endif