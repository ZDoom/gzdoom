#ifndef __GL_TONEMAPSHADER_H
#define __GL_TONEMAPSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FTonemapShader
{
public:
	void Bind(IRenderQueue *q);

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

	std::unique_ptr<IShaderProgram> mShader[NumTonemapModes];
};

#endif