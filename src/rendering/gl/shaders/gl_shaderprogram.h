
#pragma once

#include "gl_load/gl_system.h"
#include "gl_shader.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

namespace OpenGLRenderer
{

class FShaderProgram : public PPShaderBackend
{
public:
	FShaderProgram();
	~FShaderProgram();

	enum ShaderType
	{
		Vertex,
		Fragment,
		NumShaderTypes
	};

	void Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion);
	void Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion);
	void Link(const char *name);
	void SetUniformBufferLocation(int index, const char *name);

	void Bind();

	GLuint Handle() { return mProgram; }
	//explicit operator bool() const { return mProgram != 0; }

	std::unique_ptr<IDataBuffer> Uniforms;

private:
	FShaderProgram(const FShaderProgram &) = delete;
	FShaderProgram &operator=(const FShaderProgram &) = delete;

	void CompileShader(ShaderType type);
	FString PatchShader(ShaderType type, const FString &code, const char *defines, int maxGlslVersion);

	void CreateShader(ShaderType type);
	FString GetShaderInfoLog(GLuint handle);
	FString GetProgramInfoLog(GLuint handle);

	GLuint mProgram = 0;
	GLuint mShaders[NumShaderTypes];
	FString mShaderSources[NumShaderTypes];
	FString mShaderNames[NumShaderTypes];
	TArray<std::pair<FString, int>> samplerstobind;
};

class FPresentShaderBase
{
public:
	virtual ~FPresentShaderBase() {}
	virtual void Bind() = 0;

	ShaderUniforms<PresentUniforms, POSTPROCESS_BINDINGPOINT> Uniforms;

protected:
	virtual void Init(const char * vtx_shader_name, const char * program_name);
	std::unique_ptr<FShaderProgram> mShader;
};

class FPresentShader : public FPresentShaderBase
{
public:
	void Bind() override;

};

class FPresent3DCheckerShader : public FPresentShaderBase
{
public:
	void Bind() override;
};

class FPresent3DColumnShader : public FPresentShaderBase
{
public:
	void Bind() override;
};

class FPresent3DRowShader : public FPresentShaderBase
{
public:
	void Bind() override;
};

class FShadowMapShader
{
public:
	void Bind();

	ShaderUniforms<ShadowMapUniforms, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	std::unique_ptr<FShaderProgram> mShader;
};

}