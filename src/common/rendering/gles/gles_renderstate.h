// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2009-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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

#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <algorithm>
#include <string.h>
#include "matrix.h"
#include "hw_renderstate.h"
#include "hw_material.h"
#include "c_cvars.h"
#include "hwrenderer/data/hw_viewpointuniforms.h"

namespace OpenGLESRenderer
{

class FShader;
struct HWSectorPlane;

class FGLRenderState final : public FRenderState
{
	uint8_t mLastDepthClamp : 1;

	float mGlossiness, mSpecularLevel;
	float mShaderTimer;

	int mEffectState;
	int mTempTM = TM_NORMAL;

	FRenderStyle stRenderStyle;
	int stSrcBlend, stDstBlend;
	bool stAlphaTest;
	bool stSplitEnabled;
	int stBlendEquation;

	FShader *activeShader;

	int mNumDrawBuffers = 1;

	bool ApplyShader();
	void ApplyState();

	// Texture binding state
	FMaterial *lastMaterial = nullptr;
	int lastClamp = 0;
	int lastTranslation = 0;
	int maxBoundMaterial = -1;
	size_t mLastMappedLightIndex = SIZE_MAX;

	IVertexBuffer *mCurrentVertexBuffer;
	int mCurrentVertexOffsets[2];	// one per binding point
	IIndexBuffer *mCurrentIndexBuffer;

	HWViewpointUniforms* mHwUniforms = nullptr;

public:

	FGLRenderState()
	{
		Reset();
	}

	void Reset();

	void ClearLastMaterial()
	{
		lastMaterial = nullptr;
	}

	void ApplyMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader);

	void Apply();
	void ApplyBuffers();
	void ApplyBlendMode();

	void ResetVertexBuffer()
	{
		// forces rebinding with the next 'apply' call.
		mCurrentVertexBuffer = nullptr;
		mCurrentIndexBuffer = nullptr;
	}

	void SetSpecular(float glossiness, float specularLevel)
	{
		mGlossiness = glossiness;
		mSpecularLevel = specularLevel;
	}

	void EnableDrawBuffers(int count, bool apply = false) override
	{
		/*
		count = min(count, 3);
		if (mNumDrawBuffers != count)
		{
			static GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(count, buffers);
			mNumDrawBuffers = count;
		}
		if (apply) Apply();
		*/
	}

	void ToggleState(int state, bool on);

	void ClearScreen() override;
	void Draw(int dt, int index, int count, bool apply = true) override;
	void DrawIndexed(int dt, int index, int count, bool apply = true) override;

	bool SetDepthClamp(bool on) override;
	void SetDepthMask(bool on) override;
	void SetDepthFunc(int func) override;
	void SetDepthRange(float min, float max) override;
	void SetColorMask(bool r, bool g, bool b, bool a) override;
	void SetStencil(int offs, int op, int flags) override;
	void SetCulling(int mode) override;
	void EnableClipDistance(int num, bool state) override;
	void Clear(int targets) override;
	void EnableStencil(bool on) override;
	void SetScissor(int x, int y, int w, int h) override;
	void SetViewport(int x, int y, int w, int h) override;
	void EnableDepthTest(bool on) override;
	void EnableMultisampling(bool on) override;
	void EnableLineSmooth(bool on) override;

	void ApplyViewport(void* data);
};

extern FGLRenderState gl_RenderState;

}

#endif
