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


#include "gles_system.h"
#include "hw_cvars.h"
#include "flatvertices.h"
#include "gles_shader.h"
#include "gles_renderer.h"
#include "hw_lightbuffer.h"
#include "gles_renderbuffers.h"
#include "gles_hwtexture.h"
#include "gles_buffers.h"
#include "gles_renderer.h"
#include "gles_samplers.h"

#include "hw_clock.h"
#include "printf.h"

#include "hwrenderer/data/hw_viewpointbuffer.h"

namespace OpenGLESRenderer
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

	ShaderFlavourData flavour;

	// Need to calc light data now in order to select correct shader
	float* lightPtr = NULL;
	int modLights = 0;
	int subLights = 0;
	int addLights = 0;
	int totalLights = 0;

	flavour.hasSpotLight = false;

	if (mLightIndex >= 0)
	{
		lightPtr = ((float*)screen->mLights->GetBuffer()->Memory());
		lightPtr += ((int64_t)mLightIndex * 4);
		//float array[64];
		//memcpy(array, ptr, 4 * 64);

		// Calculate how much light data there is to upload, this is stored in the first 4 floats
		modLights = int(lightPtr[1]) / LIGHT_VEC4_NUM;
		subLights = (int(lightPtr[2]) - int(lightPtr[1])) / LIGHT_VEC4_NUM;
		addLights = (int(lightPtr[3]) - int(lightPtr[2])) / LIGHT_VEC4_NUM;

		// Here we limit the number of lights, but dont' change the light data so priority has to be mod, sub then add
		if (modLights > (int)gles.maxlights)
			modLights = gles.maxlights;

		if (modLights + subLights > (int)gles.maxlights)
			subLights = gles.maxlights - modLights;

		if (modLights + subLights + addLights > (int)gles.maxlights)
			addLights = gles.maxlights - modLights - subLights;

		totalLights = modLights + subLights + addLights;

		// Skip passed the first 4 floats so the upload below only contains light data
		lightPtr += 4;

		float* findSpotsPtr = lightPtr + 11; // The 11th float contains '1' if the light is a spot light, see hw_dynlightdata.cpp

		for (int n = 0; n < totalLights; n++)
		{
			if (*findSpotsPtr > 0) // This is a spot light
			{
				flavour.hasSpotLight = true;
				break;
			}
			findSpotsPtr += LIGHT_VEC4_NUM * 4;
		}
	}


	int tm = GetTextureModeAndFlags(mTempTM);
	flavour.textureMode = tm & 0xffff;
	flavour.texFlags = tm >> 16; //Move flags to start of word

	if (mTextureClamp && flavour.textureMode == TM_NORMAL) flavour.textureMode = TM_CLAMPY; // fixme. Clamp can now be combined with all modes.

	if (flavour.textureMode == -1)
		flavour.textureMode = 0;


	flavour.blendFlags = (int)(mStreamData.uTextureAddColor.a + 0.01);
	flavour.paletteInterpolate = !!(flavour.blendFlags & 0x4000);

	flavour.twoDFog = false;
	flavour.fogEnabled = false;
	flavour.fogEquationRadial = false;
	flavour.colouredFog = false;

	flavour.fogEquationRadial = (gl_fogmode == 2);

	flavour.twoDFog = false;
	flavour.fogEnabled = false;
	flavour.colouredFog = false;

	if (mFogEnabled)
	{
		if (mFogEnabled == 2)
		{
			flavour.twoDFog = true;
		}
		else if (gl_fogmode)
		{
			flavour.fogEnabled = true;

			if ((GetFogColor() & 0xffffff) != 0)
				flavour.colouredFog = true;
		}
	}

	flavour.doDesaturate = mStreamData.uDesaturationFactor != 0;
	flavour.useULightLevel = (mLightParms[3] >= 0); //#define uLightLevel uLightAttr.a

	// Yes create shaders for all combinations of active lights to avoid more branches
	flavour.dynLightsMod = (modLights > 0);
	flavour.dynLightsSub = (subLights > 0);
	flavour.dynLightsAdd = (addLights > 0);

	flavour.useObjectColor2 = (mStreamData.uObjectColor2.a > 0);
	flavour.useGlowTopColor = mGlowEnabled && (mStreamData.uGlowTopColor[3] > 0);
	flavour.useGlowBottomColor = mGlowEnabled && (mStreamData.uGlowBottomColor[3] > 0);

	flavour.useColorMap = (mColorMapSpecial >= CM_FIRSTSPECIALCOLORMAP) || (mColorMapFlash != 1);

	flavour.buildLighting = (mHwUniforms->mPalLightLevels >> 16) == 5; // Build engine mode
	flavour.bandedSwLight = !!(mHwUniforms->mPalLightLevels & 0xFF);

