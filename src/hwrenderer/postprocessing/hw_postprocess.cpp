
#include "v_video.h"
#include "hw_postprocess.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include <random>

Postprocess hw_postprocess;

Postprocess::Postprocess()
{
	Managers.Push(new PPBloom());
	Managers.Push(new PPLensDistort());
	Managers.Push(new PPFXAA());
	Managers.Push(new PPCameraExposure());
	Managers.Push(new PPColormap());
	Managers.Push(new PPTonemap());
	Managers.Push(new PPAmbientOcclusion());
}

Postprocess::~Postprocess()
{
	for (unsigned int i = 0; i < Managers.Size(); i++)
		delete Managers[i];
}

/////////////////////////////////////////////////////////////////////////////

void PPBloom::DeclareShaders()
{
	hw_postprocess.Shaders["BloomCombine"] = { "shaders/glsl/bloomcombine.fp", "", {} };
	hw_postprocess.Shaders["BloomExtract"] = { "shaders/glsl/bloomextract.fp", "", ExtractUniforms::Desc() };
	hw_postprocess.Shaders["BlurVertical"] = { "shaders/glsl/blur.fp", "#define BLUR_VERTICAL\n", BlurUniforms::Desc() };
	hw_postprocess.Shaders["BlurHorizontal"] = { "shaders/glsl/blur.fp", "#define BLUR_HORIZONTAL\n", BlurUniforms::Desc() };
}

void PPBloom::UpdateTextures()
{
	int width = hw_postprocess.SceneWidth;
	int height = hw_postprocess.SceneHeight;

	// No scene, no bloom!
	if (width <= 0 || height <= 0)
		return;

	int bloomWidth = (width + 1) / 2;
	int bloomHeight = (height + 1) / 2;

	for (int i = 0; i < NumBloomLevels; i++)
	{
		auto &level = levels[i];
		level.VTexture.Format("Bloom.VTexture.%d", i);
		level.HTexture.Format("Bloom.HTexture.%d", i);
		level.Viewport.left = 0;
		level.Viewport.top = 0;
		level.Viewport.width = (bloomWidth + 1) / 2;
		level.Viewport.height = (bloomHeight + 1) / 2;

		PPTextureDesc texture = { level.Viewport.width, level.Viewport.height, PixelFormat::Rgba16f };
		hw_postprocess.Textures[level.VTexture] = texture;
		hw_postprocess.Textures[level.HTexture] = texture;

		bloomWidth = level.Viewport.width;
		bloomHeight = level.Viewport.height;
	}
}

void PPBloom::UpdateSteps()
{
	UpdateBlurSteps();

	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || hw_postprocess.fixedcm != CM_DEFAULT || gl_ssao_debug || hw_postprocess.SceneWidth <= 0 || hw_postprocess.SceneHeight <= 0)
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
	step.ShaderName = "BloomExtract";
	step.Uniforms.Set(extractUniforms);
	step.Viewport = level0.Viewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetInputTexture(1, "Exposure.CameraTexture");
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
		steps.Push(BlurStep(blurUniforms, level.HTexture, level.VTexture, level.Viewport, true));

		// Linear downscale:
		step.ShaderName = "BloomCombine";
		step.Uniforms.Clear();
		step.Viewport = next.Viewport;
		step.SetInputTexture(0, level.VTexture, PPFilterMode::Linear);
		step.SetOutputTexture(next.VTexture);
		step.SetNoBlend();
		steps.Push(step);
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i > 0; i--)
	{
		auto &level = levels[i];
		auto &next = levels[i - 1];

		steps.Push(BlurStep(blurUniforms, level.VTexture, level.HTexture, level.Viewport, false));
		steps.Push(BlurStep(blurUniforms, level.HTexture, level.VTexture, level.Viewport, true));

		// Linear upscale:
		step.ShaderName = "BloomCombine";
		step.Uniforms.Clear();
		step.Viewport = next.Viewport;
		step.SetInputTexture(0, level.VTexture, PPFilterMode::Linear);
		step.SetOutputTexture(next.VTexture);
		step.SetNoBlend();
		steps.Push(step);
	}

	steps.Push(BlurStep(blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false));
	steps.Push(BlurStep(blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true));

	// Add bloom back to scene texture:
	step.ShaderName = "BloomCombine";
	step.Uniforms.Clear();
	step.Viewport = screen->mSceneViewport;
	step.SetInputTexture(0, level0.VTexture, PPFilterMode::Linear);
	step.SetOutputCurrent();
	step.SetAdditiveBlend();
	steps.Push(step);

	hw_postprocess.Effects["BloomScene"] = steps;
}

