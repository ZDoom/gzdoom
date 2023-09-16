#pragma once

#include "vectors.h"
#include "matrix.h"
#include "hw_material.h"
#include "hw_levelmesh.h"
#include "texmanip.h"
#include "version.h"
#include "i_interface.h"
#include "hw_viewpointuniforms.h"
#include "hw_cvars.h"

#include <atomic>

struct FColormap;
class IBuffer;
struct HWViewpointUniforms;
struct FDynLightData;

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
	FMaterial *mMaterial = nullptr;
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

struct FFlatVertex;

enum EPassType
{
	NORMAL_PASS,
	GBUFFER_PASS,
	MAX_PASS_TYPES
};

struct FVector4PalEntry
{
	float r, g, b, a;

	bool operator==(const FVector4PalEntry &other) const
	{
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	bool operator!=(const FVector4PalEntry &other) const
	{
		return r != other.r || g != other.g || b != other.b || a != other.a;
	}

	FVector4PalEntry &operator=(PalEntry newvalue)
	{
		const float normScale = 1.0f / 255.0f;
		r = newvalue.r * normScale;
		g = newvalue.g * normScale;
		b = newvalue.b * normScale;
		a = newvalue.a * normScale;
		return *this;
	}

	FVector4PalEntry& SetIA(PalEntry newvalue)
	{
		const float normScale = 1.0f / 255.0f;
		r = newvalue.r * normScale;
		g = newvalue.g * normScale;
		b = newvalue.b * normScale;
		a = newvalue.a;
		return *this;
	}

	FVector4PalEntry& SetFlt(float v1, float v2, float v3, float v4)	
	{
		r = v1;
		g = v2;
		b = v3;
		a = v4;
		return *this;
	}

};

struct StreamData
{
	FVector4PalEntry uObjectColor;
	FVector4PalEntry uObjectColor2;
	FVector4 uDynLightColor;
	FVector4PalEntry uAddColor;
	FVector4PalEntry uTextureAddColor;
	FVector4PalEntry uTextureModulateColor;
	FVector4PalEntry uTextureBlendColor;
	FVector4PalEntry uFogColor;
	float uDesaturationFactor; // HWDrawInfo::SetColor
	float uInterpolationFactor;
	float timer;
	int useVertexData;
	FVector4 uVertexColor; // HWDrawInfo::SetColor
	FVector4 uVertexNormal;

	FVector4 uGlowTopPlane;
	FVector4 uGlowTopColor;
	FVector4 uGlowBottomPlane;
	FVector4 uGlowBottomColor;

	FVector4 uGradientTopPlane;
	FVector4 uGradientBottomPlane;

	FVector4 uSplitTopPlane;
	FVector4 uSplitBottomPlane;

	FVector4 uDetailParms;
	FVector4 uNpotEmulation;

	FVector2 uClipSplit;
	FVector2 uSpecularMaterial;

	float uLightLevel; // HWDrawInfo::SetColor
	float uFogDensity;
	float uLightFactor;
	float uLightDist;

	float uAlphaThreshold;
	float padding1;
	float padding2;
	float padding3;
};

class FRenderState
{
protected:
	uint8_t mFogEnabled;
	uint8_t mTextureEnabled:1;
	uint8_t mGlowEnabled : 1;
	uint8_t mGradientEnabled : 1;
	uint8_t mSplitEnabled : 1;
	uint8_t mBrightmapEnabled : 1;

	int mLightIndex;
	int mBoneIndexBase;
	int mSpecialEffect;
	int mTextureMode;
	int mTextureClamp;
	int mTextureModeFlags;
	int mSoftLight;
	int mLightMode = -1;

	int mColorMapSpecial;
	float mColorMapFlash;

	StreamData mStreamData = {};
	PalEntry mFogColor;

	FRenderStyle mRenderStyle;

	FMaterialState mMaterial;
	FDepthBiasState mBias;

	IBuffer* mVertexBuffer;
	int mVertexOffsets[2];	// one per binding point
	IBuffer* mIndexBuffer;

