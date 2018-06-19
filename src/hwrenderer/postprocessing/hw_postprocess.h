#pragma once

#include "hwrenderer/data/shaderuniforms.h"

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
		delete[] Data;
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

	template<typename T>
	void Set(const T &v)
	{
		if (Size != (int)sizeof(T))
		{
			delete[] Data;
			Data = nullptr;
			Size = 0;

			Data = new uint8_t[Size];
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
		if ((int)Textures.Size() < index)
			Textures.Resize(index + 1);
		auto &tex = Textures[index];
		tex.Filter = filter;
		tex.Type = PPTextureType::PPTexture;
		tex.Texture = texture;
	}

	void SetInputCurrent(int index, PPFilterMode filter = PPFilterMode::Nearest)
	{
		if ((int)Textures.Size() < index)
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
	Rgba16f
};

class PPTextureDesc
{
public:
	PPTextureDesc() { }
	PPTextureDesc(int width, int height, PixelFormat format) : Width(width), Height(height), Format(format) { }

	int Width;
	int Height;
	PixelFormat Format;
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
