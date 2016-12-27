#ifndef __GL_BLOOMSHADER_H
#define __GL_BLOOMSHADER_H

#include "gl_shaderprogram.h"

class FBloomExtractShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;
	FBufferedUniformSampler ExposureTexture;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;

private:
	FShaderProgram mShader;
};

class FBloomCombineShader
{
public:
	void Bind();

	FBufferedUniformSampler BloomTexture;

private:
	FShaderProgram mShader;
};

#endif