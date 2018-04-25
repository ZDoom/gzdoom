#ifndef __GL_AMBIENTSHADER_H
#define __GL_AMBIENTSHADER_H

#include <memory>

#include "gl_shaderprogram.h"

class FLinearDepthShader
{
public:
	void Bind();

	FBufferedUniformSampler DepthTexture;
	FBufferedUniformSampler ColorTexture;
	FBufferedUniform1i SampleIndex;
	FBufferedUniform1f LinearizeDepthA;
	FBufferedUniform1f LinearizeDepthB;
	FBufferedUniform1f InverseDepthRangeA;
	FBufferedUniform1f InverseDepthRangeB;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;

private:
	std::unique_ptr<FShaderProgram> mShader;
	bool mMultisample = false;
};

class FSSAOShader
{
public:
	void Bind();

	FBufferedUniformSampler DepthTexture;
	FBufferedUniformSampler NormalTexture;
	FBufferedUniformSampler RandomTexture;
	FBufferedUniform2f UVToViewA;
	FBufferedUniform2f UVToViewB;
	FBufferedUniform2f InvFullResolution;
	FBufferedUniform1f NDotVBias;
	FBufferedUniform1f NegInvR2;
	FBufferedUniform1f RadiusToScreen;
	FBufferedUniform1f AOMultiplier;
	FBufferedUniform1f AOStrength;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;
	FBufferedUniform1i SampleIndex;

private:
	enum Quality
	{
		Off,
		LowQuality,
		MediumQuality,
		HighQuality,
		NumQualityModes
	};

	FString GetDefines(int mode, bool multisample);

	std::unique_ptr<FShaderProgram> mShader;
	Quality mCurrentQuality = Off;
	bool mMultisample = false;
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
	void Bind();

	FBufferedUniformSampler AODepthTexture;
	FBufferedUniformSampler SceneFogTexture;
	FBufferedUniform1i SampleCount;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;

private:
	std::unique_ptr<FShaderProgram> mShader;
	bool mMultisample = false;
};

#endif
