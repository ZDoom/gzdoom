#ifndef __GL_SHADERPROGRAM_H
#define __GL_SHADERPROGRAM_H

#include "gl_shader.h"

class FShaderProgram
{
public:
	~FShaderProgram();

	enum ShaderType
	{
		Vertex,
		Fragment,
		NumShaderTypes
	};

	void Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion);
	void Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion);
	void SetFragDataLocation(int index, const char *name);
	void Link(const char *name);
	void SetAttribLocation(int index, const char *name);
	void Bind();

	operator GLuint() const { return mProgram; }
	explicit operator bool() const { return mProgram != 0; }

	// Needed by FShader
	static void PatchVertShader(FString &code);
	static void PatchFragShader(FString &code);

private:
	static FString PatchShader(ShaderType type, const FString &code, const char *defines, int maxGlslVersion);
	static void PatchCommon(FString &code);

	void CreateShader(ShaderType type);
	FString GetShaderInfoLog(GLuint handle);
	FString GetProgramInfoLog(GLuint handle);

	GLuint mProgram = 0;
	GLuint mShaders[NumShaderTypes];
};

#endif