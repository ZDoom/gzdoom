#pragma once

#include "v_palette.h"
#include "vectors.h"
#include "g_levellocals.h"
#include "hw_drawstructs.h"
#include "hw_drawlist.h"
#include "matrix.h"
#include "hwrenderer/textures/hw_material.h"
#include "hwrenderer/data/shaderuniforms.h"

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

enum class UniformName
{
	uObjectColor,
	uObjectColor2,
	uDynLightColor,
	uAddColor,
	uTextureAddColor,
	uTextureModulateColor,
	uTextureBlendColor,
	uFogColor,
	uDesaturationFactor,
	uInterpolationFactor,
	timer,
	useVertexData,
	uVertexColor,
	uVertexNormal,

	uGlowTopPlane,
	uGlowTopColor,
	uGlowBottomPlane,
	uGlowBottomColor,

	uGradientTopPlane,
	uGradientBottomPlane,

	uSplitTopPlane,
	uSplitBottomPlane,

	uTextureMode,
	uAlphaThreshold,
	uClipSplit,
	uLightLevel,
	uFogDensity,
	uLightFactor,
	uLightDist,
	uFogEnabled,
	uLightIndex,
	uSpecularMaterial,
	uDataIndex,
	uPadding1,
	uPadding2,
	uPadding3
};

enum class UniformFamily
{
	Normal,
	//Matrix,
	PushConstant,
	NumFamilies
};

class FRenderState
{
protected:
	FRenderState()
	{
		// Important: the following declarations must be compatible with the GLSL std140 layout rules.
		// DeclareUniform does not automatically insert padding to enforce this.

		DeclareUniform(UniformName::uObjectColor, "uObjectColor", UniformType::Vec4);
		DeclareUniform(UniformName::uObjectColor2, "uObjectColor2", UniformType::Vec4);
		DeclareUniform(UniformName::uDynLightColor, "uDynLightColor", UniformType::Vec4);
		DeclareUniform(UniformName::uAddColor, "uAddColor", UniformType::Vec4);
		DeclareUniform(UniformName::uTextureAddColor, "uTextureAddColor", UniformType::Vec4);
		DeclareUniform(UniformName::uTextureModulateColor, "uTextureModulateColor", UniformType::Vec4);
		DeclareUniform(UniformName::uTextureBlendColor, "uTextureBlendColor", UniformType::Vec4);
		DeclareUniform(UniformName::uFogColor, "uFogColor", UniformType::Vec4);

		DeclareUniform(UniformName::uDesaturationFactor, "uDesaturationFactor", UniformType::Float);
		DeclareUniform(UniformName::uInterpolationFactor, "uInterpolationFactor", UniformType::Float);
		DeclareUniform(UniformName::timer, "timer", UniformType::Float);
		DeclareUniform(UniformName::useVertexData, "useVertexData", UniformType::Int);

		DeclareUniform(UniformName::uVertexColor, "uVertexColor", UniformType::Vec4);
		DeclareUniform(UniformName::uVertexNormal, "uVertexNormal", UniformType::Vec4);

		DeclareUniform(UniformName::uGlowTopPlane, "uGlowTopPlane", UniformType::Vec4);
		DeclareUniform(UniformName::uGlowTopColor, "uGlowTopColor", UniformType::Vec4);
		DeclareUniform(UniformName::uGlowBottomPlane, "uGlowBottomPlane", UniformType::Vec4);
		DeclareUniform(UniformName::uGlowBottomColor, "uGlowBottomColor", UniformType::Vec4);

		DeclareUniform(UniformName::uGradientTopPlane, "uGradientTopPlane", UniformType::Vec4);
		DeclareUniform(UniformName::uGradientBottomPlane, "uGradientBottomPlane", UniformType::Vec4);

		DeclareUniform(UniformName::uSplitTopPlane, "uSplitTopPlane", UniformType::Vec4);
		DeclareUniform(UniformName::uSplitBottomPlane, "uSplitBottomPlane", UniformType::Vec4);

		DeclareUniform(UniformName::uTextureMode, "uTextureMode", UniformType::Int, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uAlphaThreshold, "uAlphaThreshold", UniformType::Float, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uClipSplit, "uClipSplit", UniformType::Vec2, UniformFamily::PushConstant);

		DeclareUniform(UniformName::uLightLevel, "uLightLevel", UniformType::Float, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uFogDensity, "uFogDensity", UniformType::Float, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uLightFactor, "uLightFactor", UniformType::Float, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uLightDist, "uLightDist", UniformType::Float, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uFogEnabled, "uFogEnabled", UniformType::Int, UniformFamily::PushConstant);

		DeclareUniform(UniformName::uLightIndex, "uLightIndex", UniformType::Int, UniformFamily::PushConstant);

		DeclareUniform(UniformName::uSpecularMaterial, "uSpecularMaterial", UniformType::Vec2, UniformFamily::PushConstant);
		
		DeclareUniform(UniformName::uDataIndex, "uDataIndex", UniformType::Int, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uPadding1, "uPadding1", UniformType::Int, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uPadding2, "uPadding2", UniformType::Int, UniformFamily::PushConstant);
		DeclareUniform(UniformName::uPadding3, "uPadding3", UniformType::Int, UniformFamily::PushConstant);

		int i = 0;
		for (const auto& info : mUniformInfo)
			mSortedUniformInfo[(int)info.Type].push_back(i++);
	}