void PPBloom::UpdateBlurSteps()
{
	// first, respect the CVar
	float blurAmount = gl_menu_blur;

	// if CVar is negative, use the gameinfo entry
	if (gl_menu_blur < 0)
		blurAmount = hw_postprocess.gameinfobluramount;

	// if blurAmount == 0 or somehow still returns negative, exit to prevent a crash, clearly we don't want this
	if (blurAmount <= 0.0)
	{
		hw_postprocess.Effects["BlurScene"] = {};
		return;
	}

	TArray<PPStep> steps;
	PPStep step;

	int numLevels = 3;
	assert(numLevels <= NumBloomLevels);

	const auto &level0 = levels[0];

	// Grab the area we want to bloom:
	step.ShaderName = "BloomCombine";
	step.Uniforms.Clear();
	step.Viewport = level0.Viewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetOutputTexture(level0.VTexture);
	step.SetNoBlend();
	steps.Push(step);

	BlurUniforms blurUniforms;
	ComputeBlurSamples(7, blurAmount, blurUniforms.SampleWeights);

	// Blur and downscale:
	for (int i = 0; i < numLevels - 1; i++)
	{
		auto &level = levels[i];
		auto &next = levels[i + 1];

		steps.Push(BlurStep(blurUniforms, level.VTexture, level.HTexture, level.Viewport, false));
		steps.Push(BlurStep(blurUniforms, level.HTexture, level.VTexture, level.Viewport, true));

		// Linear downscale:
		step.ShaderName = "BloomCombine";
		step.Uniforms.Clear();
		step.Viewport = next.Viewport;
		step.SetInputTexture(0, level.VTexture, PPFilterMode::Linear);
		step.SetOutputTexture(next.VTexture);
		step.SetNoBlend();
		steps.Push(step);
	}

	// Blur and upscale:
	for (int i = numLevels - 1; i > 0; i--)
	{
		auto &level = levels[i];
		auto &next = levels[i - 1];

		steps.Push(BlurStep(blurUniforms, level.VTexture, level.HTexture, level.Viewport, false));
		steps.Push(BlurStep(blurUniforms, level.HTexture, level.VTexture, level.Viewport, true));

		// Linear upscale:
		step.ShaderName = "BloomCombine";
		step.Uniforms.Clear();
		step.Viewport = next.Viewport;
		step.SetInputTexture(0, level.VTexture, PPFilterMode::Linear);
		step.SetOutputTexture(next.VTexture);
		step.SetNoBlend();
		steps.Push(step);
	}

	steps.Push(BlurStep(blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false));
	steps.Push(BlurStep(blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true));

	// Copy blur back to scene texture:
	step.ShaderName = "BloomCombine";
	step.Uniforms.Clear();
	step.Viewport = screen->mScreenViewport;
	step.SetInputTexture(0, level0.VTexture, PPFilterMode::Linear);
	step.SetOutputCurrent();
	step.SetNoBlend();
	steps.Push(step);

	hw_postprocess.Effects["BlurScene"] = steps;
}

