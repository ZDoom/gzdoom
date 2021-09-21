// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2009-2016 Christoph Oelckers
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
** gl_renderstate.cpp
** Render state maintenance
**
*/

#include "templates.h"
#include "gl_system.h"
#include "gl_interface.h"
#include "hw_cvars.h"
#include "flatvertices.h"
#include "gl_shader.h"
#include "gl_renderer.h"
#include "hw_lightbuffer.h"
#include "gl_renderbuffers.h"
#include "gl_hwtexture.h"
#include "gl_buffers.h"
#include "hw_clock.h"
#include "hwrenderer/data/hw_viewpointbuffer.h"

namespace OpenGLRenderer
{

FGLRenderState gl_RenderState;

static VSMatrix identityMatrix(1);

static void matrixToGL(const VSMatrix &mat, int loc)
{
	glUniformMatrix4fv(loc, 1, false, (float*)&mat);
}

//==========================================================================
//
// This only gets called once upon setup.
// With OpenGL the state is persistent and cannot be cleared, once set up.
//
//==========================================================================

void FGLRenderState::Reset()
{
	FRenderState::Reset();
	mVertexBuffer = mCurrentVertexBuffer = nullptr;
	mGlossiness = 0.0f;
	mSpecularLevel = 0.0f;
	mShaderTimer = 0.0f;

	stRenderStyle = DefaultRenderStyle();
	stSrcBlend = stDstBlend = -1;
	stBlendEquation = -1;
	stAlphaTest = 0;
	mLastDepthClamp = true;

	mEffectState = 0;
	activeShader = nullptr;

	mCurrentVertexBuffer = nullptr;
	mCurrentVertexOffsets[0] = mVertexOffsets[0] = 0;
	mCurrentIndexBuffer = nullptr;

}

//==========================================================================
//
// Apply shader settings
//
//==========================================================================

bool FGLRenderState::ApplyShader()
{
	static const float nulvec[] = { 0.f, 0.f, 0.f, 0.f };
	if (mSpecialEffect > EFF_NONE)
	{
		activeShader = GLRenderer->mShaderManager->BindEffect(mSpecialEffect, mPassType);
	}
	else
	{
		activeShader = GLRenderer->mShaderManager->Get(mTextureEnabled ? mEffectState : SHADER_NoTexture, mAlphaThreshold >= 0.f, mPassType);
		activeShader->Bind();
	}

	int fogset = 0;

	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			fogset = -3;	// 2D rendering with 'foggy' overlay.
		}
		else if ((GetFogColor() & 0xffffff) == 0)
		{
			fogset = gl_fogmode;
		}
		else
		{
			fogset = -gl_fogmode;
		}
	}

	glVertexAttrib4fv(VATTR_COLOR, &mStreamData.uVertexColor.X);
	glVertexAttrib4fv(VATTR_NORMAL, &mStreamData.uVertexNormal.X);

	activeShader->muDesaturation.Set(mStreamData.uDesaturationFactor);
	activeShader->muFogEnabled.Set(fogset);

	activeShader->muTextureMode.Set(GetTextureModeAndFlags(mTempTM));
	activeShader->muLightParms.Set(mLightParms);
	activeShader->muFogColor.Set(mStreamData.uFogColor);
	activeShader->muObjectColor.Set(mStreamData.uObjectColor);
	activeShader->muDynLightColor.Set(&mStreamData.uDynLightColor.X);
	activeShader->muInterpolationFactor.Set(mStreamData.uInterpolationFactor);
	activeShader->muTimer.Set((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);
	activeShader->muAlphaThreshold.Set(mAlphaThreshold);
	activeShader->muLightIndex.Set(-1);
	activeShader->muClipSplit.Set(mClipSplit);
	activeShader->muSpecularMaterial.Set(mGlossiness, mSpecularLevel);
	activeShader->muAddColor.Set(mStreamData.uAddColor);
	activeShader->muTextureAddColor.Set(mStreamData.uTextureAddColor);
	activeShader->muTextureModulateColor.Set(mStreamData.uTextureModulateColor);
	activeShader->muTextureBlendColor.Set(mStreamData.uTextureBlendColor);
	activeShader->muDetailParms.Set(&mStreamData.uDetailParms.X);
#ifdef NPOT_EMULATION
	activeShader->muNpotEmulation.Set(&mStreamData.uNpotEmulation.X);
#endif

	if (mGlowEnabled || activeShader->currentglowstate)
	{
		activeShader->muGlowTopColor.Set(&mStreamData.uGlowTopColor.X);
		activeShader->muGlowBottomColor.Set(&mStreamData.uGlowBottomColor.X);
		activeShader->muGlowTopPlane.Set(&mStreamData.uGlowTopPlane.X);
		activeShader->muGlowBottomPlane.Set(&mStreamData.uGlowBottomPlane.X);
		activeShader->currentglowstate = mGlowEnabled;
	}

	if (mGradientEnabled || activeShader->currentgradientstate)
	{
		activeShader->muObjectColor2.Set(mStreamData.uObjectColor2);
		activeShader->muGradientTopPlane.Set(&mStreamData.uGradientTopPlane.X);
		activeShader->muGradientBottomPlane.Set(&mStreamData.uGradientBottomPlane.X);
		activeShader->currentgradientstate = mGradientEnabled;
	}

	if (mSplitEnabled || activeShader->currentsplitstate)
	{
		activeShader->muSplitTopPlane.Set(&mStreamData.uSplitTopPlane.X);
		activeShader->muSplitBottomPlane.Set(&mStreamData.uSplitBottomPlane.X);
		activeShader->currentsplitstate = mSplitEnabled;
	}


	if (mTextureMatrixEnabled)
	{
		matrixToGL(mTextureMatrix, activeShader->texturematrix_index);
		activeShader->currentTextureMatrixState = true;
	}
	else if (activeShader->currentTextureMatrixState)
	{
		activeShader->currentTextureMatrixState = false;
		matrixToGL(identityMatrix, activeShader->texturematrix_index);
	}

	if (mModelMatrixEnabled)
	{
		matrixToGL(mModelMatrix, activeShader->modelmatrix_index);
		VSMatrix norm;
		norm.computeNormalMatrix(mModelMatrix);
		matrixToGL(norm, activeShader->normalmodelmatrix_index);
		activeShader->currentModelMatrixState = true;
	}
	else if (activeShader->currentModelMatrixState)
	{
		activeShader->currentModelMatrixState = false;
		matrixToGL(identityMatrix, activeShader->modelmatrix_index);
		matrixToGL(identityMatrix, activeShader->normalmodelmatrix_index);
	}

	int index = mLightIndex;
	// Mess alert for crappy AncientGL!
	if (!screen->mLights->GetBufferType() && index >= 0)
	{
		size_t start, size;
		index = screen->mLights->GetBinding(index, &start, &size);

		if (start != mLastMappedLightIndex || screen->mPipelineNbr > 1) // If multiple buffers always bind
		{
			mLastMappedLightIndex = start;
			static_cast<GLDataBuffer*>(screen->mLights->GetBuffer())->BindRange(nullptr, start, size);
		}
	}

	activeShader->muLightIndex.Set(index);
	return true;
}


