#ifndef __GL_BLOOMSHADER_H
#define __GL_BLOOMSHADER_H

#include "gl_shaderprogram.h"

class FBloomExtractShader
{
public:
	void Bind();

	FBufferedUniform1i SceneTexture;
	FBufferedUniform1f Exposure;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;

private:
	FShaderProgram mShader;
};

class FBloomCombineShader
{
public:
	void Bind();

	FBufferedUniform1i BloomTexture;

private:
	FShaderProgram mShader;
};

#endif