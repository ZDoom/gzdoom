
#pragma once

#include "gl_load/gl_system.h"
#include "gl_shader.h"
#include "hwrenderer/data/shaderuniforms.h"

class FShaderProgram
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
	void SetAttribLocation(int index, const char *name);
	void SetUniformBufferLocation(int index, const char *name);
	void Bind();

	operator GLuint() const { return mProgram; }
	explicit operator bool() const { return mProgram != 0; }

private:
	FShaderProgram(const FShaderProgram &) = delete;
	FShaderProgram &operator=(const FShaderProgram &) = delete;

	static FString PatchShader(ShaderType type, const FString &code, const char *defines, int maxGlslVersion);

	void CreateShader(ShaderType type);
	FString GetShaderInfoLog(GLuint handle);
	FString GetProgramInfoLog(GLuint handle);

	GLuint mProgram = 0;
	GLuint mShaders[NumShaderTypes];
};
