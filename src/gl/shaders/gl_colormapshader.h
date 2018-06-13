#ifndef __GL_COLORMAPSHADER_H
#define __GL_COLORMAPSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FColormapShader
{
public:
	void Bind(IRenderQueue *q);

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

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:

	std::unique_ptr<IShaderProgram> mShader;
};

#endif