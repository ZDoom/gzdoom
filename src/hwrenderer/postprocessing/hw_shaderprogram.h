
#pragma once

#include <memory>

#include "hwrenderer/data/shaderuniforms.h"

class IRenderQueue;

class IShaderProgram
{
public:
	IShaderProgram() {}
	virtual ~IShaderProgram() {}

	enum ShaderType
	{
		Vertex,
		Fragment,
		NumShaderTypes
	};

	virtual void Compile(ShaderType type, const char *lumpName, const char *defines, int maxGlslVersion) = 0;
	virtual void Compile(ShaderType type, const char *name, const FString &code, const char *defines, int maxGlslVersion) = 0;
	virtual void Link(const char *name) = 0;
	virtual void SetUniformBufferLocation(int index, const char *name) = 0;

	virtual void Bind(IRenderQueue *q) = 0;	// the parameter here is just a preparation for Vulkan

private:
	IShaderProgram(const IShaderProgram &) = delete;
	IShaderProgram &operator=(const IShaderProgram &) = delete;
};
