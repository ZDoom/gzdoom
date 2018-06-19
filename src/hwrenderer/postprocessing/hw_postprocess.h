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
	PPShaderName ShaderName;
	TArray<PPTextureInput> Textures;
	PPUniforms Uniforms;
	PPViewport Viewport;
	PPBlendMode BlendMode;
	PPOutput Output;
};

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

class PPTextureDesc
{
public:
	int Width;
	int Height;
	int Format;
};

class PPShader
{
public:
	FString VertexShader;
	FString FragmentShader;
	FString Defines;
	std::vector<UniformFieldDesc> Uniforms;
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
