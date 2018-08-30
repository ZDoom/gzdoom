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
#include "gl_load/gl_interface.h"
#include "r_data/matrix.h"
#include "hwrenderer/scene//hw_drawstructs.h"
#include "hwrenderer/textures/hw_material.h"
#include "c_cvars.h"
#include "r_defs.h"
#include "r_data/r_translate.h"
#include "g_levellocals.h"
#include "gl/data/gl_attributebuffer.h"

class FVertexBuffer;
class FShader;
struct GLSectorPlane;

EXTERN_CVAR(Bool, gl_direct_state_change)

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
	uint8_t mFogEnabled;
	bool mGlowEnabled;
	bool mSplitEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mSrcBlend, mDstBlend;
	int mBlendEquation;
	bool mLastDepthClamp;
	FVector2 uClipSplit;
    float uTimer;

	FVertexBuffer *mVertexBuffer, *mCurrentVertexBuffer;
	FVector4 mNormal;

	int mEffectState;
	int mTempTM = TM_MODULATE;

	FShader *activeShader;

	EPassType mPassType = NORMAL_PASS;
	int mNumDrawBuffers = 1;

	bool ApplyShader(int attrindex, bool alphateston);

	// Texture binding state
	FMaterial *lastMaterial = nullptr;
	int lastClamp = 0;
	int lastTranslation = 0;
	int maxBoundMaterial = -1;


public:

	FRenderState()
	{
		Reset();
	}

	void Reset();

	void ClearLastMaterial()
	{
		lastMaterial = nullptr;
	}

	void SetMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader);

	void Apply(int attrindex = INT_MAX, bool alphateston = true);

	void SetVertexBuffer(FVertexBuffer *vb)
	{
		mVertexBuffer = vb;
	}

	void ResetVertexBuffer()
	{
		// forces rebinding with the next 'apply' call.
		mCurrentVertexBuffer = NULL;
	}

	void SetNormal(FVector3 norm)
	{
		mNormal = { norm.X, norm.Y, norm.Z, 0 };
	}

	void SetNormal(float x, float y, float z)
	{
		mNormal = { x, y, z, 0 };
	}

	/*
	void SetColor(float r, float g, float b, float a = 1.f, int desat = 0)
	{
		mAttributes.uLightColor = { r, g, b, a };
		mAttributes.uDesaturationFactor = desat / 255.f;
	}

	void SetColor(PalEntry pe, int desat = 0)
	{
		mAttributes.uLightColor = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, pe.a / 255.f };
		mAttributes.uDesaturationFactor = desat / 255.f;
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		mAttributes.uLightColor = { pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, alpha };
		mAttributes.uDesaturationFactor = desat / 255.f;
	}

	void ResetColor()
	{
		mAttributes.uLightColor = { 1.f, 1.f, 1.f, 1.f };
		mAttributes.uDesaturationFactor = 0.f;
	}

	void SetTextureMode(int mode)
	{
		mAttributes.uTextureMode = mode;
	}

	int GetTextureMode()
	{
		return mAttributes.uTextureMode;
	}
	*/

	void EnableTexture(bool on)
	{
		mTextureEnabled = on;
	}

	void EnableFog(uint8_t on)
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

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	void SetClipSplit(float bottom, float top)
	{
		uClipSplit[0] = bottom;
		uClipSplit[1] = top;
	}

	void SetClipSplit(float *vals)
	{
		uClipSplit = { vals[0], vals[1] };
	}

	void GetClipSplit(float *out)
	{
		out[0] = uClipSplit.X;
		out[1] = uClipSplit.Y;
	}

	void ClearClipSplit()
	{
		uClipSplit[0] = -1000000.f;
		uClipSplit[1] = 1000000.f;
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

};

extern FRenderState gl_RenderState;

#endif
