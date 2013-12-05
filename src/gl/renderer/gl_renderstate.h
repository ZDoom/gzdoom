#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "c_cvars.h"

EXTERN_CVAR(Bool, gl_direct_state_change)

struct FStateAttr
{
	static int ChangeCounter;
	int mLastChange;

	FStateAttr()
	{
		mLastChange = -1;
	}

	bool operator == (const FStateAttr &other)
	{
		return mLastChange == other.mLastChange;
	}

	bool operator != (const FStateAttr &other)
	{
		return mLastChange != other.mLastChange;
	}

};

struct FStateVec3 : public FStateAttr
{
	float vec[3];

	bool Update(FStateVec3 *other)
	{
		if (mLastChange != other->mLastChange)
		{
			*this = *other;
			return true;
		}
		return false;
	}

	void Set(float x, float y, float z)
	{
		vec[0] = x;
		vec[1] = z;
		vec[2] = y;
		mLastChange = ++ChangeCounter;
	}
};

struct FStateVec4 : public FStateAttr
{
	float vec[4];

	bool Update(FStateVec4 *other)
	{
		if (mLastChange != other->mLastChange)
		{
			*this = *other;
			return true;
		}
		return false;
	}

	void Set(float r, float g, float b, float a)
	{
		vec[0] = r;
		vec[1] = g;
		vec[2] = b;
		vec[3] = a;
		mLastChange = ++ChangeCounter;
	}
};


enum EEffect
{
	EFF_NONE,
	EFF_FOGBOUNDARY,
	EFF_SPHEREMAP,
};

class FRenderState
{
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mLightEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	float mDynLight[3];
	float mLightParms[2];
	int mNumLights[3];
	float *mLightData;
	int mSrcBlend, mDstBlend;
	int mAlphaFunc;
	float mAlphaThreshold;
	bool mAlphaTest;
	int mBlendEquation;
	bool m2D;

	FStateVec3 mCameraPos;
	FStateVec4 mGlowTop, mGlowBottom;
	PalEntry mFogColor;
	float mFogDensity;

	int mEffectState;
	int mColormapState;
	float mWarpTime;

	int glSrcBlend, glDstBlend;
	int glAlphaFunc;
	float glAlphaThreshold;
	bool glAlphaTest;
	int glBlendEquation;

	bool ffTextureEnabled;
	bool ffFogEnabled;
	int ffTextureMode;
	int ffSpecialEffect;
	PalEntry ffFogColor;
	float ffFogDensity;

	bool ApplyShader();

public:
	FRenderState()
	{
		Reset();
	}

	void Reset();

	int SetupShader(bool cameratexture, int &shaderindex, int &cm, float warptime);
	void Apply(bool forcenoshader = false);

	void SetTextureMode(int mode)
	{
		mTextureMode = mode;
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

	void EnableLight(bool on)
	{
		mLightEnabled = on;
	}

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	void SetCameraPos(float x, float y, float z)
	{
		mCameraPos.Set(x,y,z);
	}

	void SetGlowParams(float *t, float *b)
	{
		mGlowTop.Set(t[0], t[1], t[2], t[3]);
		mGlowBottom.Set(b[0], b[1], b[2], b[3]);
	}

	void SetDynLight(float r,float g, float b)
	{
		mDynLight[0] = r;
		mDynLight[1] = g;
		mDynLight[2] = b;
	}

	void SetFog(PalEntry c, float d)
	{
		mFogColor = c;
		if (d >= 0.0f) mFogDensity = d;
	}

	void SetLightParms(float f, float d)
	{
		mLightParms[0] = f;
		mLightParms[1] = d;
	}

	void SetLights(int *numlights, float *lightdata)
	{
		mNumLights[0] = numlights[0];
		mNumLights[1] = numlights[1];
		mNumLights[2] = numlights[2];
		mLightData = lightdata;	// caution: the data must be preserved by the caller until the 'apply' call!
	}

	void SetFixedColormap(int cm)
	{
		mColormapState = cm;
	}

	PalEntry GetFogColor() const
	{
		return mFogColor;
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
		if (!gl_direct_state_change)
		{
			mAlphaFunc = func;
			mAlphaThreshold = thresh;
		}
		else
		{
			::glAlphaFunc(func, thresh);
		}
	}

	void EnableAlphaTest(bool on)
	{
		if (!gl_direct_state_change)
		{
			mAlphaTest = on;
		}
		else
		{
			if (on) glEnable(GL_ALPHA_TEST);
			else glDisable(GL_ALPHA_TEST);
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
			::glBlendEquation(eq);
		}
	}

	void Set2DMode(bool on)
	{
		m2D = on;
	}
};

extern FRenderState gl_RenderState;

#endif
