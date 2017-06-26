#ifndef __GL_PRESENTSHADER_H
#define __GL_PRESENTSHADER_H

#include "gl_shaderprogram.h"

class FPresentShaderBase
{
public:
	virtual ~FPresentShaderBase() {}
	virtual void Bind() = 0;

	FBufferedUniform1f InvGamma;
	FBufferedUniform1f Contrast;
	FBufferedUniform1f Brightness;
	FBufferedUniform1f Saturation;
	FBufferedUniform1i GrayFormula;
	FBufferedUniform2f Scale;

protected:
	virtual void Init(const char * vtx_shader_name, const char * program_name);
	FShaderProgram mShader;
};

class FPresentShader : public FPresentShaderBase
{
public:
	void Bind() override;

	FBufferedUniformSampler InputTexture;
};

#endif