PPStep PPBloom::BlurStep(const BlurUniforms &blurUniforms, PPTextureName input, PPTextureName output, PPViewport viewport, bool vertical)
{
	PPStep step;
	step.ShaderName = vertical ? "BlurVertical" : "BlurHorizontal";
	step.Uniforms.Set(blurUniforms);
	step.Viewport = viewport;
	step.SetInputTexture(0, input);
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
	step.ShaderName = "Lens";
	step.Uniforms.Set(uniforms);
	step.Viewport = screen->mScreenViewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetOutputNext();
	step.SetNoBlend();
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
	step.ShaderName = "FXAALuma";
	step.Uniforms.Clear();
	step.Viewport = screen->mScreenViewport;
	step.SetInputCurrent(0, PPFilterMode::Nearest);
	step.SetOutputNext();
	step.SetNoBlend();
	steps.Push(step);

	step.ShaderName = "FXAA";
	step.Uniforms.Set(uniforms);
	step.Viewport = screen->mScreenViewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetOutputNext();
	step.SetNoBlend();
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

/////////////////////////////////////////////////////////////////////////////

void PPCameraExposure::DeclareShaders()
{
	hw_postprocess.Shaders["ExposureExtract"] = { "shaders/glsl/exposureextract.fp", "", ExposureExtractUniforms::Desc() };
	hw_postprocess.Shaders["ExposureAverage"] = { "shaders/glsl/exposureaverage.fp", "", {}, 400 };
	hw_postprocess.Shaders["ExposureCombine"] = { "shaders/glsl/exposurecombine.fp", "", ExposureCombineUniforms::Desc() };
}

void PPCameraExposure::UpdateTextures()
{
	int width = hw_postprocess.SceneWidth;
	int height = hw_postprocess.SceneHeight;

	if (ExposureLevels.Size() > 0 && ExposureLevels[0].Viewport.width == width && ExposureLevels[0].Viewport.height == height)
	{
		return;
	}

	ExposureLevels.Clear();

	int i = 0;
	do
	{
		width = MAX(width / 2, 1);
		height = MAX(height / 2, 1);

		PPExposureLevel level;
		level.Viewport.left = 0;
		level.Viewport.top = 0;
		level.Viewport.width = width;
		level.Viewport.height = height;
		level.Texture.Format("Exposure.Level.%d", i);
		ExposureLevels.Push(level);

		PPTextureDesc texture = { level.Viewport.width, level.Viewport.height, PixelFormat::R32f };
		hw_postprocess.Textures[level.Texture] = texture;

		i++;

	} while (width > 1 || height > 1);

	hw_postprocess.Textures["Exposure.CameraTexture"] = { 1, 1, PixelFormat::R32f };

	FirstExposureFrame = true;
}

void PPCameraExposure::UpdateSteps()
{
	if (!gl_bloom)
	{
		hw_postprocess.Effects["UpdateCameraExposure"] = {};
		return;
	}

	TArray<PPStep> steps;
	PPStep step;

	ExposureExtractUniforms extractUniforms;
	extractUniforms.Scale = screen->SceneScale();
	extractUniforms.Offset = screen->SceneOffset();

	ExposureCombineUniforms combineUniforms;
	combineUniforms.ExposureBase = gl_exposure_base;
	combineUniforms.ExposureMin = gl_exposure_min;
	combineUniforms.ExposureScale = gl_exposure_scale;
	combineUniforms.ExposureSpeed = gl_exposure_speed;

	auto &level0 = ExposureLevels[0];

	// Extract light level from scene texture:
	step.ShaderName = "ExposureExtract";
	step.Uniforms.Set(extractUniforms);
	step.Viewport = level0.Viewport;
	step.SetInputCurrent(0, PPFilterMode::Linear);
	step.SetOutputTexture(level0.Texture);
	step.SetNoBlend();
	steps.Push(step);

	// Find the average value:
	for (unsigned int i = 0; i + 1 < ExposureLevels.Size(); i++)
	{
		auto &level = ExposureLevels[i];
		auto &next = ExposureLevels[i + 1];

		step.ShaderName = "ExposureAverage";
		step.Uniforms.Clear();
		step.Viewport = next.Viewport;
		step.SetInputTexture(0, level.Texture, PPFilterMode::Linear);
		step.SetOutputTexture(next.Texture);
		step.SetNoBlend();
		steps.Push(step);
	}

	// Combine average value with current camera exposure:
	step.ShaderName = "ExposureCombine";
	step.Uniforms.Set(combineUniforms);
	step.Viewport.left = 0;
	step.Viewport.top = 0;
	step.Viewport.width = 1;
	step.Viewport.height = 1;
	step.SetInputTexture(0, ExposureLevels.Last().Texture, PPFilterMode::Linear);
	step.SetOutputTexture("Exposure.CameraTexture");
	if (!FirstExposureFrame)
		step.SetAlphaBlend();
	else
		step.SetNoBlend();
	steps.Push(step);

	FirstExposureFrame = false;

	hw_postprocess.Effects["UpdateCameraExposure"] = steps;
}

/////////////////////////////////////////////////////////////////////////////

void PPColormap::DeclareShaders()
{
	hw_postprocess.Shaders["Colormap"] = { "shaders/glsl/colormap.fp", "", ColormapUniforms::Desc() };
}

void PPColormap::UpdateSteps()
{
	int fixedcm = hw_postprocess.fixedcm;

	if (fixedcm < CM_FIRSTSPECIALCOLORMAP || fixedcm >= CM_MAXCOLORMAP)
	{
		hw_postprocess.Effects["ColormapScene"] = {};
		return;
	}

	FSpecialColormap *scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];
	float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
		scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

	ColormapUniforms uniforms;
	uniforms.MapStart = { scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], 0.f };
	uniforms.MapRange = m;

	PPStep step;
	step.ShaderName = "Colormap";
	step.Uniforms.Set(uniforms);
	step.Viewport = screen->mScreenViewport;
	step.SetInputCurrent(0);
	step.SetOutputNext();
	step.SetNoBlend();

	TArray<PPStep> steps;
	steps.Push(step);
	hw_postprocess.Effects["ColormapScene"] = steps;
}