	void DeclareUniform(UniformName nameIndex, const char* glslname, UniformType type, UniformFamily family = UniformFamily::Normal)
	{
		size_t index = (size_t)nameIndex;
		if (mUniformInfo.size() <= index)
			mUniformInfo.resize(index + 1);

		auto& data = mUniformData[(int)family];

		UniformInfo& info = mUniformInfo[index];
		info.Name = glslname;
		info.Type = type;
		info.Family = family;
		info.Offset = (int)data.size();

		static const int bytesize[] = { 4, 4, 4, 8, 12, 16, 8, 12, 16, 8, 12, 16, 16 };
		data.resize(data.size() + bytesize[(int)type]);
	}

	void SetUniform(UniformName nameIndex, const PalEntry& value)
	{
		SetUniform(nameIndex, FVector4(value.r * (1.0f / 255.0f), value.g * (1.0f / 255.0f), value.b * (1.0f / 255.0f), value.a * (1.0f / 255.0f)));
	}

	void SetUniform(UniformName nameIndex, double value)
	{
		SetUniform(nameIndex, (float)value);
	}

	void SetUniform(UniformName nameIndex, const DVector2& value)
	{
		SetUniform(nameIndex, FVector2((float)value.X, (float)value.Y));
	}

	void SetUniform(UniformName nameIndex, const DVector3& value)
	{
		SetUniform(nameIndex, FVector3((float)value.X, (float)value.Y, (float)value.Z));
	}

	void SetUniform(UniformName nameIndex, const DVector4& value)
	{
		SetUniform(nameIndex, FVector4((float)value.X, (float)value.Y, (float)value.Z, (float)value.W));
	}

	template<typename T>
	void SetUniform(UniformName nameIndex, const T& value)
	{
		T* dest = (T*)GetUniformData(nameIndex);
		if (*dest != value)
		{
			*dest = value;
			mUniformInfo[(int)nameIndex].LastUpdate++;
		}
	}

	void SetUniformAlpha(UniformName nameIndex, float alpha)
	{
		float* dest = (float*)GetUniformData(nameIndex);
		if (dest[3] != alpha)
		{
			dest[3] = alpha;
			mUniformInfo[(int)nameIndex].LastUpdate++;
		}
	}

	void SetUniformIntegerAlpha(UniformName nameIndex, const PalEntry& value)
	{
		SetUniform(nameIndex, FVector4(value.r * (1.0f / 255.0f), value.g * (1.0f / 255.0f), value.b * (1.0f / 255.0f), (float)value.a));
	}

	template<typename T>
	T GetUniform(UniformName nameIndex)
	{
		return *(T*)GetUniformData(nameIndex);
	}

	void* GetUniformData(UniformName nameIndex)
	{
		const auto& info = mUniformInfo[(int)nameIndex];
		return mUniformData[(int)info.Family].data() + info.Offset;
	}

	struct UniformInfo
	{
		std::string Name;
		UniformType Type = {};
		UniformFamily Family = {};
		int Offset = 0;
		int LastUpdate = 0;
	};

	std::vector<UniformInfo> mUniformInfo;
	std::vector<uint8_t> mUniformData[(int)UniformFamily::NumFamilies];

	// For the OpenGL backend so that it doesn't have to do a switch between the uniform types
	std::vector<int> mSortedUniformInfo[(int)UniformType::NumUniformTypes];

	uint8_t mFogEnabled;
	uint8_t mTextureEnabled:1;
	uint8_t mGlowEnabled : 1;
	uint8_t mGradientEnabled : 1;
	uint8_t mBrightmapEnabled : 1;
	uint8_t mModelMatrixEnabled : 1;
	uint8_t mTextureMatrixEnabled : 1;
	uint8_t mSplitEnabled : 1;

	int mSpecialEffect;
	int mTextureMode;
	int mSoftLight;

	PalEntry mFogColor;

	FRenderStyle mRenderStyle;

	FMaterialState mMaterial;
	FDepthBiasState mBias;

	IVertexBuffer *mVertexBuffer;
	int mVertexOffsets[2];	// one per binding point
	IIndexBuffer *mIndexBuffer;

	EPassType mPassType = NORMAL_PASS;

