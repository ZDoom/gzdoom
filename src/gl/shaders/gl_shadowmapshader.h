#ifndef __GL_SHADOWMAPSHADER_H
#define __GL_SHADOWMAPSHADER_H

#include "gl_shaderprogram.h"

class FShadowMapShader
{
public:
	void Bind();

	struct UniformBlock
	{
		float ShadowmapQuality;
		float Padding0, Padding1, Padding2;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "ShadowmapQuality", UniformType::Float, offsetof(UniformBlock, ShadowmapQuality) },
				{ "Padding0", UniformType::Float, offsetof(UniformBlock, Padding0) },
				{ "Padding1", UniformType::Float, offsetof(UniformBlock, Padding1) },
				{ "Padding2", UniformType::Float, offsetof(UniformBlock, Padding2) },
			};
		}
	};

	ShaderUniforms<UniformBlock> Uniforms;

private:
	FShaderProgram mShader;
};

#endif