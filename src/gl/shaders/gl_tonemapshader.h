#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "gl_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;
	FBufferedUniformSampler ExposureTexture;
	FBufferedUniformSampler PaletteLUT;

	static bool IsPaletteMode();

private:
	enum TonemapMode
	{
		None,
		Uncharted2,
		HejlDawson,
		Reinhard,
		Linear,
		Palette,
		NumTonemapModes
	};

	static const char *GetDefines(int mode);

	FShaderProgram mShader[NumTonemapModes];
};

class FExposureExtractShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;
	FBufferedUniform2f Scale;
	FBufferedUniform2f Offset;

private:
	FShaderProgram mShader;
};

class FExposureAverageShader
{
public:
	void Bind();

	FBufferedUniformSampler ExposureTexture;

private:
	FShaderProgram mShader;
};

class FExposureCombineShader
{
public:
	void Bind();

	FBufferedUniformSampler ExposureTexture;
	FBufferedUniform1f ExposureBase;
	FBufferedUniform1f ExposureMin;
	FBufferedUniform1f ExposureScale;
	FBufferedUniform1f ExposureSpeed;

private:
	FShaderProgram mShader;
};

#endif