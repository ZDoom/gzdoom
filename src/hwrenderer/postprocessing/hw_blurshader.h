
#pragma once

#include "hwrenderer/postprocessing/hw_shaderprogram.h"

class FGLRenderer;
class PPFrameBuffer;
class PPTexture;

class FBlurShader
{
public:
	void Bind(IRenderQueue *q, bool vertical);

	struct UniformBlock
	{
		float SampleWeights[8];

		static std::vector<UniformFieldDesc> Desc()
		{
			return
			{
				{ "SampleWeights0", UniformType::Float, offsetof(UniformBlock, SampleWeights[0]) },
				{ "SampleWeights1", UniformType::Float, offsetof(UniformBlock, SampleWeights[1]) },
				{ "SampleWeights2", UniformType::Float, offsetof(UniformBlock, SampleWeights[2]) },
				{ "SampleWeights3", UniformType::Float, offsetof(UniformBlock, SampleWeights[3]) },
				{ "SampleWeights4", UniformType::Float, offsetof(UniformBlock, SampleWeights[4]) },
				{ "SampleWeights5", UniformType::Float, offsetof(UniformBlock, SampleWeights[5]) },
				{ "SampleWeights6", UniformType::Float, offsetof(UniformBlock, SampleWeights[6]) },
				{ "SampleWeights7", UniformType::Float, offsetof(UniformBlock, SampleWeights[7]) },
			};
		}
	};

	ShaderUniforms<UniformBlock, POSTPROCESS_BINDINGPOINT> Uniforms[2];

private:
	std::unique_ptr<IShaderProgram> mShader[2];
};
