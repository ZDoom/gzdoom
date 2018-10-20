#pragma once

#include "v_palette.h"
#include "vectors.h"
#include "g_levellocals.h"
#include "r_data/matrix.h"

struct FColormap;

enum EEffect
{
	EFF_NONE = -1,
	EFF_FOGBOUNDARY,
	EFF_SPHEREMAP,
	EFF_BURN,
	EFF_STENCIL,

	MAX_EFFECTS
};

enum
{
	Alpha_GEqual = 0,
	Alpha_Greater = 1
};

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

class FRenderState
{
protected:
	uint8_t mFogEnabled;
	uint8_t mTextureEnabled:1;
	uint8_t mGlowEnabled : 1;
	uint8_t mBrightmapEnabled : 1;
	uint8_t mModelMatrixEnabled : 1;
	uint8_t mTextureMatrixEnabled : 1;

	int mLightIndex;
	int mSpecialEffect;
	int mTextureMode;
	int mDesaturation;
	int mSoftLight;
	float mLightParms[4];

	float mAlphaThreshold;

	FStateVec4 mNormal;
	FStateVec4 mColor;
	FStateVec4 mGlowTop, mGlowBottom;
	FStateVec4 mGlowTopPlane, mGlowBottomPlane;
	FStateVec4 mSplitTopPlane, mSplitBottomPlane;
	PalEntry mFogColor;
	PalEntry mObjectColor;
	PalEntry mObjectColor2;
	FStateVec4 mDynColor;


	// fixed function state
	float mBias[2];
	bool mBiasOn;

public:
	VSMatrix mModelMatrix;
	VSMatrix mTextureMatrix;

public:

	void Reset()
	{
		mTextureEnabled = true;
		mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
		mFogColor.d = -1;
		mTextureMode = -1;
		mDesaturation = 0;
		mAlphaThreshold = 0.5f;
		mModelMatrixEnabled = false;
		mTextureMatrixEnabled = false;
		mObjectColor = 0xffffffff;
		mObjectColor2 = 0;
		mSoftLight = 0;
		mLightParms[0] = mLightParms[1] = mLightParms[2] = 0.0f;
		mLightParms[3] = -1.f;
		mSpecialEffect = EFF_NONE;
		mLightIndex = -1;
		mBiasOn = false;

		mColor.Set(1.0f, 1.0f, 1.0f, 1.0f);
		mGlowTop.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowBottom.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mSplitTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mSplitBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mDynColor.Set(0.0f, 0.0f, 0.0f, 0.0f);

		mModelMatrix.loadIdentity();
		mTextureMatrix.loadIdentity();
	}

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
		mColor.Set(pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, pe.a / 255.f);
		mDesaturation = desat;
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		mColor.Set(pe.r / 255.f, pe.g / 255.f, pe.b / 255.f, alpha);
		mDesaturation = desat;
	}

	void ResetColor()
	{
		mColor.Set(1, 1, 1, 1);
		mDesaturation = 0;
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

	void SetGlowParams(float *t, float *b)
	{
		mGlowTop.Set(t[0], t[1], t[2], t[3]);
		mGlowBottom.Set(b[0], b[1], b[2], b[3]);
	}

	void SetSoftLightLevel(int llevel, int blendfactor = 0)
	{
		if (level.lightmode == 8 && blendfactor == 0) mLightParms[3] = llevel / 255.f;
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

	PalEntry GetFogColor() const
	{
		return mFogColor;
	}

	void AlphaFunc(int func, float thresh)
	{
		if (func == Alpha_Greater) mAlphaThreshold = thresh;
		else mAlphaThreshold = thresh - 0.001f;
	}

	void SetPlaneTextureRotation(GLSectorPlane *plane, FMaterial *texture)
	{
		if (hw_SetPlaneTextureRotation(plane, texture, mTextureMatrix))
		{
			EnableTextureMatrix(true);
		}
	}

	void SetLightIndex(int index)
	{
		mLightIndex = index;
	}


	void SetDepthBias(float a, float b)
	{
		mBias[0] = a;
		mBias[1] = b;
		mBiasOn = true;
	}

	void ClearDepthBias()
	{
		mBias[0] = 0;
		mBias[1] = 0;
		mBiasOn = false;
	}

	void SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon = false);
	void SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive);

};

