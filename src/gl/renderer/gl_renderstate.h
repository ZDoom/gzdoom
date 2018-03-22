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

#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "r_data/matrix.h"
#include "gl/textures/gl_material.h"
#include "c_cvars.h"
#include "r_defs.h"
#include "r_data/r_translate.h"

class FVertexBuffer;
class FShader;
extern TArray<VSMatrix> gl_MatrixStack;

EXTERN_CVAR(Bool, gl_direct_state_change)

struct FStateVec4
{
	float vec[4];

	void Set(float r, float g, float b, float a)
	{
		vec[0] = r;
		vec[1] = g;
		vec[2] = b;
		vec[3] = a;
	}
};


enum EEffect
{
	EFF_NONE=-1,
	EFF_FOGBOUNDARY,
	EFF_SPHEREMAP,
	EFF_BURN,
	EFF_STENCIL,

	MAX_EFFECTS
};

enum EPassType
{
	NORMAL_PASS,
	GBUFFER_PASS,
	MAX_PASS_TYPES
};

class FRenderState
{
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mSplitEnabled;
	bool mClipLineEnabled;
	bool mBrightmapEnabled;
	bool mColorMask[4];
	bool currentColorMask[4];
	int mLightIndex;
	int mSpecialEffect;
	int mTextureMode;
	int mDesaturation;
	int mSoftLight;
	float mLightParms[4];
	int mSrcBlend, mDstBlend;
	float mAlphaThreshold;
	int mBlendEquation;
	bool mModelMatrixEnabled;
	bool mTextureMatrixEnabled;
	bool mLastDepthClamp;
	float mInterpolationFactor;
	float mClipHeight, mClipHeightDirection;
	float mGlossiness, mSpecularLevel;
	float mShaderTimer;

	FVertexBuffer *mVertexBuffer, *mCurrentVertexBuffer;
	FStateVec4 mNormal;
	FStateVec4 mColor;
	FStateVec4 mCameraPos;
	FStateVec4 mGlowTop, mGlowBottom;
	FStateVec4 mGlowTopPlane, mGlowBottomPlane;
	FStateVec4 mSplitTopPlane, mSplitBottomPlane;
	FStateVec4 mClipLine;
	PalEntry mFogColor;
	PalEntry mObjectColor;
	PalEntry mObjectColor2;
	FStateVec4 mDynColor;
	float mClipSplit[2];

	int mEffectState;
	int mColormapState;
	int mTempTM = TM_MODULATE;

	float stAlphaThreshold;
	int stSrcBlend, stDstBlend;
	bool stAlphaTest;
	int stBlendEquation;

	FShader *activeShader;

	EPassType mPassType = NORMAL_PASS;
	int mNumDrawBuffers = 1;

	bool ApplyShader();

public:

	VSMatrix mProjectionMatrix;
	VSMatrix mViewMatrix;
	VSMatrix mModelMatrix;
	VSMatrix mTextureMatrix;
	VSMatrix mNormalViewMatrix;

	FRenderState()
	{
		Reset();
	}

	void Reset();

