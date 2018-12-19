#pragma once

#include "v_palette.h"
#include "vectors.h"
#include "g_levellocals.h"
#include "hw_drawstructs.h"
#include "hw_drawlist.h"
#include "r_data/matrix.h"
#include "hwrenderer/textures/hw_material.h"

struct FColormap;
class IVertexBuffer;
class IIndexBuffer;

enum EClearTarget
{
	CT_Depth = 1,
	CT_Stencil = 2,
	CT_Color = 4
};

enum ERenderEffect
{
	EFF_NONE = -1,
	EFF_FOGBOUNDARY,
	EFF_SPHEREMAP,
	EFF_BURN,
	EFF_STENCIL,

	MAX_EFFECTS
};

enum EAlphaFunc
{
	Alpha_GEqual = 0,
	Alpha_Greater = 1
};

enum EDrawType
{
	DT_Points = 0,
	DT_Lines = 1,
	DT_Triangles = 2,
	DT_TriangleFan = 3,
	DT_TriangleStrip = 4
};

enum EDepthFunc
{
	DF_Less,
	DF_LEqual,
	DF_Always
};

enum EStencilFlags
{
	SF_AllOn = 0,
	SF_ColorMaskOff = 1,
	SF_DepthMaskOff = 2,
};

enum EStencilOp
{
	SOP_Keep = 0,
	SOP_Increment = 1,
	SOP_Decrement = 2
};

enum ECull
{
	Cull_None,
	Cull_CCW,
	Cull_CW
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

struct FMaterialState
{
	FMaterial *mMaterial;
	int mClampMode;
	int mTranslation;
	int mOverrideShader;
	bool mChanged;

	void Reset()
	{
		mMaterial = nullptr;
		mTranslation = 0;
		mClampMode = CLAMP_NONE;
		mOverrideShader = -1;
		mChanged = false;
	}
};

struct FDepthBiasState
{
	float mFactor;
	float mUnits;
	bool mChanged;

	void Reset()
	{
		mFactor = 0;
		mUnits = 0;
		mChanged = false;
	}
};

class FRenderState
{
protected:
	uint8_t mFogEnabled;
	uint8_t mTextureEnabled:1;
	uint8_t mGlowEnabled : 1;
	uint8_t mGradientEnabled : 1;
	uint8_t mBrightmapEnabled : 1;
	uint8_t mModelMatrixEnabled : 1;
	uint8_t mTextureMatrixEnabled : 1;
	uint8_t mSplitEnabled : 1;

	int mLightIndex;
	int mSpecialEffect;
	int mTextureMode;
	int mDesaturation;
	int mSoftLight;
	float mLightParms[4];

	float mAlphaThreshold;
	float mClipSplit[2];
	float mInterpolationFactor;

	FStateVec4 mNormal;
	FStateVec4 mColor;
	FStateVec4 mGlowTop, mGlowBottom;
	FStateVec4 mGlowTopPlane, mGlowBottomPlane;
	FStateVec4 mGradientTopPlane, mGradientBottomPlane;
	FStateVec4 mSplitTopPlane, mSplitBottomPlane;
	PalEntry mAddColor;
	PalEntry mFogColor;
	PalEntry mObjectColor;
	PalEntry mObjectColor2;
	FStateVec4 mDynColor;
	FRenderStyle mRenderStyle;

	FMaterialState mMaterial;
	FDepthBiasState mBias;

	IVertexBuffer *mVertexBuffer;
	int mVertexOffsets[2];	// one per binding point
	IIndexBuffer *mIndexBuffer;

	void SetShaderLight(float level, float olight);

public:
	VSMatrix mModelMatrix;
	VSMatrix mTextureMatrix;

public:

	void Reset()
	{
		mTextureEnabled = true;
		mGradientEnabled = mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
		mFogColor.d = -1;
		mTextureMode = -1;
		mDesaturation = 0;
		mAlphaThreshold = 0.5f;
		mModelMatrixEnabled = false;
		mTextureMatrixEnabled = false;
		mSplitEnabled = false;
		mAddColor = 0;
		mObjectColor = 0xffffffff;
		mObjectColor2 = 0;
		mSoftLight = 0;
		mLightParms[0] = mLightParms[1] = mLightParms[2] = 0.0f;
		mLightParms[3] = -1.f;
		mSpecialEffect = EFF_NONE;
		mLightIndex = -1;
		mInterpolationFactor = 0;
		mRenderStyle = DefaultRenderStyle();
		mMaterial.Reset();
		mBias.Reset();

		mVertexBuffer = nullptr;
		mVertexOffsets[0] = mVertexOffsets[1] = 0;
		mIndexBuffer = nullptr;

		mColor.Set(1.0f, 1.0f, 1.0f, 1.0f);
		mGlowTop.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowBottom.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGlowBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGradientTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mGradientBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mSplitTopPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mSplitBottomPlane.Set(0.0f, 0.0f, 0.0f, 0.0f);
		mDynColor.Set(0.0f, 0.0f, 0.0f, 0.0f);

		mModelMatrix.loadIdentity();
		mTextureMatrix.loadIdentity();
		ClearClipSplit();
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

	void SetTextureMode(FRenderStyle style)
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			mTextureMode = TM_ALPHATEXTURE;
		}
		else if (style.Flags & STYLEF_ColorIsFixed)
		{
			mTextureMode = TM_STENCIL;
		}
		else if (style.Flags & STYLEF_InvertSource)
		{
			mTextureMode = TM_INVERSE;
		}
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

