#ifndef __GL_SHADOWMAPSHADER_H
#define __GL_SHADOWMAPSHADER_H

#include "hwrenderer/postprocessing/hw_shaderprogram.h"
#include "hwrenderer/postprocessing/hw_postprocess.h"

class FShadowMapShader
{
public:
	void Bind(IRenderQueue *q);

	ShaderUniforms<ShadowMapUniforms, POSTPROCESS_BINDINGPOINT> Uniforms;

private:
	std::unique_ptr<IShaderProgram> mShader;
};

#endif