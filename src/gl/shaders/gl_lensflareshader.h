#pragma once

#include "gl_shaderprogram.h"

class FLensFlareShader
{
public:
	void Bind();

	//FBufferedUniformSampler FlareTexture;
	FBufferedUniformSampler InputTexture;

private:
	FShaderProgram mShader;
};