/////////////////////////////////////////////////////////////////////////////

void PPTonemap::DeclareShaders()
{
	hw_postprocess.Shaders["Tonemap.Linear"] = { "shaders/glsl/tonemap.fp", "#define LINEAR\n", {} };
	hw_postprocess.Shaders["Tonemap.Reinhard"] = { "shaders/glsl/tonemap.fp", "#define REINHARD\n", {} };
	hw_postprocess.Shaders["Tonemap.HejlDawson"] = { "shaders/glsl/tonemap.fp", "#define HEJLDAWSON\n", {} };
	hw_postprocess.Shaders["Tonemap.Uncharted2"] = { "shaders/glsl/tonemap.fp", "#define UNCHARTED2\n", {} };
	hw_postprocess.Shaders["Tonemap.Palette"] = { "shaders/glsl/tonemap.fp", "#define PALETTE\n", {} };
}

void PPTonemap::UpdateTextures()
{
	if (gl_tonemap == Palette)
	{
		if (!hw_postprocess.Textures.CheckKey("Tonemap.Palette"))
		{
			std::shared_ptr<void> data(new uint32_t[512 * 512], [](void *p) { delete[](uint32_t*)p; });

			uint8_t *lut = (uint8_t *)data.get();
			for (int r = 0; r < 64; r++)
			{
				for (int g = 0; g < 64; g++)
				{
					for (int b = 0; b < 64; b++)
					{
						PalEntry color = GPalette.BaseColors[(uint8_t)PTM_BestColor((uint32_t *)GPalette.BaseColors, (r << 2) | (r >> 4), (g << 2) | (g >> 4), (b << 2) | (b >> 4),
							gl_paltonemap_reverselookup, gl_paltonemap_powtable, 0, 256)];
						int index = ((r * 64 + g) * 64 + b) * 4;
						lut[index] = color.r;
						lut[index + 1] = color.g;
						lut[index + 2] = color.b;
						lut[index + 3] = 255;
					}
				}
			}

			hw_postprocess.Textures["Tonemap.Palette"] = { 512, 512, PixelFormat::Rgba8, data };
		}
	}
}

