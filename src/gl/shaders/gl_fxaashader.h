// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2016 Alexey Lysiuk
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

//
// Fast approXimate Anti-Aliasing (FXAA) post-processing
//

#ifndef __GL_FXAASHADER_H__
#define __GL_FXAASHADER_H__

#include "gl_shaderprogram.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"

class FFXAALumaShader
{
public:
	void Bind();

	FBufferedUniform1i InputTexture;

private:
	FShaderProgram mShader;
};


class FFXAAShader : public IFXAAShader
{
public:
	void Bind();

	FBufferedUniform1i InputTexture;
	FBufferedUniform2f ReciprocalResolution;

private:
	FShaderProgram mShaders[Count];
};

#endif // __GL_FXAASHADER_H__