	void SetMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader, bool alphatexture)
	{
		// alpha textures need special treatment in the legacy renderer because withouz shaders they need a different texture.
		if (alphatexture &&  gl.legacyMode) translation = INT_MAX;
		
		if (mat->tex->bHasCanvas)
		{
			mTempTM = TM_OPAQUE;
		}
		else
		{
			mTempTM = TM_MODULATE;
		}
		mEffectState = overrideshader >= 0? overrideshader : mat->mShaderIndex;
		mShaderTimer = mat->tex->gl_info.shaderspeed;
		SetSpecular(mat->tex->gl_info.Glossiness, mat->tex->gl_info.SpecularLevel);
		mat->Bind(clampmode, translation);
	}

	void Apply();
	void ApplyColorMask();
	void ApplyMatrices();
	void ApplyLightIndex(int index);

	void SetVertexBuffer(FVertexBuffer *vb)
	{
		mVertexBuffer = vb;
	}

	void ResetVertexBuffer()
	{
		// forces rebinding with the next 'apply' call.
		mCurrentVertexBuffer = NULL;
	}

	float GetClipHeight()
	{
		return mClipHeight;
	}

	float GetClipHeightDirection()
	{
		return mClipHeightDirection;
	}

	FStateVec4 &GetClipLine()
	{
		return mClipLine;
	}

	bool GetClipLineState()
	{
		return mClipLineEnabled;
	}

	void SetClipHeight(float height, float direction);

	void SetNormal(FVector3 norm)
	{
		mNormal.Set(norm.X, norm.Y, norm.Z, 0.f);
	}

	void SetNormal(float x, float y, float z)
	{
		mNormal.Set(x, y, z, 0.f);
	}

	void SetColor(float r, float g, float b, float a = 1.f, int desat = 0)
	{
		mColor.Set(r, g, b, a);
		mDesaturation = desat;
	}

	void SetColor(PalEntry pe, int desat = 0)
	{
		mColor.Set(pe.r/255.f, pe.g/255.f, pe.b/255.f, pe.a/255.f);
		mDesaturation = desat;
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		mColor.Set(pe.r/255.f, pe.g/255.f, pe.b/255.f, alpha);
		mDesaturation = desat;
	}

	void ResetColor()
	{
		mColor.Set(1,1,1,1);
		mDesaturation = 0;
	}

	void GetColorMask(bool& r, bool &g, bool& b, bool& a) const
	{
		r = mColorMask[0];
		g = mColorMask[1];
		b = mColorMask[2];
		a = mColorMask[3];
	}

	void SetColorMask(bool r, bool g, bool b, bool a)
	{
		mColorMask[0] = r;
		mColorMask[1] = g;
		mColorMask[2] = b;
		mColorMask[3] = a;
	}

	void ResetColorMask()
	{
		for (int i = 0; i < 4; ++i)
			mColorMask[i] = true;
	}

	void SetTextureMode(int mode)
	{
		mTextureMode = mode;
	}

	int GetTextureMode()
	{
		return mTextureMode;
	}

	void EnableTexture(bool on)
	{
		mTextureEnabled = on;
	}

	void EnableFog(bool on)
	{
		mFogEnabled = on;
	}

	void SetEffect(int eff)
	{
		mSpecialEffect = eff;
	}

	void EnableGlow(bool on)
	{
		mGlowEnabled = on;
	}

	void EnableSplit(bool on)
	{
		if (!(gl.flags & RFL_NO_CLIP_PLANES))
		{
			mSplitEnabled = on;
			if (on)
			{
				glEnable(GL_CLIP_DISTANCE3);
				glEnable(GL_CLIP_DISTANCE4);
			}
			else
			{
				glDisable(GL_CLIP_DISTANCE3);
				glDisable(GL_CLIP_DISTANCE4);
			}
		}
	}

	void SetClipLine(line_t *line)
	{
		mClipLine.Set(line->v1->fX(), line->v1->fY(), line->Delta().X, line->Delta().Y);
	}

	void EnableClipLine(bool on)
	{
		if (!(gl.flags & RFL_NO_CLIP_PLANES))
		{
			mClipLineEnabled = on;
			if (on)
			{
				glEnable(GL_CLIP_DISTANCE0);
			}
			else
			{
				glDisable(GL_CLIP_DISTANCE0);
			}
		}
	}

	void SetLightIndex(int n)
	{
		mLightIndex = n;
	}

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	void EnableModelMatrix(bool on)
	{
		mModelMatrixEnabled = on;
	}

	void EnableTextureMatrix(bool on)
	{
		mTextureMatrixEnabled = on;
	}

	void SetCameraPos(float x, float y, float z)
	{
		mCameraPos.Set(x, z, y, 0);
	}

	void SetGlowParams(float *t, float *b)
	{
		mGlowTop.Set(t[0], t[1], t[2], t[3]);
		mGlowBottom.Set(b[0], b[1], b[2], b[3]);
	}

	void SetSoftLightLevel(int level)
	{
		if (glset.lightmode == 8) mLightParms[3] = level / 255.f;
		else mLightParms[3] = -1.f;
	}

	void SetGlowPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		DVector3 tn = top.Normal();
		DVector3 bn = bottom.Normal();
		mGlowTopPlane.Set(tn.X, tn.Y, 1. / tn.Z, top.fD());
		mGlowBottomPlane.Set(bn.X, bn.Y, 1. / bn.Z, bottom.fD());
	}

	void SetSplitPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		DVector3 tn = top.Normal();
		DVector3 bn = bottom.Normal();
		mSplitTopPlane.Set(tn.X, tn.Y, 1. / tn.Z, top.fD());
		mSplitBottomPlane.Set(bn.X, bn.Y, 1. / bn.Z, bottom.fD());
	}

	void SetDynLight(float r, float g, float b)
	{
		mDynColor.Set(r, g, b, 0);
	}

	void SetObjectColor(PalEntry pe)
	{
		mObjectColor = pe;
	}

	void SetObjectColor2(PalEntry pe)
	{
		mObjectColor2 = pe;
	}

	void SetSpecular(float glossiness, float specularLevel)
	{
		mGlossiness = glossiness;
		mSpecularLevel = specularLevel;
	}

	void SetFog(PalEntry c, float d)
	{
		const float LOG2E = 1.442692f;	// = 1/log(2)
		mFogColor = c;
		if (d >= 0.0f) mLightParms[2] = d * (-LOG2E / 64000.f);
	}

	void SetLightParms(float f, float d)
	{
		mLightParms[1] = f;
		mLightParms[0] = d;
	}

	void SetFixedColormap(int cm)
	{
		mColormapState = cm;
	}

	int GetFixedColormap()
	{
		return mColormapState;
	}

	PalEntry GetFogColor() const
	{
		return mFogColor;
	}

	void SetClipSplit(float bottom, float top)
	{
		mClipSplit[0] = bottom;
		mClipSplit[1] = top;
	}

	void SetClipSplit(float *vals)
	{
		memcpy(mClipSplit, vals, 2 * sizeof(float));
	}

	void GetClipSplit(float *out)
	{
		memcpy(out, mClipSplit, 2 * sizeof(float));
	}

	void ClearClipSplit()
	{
		mClipSplit[0] = -1000000.f;
		mClipSplit[1] = 1000000.f;
	}

	void BlendFunc(int src, int dst)
	{
		if (!gl_direct_state_change)
		{
			mSrcBlend = src;
			mDstBlend = dst;
		}
		else
		{
			glBlendFunc(src, dst);
		}
	}

	void AlphaFunc(int func, float thresh)
	{
		if (func == GL_GREATER) mAlphaThreshold = thresh;
		else mAlphaThreshold = thresh - 0.001f;
	}

	void BlendEquation(int eq)
	{
		if (!gl_direct_state_change)
		{
			mBlendEquation = eq;
		}
		else
		{
			glBlendEquation(eq);
		}
	}

	// This wraps the depth clamp setting because we frequently need to read it which OpenGL is not particularly performant at...
	bool SetDepthClamp(bool on)
	{
		bool res = mLastDepthClamp;
		if (!on) glDisable(GL_DEPTH_CLAMP);
		else glEnable(GL_DEPTH_CLAMP);
		mLastDepthClamp = on;
		return res;
	}

	void SetInterpolationFactor(float fac)
	{
		mInterpolationFactor = fac;
	}

	float GetInterpolationFactor()
	{
		return mInterpolationFactor;
	}

	void SetPassType(EPassType passType)
	{
		mPassType = passType;
	}

	EPassType GetPassType()
	{
		return mPassType;
	}

	void EnableDrawBuffers(int count)
	{
		count = MIN(count, 3);
		if (mNumDrawBuffers != count)
		{
			static GLenum buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(count, buffers);
			mNumDrawBuffers = count;
		}
	}

	int GetPassDrawBufferCount()
	{
		return mPassType == GBUFFER_PASS ? 3 : 1;
	}

	// Backwards compatibility crap follows
	void ApplyFixedFunction();
	void DrawColormapOverlay();
};

extern FRenderState gl_RenderState;

#endif