void PPTonemap::UpdateSteps()
{
	if (gl_tonemap == 0)
	{
		hw_postprocess.Effects["TonemapScene"] = {};
		return;
	}

	PPShaderName shader;
	switch (gl_tonemap)
	{
	default:
	case Linear:		shader = "Tonemap.Linear"; break;
	case Reinhard:		shader = "Tonemap.Reinhard"; break;
	case HejlDawson:	shader = "Tonemap.HejlDawson"; break;
	case Uncharted2:	shader = "Tonemap.Uncharted2"; break;
	case Palette:		shader = "Tonemap.Palette"; break;
	}

	PPStep step;
	step.ShaderName = shader;
	step.Viewport = screen->mScreenViewport;
	step.SetInputCurrent(0);
	if (gl_tonemap == Palette)
		step.SetInputTexture(1, "Tonemap.Palette");
	step.SetOutputNext();
	step.SetNoBlend();

	TArray<PPStep> steps;
	steps.Push(step);
	hw_postprocess.Effects["TonemapScene"] = steps;
}


/////////////////////////////////////////////////////////////////////////////

void PPAmbientOcclusion::DeclareShaders()
{
	// Must match quality values in PPAmbientOcclusion::UpdateTextures
	int numDirections, numSteps;
	switch (gl_ssao)
	{
	default:
	case LowQuality:    numDirections = 2; numSteps = 4; break;
	case MediumQuality: numDirections = 4; numSteps = 4; break;
	case HighQuality:   numDirections = 8; numSteps = 4; break;
	}

	FString defines;
	defines.Format(R"(
		#define USE_RANDOM_TEXTURE
		#define RANDOM_TEXTURE_WIDTH 4.0
		#define NUM_DIRECTIONS %d.0
		#define NUM_STEPS %d.0
	)", numDirections, numSteps);

	hw_postprocess.Shaders["SSAO.LinearDepth"] = { "shaders/glsl/lineardepth.fp", "", LinearDepthUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.LinearDepthMS"] = { "shaders/glsl/lineardepth.fp", "#define MULTISAMPLE\n", LinearDepthUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.AmbientOcclude"] = { "shaders/glsl/ssao.fp", defines, SSAOUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.AmbientOccludeMS"] = { "shaders/glsl/ssao.fp", defines + "\n#define MULTISAMPLE\n", SSAOUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.BlurVertical"] = { "shaders/glsl/depthblur.fp", "#define BLUR_VERTICAL\n", DepthBlurUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.BlurHorizontal"] = { "shaders/glsl/depthblur.fp", "#define BLUR_HORIZONTAL\n", DepthBlurUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.Combine"] = { "shaders/glsl/ssaocombine.fp", "", AmbientCombineUniforms::Desc() };
	hw_postprocess.Shaders["SSAO.CombineMS"] = { "shaders/glsl/ssaocombine.fp", "#define MULTISAMPLE\n", AmbientCombineUniforms::Desc() };
}

void PPAmbientOcclusion::UpdateTextures()
{
	int width = hw_postprocess.SceneWidth;
	int height = hw_postprocess.SceneHeight;

	if (width <= 0 || height <= 0)
		return;

	AmbientWidth = (width + 1) / 2;
	AmbientHeight = (height + 1) / 2;

	hw_postprocess.Textures["SSAO.LinearDepth"] = { AmbientWidth, AmbientHeight, PixelFormat::R32f };
	hw_postprocess.Textures["SSAO.Ambient0"] = { AmbientWidth, AmbientHeight, PixelFormat::Rg16f };
	hw_postprocess.Textures["SSAO.Ambient1"] = { AmbientWidth, AmbientHeight, PixelFormat::Rg16f };

	// We only need to create the random texture once
	if (!hw_postprocess.Textures.CheckKey("SSAO.Random0"))
	{
		// Must match quality enum in PPAmbientOcclusion::DeclareShaders
		double numDirections[NumAmbientRandomTextures] = { 2.0, 4.0, 8.0 };

		std::mt19937 generator(1337);
		std::uniform_real_distribution<double> distribution(0.0, 1.0);
		for (int quality = 0; quality < NumAmbientRandomTextures; quality++)
		{
			std::shared_ptr<void> data(new int16_t[16 * 4], [](void *p) { delete[](int16_t*)p; });
			int16_t *randomValues = (int16_t *)data.get();

			for (int i = 0; i < 16; i++)
			{
				double angle = 2.0 * M_PI * distribution(generator) / numDirections[quality];
				double x = cos(angle);
				double y = sin(angle);
				double z = distribution(generator);
				double w = distribution(generator);

				randomValues[i * 4 + 0] = (int16_t)clamp(x * 32767.0, -32768.0, 32767.0);
				randomValues[i * 4 + 1] = (int16_t)clamp(y * 32767.0, -32768.0, 32767.0);
				randomValues[i * 4 + 2] = (int16_t)clamp(z * 32767.0, -32768.0, 32767.0);
				randomValues[i * 4 + 3] = (int16_t)clamp(w * 32767.0, -32768.0, 32767.0);
			}

			FString name;
			name.Format("SSAO.Random%d", quality);

			hw_postprocess.Textures[name] = { 4, 4, PixelFormat::Rgba16_snorm, data };
			AmbientRandomTexture[quality] = name;
		}
	}
}

void PPAmbientOcclusion::UpdateSteps()
{
	if (gl_ssao == 0 || hw_postprocess.SceneWidth == 0 || hw_postprocess.SceneHeight == 0)
	{
		hw_postprocess.Effects["AmbientOccludeScene"] = {};
		return;
	}

	float bias = gl_ssao_bias;
	float aoRadius = gl_ssao_radius;
	const float blurAmount = gl_ssao_blur;
	float aoStrength = gl_ssao_strength;

	//float tanHalfFovy = tan(fovy * (M_PI / 360.0f));
	float tanHalfFovy = 1.0f / hw_postprocess.m5;
	float invFocalLenX = tanHalfFovy * (hw_postprocess.SceneWidth / (float)hw_postprocess.SceneHeight);
	float invFocalLenY = tanHalfFovy;
	float nDotVBias = clamp(bias, 0.0f, 1.0f);
	float r2 = aoRadius * aoRadius;

	float blurSharpness = 1.0f / blurAmount;

	auto sceneScale = screen->SceneScale();
	auto sceneOffset = screen->SceneOffset();

	int randomTexture = clamp(gl_ssao - 1, 0, NumAmbientRandomTextures - 1);

	LinearDepthUniforms linearUniforms;
	linearUniforms.SampleIndex = 0;
	linearUniforms.LinearizeDepthA = 1.0f / screen->GetZFar() - 1.0f / screen->GetZNear();
	linearUniforms.LinearizeDepthB = MAX(1.0f / screen->GetZNear(), 1.e-8f);
	linearUniforms.InverseDepthRangeA = 1.0f;
	linearUniforms.InverseDepthRangeB = 0.0f;
	linearUniforms.Scale = sceneScale;
	linearUniforms.Offset = sceneOffset;

	SSAOUniforms ssaoUniforms;
	ssaoUniforms.SampleIndex = 0;
	ssaoUniforms.UVToViewA = { 2.0f * invFocalLenX, 2.0f * invFocalLenY };
	ssaoUniforms.UVToViewB = { -invFocalLenX, -invFocalLenY };
	ssaoUniforms.InvFullResolution = { 1.0f / AmbientWidth, 1.0f / AmbientHeight };
	ssaoUniforms.NDotVBias = nDotVBias;
	ssaoUniforms.NegInvR2 = -1.0f / r2;
	ssaoUniforms.RadiusToScreen = aoRadius * 0.5f / tanHalfFovy * AmbientHeight;
	ssaoUniforms.AOMultiplier = 1.0f / (1.0f - nDotVBias);
	ssaoUniforms.AOStrength = aoStrength;
	ssaoUniforms.Scale = sceneScale;
	ssaoUniforms.Offset = sceneOffset;

	DepthBlurUniforms blurUniforms;
	blurUniforms.BlurSharpness = blurSharpness;
	blurUniforms.InvFullResolution = { 1.0f / AmbientWidth, 1.0f / AmbientHeight };
	blurUniforms.PowExponent = gl_ssao_exponent;

	AmbientCombineUniforms combineUniforms;
	combineUniforms.SampleCount = gl_multisample;
	combineUniforms.Scale = screen->SceneScale();
	combineUniforms.Offset = screen->SceneOffset();

	IntRect ambientViewport;
	ambientViewport.left = 0;
	ambientViewport.top = 0;
	ambientViewport.width = AmbientWidth;
	ambientViewport.height = AmbientHeight;

	TArray<PPStep> steps;

	// Calculate linear depth values
	{
		PPStep step;
		step.ShaderName = gl_multisample > 1 ? "SSAO.LinearDepthMS" : "SSAO.LinearDepth";
		step.Uniforms.Set(linearUniforms);
		step.Viewport = ambientViewport;
		step.SetInputSceneDepth(0);
		step.SetInputSceneColor(1);
		step.SetOutputTexture("SSAO.LinearDepth");
		step.SetNoBlend();
		steps.Push(step);
	}

	// Apply ambient occlusion
	{
		PPStep step;
		step.ShaderName = gl_multisample > 1 ? "SSAO.AmbientOccludeMS" : "SSAO.AmbientOcclude";
		step.Uniforms.Set(ssaoUniforms);
		step.Viewport = ambientViewport;
		step.SetInputTexture(0, "SSAO.LinearDepth");
		step.SetInputSceneNormal(1);
		step.SetInputTexture(2, AmbientRandomTexture[randomTexture], PPFilterMode::Nearest, PPWrapMode::Repeat);
		step.SetOutputTexture("SSAO.Ambient0");
		step.SetNoBlend();
		steps.Push(step);
	}

	// Blur SSAO texture
	if (gl_ssao_debug < 2)
	{
		PPStep step;
		step.ShaderName = "SSAO.BlurHorizontal";
		step.Uniforms.Set(blurUniforms);
		step.Viewport = ambientViewport;
		step.SetInputTexture(0, "SSAO.Ambient0");
		step.SetOutputTexture("SSAO.Ambient1");
		step.SetNoBlend();
		steps.Push(step);

		step.ShaderName = "SSAO.BlurVertical";
		step.SetInputTexture(0, "SSAO.Ambient1");
		step.SetOutputTexture("SSAO.Ambient0");
		steps.Push(step);
	}

	// Add SSAO back to scene texture:
	{
		PPStep step;
		step.ShaderName = gl_multisample > 1 ? "SSAO.CombineMS" : "SSAO.Combine";
		step.Uniforms.Set(combineUniforms);
		step.Viewport = screen->mSceneViewport;
		step.SetInputTexture(0, "SSAO.Ambient0", PPFilterMode::Linear);
		step.SetInputSceneFog(1);
		step.SetOutputSceneColor();
		if (gl_ssao_debug != 0)
			step.SetNoBlend();
		else
			step.SetAlphaBlend();
		steps.Push(step);
	}

	hw_postprocess.Effects["AmbientOccludeScene"] = steps;
}
