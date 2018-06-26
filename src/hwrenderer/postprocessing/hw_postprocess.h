#pragma once

#include "hwrenderer/data/shaderuniforms.h"
#include <memory>

typedef FString PPTextureName;
typedef FString PPShaderName;

typedef FRenderStyle PPBlendMode;
typedef IntRect PPViewport;

enum class PPFilterMode { Nearest, Linear };
enum class PPTextureType { CurrentPipelineTexture, NextPipelineTexture, PPTexture };

class PPTextureInput
{
public:
	PPFilterMode Filter;
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

	void *Data = nullptr;
	int Size = 0;
};

class PPStep
{
public:
	void SetInputTexture(int index, PPTextureName texture, PPFilterMode filter = PPFilterMode::Nearest)
	{
		if ((int)Textures.Size() < index + 1)
			Textures.Resize(index + 1);
		auto &tex = Textures[index];
		tex.Filter = filter;
		tex.Type = PPTextureType::PPTexture;
		tex.Texture = texture;
	}

	void SetInputCurrent(int index, PPFilterMode filter = PPFilterMode::Nearest)
	{
		if ((int)Textures.Size() < index + 1)
			Textures.Resize(index + 1);
		auto &tex = Textures[index];
		tex.Filter = filter;
		tex.Type = PPTextureType::CurrentPipelineTexture;
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
	R32f
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

class Postprocess
{
public:
	TMap<FString, TArray<PPStep>> Effects;
	TMap<FString, PPTextureDesc> Textures;
	TMap<FString, PPShader> Shaders;
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

class PPBloom
{
public:
	void DeclareShaders();
	void UpdateTextures(int sceneWidth, int sceneHeight);
	void UpdateSteps(int fixedcm);
	void UpdateBlurSteps(float gameinfobluramount);

private:
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

class PPLensDistort
{
public:
	void DeclareShaders();
	void UpdateSteps();
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

class PPFXAA
{
public:
	void DeclareShaders();
	void UpdateSteps();

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

class PPCameraExposure
{
public:
	void DeclareShaders();
	void UpdateTextures(int sceneWidth, int sceneHeight);
	void UpdateSteps();

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

class PPColormap
{
public:
	void DeclareShaders();
	void UpdateSteps(int fixedcm);
};

/////////////////////////////////////////////////////////////////////////////

class PPTonemap
{
public:
	void DeclareShaders();
	void UpdateTextures();
	void UpdateSteps();

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
