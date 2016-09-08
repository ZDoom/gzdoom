/*
** gl_quaddrawer.h
**
**---------------------------------------------------------------------------
** Copyright 2016 Christoph Oelckers
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
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/renderer/gl_quaddrawer.h"
#include "gl/data/gl_matrix.h"

/*
** For handling of dynamically created quads when no persistently mapped
** buffer or client array is available (i.e. GL 3.x core profiles)
**
** In this situation the 4 vertices of a quad primitive are being passed
** as a matrix uniform because that is a lot faster than any kind of
** temporary buffer change.
*/

FFlatVertex FQuadDrawer::buffer[4];

//==========================================================================
//
//
//
//==========================================================================

void FQuadDrawer::DoRender(int type)
{
	// When this gets called, the render state must already be applied so we can just
	// send our vertices to the current shader.
	float matV[16], matT[16];
	
	for(int i=0;i<4;i++)
	{
		matV[i*4+0] = buffer[i].x;
		matV[i*4+1] = buffer[i].z;
		matV[i*4+2] = buffer[i].y;
		matV[i*4+3] = 1;
		matT[i*4+0] = buffer[i].u;
		matT[i*4+1] = buffer[i].v;
		matT[i*4+2] = matT[i*4+3] = 0;
	}
	FShader *shader = GLRenderer->mShaderManager->GetActiveShader();
	glUniformMatrix4fv(shader->vertexmatrix_index, 1, false, matV);
	glUniformMatrix4fv(shader->texcoordmatrix_index, 1, false, matT);
	glUniform1i(shader->quadmode_index, 1);
	GLRenderer->mVBO->RenderArray(type, FFlatVertexBuffer::QUAD_INDEX, 4);
	glUniform1i(shader->quadmode_index, 0);
}
