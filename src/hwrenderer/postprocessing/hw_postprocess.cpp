
#include "v_video.h"
#include "hw_postprocess.h"
#include "hwrenderer/utility/hw_cvars.h"

Postprocess hw_postprocess;

void PPBloom::DeclareShaders()
{
	hw_postprocess.Shaders["BloomCombine"] = { "shaders/glsl/bloomcombine.fp", "", {} };
	hw_postprocess.Shaders["BloomExtract"] = { "shaders/glsl/bloomextract.fp", "", ExtractUniforms::Desc() };
	hw_postprocess.Shaders["BlurVertical"] = { "shaders/glsl/blur.fp", "#define BLUR_VERTICAL\n", BlurUniforms::Desc() };
	hw_postprocess.Shaders["BlurHorizontal"] = { "shaders/glsl/blur.fp", "#define BLUR_HORIZONTAL\n", BlurUniforms::Desc() };
}

void PPBloom::UpdateTextures(int width, int height)
{
	// No scene, no bloom!
	if (width <= 0 || height <= 0)
		return;

	int bloomWidth = (width + 1) / 2;
	int bloomHeight = (height + 1) / 2;

	for (int i = 0; i < NumBloomLevels; i++)
	{
		FString vtexture, htexture;
		vtexture.Format("Bloom.VTexture.%d", i);
		htexture.Format("Bloom.HTexture.%d", i);

		auto &level = levels[i];
		level.Viewport.left = 0;
		level.Viewport.top = 0;
		level.Viewport.width = (bloomWidth + 1) / 2;
		level.Viewport.height = (bloomHeight + 1) / 2;

		PPTextureDesc texture = { level.Viewport.width, level.Viewport.height, PixelFormat::Rgba16f };
		hw_postprocess.Textures[vtexture] = texture;
		hw_postprocess.Textures[htexture] = texture;

		bloomWidth = level.Viewport.width;
		bloomHeight = level.Viewport.height;
	}
}

void PPBloom::UpdateSteps(int fixedcm)
{
	TArray<PPStep> steps;

	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || fixedcm != CM_DEFAULT || gl_ssao_debug)
	{
		hw_postprocess.Effects["BloomScene"] = steps;
		return;
	}

	PPStep step;

	ExtractUniforms extractUniforms;
	extractUniforms.Scale = screen->SceneScale();
	extractUniforms.Offset = screen->SceneOffset();

	auto &level0 = levels[0];

	// Extract blooming pixels from scene texture:
	step.Viewport = level0.Viewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.ShaderName = "BloomExtract";
	step.Uniforms.Set(extractUniforms);
	step.SetOutputTexture(level0.VTexture);
	step.SetDisabledBlend();
	steps.Push(step);

	const float blurAmount = gl_bloom_amount;
	BlurUniforms blurUniforms;
	ComputeBlurSamples(7, blurAmount, blurUniforms.SampleWeights);

	// Blur and downscale:
	for (int i = 0; i < NumBloomLevels - 1; i++)
	{
		auto &level = levels[i];
		auto &next = levels[i + 1];
		steps.Push(BlurStep(blurUniforms, level.VTexture, level.HTexture, level.Viewport, false));
		steps.Push(BlurStep(blurUniforms, level.HTexture, next.VTexture, next.Viewport, true));
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i > 0; i--)
	{
		auto &level = levels[i];
		auto &next = levels[i - 1];

		steps.Push(BlurStep(blurUniforms, level.VTexture, level.HTexture, level.Viewport, false));
		steps.Push(BlurStep(blurUniforms, level.HTexture, level.VTexture, level.Viewport, true));

		// Linear upscale:
		step.SetInputTexture(0, next.VTexture, PPFilterMode::Linear);
		step.SetOutputTexture(next.HTexture);
		step.Viewport = next.Viewport;
		step.ShaderName = "BloomCombine";
		steps.Push(step);
	}

	steps.Push(BlurStep(blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false));
	steps.Push(BlurStep(blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true));

	// Add bloom back to scene texture:
	step.SetInputTexture(0, level0.VTexture, PPFilterMode::Linear);
	step.SetOutputCurrent();
	step.Viewport = screen->mSceneViewport;
	step.ShaderName = "BloomCombine";
	step.SetAdditiveBlend();
	steps.Push(step);

	hw_postprocess.Effects["BloomScene"] = steps;
}

PPStep PPBloom::BlurStep(const BlurUniforms &blurUniforms, PPTextureName input, PPTextureName output, PPViewport viewport, bool vertical)
{
	PPStep step;
	step.Viewport = viewport;
	step.SetInputTexture(0, input);
	step.ShaderName = vertical ? "BlurVertical" : "BlurHorizontal";
	step.Uniforms.Set(blurUniforms);
	step.SetOutputTexture(output);
	step.SetDisabledBlend();
	return step;
}

float PPBloom::ComputeBlurGaussian(float n, float theta) // theta = Blur Amount
{
	return (float)((1.0f / sqrtf(2 * (float)M_PI * theta)) * expf(-(n * n) / (2.0f * theta * theta)));
}

void PPBloom::ComputeBlurSamples(int sampleCount, float blurAmount, float *sampleWeights)
{
	sampleWeights[0] = ComputeBlurGaussian(0, blurAmount);

	float totalWeights = sampleWeights[0];

	for (int i = 0; i < sampleCount / 2; i++)
	{
		float weight = ComputeBlurGaussian(i + 1.0f, blurAmount);

		sampleWeights[i * 2 + 1] = weight;
		sampleWeights[i * 2 + 2] = weight;

		totalWeights += weight * 2;
	}

	for (int i = 0; i < sampleCount; i++)
	{
		sampleWeights[i] /= totalWeights;
	}
}
