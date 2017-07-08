#pragma once

#include "gl_shaderprogram.h"

class PostProcessShaderInstance;

enum class PostProcessUniformType
{
	Undefined,
	Int,
	Float,
	Vec2,
	Vec3
};

struct PostProcessUniformValue
{
	PostProcessUniformType Type = PostProcessUniformType::Undefined;
	double Values[4] = { 0.0, 0.0, 0.0, 0.0 };
};

struct PostProcessShader
{
	FString Target;
	FString ShaderLumpName;
	int ShaderVersion = 0;

	FString Name;
	bool Enabled = false;

	TMap<FString, PostProcessUniformValue> Uniforms;
};

extern TArray<PostProcessShader> PostProcessShaders;

class PostProcessShaderInstance
{
public:
	PostProcessShaderInstance(PostProcessShader *desc) : Desc(desc) { }

	void Run();

	PostProcessShader *Desc;

private:
	bool IsShaderSupported();
	void CompileShader();
	void UpdateUniforms();

	FShaderProgram mProgram;
	FBufferedUniformSampler mInputTexture;
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
