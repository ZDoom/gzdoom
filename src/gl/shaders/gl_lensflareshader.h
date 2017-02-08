#pragma once

#include "gl_shaderprogram.h"

class FLensFlareGhostShader
{
public:
	void Bind();

	//FBufferedUniformSampler FlareTexture;
	FBufferedUniformSampler InputTexture;
	FBufferedUniform1i nSamples;
	FBufferedUniform1f flareDispersal;
	FBufferedUniform1f flareHaloWidth;
	FBufferedUniform3f flareChromaticDistortion;

private:
	FShaderProgram mShader;
};