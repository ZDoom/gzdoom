
#pragma once

#include "gl_load/gl_system.h"
#include "gl_shader.h"
#include "hwrenderer/postprocessing/hw_shaderprogram.h"

namespace OpenGLRenderer
{

class FShaderProgram : public IShaderProgram
{
public:
	FShaderProgram();
	~FShaderProgram();

	void Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion) override;
	void Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion) override;
	void Link(const char *name);
	void SetUniformBufferLocation(int index, const char *name);

	void Bind(IRenderQueue *q) override;	// the parameter here is just a preparation for Vulkan

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

}