//==========================================================================
//
// Apply State
//
//==========================================================================

void FGLRenderState::ApplyState()
{
	if (mRenderStyle != stRenderStyle)
	{
		ApplyBlendMode();
		stRenderStyle = mRenderStyle;
	}

	if (mSplitEnabled != stSplitEnabled)
	{
		if (mSplitEnabled)
		{
			glEnable(GL_CLIP_DISTANCE3);
			glEnable(GL_CLIP_DISTANCE4);
		}
		else
		{
			glDisable(GL_CLIP_DISTANCE3);
			glDisable(GL_CLIP_DISTANCE4);
		}
		stSplitEnabled = mSplitEnabled;
	}

	if (mMaterial.mChanged)
	{
		ApplyMaterial(mMaterial.mMaterial, mMaterial.mClampMode, mMaterial.mTranslation, mMaterial.mOverrideShader);
		mMaterial.mChanged = false;
	}

	if (mBias.mChanged)
	{
		if (mBias.mFactor == 0 && mBias.mUnits == 0)
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		else
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
		}
		glPolygonOffset(mBias.mFactor, mBias.mUnits);
		mBias.mChanged = false;
	}
}

void FGLRenderState::ApplyBuffers()
{
	if (mVertexBuffer != mCurrentVertexBuffer || mVertexOffsets[0] != mCurrentVertexOffsets[0] || mVertexOffsets[1] != mCurrentVertexOffsets[1])
	{
		assert(mVertexBuffer != nullptr);
		static_cast<GLVertexBuffer*>(mVertexBuffer)->Bind(mVertexOffsets);
		mCurrentVertexBuffer = mVertexBuffer;
		mCurrentVertexOffsets[0] = mVertexOffsets[0];
		mCurrentVertexOffsets[1] = mVertexOffsets[1];
	}
	if (mIndexBuffer != mCurrentIndexBuffer)
	{
		if (mIndexBuffer) static_cast<GLIndexBuffer*>(mIndexBuffer)->Bind();
		mCurrentIndexBuffer = mIndexBuffer;
	}
}

