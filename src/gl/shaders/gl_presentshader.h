#ifndef __GL_PRESENTSHADER_H
#define __GL_PRESENTSHADER_H

#include "gl_shaderprogram.h"

class FPresentShader
{
public:
	void Bind();

	FBufferedUniform1i InputTexture;
	FBufferedUniform1f Gamma;
	FBufferedUniform1f Contrast;
	FBufferedUniform1f Brightness;

private:
	FShaderProgram mShader;
};

#endif