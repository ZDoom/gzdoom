#ifndef __GL_COLORMAPSHADER_H
#define __GL_COLORMAPSHADER_H

#include "gl_shaderprogram.h"

class FColormapShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;
	FUniform4f MapStart;
	FUniform4f MapRange;

private:

	FShaderProgram mShader;
};

#endif