	uint64_t firstFrame = 0;

public:
	VSMatrix mModelMatrix;
	VSMatrix mTextureMatrix;

public:
	const std::vector<UniformInfo>& GetUniformInfo() const { return mUniformInfo; }
	const void* GetUniformData(UniformFamily family) const { return mUniformData[(int)family].data(); }
	uint32_t GetUniformDataSize(UniformFamily family) const { return (uint32_t)mUniformData[(int)family].size(); }

	void Reset()
	{
		mTextureEnabled = true;
		mGradientEnabled = mBrightmapEnabled = mFogEnabled = mGlowEnabled = false;
		mFogColor = 0xffffffff;
		SetUniform(UniformName::uFogColor, mFogColor);
		mTextureMode = -1;
		SetUniform(UniformName::uDesaturationFactor, 0.0f);
		SetUniform(UniformName::uAlphaThreshold, 0.5f);
		mModelMatrixEnabled = false;
		mTextureMatrixEnabled = false;
		mSplitEnabled = false;
		SetUniform(UniformName::uAddColor, 0);
		SetUniform(UniformName::uObjectColor, 0xffffffff);
		SetUniform(UniformName::uObjectColor2, 0);
		SetUniform(UniformName::uTextureBlendColor, 0);
		SetUniform(UniformName::uTextureAddColor, 0);
		SetUniform(UniformName::uTextureModulateColor, 0);
		mSoftLight = 0;
		SetUniform(UniformName::uLightDist, 0.0f);
		SetUniform(UniformName::uLightFactor, 0.0f);
		SetUniform(UniformName::uFogDensity, 0.0f);
		SetUniform(UniformName::uLightLevel, -1.f);
		mSpecialEffect = EFF_NONE;
		SetUniform(UniformName::uLightIndex, -1);
		SetUniform(UniformName::uInterpolationFactor, 0);
		mRenderStyle = DefaultRenderStyle();
		mMaterial.Reset();
		mBias.Reset();
		mPassType = NORMAL_PASS;

		mVertexBuffer = nullptr;
		mVertexOffsets[0] = mVertexOffsets[1] = 0;
		mIndexBuffer = nullptr;

		SetUniform(UniformName::uVertexColor, FVector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		SetUniform(UniformName::uGlowTopColor, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uGlowBottomColor, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uGlowTopPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uGlowBottomPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uGradientTopPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uGradientBottomPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uSplitTopPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uSplitBottomPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		SetUniform(UniformName::uDynLightColor, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });

		mModelMatrix.loadIdentity();
		mTextureMatrix.loadIdentity();
		ClearClipSplit();
	}

	void SetNormal(FVector3 norm)
	{
		SetUniform(UniformName::uVertexNormal, FVector4{ norm.X, norm.Y, norm.Z, 0.f });
	}

	void SetNormal(float x, float y, float z)
	{
		SetUniform(UniformName::uVertexNormal, FVector4{ x, y, z, 0.f });
	}

	void SetColor(float r, float g, float b, float a = 1.f, int desat = 0)
	{
		SetUniform(UniformName::uVertexColor, FVector4{ r, g, b, a });
		SetUniform(UniformName::uDesaturationFactor, desat * (1.0f / 255.0f));
	}

	void SetColor(PalEntry pe, int desat = 0)
	{
		const float scale = 1.0f / 255.0f;
		SetUniform(UniformName::uVertexColor, FVector4{ pe.r * scale, pe.g * scale, pe.b * scale, pe.a * scale });
		SetUniform(UniformName::uDesaturationFactor, desat * (1.0f / 255.0f));
	}

	void SetColorAlpha(PalEntry pe, float alpha = 1.f, int desat = 0)
	{
		const float scale = 1.0f / 255.0f;
		SetUniform(UniformName::uVertexColor, FVector4{ pe.r * scale, pe.g * scale, pe.b * scale, alpha });
		SetUniform(UniformName::uDesaturationFactor, desat * (1.0f / 255.0f));
	}

	void ResetColor()
	{
		SetUniform(UniformName::uVertexColor, FVector4{ 1.0f, 1.0f, 1.0f, 1.0f });
		SetUniform(UniformName::uDesaturationFactor, 0.0f);
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
		if (mGlowEnabled && !on)
		{
			SetUniform(UniformName::uGlowTopColor, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
			SetUniform(UniformName::uGlowBottomColor, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
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
			SetUniform(UniformName::uSplitTopPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
			SetUniform(UniformName::uSplitBottomPlane, FVector4{ 0.0f, 0.0f, 0.0f, 0.0f });
		}
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
		SetUniform(UniformName::uGlowTopColor, FVector4{ t[0], t[1], t[2], t[3] });
		SetUniform(UniformName::uGlowBottomColor, FVector4{ b[0], b[1], b[2], b[3] });
	}

	void SetSoftLightLevel(int llevel, int blendfactor = 0)
	{
		if (blendfactor == 0) SetUniform(UniformName::uLightLevel, llevel / 255.f);
		else SetUniform(UniformName::uLightLevel, -1.f);
	}

	void SetNoSoftLightLevel()
	{
		SetUniform(UniformName::uLightLevel, -1.f);
	}

	void SetGlowPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		SetUniform(UniformName::uGlowTopPlane, FVector4{ (float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD() });
		SetUniform(UniformName::uGlowBottomPlane, FVector4{ (float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD() });
	}

	void SetGradientPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		SetUniform(UniformName::uGradientTopPlane, FVector4{ (float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD() });
		SetUniform(UniformName::uGradientBottomPlane, FVector4{ (float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD() });
	}

	void SetSplitPlanes(const secplane_t &top, const secplane_t &bottom)
	{
		auto &tn = top.Normal();
		auto &bn = bottom.Normal();
		SetUniform(UniformName::uSplitTopPlane, FVector4{ (float)tn.X, (float)tn.Y, (float)top.negiC, (float)top.fD() });
		SetUniform(UniformName::uSplitBottomPlane, FVector4{ (float)bn.X, (float)bn.Y, (float)bottom.negiC, (float)bottom.fD() });
	}

	void SetDynLight(float r, float g, float b)
	{
		SetUniform(UniformName::uDynLightColor, FVector4{ r, g, b, 0.0f });
	}

	void SetObjectColor(PalEntry pe)
	{
		SetUniform(UniformName::uObjectColor, pe);
	}

	void SetObjectColor2(PalEntry pe)
	{
		SetUniform(UniformName::uObjectColor2, pe);
	}

	void SetAddColor(PalEntry pe)
	{
		SetUniform(UniformName::uAddColor, pe);
	}

	void ApplyTextureManipulation(TextureManipulation* texfx)
	{
		if (!texfx || texfx->AddColor.a == 0)
		{
			SetUniformAlpha(UniformName::uTextureAddColor, 0);	// we only need to set the flags to 0
		}
		else
		{
			// set up the whole thing
			SetUniformIntegerAlpha(UniformName::uTextureAddColor, texfx->AddColor);
			auto pe = texfx->ModulateColor;
			SetUniform(UniformName::uTextureModulateColor, FVector4{ pe.r * pe.a / 255.f, pe.g * pe.a / 255.f, pe.b * pe.a / 255.f, texfx->DesaturationFactor });
			SetUniform(UniformName::uTextureBlendColor, texfx->BlendColor);
		}
	}

	void SetFog(PalEntry c, float d)
	{
		const float LOG2E = 1.442692f;	// = 1/log(2)
		mFogColor = c;
		SetUniform(UniformName::uFogColor, mFogColor);
		if (d >= 0.0f) SetUniform(UniformName::uFogDensity, d * (-LOG2E / 64000.f));
	}

	void SetLightParms(float f, float d)
	{
		SetUniform(UniformName::uLightFactor, f);
		SetUniform(UniformName::uLightDist, d);
	}

	PalEntry GetFogColor() const
	{
		return mFogColor;
	}

	void AlphaFunc(int func, float thresh)
	{
		if (func == Alpha_Greater) SetUniform(UniformName::uAlphaThreshold, thresh);
		else SetUniform(UniformName::uAlphaThreshold, thresh - 0.001f);
	}

	void SetPlaneTextureRotation(HWSectorPlane *plane, FMaterial *texture)
	{
		if (hw_SetPlaneTextureRotation(plane, texture, mTextureMatrix))
		{
			EnableTextureMatrix(true);
		}
	}

	void SetLightIndex(int index)
	{
		SetUniform(UniformName::uLightIndex, index);
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
		SetUniform(UniformName::uClipSplit, FVector2(bottom, top));
	}

	void SetClipSplit(float *vals)
	{
		SetUniform(UniformName::uClipSplit, FVector2(vals[0], vals[1]));
	}

	void GetClipSplit(float *out)
	{
		memcpy(out, GetUniformData(UniformName::uClipSplit), 2 * sizeof(float));
	}

	void ClearClipSplit()
	{
		SetUniform(UniformName::uClipSplit, FVector2(-1000000.f, 1000000.f));
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
		SetUniform(UniformName::uInterpolationFactor, fac);
	}

	float GetInterpolationFactor()
	{
		return GetUniform<float>(UniformName::uInterpolationFactor);
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

	void CheckTimer(uint64_t ShaderStartTime);

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
	virtual void EnableDrawBuffers(int count) = 0;				// Used by SSAO and EnableDrawBufferAttachments

	void SetColorMask(bool on)
	{
		SetColorMask(on, on, on, on);
	}

};

