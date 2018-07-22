#ifndef __GL_PRESENTSHADER_H
#define __GL_PRESENTSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FPresentShaderBase
{
public:
	virtual ~FPresentShaderBase() {}
	virtual void Bind(IRenderQueue *q) = 0;

	struct UniformBlock
	{
		float InvGamma;
		float Contrast;
		float Brightness;
		float Saturation;
		int GrayFormula;
		int WindowPositionParity; // top-of-window might not be top-of-screen
		FVector2 Scale;

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "InvGamma", UniformType::Float, offsetof(UniformBlock, InvGamma) },
				{ "Contrast", UniformType::Float, offsetof(UniformBlock, Contrast) },
				{ "Brightness", UniformType::Float, offsetof(UniformBlock, Brightness) },
				{ "Saturation", UniformType::Float, offsetof(UniformBlock, Saturation) },
				{ "GrayFormula", UniformType::Int, offsetof(UniformBlock, GrayFormula) },
				{ "WindowPositionParity", UniformType::Int, offsetof(UniformBlock, WindowPositionParity) },
				{ "UVScale", UniformType::Vec2, offsetof(UniformBlock, Scale) },
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms;

protected:
	virtual void Init(const char * vtx_shader_name, const char * program_name);
	std::unique_ptr<IShaderProgram> mShader;
};

class FPresentShader : public FPresentShaderBase
{
public:
	void Bind(IRenderQueue *q) override;

};

#endif