#ifdef NPOT_EMULATION
	flavour.npotEmulation = (mStreamData.uNpotEmulation.Y != 0);
#endif

	if (mSpecialEffect > EFF_NONE)
	{
		activeShader = GLRenderer->mShaderManager->BindEffect(mSpecialEffect, mPassType, flavour);
	}
	else
	{
		activeShader = GLRenderer->mShaderManager->Get(mTextureEnabled ? mEffectState : SHADER_NoTexture, mAlphaThreshold >= 0.f, mPassType);

		activeShader->Bind(flavour);
	}


	if (mHwUniforms)
	{
		activeShader->cur->muProjectionMatrix.Set(&mHwUniforms->mProjectionMatrix);
		activeShader->cur->muViewMatrix.Set(&mHwUniforms->mViewMatrix);
		activeShader->cur->muNormalViewMatrix.Set(&mHwUniforms->mNormalViewMatrix);

		activeShader->cur->muCameraPos.Set(&mHwUniforms->mCameraPos.X);
		activeShader->cur->muClipLine.Set(&mHwUniforms->mClipLine.X);

		activeShader->cur->muGlobVis.Set(mHwUniforms->mGlobVis);

		activeShader->cur->muPalLightLevels.Set(mHwUniforms->mPalLightLevels & 0xFF); // JUST pass the pal levels, clear the top bits
		activeShader->cur->muViewHeight.Set(mHwUniforms->mViewHeight);
		activeShader->cur->muClipHeight.Set(mHwUniforms->mClipHeight);
		activeShader->cur->muClipHeightDirection.Set(mHwUniforms->mClipHeightDirection);
		//activeShader->cur->muShadowmapFilter.Set(mHwUniforms->mShadowmapFilter);
	}

	glVertexAttrib4fv(VATTR_COLOR, &mStreamData.uVertexColor.X);
	glVertexAttrib4fv(VATTR_NORMAL, &mStreamData.uVertexNormal.X);

	activeShader->cur->muDesaturation.Set(mStreamData.uDesaturationFactor);
	//activeShader->cur->muFogEnabled.Set(fogset);

	activeShader->cur->muLightParms.Set(mLightParms);
	activeShader->cur->muFogColor.Set(mStreamData.uFogColor);
	activeShader->cur->muObjectColor.Set(mStreamData.uObjectColor);
	activeShader->cur->muDynLightColor.Set(&mStreamData.uDynLightColor.X);
	activeShader->cur->muInterpolationFactor.Set(mStreamData.uInterpolationFactor);
	activeShader->cur->muTimer.Set((double)(screen->FrameTime - firstFrame) * (double)mShaderTimer / 1000.);
	activeShader->cur->muAlphaThreshold.Set(mAlphaThreshold);
	activeShader->cur->muClipSplit.Set(mClipSplit);
	activeShader->cur->muSpecularMaterial.Set(mGlossiness, mSpecularLevel);
	activeShader->cur->muAddColor.Set(mStreamData.uAddColor);
	activeShader->cur->muTextureAddColor.Set(mStreamData.uTextureAddColor);
	activeShader->cur->muTextureModulateColor.Set(mStreamData.uTextureModulateColor);
	activeShader->cur->muTextureBlendColor.Set(mStreamData.uTextureBlendColor);
	activeShader->cur->muDetailParms.Set(&mStreamData.uDetailParms.X);
#ifdef NPOT_EMULATION
	activeShader->cur->muNpotEmulation.Set(&mStreamData.uNpotEmulation.X);
