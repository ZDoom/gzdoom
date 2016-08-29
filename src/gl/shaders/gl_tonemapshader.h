#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "gl_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;
	FBufferedUniform1f Exposure;
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

#endif