void FGLRenderState::Apply()
{
	ApplyState();
	ApplyBuffers();
	ApplyShader();
}

//===========================================================================
// 
//	Binds a texture to the renderer
//
//===========================================================================

void FGLRenderState::ApplyMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader)
{
	if (mat->Source()->isHardwareCanvas())
	{
		mTempTM = TM_OPAQUE;
	}
	else
	{
		mTempTM = TM_NORMAL;
	}
	auto tex = mat->Source();
	mEffectState = overrideshader >= 0 ? overrideshader : mat->GetShaderIndex();
	mShaderTimer = tex->GetShaderSpeed();
	SetSpecular(tex->GetGlossiness(), tex->GetSpecularLevel());
	if (tex->isHardwareCanvas()) static_cast<FCanvasTexture*>(tex->GetTexture())->NeedUpdate();

	clampmode = tex->GetClampMode(clampmode);
	
	// avoid rebinding the same texture multiple times.
	if (mat == lastMaterial && lastClamp == clampmode && translation == lastTranslation) return;
	lastMaterial = mat;
	lastClamp = clampmode;
	lastTranslation = translation;

	int usebright = false;
	int maxbound = 0;

	int numLayers = mat->NumLayers();
	MaterialLayerInfo* layer;
	auto base = static_cast<FHardwareTexture*>(mat->GetLayer(0, translation, &layer));

	if (base->BindOrCreate(tex->GetTexture(), 0, clampmode, translation, layer->scaleFlags))
	{
		if (!(layer->scaleFlags & CTF_Indexed))
		{
			for (int i = 1; i < numLayers; i++)
			{
				auto systex = static_cast<FHardwareTexture*>(mat->GetLayer(i, 0, &layer));
				// fixme: Upscale flags must be disabled for certain layers.
				systex->BindOrCreate(layer->layerTexture, i, clampmode, 0, layer->scaleFlags);
				maxbound = i;
			}
		}
		else
		{
			for (int i = 1; i < 3; i++)
			{
				auto systex = static_cast<FHardwareTexture*>(mat->GetLayer(i, translation, &layer));
				systex->Bind(i, false);
				maxbound = i;
			}
		}
	}
	// unbind everything from the last texture that's still active
	for (int i = maxbound + 1; i <= maxBoundMaterial; i++)
	{
		FHardwareTexture::Unbind(i);
		maxBoundMaterial = maxbound;
	}
}

//==========================================================================
//
// Apply blend mode from RenderStyle
//
//==========================================================================

void FGLRenderState::ApplyBlendMode()
{
	static int blendstyles[] = { GL_ZERO, GL_ONE, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA };
	static int renderops[] = { 0, GL_FUNC_ADD, GL_FUNC_SUBTRACT, GL_FUNC_REVERSE_SUBTRACT, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1 };

	int srcblend = blendstyles[mRenderStyle.SrcAlpha%STYLEALPHA_MAX];
	int dstblend = blendstyles[mRenderStyle.DestAlpha%STYLEALPHA_MAX];
	int blendequation = renderops[mRenderStyle.BlendOp & 15];

	if (blendequation == -1)	// This was a fuzz style.
	{
		srcblend = GL_DST_COLOR;
		dstblend = GL_ONE_MINUS_SRC_ALPHA;
		blendequation = GL_FUNC_ADD;
	}

	// Checks must be disabled until all draw code has been converted.
	//if (srcblend != stSrcBlend || dstblend != stDstBlend)
	{
		stSrcBlend = srcblend;
		stDstBlend = dstblend;
		glBlendFunc(srcblend, dstblend);
	}
	//if (blendequation != stBlendEquation)
	{
		stBlendEquation = blendequation;
		glBlendEquation(blendequation);
	}

}

//==========================================================================
//
// API dependent draw calls
//
//==========================================================================

static int dt2gl[] = { GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_FAN, GL_TRIANGLE_STRIP };

void FGLRenderState::Draw(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply();
	}
	drawcalls.Clock();
	glDrawArrays(dt2gl[dt], index, count);
	drawcalls.Unclock();
}

