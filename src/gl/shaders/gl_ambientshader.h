#ifndef __GL_AMBIENTSHADER_H
#define __GL_AMBIENTSHADER_H

#include "gl_shaderprogram.h"

class FLinearDepthShader
{
public:
	void Bind();

	FBufferedUniformSampler DepthTexture;
	FBufferedUniform1f LinearizeDepthA;
	FBufferedUniform1f LinearizeDepthB;
	FBufferedUniform1f InverseDepthRangeA;
	FBufferedUniform1f InverseDepthRangeB;

private:
	FShaderProgram mShader;
};

class FSSAOShader
{
public:
	void Bind();

	FBufferedUniformSampler DepthTexture;
	FBufferedUniform2f UVToViewA;
	FBufferedUniform2f UVToViewB;
	FBufferedUniform2f InvFullResolution;
	FBufferedUniform1f NDotVBias;
	FBufferedUniform1f NegInvR2;
	FBufferedUniform1f RadiusToScreen;
	FBufferedUniform1f AOMultiplier;

private:
	FShaderProgram mShader;
};

#endif