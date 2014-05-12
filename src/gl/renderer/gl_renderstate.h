#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "gl/system/gl_interface.h"
#include "gl/data/gl_data.h"
#include "c_cvars.h"
#include "r_defs.h"

class FVertexBuffer;

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

class FRenderState
{
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mLightEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	int mDesaturation;
	int mSoftLight;
	float mLightParms[4];
	int mNumLights[4];
	float *mLightData;
	int mSrcBlend, mDstBlend;
	int mAlphaFunc;
	float mAlphaThreshold;
	bool mAlphaTest;
	int mBlendEquation;
	bool m2D;

	FVertexBuffer *mVertexBuffer, *mCurrentVertexBuffer;
	FStateVec4 mColor;
	FStateVec4 mCameraPos;
	FStateVec4 mGlowTop, mGlowBottom;
	FStateVec4 mGlowTopPlane, mGlowBottomPlane;
	PalEntry mFogColor;
	PalEntry mObjectColor;
	PalEntry mDynColor;
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

	int SetupShader(bool cameratexture, int &shaderindex, float warptime);
	void Apply(bool forcenoshader = false);

	void SetVertexBuffer(FVertexBuffer *vb)
	{
		mVertexBuffer = vb;
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
		mGlowTopPlane.Set(FIXED2FLOAT(top.a), FIXED2FLOAT(top.b), FIXED2FLOAT(top.ic), FIXED2FLOAT(top.d));
		mGlowBottomPlane.Set(FIXED2FLOAT(bottom.a), FIXED2FLOAT(bottom.b), FIXED2FLOAT(bottom.ic), FIXED2FLOAT(bottom.d));
	}

	void SetDynLight(float r, float g, float b)
	{
		mDynColor = PalEntry(255, xs_CRoundToInt(r*255), xs_CRoundToInt(g*255), xs_CRoundToInt(b*255));
	}

	void SetDynLight(PalEntry pe)
	{
		mDynColor = pe;
	}

	void SetObjectColor(PalEntry pe)
	{
		mObjectColor = pe;
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

	void SetLights(int *numlights, float *lightdata)
	{
		mNumLights[0] = 0;
		mNumLights[1] = numlights[0];
		mNumLights[2] = numlights[1];
		mNumLights[3] = numlights[2];
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
