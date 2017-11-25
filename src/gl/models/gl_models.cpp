// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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
** gl_models.cpp
**
** OpenGL renderer model handling code
**
**/

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "c_console.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "r_state.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "r_utility.h"
#include "i_time.h"
//#include "resources/voxels.h"
//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"
#include "gl/shaders/gl_shader.h"

CVAR(Bool, gl_light_models, true, CVAR_ARCHIVE)

extern int modellightindex;

VSMatrix FGLModelRenderer::GetViewToWorldMatrix()
{
	VSMatrix objectToWorldMatrix;
	gl_RenderState.mViewMatrix.inverseMatrix(objectToWorldMatrix);
	return objectToWorldMatrix;
}

void FGLModelRenderer::BeginDrawModel(AActor *actor, FSpriteModelFrame *smf, const VSMatrix &objectToWorldMatrix)
{
	glDepthFunc(GL_LEQUAL);
	gl_RenderState.EnableTexture(true);
	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// [Nash] Don't do back face culling if explicitly specified in MODELDEF
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]) && !(smf->flags & MDL_DONTCULLBACKFACES))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
	}

	gl_RenderState.mModelMatrix = objectToWorldMatrix;
	gl_RenderState.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawModel(AActor *actor, FSpriteModelFrame *smf)
{
	gl_RenderState.EnableModelMatrix(false);

	glDepthFunc(GL_LESS);
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]) && !(smf->flags & MDL_DONTCULLBACKFACES))
		glDisable(GL_CULL_FACE);
}

void FGLModelRenderer::BeginDrawHUDModel(AActor *actor, const VSMatrix &objectToWorldMatrix)
{
	glDepthFunc(GL_LEQUAL);

	// [BB] In case the model should be rendered translucent, do back face culling.
	// This solves a few of the problems caused by the lack of depth sorting.
	// TO-DO: Implement proper depth sorting.
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]))
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
	}

	gl_RenderState.mModelMatrix = objectToWorldMatrix;
	gl_RenderState.EnableModelMatrix(true);
}

void FGLModelRenderer::EndDrawHUDModel(AActor *actor)
{
	gl_RenderState.EnableModelMatrix(false);

	glDepthFunc(GL_LESS);
	if (!(actor->RenderStyle == LegacyRenderStyles[STYLE_Normal]))
		glDisable(GL_CULL_FACE);
}

IModelVertexBuffer *FGLModelRenderer::CreateVertexBuffer(bool needindex, bool singleframe)
{
	return new FModelVertexBuffer(needindex, singleframe);
}

void FGLModelRenderer::SetVertexBuffer(IModelVertexBuffer *buffer)
{
	gl_RenderState.SetVertexBuffer((FModelVertexBuffer*)buffer);
}

void FGLModelRenderer::ResetVertexBuffer()
{
	gl_RenderState.SetVertexBuffer(GLRenderer->mVBO);
}

void FGLModelRenderer::SetInterpolation(double inter)
{
	gl_RenderState.SetInterpolationFactor((float)inter);
}

void FGLModelRenderer::SetMaterial(FTexture *skin, bool clampNoFilter, int translation)
{
	FMaterial * tex = FMaterial::ValidateTexture(skin, false);
	gl_RenderState.SetMaterial(tex, clampNoFilter ? CLAMP_NOFILTER : CLAMP_NONE, translation, -1, false);

	gl_RenderState.Apply();
	if (modellightindex != -1) gl_RenderState.ApplyLightIndex(modellightindex);
}

void FGLModelRenderer::DrawArrays(int start, int count)
{
	glDrawArrays(GL_TRIANGLES, start, count);
}

void FGLModelRenderer::DrawElements(int numIndices, size_t offset)
{
	glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_INT, (void*)(intptr_t)offset);
}

double FGLModelRenderer::GetTimeFloat()
{
	return (float)I_msTime() * (float)TICRATE / 1000.0f;
}

//===========================================================================
//
// Uses a hardware buffer if either single frame (i.e. no interpolation needed)
// or shading is available (interpolation is done by the vertex shader)
//
// If interpolation has to be done on the CPU side this will fall back
// to CPU-side arrays.
//
//===========================================================================

