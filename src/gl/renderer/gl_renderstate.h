#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "c_cvars.h"
#include "m_fixed.h"
#include "r_defs.h"

EXTERN_CVAR(Bool, gl_direct_state_change)

enum
{
	CCTRL_ATTRIB = 0,
	CCTRL_ATTRIBALPHA = 1,
	CCTRL_BUFFER = 2
};

enum
{
	SPECMODE_DEFAULT,
	SPECMODE_INFRARED,
	SPECMODE_FOGLAYER
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
	TArray<unsigned int> mVAOStack;
	PalEntry mColor;
	PalEntry mLightAttr;
	int mColorControl;
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	float mSoftLightLevel;
	float mDynLight[3];
	int mDynTick;
	int mSrcBlend, mDstBlend;
	float mAlphaThreshold;
	bool mAlphaTest;
	int mBlendEquation;
	int mSpecialMode;
	unsigned int mVertexArray, mLastVertexArray;
	int mLightIndex;
	int mTexMatrixIndex;
	int mTexMatrixTick;

	float mGlowParms[16];
	PalEntry mFogColor, mObjectColor;
	float mFogDensity;

	int mEffectState;

	int glSrcBlend, glDstBlend;
	int gl_BlendEquation;

	bool ApplyShader();

public:
	FRenderState()
	{
		mDynTick = 0;
		Reset();
	}

	void Reset();

	int SetupShader(int &shaderindex);
	void Apply();
	void PushVertexArray()
	{
		mVAOStack.Push(mVertexArray);
	}

	void SetDynLightIndex(int li)
	{
		mLightIndex = li;
	}

	void SetColorControl(int ctrl)
	{
		mColorControl = ctrl;
	}

	void PopVertexArray()
	{
		if (mVAOStack.Size() > 0)
		{
			mVAOStack.Pop(mVertexArray);
		}
	}

	void SetVertexArray(unsigned int vao)
	{
		mVertexArray = vao;
	}

	void SetColor(float r, float g, float b, float a = 1.f, int desat = 0)
	{
		mColor = PalEntry(r*255.f, g*255.f, b*255.f, a*255.f);
		mLightAttr.a = desat;
	}

	void SetColor(PalEntry pe, int desat = 0)
	{
		mColor = pe;
		mLightAttr.a = desat;
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		mColor = pe;
		mColor.a = alpha*255.f;
		mLightAttr.a = desat;
	}

	void ResetColor()
	{
		mColor = 0xffffffff;
		mLightAttr.a = 0;
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

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	void SetGlowParams(float *t, float *b, const secplane_t &top, const secplane_t &bottom)
	{
		mGlowEnabled = true;
		memcpy(&mGlowParms[0], t, 4 * sizeof(float));
		memcpy(&mGlowParms[4], b, 4 * sizeof(float));
		mGlowParms[8] = FIXED2FLOAT(top.a);
		mGlowParms[9] = FIXED2FLOAT(top.b);
		mGlowParms[10] = FIXED2FLOAT(top.ic);
		mGlowParms[11] = FIXED2FLOAT(top.d);
		mGlowParms[12] = FIXED2FLOAT(bottom.a);
		mGlowParms[13] = FIXED2FLOAT(bottom.b);
		mGlowParms[14] = FIXED2FLOAT(bottom.ic);
		mGlowParms[15] = FIXED2FLOAT(bottom.d);
	}

	void SetSpecialMode(int mode)
	{
		mSpecialMode = mode;
	}

	void SetSoftLightLevel(int lightlev)
	{
		mLightAttr.b= lightlev;
	}

	void SetDynLight(float r,float g, float b)
	{
		mDynTick++;
		mDynLight[0] = r;
		mDynLight[1] = g;
		mDynLight[2] = b;
	}

	void SetFog(PalEntry c, float d)
	{
		mFogColor = c;
		if (d >= 0.0f) mFogDensity = d;
	}

	void SetLightParms(int f, int d)
	{
		mLightAttr.r = f;
		mLightAttr.g = d;
	}

	void SetObjectColor(PalEntry c)
	{
		mObjectColor = c;
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

	// note: func may only be GL_GREATER or GL_GEQUAL to keep this simple. The engine won't need any other settings for the alpha test.
	void AlphaFunc(int func, float thresh)
	{
		if (func == GL_GEQUAL) thresh -= 0.001f;	// reduce by 1/1000.
		mAlphaThreshold = thresh;
	}

	void EnableAlphaTest(bool on)
	{
		mAlphaTest = on;
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

};

extern FRenderState gl_RenderState;

#endif
