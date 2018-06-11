#ifndef __GL_COLORMAPSHADER_H
#define __GL_COLORMAPSHADER_H

#include "gl_shaderprogram.h"

class FColormapShader
{
public:
	void Bind();

	FBufferedUniformSampler SceneTexture;

	struct UniformBlock
	{
		FVector4 MapStart;
		FVector4 MapRange;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "uFixedColormapStart", UniformType::Vec4, offsetof(UniformBlock, MapStart) },
				{ "uFixedColormapRange", UniformType::Vec4, offsetof(UniformBlock, MapRange) },
			};
		}
	};

	ShaderUniforms<UniformBlock> Uniforms;

private:

	FShaderProgram mShader;
};

#endif