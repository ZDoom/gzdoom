
#include "v_video.h"
#include "hw_postprocess.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "gl_load/gl_load.h" // for GL_RGBA16F - should we create our own enum instead?

Postprocess hw_postprocess;

void PPBloom::DeclareShaders()
{
	PPShader shader;
	shader.VertexShader = "shaders/glsl/screenquad.vp";
	shader.FragmentShader = "shaders/glsl/bloomcombine.fp";
	hw_postprocess.Shaders["BloomCombine"] = shader;

	shader.Uniforms = ExtractUniforms::Desc();
	shader.FragmentShader = "shaders/glsl/bloomextract.fp";
	hw_postprocess.Shaders["BloomExtract"] = shader;

	shader.Uniforms = BlurUniforms::Desc();
	shader.FragmentShader = "shaders/glsl/blur.fp";
	shader.Defines = "#define BLUR_VERTICAL\n";
	hw_postprocess.Shaders["BlurVertical"] = shader;

	shader.Defines = "#define BLUR_HORIZONTAL\n";
	hw_postprocess.Shaders["BlurHorizontal"] = shader;
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

		PPTextureDesc texture;
		texture.Width = level.Viewport.width;
		texture.Height = level.Viewport.height;
		texture.Format = GL_RGBA16F;

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
	step.Textures.Resize(1);

	ExtractUniforms extractUniforms;
	extractUniforms.Scale = screen->SceneScale();
	extractUniforms.Offset = screen->SceneOffset();

	auto &level0 = levels[0];

	// Extract blooming pixels from scene texture:
	step.Viewport = level0.Viewport;
	step.Textures[0].Type = PPTextureType::CurrentPipelineTexture;
	step.Textures[0].Filter = PPFilterMode::Linear;
	step.ShaderName = "BloomExtract";
	step.Uniforms.Set(extractUniforms);
	step.Output.Type = PPTextureType::PPTexture;
	step.Output.Texture = level0.VTexture;
	step.BlendMode.BlendOp = STYLEOP_Add;
	step.BlendMode.SrcAlpha = STYLEALPHA_One;
	step.BlendMode.DestAlpha = STYLEALPHA_Zero;
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
		step.Textures[0].Type = PPTextureType::PPTexture;
		step.Textures[0].Filter = PPFilterMode::Linear;
		step.Textures[0].Texture = next.VTexture;
		step.Output.Texture = next.HTexture;
		step.Viewport = next.Viewport;
		step.ShaderName = "BloomCombine";
		steps.Push(step);
	}

	steps.Push(BlurStep(blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false));
	steps.Push(BlurStep(blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true));

	// Add bloom back to scene texture:
	step.Textures[0].Type = PPTextureType::PPTexture;
	step.Textures[0].Filter = PPFilterMode::Linear;
	step.Textures[0].Texture = level0.VTexture;
	step.Output.Type = PPTextureType::CurrentPipelineTexture;
	step.Viewport = screen->mSceneViewport;
	step.ShaderName = "BloomCombine";
	step.BlendMode.BlendOp = STYLEOP_Add;
	step.BlendMode.SrcAlpha = STYLEALPHA_One;
	step.BlendMode.DestAlpha = STYLEALPHA_One;
	step.BlendMode.Flags = 0;
	steps.Push(step);

	hw_postprocess.Effects["BloomScene"] = steps;
}

PPStep PPBloom::BlurStep(const BlurUniforms &blurUniforms, PPTextureName input, PPTextureName output, PPViewport viewport, bool vertical)
{
	PPStep step;
	step.Textures.Resize(1);
	step.Viewport = viewport;
	step.Textures[0].Type = PPTextureType::PPTexture;
	step.Textures[0].Filter = PPFilterMode::Nearest;
	step.Textures[0].Texture = input;
	step.ShaderName = vertical ? "BlurVertical" : "BlurHorizontal";
	step.Uniforms.Set(blurUniforms);
	step.Output.Type = PPTextureType::PPTexture;
	step.Output.Texture = output;
	step.BlendMode.BlendOp = STYLEOP_Add;
	step.BlendMode.SrcAlpha = STYLEALPHA_One;
	step.BlendMode.DestAlpha = STYLEALPHA_Zero;
	step.BlendMode.Flags = 0;
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
