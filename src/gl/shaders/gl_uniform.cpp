/*
** gl_uniform.cpp
**
** GLSL uniform handling
**
**---------------------------------------------------------------------------
** Copyright 2004-2009 Christoph Oelckers
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

#include "gl/shaders/gl_shader.h"


FMaterialUniform FGLUniform::getMaterialUniform(FShader *shader1, FShader *shader2)
{
	FMaterialUniform matu;

	matu.mUni = this;
	matu.mLocation[0] = shader1 == NULL ? -1 : glGetUniformLocation(shader1->GetHandle(), mName);
	matu.mLocation[1] = shader2 == NULL ? -1 : glGetUniformLocation(shader2->GetHandle(), mName);
	return matu;
}

// the following methods assume that the proper shader has already been bound.
// If GL had a better API design this multi-method nonsense woulsn't even be necessary...

void FGLUniformConst1f::apply(int location)
{
	glUniform1f(location, mValue);
}

void FGLUniformConst2f::apply(int location)
{
	glUniform2f(location, mValue[0], mValue[1]);
}

void FGLUniformConst3f::apply(int location)
{
	glUniform3f(location, mValue[0], mValue[1], mValue[2]);
}

void FGLUniformConst4f::apply(int location)
{
	glUniform4f(location, mValue[0], mValue[1], mValue[2], mValue[3]);
}

void FGLUniformConst1i::apply(int location)
{
	glUniform1i(location, mValue);
}

void FGLUniformConst2i::apply(int location)
{
	glUniform2i(location, mValue[0], mValue[1]);
}

void FGLUniformConst3i::apply(int location)
{
	glUniform3i(location, mValue[0], mValue[1], mValue[2]);
}

void FGLUniformConst4i::apply(int location)
{
	glUniform4i(location, mValue[0], mValue[1], mValue[2], mValue[3]);
}