	EPassType mPassType = NORMAL_PASS;

public:

	uint64_t firstFrame = 0;

	void Reset()
	{
		mTextureEnabled = true;
		mBrightmapEnabled = mGradientEnabled = mFogEnabled = mGlowEnabled = false;
		mFogColor = 0xffffffff;
		mStreamData.uFogColor = mFogColor;
		mTextureMode = -1;
		mTextureClamp = 0;
		mTextureModeFlags = 0;
		mStreamData.uDesaturationFactor = 0.0f;
		mStreamData.uAlphaThreshold = 0.5f;
		mSplitEnabled = false;
		mStreamData.uAddColor = 0;
		mStreamData.uObjectColor = 0xffffffff;
		mStreamData.uObjectColor2 = 0;
		mStreamData.uTextureBlendColor = 0;
		mStreamData.uTextureAddColor = 0;
		mStreamData.uTextureModulateColor = 0;
		mSoftLight = 0;
		mStreamData.uLightDist = 0.0f;
		mStreamData.uLightFactor = 0.0f;
		mStreamData.uFogDensity = 0.0f;
		mStreamData.uLightLevel = -1.0f;
		mSpecialEffect = EFF_NONE;
		mLightIndex = -1;
		mBoneIndexBase = -1;
		mStreamData.uInterpolationFactor = 0;
		mRenderStyle = DefaultRenderStyle();
		mMaterial.Reset();
		mBias.Reset();
		mPassType = NORMAL_PASS;

		mColorMapSpecial = 0;
		mColorMapFlash = 1;

		mVertexBuffer = nullptr;
		mVertexOffsets[0] = mVertexOffsets[1] = 0;
		mIndexBuffer = nullptr;

		mStreamData.uVertexColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		mStreamData.uGlowTopColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uGlowBottomColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uGlowTopPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uGlowBottomPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uGradientTopPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uGradientBottomPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uSplitTopPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uSplitBottomPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		mStreamData.uDynLightColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		mStreamData.uDetailParms = { 0.0f, 0.0f, 0.0f, 0.0f };
#ifdef NPOT_EMULATION
		mStreamData.uNpotEmulation = { 0,0,0,0 };
#endif
		ClearClipSplit();
	}

	void SetNormal(FVector3 norm)
	{
		mStreamData.uVertexNormal = { norm.X, norm.Y, norm.Z, 0.f };
	}

	void SetNormal(float x, float y, float z)
	{
		mStreamData.uVertexNormal = { x, y, z, 0.f };
	}

	void SetColor(float r, float g, float b, float a = 1.f, int desat = 0)
	{
		mStreamData.uVertexColor = { r, g, b, a };
		mStreamData.uDesaturationFactor = desat * (1.0f / 255.0f);
	}

	void SetColor(PalEntry pe, int desat = 0)
	{
		const float scale = 1.0f / 255.0f;
		mStreamData.uVertexColor = { pe.r * scale, pe.g * scale, pe.b * scale, pe.a * scale };
		mStreamData.uDesaturationFactor = desat * (1.0f / 255.0f);
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		const float scale = 1.0f / 255.0f;
		mStreamData.uVertexColor = { pe.r * scale, pe.g * scale, pe.b * scale, alpha };
		mStreamData.uDesaturationFactor = desat * (1.0f / 255.0f);
	}

	void ResetColor()
	{
		mStreamData.uVertexColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		mStreamData.uDesaturationFactor = 0.0f;
	}

	void SetTextureClamp(bool on)
	{
		if (on) mTextureClamp = TM_CLAMPY;
		else mTextureClamp = 0;
	}

	void SetTextureMode(int mode)
	{
		mTextureMode = mode;
	}

	void SetTextureMode(FRenderStyle style)
	{
		if (style.Flags & STYLEF_RedIsAlpha)
		{
			SetTextureMode(TM_ALPHATEXTURE);
		}
		else if (style.Flags & STYLEF_ColorIsFixed)
		{
			SetTextureMode(TM_STENCIL);
		}
		else if (style.Flags & STYLEF_InvertSource)
		{
			SetTextureMode(TM_INVERSE);
		}
	}

