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
	enum TonemapMode
	{
		None,
		Uncharted2,
		HejlDawson,
		Reinhard,
		Linear,
		NumTonemapModes
	};

	static const char *GetDefines(int mode);

	FShaderProgram mShader[NumTonemapModes];
};

#endif