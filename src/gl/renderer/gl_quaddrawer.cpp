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

#include "gl_load/gl_system.h"
#include "gl/shaders/gl_shader.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/renderer/gl_quaddrawer.h"

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