	int GetTextureMode()
	{
		return mTextureMode;
	}

	int GetTextureModeAndFlags(int tempTM)
	{
		int f = mTextureModeFlags;
		if (!mBrightmapEnabled) f &= ~(TEXF_Brightmap | TEXF_Glowmap);
		if (mTextureClamp) f |= TEXF_ClampY;
		return (mTextureMode == TM_NORMAL && tempTM == TM_OPAQUE ? TM_OPAQUE : mTextureMode) | f;
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
		if (mGlowEnabled && !on)
		{
			mStreamData.uGlowTopColor = { 0.0f, 0.0f, 0.0f, 0.0f };
			mStreamData.uGlowBottomColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		}
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
		if (mSplitEnabled && !on)
		{
			mStreamData.uSplitTopPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
			mStreamData.uSplitBottomPlane = { 0.0f, 0.0f, 0.0f, 0.0f };
		}
		mSplitEnabled = on;
	}

	void SetGlowParams(float *t, float *b)
	{
		mStreamData.uGlowTopColor = { t[0], t[1], t[2], t[3] };
		mStreamData.uGlowBottomColor = { b[0], b[1], b[2], b[3] };
	}

	void SetSoftLightLevel(int llevel, int blendfactor = 0)
	{
		if (blendfactor == 0) mStreamData.uLightLevel = llevel / 255.f;
		else mStreamData.uLightLevel = -1.f;
	}

	void SetNoSoftLightLevel()
	{
		mStreamData.uLightLevel = -1.f;
	}

	void SetLightMode(int lightmode)
	{
		mLightMode = lightmode;
	}

	void SetGlowPlanes(const FVector4 &tp, const FVector4& bp)
	{
		mStreamData.uGlowTopPlane = tp;
		mStreamData.uGlowBottomPlane = bp;
	}

	void SetGradientPlanes(const FVector4& tp, const FVector4& bp)
	{
		mStreamData.uGradientTopPlane = tp;
		mStreamData.uGradientBottomPlane = bp;
	}

	void SetSplitPlanes(const FVector4& tp, const FVector4& bp)
	{
		mStreamData.uSplitTopPlane = tp;
		mStreamData.uSplitBottomPlane = bp;
	}

	void SetDetailParms(float xscale, float yscale, float bias)
	{
		mStreamData.uDetailParms = { xscale, yscale, bias, 0 };
	}

	void SetDynLight(float r, float g, float b)
	{
		mStreamData.uDynLightColor.X = r;
		mStreamData.uDynLightColor.Y = g;
		mStreamData.uDynLightColor.Z = b;
	}

	void SetScreenFade(float f)
	{
		// This component is otherwise unused.
		mStreamData.uDynLightColor.W = f;
	}

	void SetObjectColor(PalEntry pe)
	{
		mStreamData.uObjectColor = pe;
	}

	void SetObjectColor2(PalEntry pe)
	{
		mStreamData.uObjectColor2 = pe;
	}

	void SetAddColor(PalEntry pe)
	{
		mStreamData.uAddColor = pe;
	}

	void SetNpotEmulation(float factor, float offset)
	{
#ifdef NPOT_EMULATION
		mStreamData.uNpotEmulation = { offset, factor, 0, 0 };
#endif
	}

