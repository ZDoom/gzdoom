#ifndef __GL_RENDERSTATE_H
#define __GL_RENDERSTATE_H

#include <string.h>
#include "c_cvars.h"
#include "m_fixed.h"
#include "r_defs.h"

EXTERN_CVAR(Bool, gl_direct_state_change)

enum
{
	SHD_DEFAULT = 0,
	SHD_COLORMAP = 1,
	SHD_FOGLAYER = 2,
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

	MAX_EFFECTS
};

class FRenderState
{
	bool mTextureEnabled;
	bool mFogEnabled;
	bool mGlowEnabled;
	bool mBrightmapEnabled;
	int mSpecialEffect;
	int mTextureMode;
	float mSoftLightLevel;
	float mDynLight[3];
	float mLightParms[2];
	int mNumLights[3];
	float *mLightData;
	int mSrcBlend, mDstBlend;
	float mAlphaThreshold;
	float mClipPlane;
	bool mAlphaTest;
	int mBlendEquation;
	bool m2D;

	FStateVec3 mCameraPos;
	FStateVec4 mGlowTop, mGlowBottom;
	FStateVec4 mGlowTopPlane, mGlowBottomPlane;
	PalEntry mFogColor, mObjectColor;
	float mFogDensity;

	int mEffectState;
	int mShaderSelect;

	int glSrcBlend, glDstBlend;
	int gl_BlendEquation;

	bool ApplyShader();

public:
	FRenderState()
	{
		Reset();
	}

	void Reset();

	int SetupShader(int &shaderindex, int &cm);
	void Apply();

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

	void SelectShader(int on)
	{
		mShaderSelect = on;
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

	void SetGlowPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		mGlowTopPlane.Set(FIXED2FLOAT(top.a), FIXED2FLOAT(top.b), FIXED2FLOAT(top.ic), FIXED2FLOAT(top.d));
		mGlowBottomPlane.Set(FIXED2FLOAT(bottom.a), FIXED2FLOAT(bottom.b), FIXED2FLOAT(bottom.ic), FIXED2FLOAT(bottom.d));
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

	void SetLights(int *numlights, float *lightdata)
	{
		if (numlights != NULL)
		{
			mNumLights[0] = numlights[0];
			mNumLights[1] = numlights[1];
			mNumLights[2] = numlights[2];
			mLightData = lightdata;	// caution: the data must be preserved by the caller until the 'apply' call!
		}
		else
		{
			mNumLights[0] = mNumLights[1] = mNumLights[2] = 0;
			mLightData = NULL;
		}
	
	}

	void SetFixedColormap(bool cm)
	{
		mShaderSelect = cm;
	}

	void SetClipPlane(float y)
	{
		mClipPlane = y;
	}

	float GetClipPlane()
	{
		return mClipPlane;
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

	void Set2DMode(bool on)
	{
		m2D = on;
	}
};

extern FRenderState gl_RenderState;

#endif