FModelVertexBuffer::FModelVertexBuffer(bool needindex, bool singleframe)
	: FVertexBuffer(singleframe || !gl.legacyMode)
{
	vbo_ptr = nullptr;
	ibo_id = 0;
	if (needindex)
	{
		glGenBuffers(1, &ibo_id);	// The index buffer can always be a real buffer.
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (!gl.legacyMode)
	{
		glEnableVertexAttribArray(VATTR_VERTEX);
		glEnableVertexAttribArray(VATTR_TEXCOORD);
		glEnableVertexAttribArray(VATTR_VERTEX2);
		glEnableVertexAttribArray(VATTR_NORMAL);
		glDisableVertexAttribArray(VATTR_COLOR);
	}
	else
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertexBuffer::~FModelVertexBuffer()
{
	if (ibo_id != 0)
	{
		glDeleteBuffers(1, &ibo_id);
	}
	if (vbo_ptr != nullptr)
	{
		delete[] vbo_ptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

FModelVertex *FModelVertexBuffer::LockVertexBuffer(unsigned int size)
{
	if (vbo_id > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glBufferData(GL_ARRAY_BUFFER, size * sizeof(FModelVertex), nullptr, GL_STATIC_DRAW);
		if (!gl.legacyMode)
			return (FModelVertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, size * sizeof(FModelVertex), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		else
			return (FModelVertex*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	}
	else
	{
		if (vbo_ptr != nullptr) delete[] vbo_ptr;
		vbo_ptr = new FModelVertex[size];
		memset(vbo_ptr, 0, size * sizeof(FModelVertex));
		return vbo_ptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockVertexBuffer()
{
	if (vbo_id > 0)
	{
		glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

//===========================================================================
//
//
//
//===========================================================================

unsigned int *FModelVertexBuffer::LockIndexBuffer(unsigned int size)
{
	if (ibo_id != 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(unsigned int), NULL, GL_STATIC_DRAW);
		if (!gl.legacyMode)
			return (unsigned int*)glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, 0, size * sizeof(unsigned int), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
		else
			return (unsigned int*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	}
	else
	{
		return nullptr;
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FModelVertexBuffer::UnlockIndexBuffer()
{
	if (ibo_id > 0)
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}


//===========================================================================
//
// Sets up the buffer starts for frame interpolation
// This must be called after gl_RenderState.Apply!
//
//===========================================================================
static TArray<FModelVertex> iBuffer;

void FModelVertexBuffer::SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size)
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	if (vbo_id > 0)
	{
		if (!gl.legacyMode)
		{
			glVertexAttribPointer(VATTR_VERTEX, 3, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame1].x);
			glVertexAttribPointer(VATTR_TEXCOORD, 2, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame1].u);
			glVertexAttribPointer(VATTR_VERTEX2, 3, GL_FLOAT, false, sizeof(FModelVertex), &VMO[frame2].x);
			glVertexAttribPointer(VATTR_NORMAL, 4, GL_INT_2_10_10_10_REV, true, sizeof(FModelVertex), &VMO[frame2].packedNormal);
		}
		else
		{
			// only used for single frame models so there is no vertex2 here, which has no use without a shader.
			glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &VMO[frame1].x);
			glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &VMO[frame1].u);
		}
	}
	else if (frame1 == frame2 || size == 0 || gl_RenderState.GetInterpolationFactor() == 0.f)
	{
		glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].u);
	}
	else
	{
		// must interpolate
		iBuffer.Resize(size);
		glVertexPointer(3, GL_FLOAT, sizeof(FModelVertex), &iBuffer[0].x);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FModelVertex), &vbo_ptr[frame1].u);
		float frac = gl_RenderState.GetInterpolationFactor();
		for (unsigned i = 0; i < size; i++)
		{
			iBuffer[i].x = vbo_ptr[frame1 + i].x * (1.f - frac) + vbo_ptr[frame2 + i].x * frac;
			iBuffer[i].y = vbo_ptr[frame1 + i].y * (1.f - frac) + vbo_ptr[frame2 + i].y * frac;
			iBuffer[i].z = vbo_ptr[frame1 + i].z * (1.f - frac) + vbo_ptr[frame2 + i].z * frac;
		}
	}
}

//===========================================================================
//
// gl_RenderModel
//
//===========================================================================

void gl_RenderModel(GLSprite * spr)
{
	FGLModelRenderer renderer;
	renderer.RenderModel(spr->x, spr->y, spr->z, spr->modelframe, spr->actor);
}

//===========================================================================
//
// gl_RenderHUDModel
//
//===========================================================================

void gl_RenderHUDModel(DPSprite *psp, float ofsX, float ofsY)
{
	FGLModelRenderer renderer;
	renderer.RenderHUDModel(psp, ofsX, ofsY);
}