	void ApplyTextureManipulation(TextureManipulation* texfx)
	{
		if (!texfx || texfx->AddColor.a == 0)
		{
			mStreamData.uTextureAddColor.a = 0;	// we only need to set the flags to 0
		}
		else
		{
			// set up the whole thing
			mStreamData.uTextureAddColor.SetIA(texfx->AddColor);
			auto pe = texfx->ModulateColor;
			mStreamData.uTextureModulateColor.SetFlt(pe.r * pe.a / 255.f, pe.g * pe.a / 255.f, pe.b * pe.a / 255.f, texfx->DesaturationFactor);
			mStreamData.uTextureBlendColor = texfx->BlendColor;
		}
	}
	void SetTextureColors(float* modColor, float* addColor, float* blendColor)
	{
		mStreamData.uTextureAddColor.SetFlt(addColor[0], addColor[1], addColor[2], addColor[3]);
		mStreamData.uTextureModulateColor.SetFlt(modColor[0], modColor[1], modColor[2], modColor[3]);
		mStreamData.uTextureBlendColor.SetFlt(blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
	}

	void SetFog(PalEntry c, float d)
	{
		const float LOG2E = 1.442692f;	// = 1/log(2)
		mFogColor = c;
		mStreamData.uFogColor = mFogColor;
		if (d >= 0.0f) mStreamData.uFogDensity = d * (-LOG2E / 64000.f);
	}

	void SetLightParms(float f, float d)
	{
		mStreamData.uLightFactor = f;
		mStreamData.uLightDist = d;
	}

	PalEntry GetFogColor() const
	{
		return mFogColor;
	}

	void AlphaFunc(int func, float thresh)
	{
		if (func == Alpha_Greater) mStreamData.uAlphaThreshold = thresh;
		else mStreamData.uAlphaThreshold = thresh - 0.001f;
	}

	void SetLightIndex(int index)
	{
		mLightIndex = index;
	}

	void SetBoneIndexBase(int index)
	{
		mBoneIndexBase = index;
	}

	void SetRenderStyle(FRenderStyle rs)
	{
		mRenderStyle = rs;
	}

	void SetRenderStyle(ERenderStyle rs)
	{
		mRenderStyle = rs;
	}

	auto GetDepthBias()
	{
		return mBias;
	}

	void SetDepthBias(float a, float b)
	{
		mBias.mChanged |= mBias.mFactor != a || mBias.mUnits != b;
		mBias.mFactor = a;
		mBias.mUnits = b;
	}

	void SetDepthBias(FDepthBiasState& bias)
	{
		SetDepthBias(bias.mFactor, bias.mUnits);
	}

	void ClearDepthBias()
	{
		mBias.mChanged |= mBias.mFactor != 0 || mBias.mUnits != 0;
		mBias.mFactor = 0;
		mBias.mUnits = 0;
	}

private:
	void SetMaterial(FMaterial *mat, int clampmode, int translation, int overrideshader)
	{
		mMaterial.mMaterial = mat;
		mMaterial.mClampMode = clampmode;
		mMaterial.mTranslation = translation;
		mMaterial.mOverrideShader = overrideshader;
		mMaterial.mChanged = true;
		mTextureModeFlags = mat->GetLayerFlags();
		auto scale = mat->GetDetailScale();
		mStreamData.uDetailParms = { scale.X, scale.Y, 2, 0 };
	}

public:
	void SetMaterial(FGameTexture* tex, EUpscaleFlags upscalemask, int scaleflags, int clampmode, int translation, int overrideshader)
	{
		tex->setSeen();
		if (!sysCallbacks.PreBindTexture || !sysCallbacks.PreBindTexture(this, tex, upscalemask, scaleflags, clampmode, translation, overrideshader))
		{
			if (shouldUpscale(tex, upscalemask)) scaleflags |= CTF_Upscale;
		}
		auto mat = FMaterial::ValidateTexture(tex, scaleflags);
		assert(mat);
		SetMaterial(mat, clampmode, translation, overrideshader);
	}

	void SetClipSplit(float bottom, float top)
	{
		mStreamData.uClipSplit.X = bottom;
		mStreamData.uClipSplit.Y = top;
	}

	void SetClipSplit(float *vals)
	{
		mStreamData.uClipSplit.X = vals[0];
		mStreamData.uClipSplit.Y = vals[1];
	}

	void GetClipSplit(float *out)
	{
		out[0] = mStreamData.uClipSplit.X;
		out[1] = mStreamData.uClipSplit.Y;
	}

	void ClearClipSplit()
	{
		mStreamData.uClipSplit.X = -1000000.f;
		mStreamData.uClipSplit.Y = 1000000.f;
	}

	void SetVertexBuffer(IBuffer* vb, int offset0 = 0, int offset1 = 0)
	{
		mVertexBuffer = vb;
		mVertexOffsets[0] = offset0;
		mVertexOffsets[1] = offset1;
	}

	void SetIndexBuffer(IBuffer* ib)
	{
		mIndexBuffer = ib;
	}

	void SetFlatVertexBuffer()
	{
		SetVertexBuffer(nullptr);
		SetIndexBuffer(nullptr);
	}

	void SetInterpolationFactor(float fac)
	{
		mStreamData.uInterpolationFactor = fac;
	}

	float GetInterpolationFactor()
	{
		return mStreamData.uInterpolationFactor;
	}

	void EnableDrawBufferAttachments(bool on) // Used by fog boundary drawer
	{
		EnableDrawBuffers(on ? GetPassDrawBufferCount() : 1);
	}

	int GetPassDrawBufferCount()
	{
		return mPassType == GBUFFER_PASS ? 3 : 1;
	}

	void SetPassType(EPassType passType)
	{
		mPassType = passType;
	}

	EPassType GetPassType()
	{
		return mPassType;
	}

	void SetSpecialColormap(int cm, float flash)
	{
		mColorMapSpecial = cm;
		mColorMapFlash = flash;
	}

	int Set2DViewpoint(int width, int height, int palLightLevels = 0)
	{
		HWViewpointUniforms matrices;
		matrices.mViewMatrix.loadIdentity();
		matrices.mNormalViewMatrix.loadIdentity();
		matrices.mViewHeight = 0;
		matrices.mGlobVis = 1.f;
		matrices.mPalLightLevels = palLightLevels;
		matrices.mClipLine.X = -10000000.0f;
		matrices.mShadowmapFilter = gl_shadowmap_filter;
		matrices.mLightBlendMode = 0;
		matrices.mProjectionMatrix.ortho(0, (float)width, (float)height, 0, -1.0f, 1.0f);
		matrices.CalcDependencies();
		return SetViewpoint(matrices);
	}

	// API-dependent render interface

	// Worker threads
	virtual void FlushCommands() { }

	// Vertices
	virtual std::pair<FFlatVertex*, unsigned int> AllocVertices(unsigned int count) = 0;
	virtual void SetShadowData(const TArray<FFlatVertex>& vertices, const TArray<uint32_t>& indexes) = 0;
	virtual void UpdateShadowData(unsigned int index, const FFlatVertex* vertices, unsigned int count) = 0;
	virtual void ResetVertices() = 0;

	// Buffers
	virtual int SetViewpoint(const HWViewpointUniforms& vp) = 0;
	virtual void SetViewpoint(int index) = 0;
	virtual void SetModelMatrix(const VSMatrix& matrix, const VSMatrix& normalMatrix) = 0;
	virtual void SetTextureMatrix(const VSMatrix& matrix) = 0;
	virtual int UploadLights(const FDynLightData& lightdata) = 0;
	virtual int UploadBones(const TArray<VSMatrix>& bones) = 0;

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
	virtual void SetStencil(int offs, int op, int flags=-1) = 0;	// Used by portal setup and render hacks.
	virtual void SetCulling(int mode) = 0;						// Used by model drawer only.
	virtual void Clear(int targets) = 0;						// not used during normal rendering
	virtual void EnableStencil(bool on) = 0;					// always on for 3D, always off for 2D
	virtual void SetScissor(int x, int y, int w, int h) = 0;	// constant for 3D, changes for 2D
	virtual void SetViewport(int x, int y, int w, int h) = 0;	// constant for all 3D and all 2D
	virtual void EnableDepthTest(bool on) = 0;					// used by 2D, portals and render hacks.
	virtual void EnableLineSmooth(bool on) = 0;					// constant setting for each 2D drawer operation
	virtual void EnableDrawBuffers(int count, bool apply = false) = 0;	// Used by SSAO and EnableDrawBufferAttachments

	void SetColorMask(bool on)
	{
		SetColorMask(on, on, on, on);
	}

	friend class Mesh;
};

