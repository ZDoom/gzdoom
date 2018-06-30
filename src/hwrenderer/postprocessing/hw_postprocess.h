#pragma once

#include "hwrenderer/data/shaderuniforms.h"
#include <memory>

typedef FString PPTextureName;
typedef FString PPShaderName;

typedef FRenderStyle PPBlendMode;
typedef IntRect PPViewport;

enum class PPFilterMode { Nearest, Linear };
enum class PPWrapMode { Clamp, Repeat };
enum class PPTextureType { CurrentPipelineTexture, NextPipelineTexture, PPTexture, SceneColor, SceneFog, SceneNormal, SceneDepth };

class PPTextureInput
{
public:
	PPFilterMode Filter;
	PPWrapMode Wrap;
	PPTextureType Type;
	PPTextureName Texture;
};

class PPOutput
{
public:
	PPTextureType Type;
	PPTextureName Texture;
};

class PPUniforms
{
public:
	PPUniforms()
	{
	}

	PPUniforms(const PPUniforms &src)
	{
		if (src.Size > 0)
		{
			Data = new uint8_t[src.Size];
			Size = src.Size;
			memcpy(Data, src.Data, Size);
		}
	}

	~PPUniforms()
	{
		Clear();
	}

	PPUniforms &operator=(const PPUniforms &src)
	{
		if (this != &src)
		{
			if (src.Size > 0)
			{
				Data = new uint8_t[src.Size];
				Size = src.Size;
				memcpy(Data, src.Data, Size);
			}
			else
			{
				delete[] Data;
				Data = nullptr;
				Size = 0;
			}
		}

		return *this;
	}

	void Clear()
	{
		delete[] Data;
		Data = nullptr;
		Size = 0;
	}

	template<typename T>
	void Set(const T &v)
	{
		if (Size != (int)sizeof(T))
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;

			Data = new uint8_t[sizeof(T)];
			Size = sizeof(T);
			memcpy(Data, &v, Size);
		}
	}

	uint8_t *Data = nullptr;
	int Size = 0;
};

class PPStep
{
public:
	void SetInputTexture(int index, PPTextureName texture, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		if ((int)Textures.Size() < index + 1)
			Textures.Resize(index + 1);
		auto &tex = Textures[index];
		tex.Filter = filter;
		tex.Wrap = wrap;
		tex.Type = PPTextureType::PPTexture;
		tex.Texture = texture;
	}

	void SetInputCurrent(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		SetInputSpecialType(index, PPTextureType::CurrentPipelineTexture, filter, wrap);
	}

	void SetInputSceneColor(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		SetInputSpecialType(index, PPTextureType::SceneColor, filter, wrap);
	}

	void SetInputSceneFog(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		SetInputSpecialType(index, PPTextureType::SceneFog, filter, wrap);
	}

	void SetInputSceneNormal(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		SetInputSpecialType(index, PPTextureType::SceneNormal, filter, wrap);
	}

	void SetInputSceneDepth(int index, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		SetInputSpecialType(index, PPTextureType::SceneDepth, filter, wrap);
	}

	void SetInputSpecialType(int index, PPTextureType type, PPFilterMode filter = PPFilterMode::Nearest, PPWrapMode wrap = PPWrapMode::Clamp)
	{
		if ((int)Textures.Size() < index + 1)
			Textures.Resize(index + 1);
		auto &tex = Textures[index];
		tex.Filter = filter;
		tex.Wrap = wrap;
		tex.Type = type;
		tex.Texture = {};
	}

	void SetOutputTexture(PPTextureName texture)
	{
		Output.Type = PPTextureType::PPTexture;
		Output.Texture = texture;
	}

	void SetOutputCurrent()
	{
		Output.Type = PPTextureType::CurrentPipelineTexture;
		Output.Texture = {};
	}

	void SetOutputNext()
	{
		Output.Type = PPTextureType::NextPipelineTexture;
		Output.Texture = {};
	}

	void SetOutputSceneColor()
	{
		Output.Type = PPTextureType::SceneColor;
		Output.Texture = {};
	}

	void SetNoBlend()
	{
		BlendMode.BlendOp = STYLEOP_Add;
		BlendMode.SrcAlpha = STYLEALPHA_One;
		BlendMode.DestAlpha = STYLEALPHA_Zero;
		BlendMode.Flags = 0;
	}