	void EnableGradient(bool on)
	{
		mGradientEnabled = on;
	}

	void EnableBrightmap(bool on)
	{
		mBrightmapEnabled = on;
	}

	void EnableSplit(bool on)
	{
		mSplitEnabled = on;
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
		if (level.isSoftwareLighting() && blendfactor == 0) mLightParms[3] = llevel / 255.f;
		else mLightParms[3] = -1.f;
	}

	void SetGlowPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		mGlowTopPlane.Set((float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD());
		mGlowBottomPlane.Set((float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD());
	}

	void SetGradientPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		mGradientTopPlane.Set((float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD());
		mGradientBottomPlane.Set((float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD());
	}

	void SetSplitPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		mSplitTopPlane.Set((float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD());
		mSplitBottomPlane.Set((float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD());
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

	void SetAddColor(PalEntry pe)
	{
		mAddColor = pe;
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

	void SetRenderStyle(FRenderStyle rs)
	{
		mRenderStyle = rs;
	}

	void SetRenderStyle(ERenderStyle rs)
	{
		mRenderStyle = rs;
	}

	void SetDepthBias(float a, float b)
	{
		mBias.mFactor = a;
		mBias.mUnits = b;
		mBias.mChanged = true;
	}

	void ClearDepthBias()
	{
		mBias.mFactor = 0;
		mBias.mUnits = 0;
		mBias.mChanged = true;
	}

	void SetMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader)
	{
		mMaterial.mMaterial = mat;
		mMaterial.mClampMode = clampmode;
		mMaterial.mTranslation = translation;
		mMaterial.mOverrideShader = overrideshader;
		mMaterial.mChanged = true;
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

	void SetVertexBuffer(IVertexBuffer *vb, int offset0, int offset1)
	{
		assert(vb);
		mVertexBuffer = vb;
		mVertexOffsets[0] = offset0;
		mVertexOffsets[1] = offset1;
	}

	void SetIndexBuffer(IIndexBuffer *ib)
	{
		mIndexBuffer = ib;
	}

	template <class T> void SetVertexBuffer(T *buffer)
	{
		auto ptrs = buffer->GetBufferObjects(); 
		SetVertexBuffer(ptrs.first, 0, 0);
		SetIndexBuffer(ptrs.second);
	}

	void SetInterpolationFactor(float fac)
	{
		mInterpolationFactor = fac;
	}

	float GetInterpolationFactor()
	{
		return mInterpolationFactor;
	}

	void SetColor(int sectorlightlevel, int rellight, bool fullbright, const FColormap &cm, float alpha, bool weapon = false);
	void SetFog(int lightlevel, int rellight, bool fullbright, const FColormap *cmap, bool isadditive);

	// API-dependent render interface

	// Draw commands
	virtual void ClearScreen() = 0;
	virtual void Draw(int dt, int index, int count, bool apply = true) = 0;
	virtual void DrawIndexed(int dt, int index, int count, bool apply = true) = 0;

	// Immediate render state change commands. These only change infrequently and should not clutter the render state.
	virtual bool SetDepthClamp(bool on) = 0;					// Deactivated only by skyboxes.
	virtual void SetDepthMask(bool on) = 0;						// Used by decals and indirectly by portal setup.
	virtual void SetDepthFunc(int func) = 0;					// Used by models, portals and mirror surfaces.
	virtual void SetDepthRange(float min, float max) = 0;		// Used by portal setup.
	virtual void SetColorMask(bool r, bool g, bool b, bool a) = 0;	// Used by portals.
	virtual void EnableDrawBufferAttachments(bool on) = 0;		// Used by fog boundary drawer.
	virtual void SetStencil(int offs, int op, int flags=-1) = 0;	// Used by portal setup and render hacks.
	virtual void SetCulling(int mode) = 0;						// Used by model drawer only.
	virtual void EnableClipDistance(int num, bool state) = 0;	// Use by sprite sorter for vertical splits.
	virtual void Clear(int targets) = 0;						// not used during normal rendering
	virtual void EnableStencil(bool on) = 0;					// always on for 3D, always off for 2D
	virtual void SetScissor(int x, int y, int w, int h) = 0;	// constant for 3D, changes for 2D
	virtual void SetViewport(int x, int y, int w, int h) = 0;	// constant for all 3D and all 2D
	virtual void EnableDepthTest(bool on) = 0;					// used by 2D, portals and render hacks.
	virtual void EnableMultisampling(bool on) = 0;				// only active for 2D
	virtual void EnableLineSmooth(bool on) = 0;					// constant setting for each 2D drawer operation

	void SetColorMask(bool on)
	{
		SetColorMask(on, on, on, on);
	}

};

