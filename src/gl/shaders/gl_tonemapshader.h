#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "gl_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind();

	FBufferedUniform1i SceneTexture;
	FBufferedUniform1f Exposure;

private:
	FShaderProgram mShader;
};

#endif