	void SetAdditiveBlend()
	{
		BlendMode.BlendOp = STYLEOP_Add;
		BlendMode.SrcAlpha = STYLEALPHA_One;
		BlendMode.DestAlpha = STYLEALPHA_One;
		BlendMode.Flags = 0;
	}

	void SetAlphaBlend()
	{
		BlendMode.BlendOp = STYLEOP_Add;
		BlendMode.SrcAlpha = STYLEALPHA_Src;
		BlendMode.DestAlpha = STYLEALPHA_InvSrc;
		BlendMode.Flags = 0;
	}

	PPShaderName ShaderName;
	TArray<PPTextureInput> Textures;
	PPUniforms Uniforms;
	PPViewport Viewport;
	PPBlendMode BlendMode;
	PPOutput Output;
};

enum class PixelFormat
{
	Rgba8,
	Rgba16f,
	R32f,
	Rg16f,
	Rgba16_snorm
};

class PPTextureDesc
{
public:
	PPTextureDesc() { }
	PPTextureDesc(int width, int height, PixelFormat format, std::shared_ptr<void> data = {}) : Width(width), Height(height), Format(format), Data(data) { }

	int Width;
	int Height;
	PixelFormat Format;
	std::shared_ptr<void> Data;
};

class PPShader
{
public:
	PPShader() { }
	PPShader(const FString &fragment, const FString &defines, const std::vector<UniformFieldDesc> &uniforms, int version = 330) : FragmentShader(fragment), Defines(defines), Uniforms(uniforms), Version(version) { }

	FString VertexShader = "shaders/glsl/screenquad.vp";
	FString FragmentShader;
	FString Defines;
	std::vector<UniformFieldDesc> Uniforms;
	int Version = 330;
};

class PPEffectManager
{
public:
	virtual ~PPEffectManager() { }
	virtual void DeclareShaders() { }
	virtual void UpdateTextures() { }
	virtual void UpdateSteps() { }
};

class Postprocess
{
public:
	Postprocess();
	~Postprocess();

	void DeclareShaders() { for (unsigned int i = 0; i < Managers.Size(); i++) Managers[i]->DeclareShaders(); }
	void UpdateTextures() { for (unsigned int i = 0; i < Managers.Size(); i++) Managers[i]->UpdateTextures(); }
	void UpdateSteps() { for (unsigned int i = 0; i < Managers.Size(); i++) Managers[i]->UpdateSteps(); }

	int SceneWidth = 0;
	int SceneHeight = 0;
	int fixedcm = 0;
	float gameinfobluramount = 0.0f;
	float m5 = 0.0f;

	TMap<FString, TArray<PPStep>> Effects;
	TMap<FString, PPTextureDesc> Textures;
	TMap<FString, PPShader> Shaders;

	TArray<PPEffectManager*> Managers;
};

extern Postprocess hw_postprocess;

/////////////////////////////////////////////////////////////////////////////

struct ExtractUniforms
{
	FVector2 Scale;
	FVector2 Offset;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "Scale", UniformType::Vec2, offsetof(ExtractUniforms, Scale) },
			{ "Offset", UniformType::Vec2, offsetof(ExtractUniforms, Offset) }
		};
	}
};

struct BlurUniforms
{
	float SampleWeights[8];

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "SampleWeights0", UniformType::Float, offsetof(BlurUniforms, SampleWeights[0]) },
			{ "SampleWeights1", UniformType::Float, offsetof(BlurUniforms, SampleWeights[1]) },
			{ "SampleWeights2", UniformType::Float, offsetof(BlurUniforms, SampleWeights[2]) },
			{ "SampleWeights3", UniformType::Float, offsetof(BlurUniforms, SampleWeights[3]) },
			{ "SampleWeights4", UniformType::Float, offsetof(BlurUniforms, SampleWeights[4]) },
			{ "SampleWeights5", UniformType::Float, offsetof(BlurUniforms, SampleWeights[5]) },
			{ "SampleWeights6", UniformType::Float, offsetof(BlurUniforms, SampleWeights[6]) },
			{ "SampleWeights7", UniformType::Float, offsetof(BlurUniforms, SampleWeights[7]) },
		};
	}
};

enum { NumBloomLevels = 4 };

class PPBlurLevel
{
public:
	PPViewport Viewport;
	PPTextureName VTexture;
	PPTextureName HTexture;
};

class PPBloom : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateTextures() override;
	void UpdateSteps() override;

