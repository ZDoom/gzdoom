#ifndef __GL_LENSSHADER_H
#define __GL_LENSSHADER_H

#include "gl_shaderprogram.h"

class FLensShader
{
public:
	void Bind();

	FBufferedUniform1i InputTexture;
	FBufferedUniform1f AspectRatio;
	FBufferedUniform1f Scale;
	FBufferedUniform4f LensDistortionCoefficient;
	FBufferedUniform4f CubicDistortionValue;

private:
	FShaderProgram mShader;
};

#endif