#ifndef __GL_BLURSHADER_H
#define __GL_BLURSHADER_H

#include "gl_shaderprogram.h"
#include <memory>

class FGLRenderer;
class PPFrameBuffer;
class PPTexture;

class FBlurShader
{
public:
	void BlurVertical(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height);
	void BlurHorizontal(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height);

private:
	void Blur(FGLRenderer *renderer, float blurAmount, int sampleCount, PPTexture inputTexture, PPFrameBuffer outputFrameBuffer, int width, int height, bool vertical);

	struct BlurSetup
	{
		BlurSetup(float blurAmount, int sampleCount) : blurAmount(blurAmount), sampleCount(sampleCount) { }

		float blurAmount;
		int sampleCount;
		std::shared_ptr<FShaderProgram> VerticalShader;
		std::shared_ptr<FShaderProgram> HorizontalShader;
		FBufferedUniform1f VerticalScaleX, VerticalScaleY;
		FBufferedUniform1f HorizontalScaleX, HorizontalScaleY;
	};

	BlurSetup *GetSetup(float blurAmount, int sampleCount);

	FString VertexShaderCode();
	FString FragmentShaderCode(float blurAmount, int sampleCount, bool vertical);

	float ComputeGaussian(float n, float theta);
	void ComputeBlurSamples(int sampleCount, float blurAmount, TArray<float> &sample_weights, TArray<int> &sample_offsets);

	TArray<BlurSetup> mBlurSetups;
};

#endif