private:
	void UpdateBlurSteps();

	PPStep BlurStep(const BlurUniforms &blurUniforms, PPTextureName input, PPTextureName output, PPViewport viewport, bool vertical);

	static float ComputeBlurGaussian(float n, float theta);
	static void ComputeBlurSamples(int sampleCount, float blurAmount, float *sampleWeights);

	PPBlurLevel levels[NumBloomLevels];
};

/////////////////////////////////////////////////////////////////////////////

struct LensUniforms
{
	float AspectRatio;
	float Scale;
	float Padding0, Padding1;
	FVector4 LensDistortionCoefficient;
	FVector4 CubicDistortionValue;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "Aspect", UniformType::Float, offsetof(LensUniforms, AspectRatio) },
			{ "Scale", UniformType::Float, offsetof(LensUniforms, Scale) },
			{ "Padding0", UniformType::Float, offsetof(LensUniforms, Padding0) },
			{ "Padding1", UniformType::Float, offsetof(LensUniforms, Padding1) },
			{ "k", UniformType::Vec4, offsetof(LensUniforms, LensDistortionCoefficient) },
			{ "kcube", UniformType::Vec4, offsetof(LensUniforms, CubicDistortionValue) }
		};
	}
};

class PPLensDistort : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateSteps() override;
};

/////////////////////////////////////////////////////////////////////////////

struct FXAAUniforms
{
	FVector2 ReciprocalResolution;
	float Padding0, Padding1;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "ReciprocalResolution", UniformType::Vec2, offsetof(FXAAUniforms, ReciprocalResolution) },
			{ "Padding0", UniformType::Float, offsetof(FXAAUniforms, Padding0) },
			{ "Padding1", UniformType::Float, offsetof(FXAAUniforms, Padding1) }
		};
	}
};

class PPFXAA : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateSteps() override;

private:
	int GetMaxVersion();
	FString GetDefines();
};

/////////////////////////////////////////////////////////////////////////////

struct ExposureExtractUniforms
{
	FVector2 Scale;
	FVector2 Offset;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "Scale", UniformType::Vec2, offsetof(ExposureExtractUniforms, Scale) },
			{ "Offset", UniformType::Vec2, offsetof(ExposureExtractUniforms, Offset) }
		};
	}
};

struct ExposureCombineUniforms
{
	float ExposureBase;
	float ExposureMin;
	float ExposureScale;
	float ExposureSpeed;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "ExposureBase", UniformType::Float, offsetof(ExposureCombineUniforms, ExposureBase) },
			{ "ExposureMin", UniformType::Float, offsetof(ExposureCombineUniforms, ExposureMin) },
			{ "ExposureScale", UniformType::Float, offsetof(ExposureCombineUniforms, ExposureScale) },
			{ "ExposureSpeed", UniformType::Float, offsetof(ExposureCombineUniforms, ExposureSpeed) }
		};
	}
};

class PPExposureLevel
{
public:
	PPViewport Viewport;
	PPTextureName Texture;
};

class PPCameraExposure : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateTextures() override;
	void UpdateSteps() override;

private:
	TArray<PPExposureLevel> ExposureLevels;
	bool FirstExposureFrame = true;
};

/////////////////////////////////////////////////////////////////////////////

struct ColormapUniforms
{
	FVector4 MapStart;
	FVector4 MapRange;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "uFixedColormapStart", UniformType::Vec4, offsetof(ColormapUniforms, MapStart) },
			{ "uFixedColormapRange", UniformType::Vec4, offsetof(ColormapUniforms, MapRange) },
		};
	}
};

class PPColormap : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateSteps() override;
};

/////////////////////////////////////////////////////////////////////////////

class PPTonemap : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateTextures() override;
	void UpdateSteps() override;

	enum TonemapMode
	{
		None,
		Uncharted2,
		HejlDawson,
		Reinhard,
		Linear,
		Palette,
		NumTonemapModes
	};
};

/////////////////////////////////////////////////////////////////////////////

