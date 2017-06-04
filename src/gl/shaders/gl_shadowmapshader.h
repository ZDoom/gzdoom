#ifndef __GL_SHADOWMAPSHADER_H
#define __GL_SHADOWMAPSHADER_H

#include "gl_shaderprogram.h"

class FShadowMapShader
{
public:
	void Bind();

	FBufferedUniform1f ShadowmapQuality;

private:
	FShaderProgram mShader;
};

#endif