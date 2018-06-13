#ifndef __GL_LENSSHADER_H
#define __GL_LENSSHADER_H

#include "gl_shaderprogram.h"

class FLensShader
{
public:
	void Bind();

	struct UniformBlock
	{
		float AspectRatio;
		float Scale;
		float Padding0, Padding1;
		FVector4 LensDistortionCoefficient;
		FVector4 CubicDistortionValue;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "Aspect", UniformType::Float, offsetof(UniformBlock, AspectRatio) },
				{ "Scale", UniformType::Float, offsetof(UniformBlock, Scale) },
				{ "Padding0", UniformType::Float, offsetof(UniformBlock, Padding0) },
				{ "Padding1", UniformType::Float, offsetof(UniformBlock, Padding1) },
				{ "k", UniformType::Vec4, offsetof(UniformBlock, LensDistortionCoefficient) },
				{ "kcube", UniformType::Vec4, offsetof(UniformBlock, CubicDistortionValue) }
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	FShaderProgram mShader;
};

#endif