struct LinearDepthUniforms
{
	int SampleIndex;
	float LinearizeDepthA;
	float LinearizeDepthB;
	float InverseDepthRangeA;
	float InverseDepthRangeB;
	float Padding0, Padding1, Padding2;
	FVector2 Scale;
	FVector2 Offset;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "SampleIndex", UniformType::Int, offsetof(LinearDepthUniforms, SampleIndex) },
			{ "LinearizeDepthA", UniformType::Float, offsetof(LinearDepthUniforms, LinearizeDepthA) },
			{ "LinearizeDepthB", UniformType::Float, offsetof(LinearDepthUniforms, LinearizeDepthB) },
			{ "InverseDepthRangeA", UniformType::Float, offsetof(LinearDepthUniforms, InverseDepthRangeA) },
			{ "InverseDepthRangeB", UniformType::Float, offsetof(LinearDepthUniforms, InverseDepthRangeB) },
			{ "Padding0", UniformType::Float, offsetof(LinearDepthUniforms, Padding0) },
			{ "Padding1", UniformType::Float, offsetof(LinearDepthUniforms, Padding1) },
			{ "Padding2", UniformType::Float, offsetof(LinearDepthUniforms, Padding2) },
			{ "Scale", UniformType::Vec2, offsetof(LinearDepthUniforms, Scale) },
			{ "Offset", UniformType::Vec2, offsetof(LinearDepthUniforms, Offset) }
		};
	}
};

struct SSAOUniforms
{
	FVector2 UVToViewA;
	FVector2 UVToViewB;
	FVector2 InvFullResolution;
	float NDotVBias;
	float NegInvR2;
	float RadiusToScreen;
	float AOMultiplier;
	float AOStrength;
	int SampleIndex;
	float Padding0, Padding1;
	FVector2 Scale;
	FVector2 Offset;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "UVToViewA", UniformType::Vec2, offsetof(SSAOUniforms, UVToViewA) },
			{ "UVToViewB", UniformType::Vec2, offsetof(SSAOUniforms, UVToViewB) },
			{ "InvFullResolution", UniformType::Vec2, offsetof(SSAOUniforms, InvFullResolution) },
			{ "NDotVBias", UniformType::Float, offsetof(SSAOUniforms, NDotVBias) },
			{ "NegInvR2", UniformType::Float, offsetof(SSAOUniforms, NegInvR2) },
			{ "RadiusToScreen", UniformType::Float, offsetof(SSAOUniforms, RadiusToScreen) },
			{ "AOMultiplier", UniformType::Float, offsetof(SSAOUniforms, AOMultiplier) },
			{ "AOStrength", UniformType::Float, offsetof(SSAOUniforms, AOStrength) },
			{ "SampleIndex", UniformType::Int, offsetof(SSAOUniforms, SampleIndex) },
			{ "Padding0", UniformType::Float, offsetof(SSAOUniforms, Padding0) },
			{ "Padding1", UniformType::Float, offsetof(SSAOUniforms, Padding1) },
			{ "Scale", UniformType::Vec2, offsetof(SSAOUniforms, Scale) },
			{ "Offset", UniformType::Vec2, offsetof(SSAOUniforms, Offset) },
		};
	}
};

struct DepthBlurUniforms
{
	float BlurSharpness;
	float PowExponent;
	FVector2 InvFullResolution;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "BlurSharpness", UniformType::Float, offsetof(DepthBlurUniforms, BlurSharpness) },
			{ "PowExponent", UniformType::Float, offsetof(DepthBlurUniforms, PowExponent) },
			{ "InvFullResolution", UniformType::Vec2, offsetof(DepthBlurUniforms, InvFullResolution) }
		};
	}
};

struct AmbientCombineUniforms
{
	int SampleCount;
	int Padding0, Padding1, Padding2;
	FVector2 Scale;
	FVector2 Offset;

	static std::vector<UniformFieldDesc> Desc()
	{
		return
		{
			{ "SampleCount", UniformType::Int, offsetof(AmbientCombineUniforms, SampleCount) },
			{ "Padding0", UniformType::Int, offsetof(AmbientCombineUniforms, Padding0) },
			{ "Padding1", UniformType::Int, offsetof(AmbientCombineUniforms, Padding1) },
			{ "Padding2", UniformType::Int, offsetof(AmbientCombineUniforms, Padding2) },
			{ "Scale", UniformType::Vec2, offsetof(AmbientCombineUniforms, Scale) },
			{ "Offset", UniformType::Vec2, offsetof(AmbientCombineUniforms, Offset) }
		};
	}
};

class PPAmbientOcclusion : public PPEffectManager
{
public:
	void DeclareShaders() override;
	void UpdateTextures() override;
	void UpdateSteps() override;

private:
	enum Quality
	{
		Off,
		LowQuality,
		MediumQuality,
		HighQuality,
		NumQualityModes
	};

	int AmbientWidth = 0;
	int AmbientHeight = 0;

	enum { NumAmbientRandomTextures = 3 };
	PPTextureName AmbientRandomTexture[NumAmbientRandomTextures];
};
