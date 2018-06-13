#ifndef __GL_BLOOMSHADER_H
#define __GL_BLOOMSHADER_H

#include "gl_shaderprogram.h"

class FBloomExtractShader
{
public:
	void Bind(IRenderQueue *q);

	struct UniformBlock
	{
		FVector2 Scale;
		FVector2 Offset;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "Scale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
				{ "Offset", UniformType::Vec2, offsetof(UniformBlock, Offset) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	FShaderProgram mShader;
};

class FBloomCombineShader
{
public:
	void Bind(IRenderQueue *q);

private:
	FShaderProgram mShader;
};

#endif