void FGLRenderState::DrawIndexed(int dt, int index, int count, bool apply)
{
	if (apply)
	{
		Apply();
	}
	drawcalls.Clock();
	glDrawElements(dt2gl[dt], count, GL_UNSIGNED_INT, (void*)(intptr_t)(index * sizeof(uint32_t)));
	drawcalls.Unclock();
}

void FGLRenderState::SetDepthMask(bool on)
{
	glDepthMask(on);
}

void FGLRenderState::SetDepthFunc(int func)
{
	static int df2gl[] = { GL_LESS, GL_LEQUAL, GL_ALWAYS };
	glDepthFunc(df2gl[func]);
}

void FGLRenderState::SetDepthRange(float min, float max)
{
	glDepthRange(min, max);
}

void FGLRenderState::SetColorMask(bool r, bool g, bool b, bool a)
{
	glColorMask(r, g, b, a);
}

void FGLRenderState::SetStencil(int offs, int op, int flags = -1)
{
	static int op2gl[] = { GL_KEEP, GL_INCR, GL_DECR };

	glStencilFunc(GL_EQUAL, screen->stencilValue + offs, ~0);		// draw sky into stencil
	glStencilOp(GL_KEEP, GL_KEEP, op2gl[op]);		// this stage doesn't modify the stencil

	if (flags != -1)
	{
		bool cmon = !(flags & SF_ColorMaskOff);
		glColorMask(cmon, cmon, cmon, cmon);						// don't write to the graphics buffer
		glDepthMask(!(flags & SF_DepthMaskOff));
	}
}

void FGLRenderState::ToggleState(int state, bool on)
{
	if (on)
	{
		glEnable(state);
	}
	else
	{
		glDisable(state);
	}
}

void FGLRenderState::SetCulling(int mode)
{
	if (mode != Cull_None)
	{
		glEnable(GL_CULL_FACE);
		glFrontFace(mode == Cull_CCW ? GL_CCW : GL_CW);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}
}

void FGLRenderState::EnableClipDistance(int num, bool state)
{
	// Update the viewpoint-related clip plane setting.
	if (!(gl.flags & RFL_NO_CLIP_PLANES))
	{
		ToggleState(GL_CLIP_DISTANCE0 + num, state);
	}
}

void FGLRenderState::Clear(int targets)
{
	// This always clears to default values.
	int gltarget = 0;
	if (targets & CT_Depth)
	{
		gltarget |= GL_DEPTH_BUFFER_BIT;
		glClearDepth(1);
	}
	if (targets & CT_Stencil)
	{
		gltarget |= GL_STENCIL_BUFFER_BIT;
		glClearStencil(0);
	}
	if (targets & CT_Color)
	{
		gltarget |= GL_COLOR_BUFFER_BIT;
		glClearColor(screen->mSceneClearColor[0], screen->mSceneClearColor[1], screen->mSceneClearColor[2], screen->mSceneClearColor[3]);
	}
	glClear(gltarget);
}

void FGLRenderState::EnableStencil(bool on)
{
	ToggleState(GL_STENCIL_TEST, on);
}

void FGLRenderState::SetScissor(int x, int y, int w, int h)
{
	if (w > -1)
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(x, y, w, h);
	}
	else
	{
		glDisable(GL_SCISSOR_TEST);
	}
}

void FGLRenderState::SetViewport(int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
}

void FGLRenderState::EnableDepthTest(bool on)
{
	ToggleState(GL_DEPTH_TEST, on);
}

void FGLRenderState::EnableMultisampling(bool on)
{
	ToggleState(GL_MULTISAMPLE, on);
}

void FGLRenderState::EnableLineSmooth(bool on)
{
	ToggleState(GL_LINE_SMOOTH, on);
}

//==========================================================================
//
//
//
//==========================================================================
void FGLRenderState::ClearScreen()
{
	bool multi = !!glIsEnabled(GL_MULTISAMPLE);

	screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
	SetColor(0, 0, 0);
	Apply();

	glDisable(GL_MULTISAMPLE);
	glDisable(GL_DEPTH_TEST);

	glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);

	glEnable(GL_DEPTH_TEST);
	if (multi) glEnable(GL_MULTISAMPLE);
}



//==========================================================================
//
// Below are less frequently altrered state settings which do not get
// buffered by the state object, but set directly instead.
//
//==========================================================================

bool FGLRenderState::SetDepthClamp(bool on)
{
	bool res = mLastDepthClamp;
	if (!on) glDisable(GL_DEPTH_CLAMP);
	else glEnable(GL_DEPTH_CLAMP);
	mLastDepthClamp = on;
	return res;
}

}
