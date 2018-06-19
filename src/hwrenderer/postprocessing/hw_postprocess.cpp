
#include "v_video.h"
#include "hw_postprocess.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"

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
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || fixedcm != CM_DEFAULT || gl_ssao_debug)
	{
		hw_postprocess.Effects["BloomScene"] = {};
		return;
	}

	TArray<PPStep> steps;
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
	step.SetNoBlend();
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
	step.SetNoBlend();
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

/////////////////////////////////////////////////////////////////////////////

void PPLensDistort::DeclareShaders()
{
	hw_postprocess.Shaders["Lens"] = { "shaders/glsl/lensdistortion.fp", "", LensUniforms::Desc() };
}

void PPLensDistort::UpdateSteps()
{
	if (gl_lens == 0)
	{
		hw_postprocess.Effects["LensDistortScene"] = {};
		return;
	}

	float k[4] =
	{
		gl_lens_k,
		gl_lens_k * gl_lens_chromatic,
		gl_lens_k * gl_lens_chromatic * gl_lens_chromatic,
		0.0f
	};
	float kcube[4] =
	{
		gl_lens_kcube,
		gl_lens_kcube * gl_lens_chromatic,
		gl_lens_kcube * gl_lens_chromatic * gl_lens_chromatic,
		0.0f
	};

	float aspect = screen->mSceneViewport.width / (float)screen->mSceneViewport.height;

	// Scale factor to keep sampling within the input texture
	float r2 = aspect * aspect * 0.25f + 0.25f;
	float sqrt_r2 = sqrt(r2);
	float f0 = 1.0f + MAX(r2 * (k[0] + kcube[0] * sqrt_r2), 0.0f);
	float f2 = 1.0f + MAX(r2 * (k[2] + kcube[2] * sqrt_r2), 0.0f);
	float f = MAX(f0, f2);
	float scale = 1.0f / f;

	LensUniforms uniforms;
	uniforms.AspectRatio = aspect;
	uniforms.Scale = scale;
	uniforms.LensDistortionCoefficient = k;
	uniforms.CubicDistortionValue = kcube;

	TArray<PPStep> steps;

	PPStep step;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetOutputNext();
	step.SetNoBlend();
	step.Uniforms.Set(uniforms);
	step.Viewport = screen->mScreenViewport;
	step.ShaderName = "Lens";
	steps.Push(step);

	hw_postprocess.Effects["LensDistortScene"] = steps;
}

/////////////////////////////////////////////////////////////////////////////

void PPFXAA::DeclareShaders()
{
	hw_postprocess.Shaders["FXAALuma"] = { "shaders/glsl/fxaa.fp", "#define FXAA_LUMA_PASS\n", {} };
	hw_postprocess.Shaders["FXAA"] = { "shaders/glsl/fxaa.fp", GetDefines(), FXAAUniforms::Desc(), GetMaxVersion() };
}

void PPFXAA::UpdateSteps()
{
	if (0 == gl_fxaa)
	{
		hw_postprocess.Effects["ApplyFXAA"] = {};
		return;
	}

	FXAAUniforms uniforms;
	uniforms.ReciprocalResolution = { 1.0f / screen->mScreenViewport.width, 1.0f / screen->mScreenViewport.height };

	TArray<PPStep> steps;

	PPStep step;
	step.SetInputCurrent(0, PPFilterMode::Nearest);
	step.SetOutputNext();
	step.SetNoBlend();
	step.Viewport = screen->mScreenViewport;
	step.ShaderName = "FXAALuma";
	steps.Push(step);

	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.Uniforms.Set(uniforms);
	step.ShaderName = "FXAA";
	steps.Push(step);

	hw_postprocess.Effects["ApplyFXAA"] = steps;
}

int PPFXAA::GetMaxVersion()
{
	return screen->glslversion >= 4.f ? 400 : 330;
}

FString PPFXAA::GetDefines()
{
	int quality;

	switch (gl_fxaa)
	{
	default:
	case IFXAAShader::Low:     quality = 10; break;
	case IFXAAShader::Medium:  quality = 12; break;
	case IFXAAShader::High:    quality = 29; break;
	case IFXAAShader::Extreme: quality = 39; break;
	}

	const int gatherAlpha = GetMaxVersion() >= 400 ? 1 : 0;

	// TODO: enable FXAA_GATHER4_ALPHA on OpenGL earlier than 4.0
	// when GL_ARB_gpu_shader5/GL_NV_gpu_shader5 extensions are supported

	FString result;
	result.Format(
		"#define FXAA_QUALITY__PRESET %i\n"
		"#define FXAA_GATHER4_ALPHA %i\n",
		quality, gatherAlpha);

	return result;
}