#endif

	if (flavour.useColorMap)
	{
		if (mColorMapSpecial < CM_FIRSTSPECIALCOLORMAP || mColorMapSpecial >= CM_MAXCOLORMAP)
		{
			activeShader->cur->muFixedColormapStart.Set( 0,0,0, mColorMapFlash );
			activeShader->cur->muFixedColormapRange.Set( 0,0,0, 1.f );
		}
		else
		{
			FSpecialColormap* scm = &SpecialColormaps[mColorMapSpecial - CM_FIRSTSPECIALCOLORMAP];

			//uniforms.MapStart = { scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], flash };
			activeShader->cur->muFixedColormapStart.Set( scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], mColorMapFlash );
			activeShader->cur->muFixedColormapRange.Set( scm->ColorizeEnd[0] - scm->ColorizeStart[0],
				scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f );
		}
	}

	if (mGlowEnabled || activeShader->cur->currentglowstate)
	{
		activeShader->cur->muGlowTopColor.Set(&mStreamData.uGlowTopColor.X);
		activeShader->cur->muGlowBottomColor.Set(&mStreamData.uGlowBottomColor.X);
		activeShader->cur->muGlowTopPlane.Set(&mStreamData.uGlowTopPlane.X);
		activeShader->cur->muGlowBottomPlane.Set(&mStreamData.uGlowBottomPlane.X);
		activeShader->cur->currentglowstate = mGlowEnabled;
	}

	if (mGradientEnabled || activeShader->cur->currentgradientstate)
	{
		activeShader->cur->muObjectColor2.Set(mStreamData.uObjectColor2);
		activeShader->cur->muGradientTopPlane.Set(&mStreamData.uGradientTopPlane.X);
		activeShader->cur->muGradientBottomPlane.Set(&mStreamData.uGradientBottomPlane.X);
		activeShader->cur->currentgradientstate = mGradientEnabled;
	}

	if (mSplitEnabled || activeShader->cur->currentsplitstate)
	{
		activeShader->cur->muSplitTopPlane.Set(&mStreamData.uSplitTopPlane.X);
		activeShader->cur->muSplitBottomPlane.Set(&mStreamData.uSplitBottomPlane.X);
		activeShader->cur->currentsplitstate = mSplitEnabled;
	}


	if (mTextureMatrixEnabled)
	{
		matrixToGL(mTextureMatrix, activeShader->cur->texturematrix_index);
		activeShader->cur->currentTextureMatrixState = true;
	}
	else if (activeShader->cur->currentTextureMatrixState)
	{
		activeShader->cur->currentTextureMatrixState = false;
		matrixToGL(identityMatrix, activeShader->cur->texturematrix_index);
	}

	if (mModelMatrixEnabled)
	{
		matrixToGL(mModelMatrix, activeShader->cur->modelmatrix_index);
		VSMatrix norm;
		norm.computeNormalMatrix(mModelMatrix);
		matrixToGL(norm, activeShader->cur->normalmodelmatrix_index);
		activeShader->cur->currentModelMatrixState = true;
	}
	else if (activeShader->cur->currentModelMatrixState)
	{
		activeShader->cur->currentModelMatrixState = false;
		matrixToGL(identityMatrix, activeShader->cur->modelmatrix_index);
		matrixToGL(identityMatrix, activeShader->cur->normalmodelmatrix_index);
	}

	// Upload the light data
	if (mLightIndex >= 0)
	{
		// Calculate the total number of vec4s we need
		int totalVectors = totalLights * LIGHT_VEC4_NUM;

		if (totalVectors > (int)gles.numlightvectors)
			totalVectors = gles.numlightvectors;

		glUniform4fv(activeShader->cur->lights_index, totalVectors, lightPtr);

		int range[4] = { 0, 
			modLights * LIGHT_VEC4_NUM, 
			(modLights + subLights) * LIGHT_VEC4_NUM, 
			(modLights + subLights + addLights) * LIGHT_VEC4_NUM };

		activeShader->cur->muLightRange.Set(range);
	}

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
		/*
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
		*/
		stSplitEnabled = mSplitEnabled;
	}

	if (mMaterial.mChanged)
	{
		ApplyMaterial(mMaterial.mMaterial, mMaterial.mClampMode, mMaterial.mTranslation, mMaterial.mOverrideShader);
		mMaterial.mChanged = false;
	}


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
				GLRenderer->mSamplerManager->Bind(i, CLAMP_NONE, 255);
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
	if (srcblend != stSrcBlend || dstblend != stDstBlend)
	{
		stSrcBlend = srcblend;
		stDstBlend = dstblend;
		glBlendFunc(srcblend, dstblend);
	}
	if (blendequation != stBlendEquation)
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
	glDepthRangef(min, max);
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

}

void FGLRenderState::Clear(int targets)
{
	// This always clears to default values.
	int gltarget = 0;
	if (targets & CT_Depth)
	{
		gltarget |= GL_DEPTH_BUFFER_BIT;

		glClearDepthf(1);
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

}

void FGLRenderState::EnableLineSmooth(bool on)
{

}


//==========================================================================
//
//
//
//==========================================================================
void FGLRenderState::ClearScreen()
{

	screen->mViewpoints->Set2D(*this, SCREENWIDTH, SCREENHEIGHT);
	SetColor(0, 0, 0);
	Apply();

	glDisable(GL_DEPTH_TEST);

	glDrawArrays(GL_TRIANGLE_STRIP, FFlatVertexBuffer::FULLSCREEN_INDEX, 4);

	glEnable(GL_DEPTH_TEST);

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

	if (gles.depthClampAvailable)
	{
		if (!on) glDisable(GL_DEPTH_CLAMP);
		else glEnable(GL_DEPTH_CLAMP);
	}

	mLastDepthClamp = on;
	return res;
}
void FGLRenderState::ApplyViewport(void* data)
{	
	mHwUniforms = reinterpret_cast<HWViewpointUniforms*>(static_cast<uint8_t*>(data));

}
}
