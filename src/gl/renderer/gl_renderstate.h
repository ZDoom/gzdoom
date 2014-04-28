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


struct PrimAttr
{
	float mFogDensity;
	float mLightFactor;
	float mLightDist;

	// indices into float parameter buffer
	// colors are stored in the parameter buffer because the most frequent case only uses default values so this keeps data amount lower.
	int mColorIndex;
	int mFogColorIndex;
	int mObjectColorIndex;
	int mAddLightIndex;

	int mDynLightIndex;	// index of first dynamic light (each light has 2 vec4's, first light's lightcolor.a contains number of lights in the list, each primitive has 3 lists: modulated, subtractive and additive lights)
	int mGlowIndex;		// each primitive has 4 vec4's: top and bottom color, top and bottom plane equations.

	// index into the matrix buffer
	int mTexMatrixIndex;	// one matrix if value != -1.
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
	float mColor[5];
	int mColorControl;
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	float mSoftLightLevel;
	float mDynLight[3];
	float mLightParms[2];
	int mSrcBlend, mDstBlend;
	float mAlphaThreshold;
	bool mAlphaTest;
	int mBlendEquation;
	unsigned int mVertexArray, mLastVertexArray;

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
		Reset();
	}

	void Reset();

	int SetupShader(int &shaderindex);
	void Apply();
	void PushVertexArray()
	{
		mVAOStack.Push(mVertexArray);
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

	void SetColor(float r, float g, float b, float a = 1.f, float desat = 0.f)
	{
		mColor[0] = r;
		mColor[1] = g;
		mColor[2] = b;
		mColor[3] = a;
		mColor[4] = desat;
	}

	void SetColor(PalEntry pe, float desat = 0.f)
	{
		mColor[0] = pe.r/255.f;
		mColor[1] = pe.g/255.f;
		mColor[2] = pe.b/255.f;
		mColor[3] = pe.a/255.f;
		mColor[4] = desat;
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, float desat = 0.f)
	{
		mColor[0] = pe.r / 255.f;
		mColor[1] = pe.g / 255.f;
		mColor[2] = pe.b / 255.f;
		mColor[3] = alpha;
		mColor[4] = desat;
	}

	void ResetColor()
	{
		mColor[0] = mColor[1] = mColor[2] = mColor[3] = 1.f;
		mColor[4] = 0.f;
	}

	const float *GetColor() const
	{
		return mColor;
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

	void SetSoftLightLevel(float lightlev)
	{
		mSoftLightLevel = lightlev;
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
