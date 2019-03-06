#ifndef __GL_PRESENTSHADER_H
#define __GL_PRESENTSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

class FPresentShaderBase
{
public:
	virtual ~FPresentShaderBase() {}
	virtual void Bind(IRenderQueue *q) = 0;

	ShaderUniforms<PresentUniforms, POSTPROCESS_BINDINGPOINT> Uniforms;

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
