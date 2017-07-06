#pragma once

#include "gl_shaderprogram.h"

class PostProcessShaderInstance;

struct PostProcessShader
{
	FString Target;
	FString ShaderLumpName;
	int ShaderVersion = 0;
	FTexture *Texture = nullptr;

	FString Name;
	TMap<FString, int> Uniform1i;
	TMap<FString, float> Uniform1f;
};

extern TArray<PostProcessShader> PostProcessShaders;

class PostProcessShaderInstance
{
public:
	PostProcessShaderInstance(PostProcessShader *desc) : Desc(desc) { }

	void Run();

	PostProcessShader *Desc;
	FShaderProgram Program;
	FBufferedUniformSampler InputTexture;
	FBufferedUniformSampler CustomTexture;
	FHardwareTexture *HWTexture = nullptr;
};

class FCustomPostProcessShaders
{
public:
	FCustomPostProcessShaders();
	~FCustomPostProcessShaders();

	void Run(FString target);

private:
	std::vector<std::unique_ptr<PostProcessShaderInstance>> mShaders;

	FCustomPostProcessShaders(const FCustomPostProcessShaders &) = delete;
	FCustomPostProcessShaders &operator=(const FCustomPostProcessShaders &) = delete;
};
