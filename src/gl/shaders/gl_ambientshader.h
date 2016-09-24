#ifndef __GL_AMBIENTSHADER_H
#define __GL_AMBIENTSHADER_H

#include "gl_shaderprogram.h"

class FLinearDepthShader
{
public:
	void Bind(bool multisample);

	FBufferedUniformSampler DepthTexture[2];
	FBufferedUniformSampler ColorTexture[2];
	FBufferedUniform1i SampleCount[2];
	FBufferedUniform1f LinearizeDepthA[2];
	FBufferedUniform1f LinearizeDepthB[2];
	FBufferedUniform1f InverseDepthRangeA[2];
	FBufferedUniform1f InverseDepthRangeB[2];
	FBufferedUniform2f Scale[2];
	FBufferedUniform2f Offset[2];

private:
	FShaderProgram mShader[2];
};

class FSSAOShader
{
public:
	void Bind();

	FBufferedUniformSampler DepthTexture;
	FBufferedUniformSampler RandomTexture;
	FBufferedUniform2f UVToViewA;
	FBufferedUniform2f UVToViewB;
	FBufferedUniform2f InvFullResolution;
	FBufferedUniform1f NDotVBias;
	FBufferedUniform1f NegInvR2;
	FBufferedUniform1f RadiusToScreen;
	FBufferedUniform1f AOMultiplier;
	FBufferedUniform1f AOStrength;

private:
	enum Quality
	{
		Off,
		LowQuality,
		MediumQuality,
		HighQuality,
		NumQualityModes
	};

	FString GetDefines(int mode);

	FShaderProgram mShader[NumQualityModes];
};

class FDepthBlurShader
{
public:
	void Bind(bool vertical);

	FBufferedUniformSampler AODepthTexture[2];
	FBufferedUniform1f BlurSharpness[2];
	FBufferedUniform2f InvFullResolution[2];
	FBufferedUniform1f PowExponent[2];

private:
	FShaderProgram mShader[2];
};

class FSSAOCombineShader
{
public:
	void Bind(bool multisample);

	FBufferedUniformSampler AODepthTexture[2];
	FBufferedUniformSampler SceneDataTexture[2];
	FBufferedUniform1i SampleCount[2];
	FBufferedUniform2f Scale[2];
	FBufferedUniform2f Offset[2];

private:
	FShaderProgram mShader[2];
};

#endif