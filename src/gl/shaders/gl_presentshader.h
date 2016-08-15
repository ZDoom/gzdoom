#ifndef __GL_PRESENTSHADER_H
#define __GL_PRESENTSHADER_H

#include "gl_shaderprogram.h"

class FPresentShader
{
public:
	void Bind();

	FBufferedUniform1i InputTexture;
	FBufferedUniform1f InvGamma;
	FBufferedUniform1f Contrast;
	FBufferedUniform1f Brightness;
	FBufferedUniform2f Scale;

private:
	FShaderProgram mShader;
};

#endif