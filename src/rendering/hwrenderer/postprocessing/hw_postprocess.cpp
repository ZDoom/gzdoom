
#include "v_video.h"
#include "hw_postprocess.h"
#include "hwrenderer/utility/hw_cvars.h"
#include "hwrenderer/postprocessing/hw_postprocess_cvars.h"
#include "hwrenderer/postprocessing/hw_postprocessshader.h"
#include <random>

Postprocess hw_postprocess;

PPResource *PPResource::First = nullptr;

bool gpuStatActive = false;
bool keepGpuStatActive = false;
FString gpuStatOutput;

ADD_STAT(gpu)
{
	keepGpuStatActive = true;
	return gpuStatOutput;
}

/////////////////////////////////////////////////////////////////////////////

void PPBloom::UpdateTextures(int width, int height)
{
	if (width == lastWidth && height == lastHeight)
		return;

	int bloomWidth = (width + 1) / 2;
	int bloomHeight = (height + 1) / 2;

	for (int i = 0; i < NumBloomLevels; i++)
	{
		auto &blevel = levels[i];
		blevel.Viewport.left = 0;
		blevel.Viewport.top = 0;
		blevel.Viewport.width = (bloomWidth + 1) / 2;
		blevel.Viewport.height = (bloomHeight + 1) / 2;
		blevel.VTexture = { blevel.Viewport.width, blevel.Viewport.height, PixelFormat::Rgba16f };
		blevel.HTexture = { blevel.Viewport.width, blevel.Viewport.height, PixelFormat::Rgba16f };

		bloomWidth = blevel.Viewport.width;
		bloomHeight = blevel.Viewport.height;
	}

	lastWidth = width;
	lastHeight = height;
}

void PPBloom::RenderBloom(PPRenderState *renderstate, int sceneWidth, int sceneHeight, int fixedcm)
{
	// Only bloom things if enabled and no special fixed light mode is active
	if (!gl_bloom || fixedcm != CM_DEFAULT || gl_ssao_debug || sceneWidth <= 0 || sceneHeight <= 0)
	{
		return;
	}

	renderstate->PushGroup("bloom");

	UpdateTextures(sceneWidth, sceneHeight);

	ExtractUniforms extractUniforms;
	extractUniforms.Scale = screen->SceneScale();
	extractUniforms.Offset = screen->SceneOffset();

	auto &level0 = levels[0];

	// Extract blooming pixels from scene texture:
	renderstate->Clear();
	renderstate->Shader = &BloomExtract;
	renderstate->Uniforms.Set(extractUniforms);
	renderstate->Viewport = level0.Viewport;
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);
	renderstate->SetInputTexture(1, &hw_postprocess.exposure.CameraTexture);
	renderstate->SetOutputTexture(&level0.VTexture);
	renderstate->SetNoBlend();
	renderstate->Draw();

	const float blurAmount = gl_bloom_amount;
	BlurUniforms blurUniforms;
	ComputeBlurSamples(7, blurAmount, blurUniforms.SampleWeights);

	// Blur and downscale:
	for (int i = 0; i < NumBloomLevels - 1; i++)
	{
		auto &blevel = levels[i];
		auto &next = levels[i + 1];

		BlurStep(renderstate, blurUniforms, blevel.VTexture, blevel.HTexture, blevel.Viewport, false);
		BlurStep(renderstate, blurUniforms, blevel.HTexture, blevel.VTexture, blevel.Viewport, true);

		// Linear downscale:
		renderstate->Clear();
		renderstate->Shader = &BloomCombine;
		renderstate->Uniforms.Clear();
		renderstate->Viewport = next.Viewport;
		renderstate->SetInputTexture(0, &blevel.VTexture, PPFilterMode::Linear);
		renderstate->SetOutputTexture(&next.VTexture);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	// Blur and upscale:
	for (int i = NumBloomLevels - 1; i > 0; i--)
	{
		auto &blevel = levels[i];
		auto &next = levels[i - 1];

		BlurStep(renderstate, blurUniforms, blevel.VTexture, blevel.HTexture, blevel.Viewport, false);
		BlurStep(renderstate, blurUniforms, blevel.HTexture, blevel.VTexture, blevel.Viewport, true);

		// Linear upscale:
		renderstate->Clear();
		renderstate->Shader = &BloomCombine;
		renderstate->Uniforms.Clear();
		renderstate->Viewport = next.Viewport;
		renderstate->SetInputTexture(0, &blevel.VTexture, PPFilterMode::Linear);
		renderstate->SetOutputTexture(&next.VTexture);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	BlurStep(renderstate, blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false);
	BlurStep(renderstate, blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true);

	// Add bloom back to scene texture:
	renderstate->Clear();
	renderstate->Shader = &BloomCombine;
	renderstate->Uniforms.Clear();
	renderstate->Viewport = screen->mSceneViewport;
	renderstate->SetInputTexture(0, &level0.VTexture, PPFilterMode::Linear);
	renderstate->SetOutputCurrent();
	renderstate->SetAdditiveBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

void PPBloom::RenderBlur(PPRenderState *renderstate, int sceneWidth, int sceneHeight, float gameinfobluramount)
{
	// No scene, no blur!
	if (sceneWidth <= 0 || sceneHeight <= 0)
		return;

	UpdateTextures(sceneWidth, sceneHeight);

	// first, respect the CVar
	float blurAmount = gl_menu_blur;

	// if CVar is negative, use the gameinfo entry
	if (gl_menu_blur < 0)
		blurAmount = gameinfobluramount;

	// if blurAmount == 0 or somehow still returns negative, exit to prevent a crash, clearly we don't want this
	if (blurAmount <= 0.0)
	{
		return;
	}

	renderstate->PushGroup("blur");

	int numLevels = 3;
	assert(numLevels <= NumBloomLevels);

	auto &level0 = levels[0];

	// Grab the area we want to bloom:
	renderstate->Clear();
	renderstate->Shader = &BloomCombine;
	renderstate->Uniforms.Clear();
	renderstate->Viewport = level0.Viewport;
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);
	renderstate->SetOutputTexture(&level0.VTexture);
	renderstate->SetNoBlend();
	renderstate->Draw();

	BlurUniforms blurUniforms;
	ComputeBlurSamples(7, blurAmount, blurUniforms.SampleWeights);

	// Blur and downscale:
	for (int i = 0; i < numLevels - 1; i++)
	{
		auto &blevel = levels[i];
		auto &next = levels[i + 1];

		BlurStep(renderstate, blurUniforms, blevel.VTexture, blevel.HTexture, blevel.Viewport, false);
		BlurStep(renderstate, blurUniforms, blevel.HTexture, blevel.VTexture, blevel.Viewport, true);

		// Linear downscale:
		renderstate->Clear();
		renderstate->Shader = &BloomCombine;
		renderstate->Uniforms.Clear();
		renderstate->Viewport = next.Viewport;
		renderstate->SetInputTexture(0, &blevel.VTexture, PPFilterMode::Linear);
		renderstate->SetOutputTexture(&next.VTexture);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	// Blur and upscale:
	for (int i = numLevels - 1; i > 0; i--)
	{
		auto &blevel = levels[i];
		auto &next = levels[i - 1];

		BlurStep(renderstate, blurUniforms, blevel.VTexture, blevel.HTexture, blevel.Viewport, false);
		BlurStep(renderstate, blurUniforms, blevel.HTexture, blevel.VTexture, blevel.Viewport, true);

		// Linear upscale:
		renderstate->Clear();
		renderstate->Shader = &BloomCombine;
		renderstate->Uniforms.Clear();
		renderstate->Viewport = next.Viewport;
		renderstate->SetInputTexture(0, &blevel.VTexture, PPFilterMode::Linear);
		renderstate->SetOutputTexture(&next.VTexture);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	BlurStep(renderstate, blurUniforms, level0.VTexture, level0.HTexture, level0.Viewport, false);
	BlurStep(renderstate, blurUniforms, level0.HTexture, level0.VTexture, level0.Viewport, true);

	// Copy blur back to scene texture:
	renderstate->Clear();
	renderstate->Shader = &BloomCombine;
	renderstate->Uniforms.Clear();
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetInputTexture(0, &level0.VTexture, PPFilterMode::Linear);
	renderstate->SetOutputCurrent();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

void PPBloom::BlurStep(PPRenderState *renderstate, const BlurUniforms &blurUniforms, PPTexture &input, PPTexture &output, PPViewport viewport, bool vertical)
{
	renderstate->Clear();
	renderstate->Shader = vertical ? &BlurVertical : &BlurHorizontal;
	renderstate->Uniforms.Set(blurUniforms);
	renderstate->Viewport = viewport;
	renderstate->SetInputTexture(0, &input);
	renderstate->SetOutputTexture(&output);
	renderstate->SetNoBlend();
	renderstate->Draw();
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

void PPLensDistort::Render(PPRenderState *renderstate)
{
	if (gl_lens == 0)
	{
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

	renderstate->PushGroup("lens");

	renderstate->Clear();
	renderstate->Shader = &Lens;
	renderstate->Uniforms.Set(uniforms);
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);
	renderstate->SetOutputNext();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////

void PPFXAA::Render(PPRenderState *renderstate)
{
	if (0 == gl_fxaa)
	{
		return;
	}

	CreateShaders();

	FXAAUniforms uniforms;
	uniforms.ReciprocalResolution = { 1.0f / screen->mScreenViewport.width, 1.0f / screen->mScreenViewport.height };

	renderstate->PushGroup("fxaa");

	renderstate->Clear();
	renderstate->Shader = &FXAALuma;
	renderstate->Uniforms.Clear();
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetInputCurrent(0, PPFilterMode::Nearest);
	renderstate->SetOutputNext();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->Shader = &FXAA;
	renderstate->Uniforms.Set(uniforms);
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);
	renderstate->Draw();

	renderstate->PopGroup();
}

int PPFXAA::GetMaxVersion()
{
	return screen->glslversion >= 4.f ? 400 : 330;
}

void PPFXAA::CreateShaders()
{
	if (LastQuality == gl_fxaa)
		return;

	FXAALuma = { "shaders/glsl/fxaa.fp", "#define FXAA_LUMA_PASS\n", {} };
	FXAA = { "shaders/glsl/fxaa.fp", GetDefines(), FXAAUniforms::Desc(), GetMaxVersion() };
	LastQuality = gl_fxaa;
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

void PPCameraExposure::Render(PPRenderState *renderstate, int sceneWidth, int sceneHeight)
{
	if (!gl_bloom)
	{
		return;
	}

	renderstate->PushGroup("exposure");

	UpdateTextures(sceneWidth, sceneHeight);

	ExposureExtractUniforms extractUniforms;
	extractUniforms.Scale = screen->SceneScale();
	extractUniforms.Offset = screen->SceneOffset();

	ExposureCombineUniforms combineUniforms;
	combineUniforms.ExposureBase = gl_exposure_base;
	combineUniforms.ExposureMin = gl_exposure_min;
	combineUniforms.ExposureScale = gl_exposure_scale;
	combineUniforms.ExposureSpeed = gl_exposure_speed;

	auto &level0 = ExposureLevels[0];

	// Extract light blevel from scene texture:
	renderstate->Clear();
	renderstate->Shader = &ExposureExtract;
	renderstate->Uniforms.Set(extractUniforms);
	renderstate->Viewport = level0.Viewport;
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);
	renderstate->SetOutputTexture(&level0.Texture);
	renderstate->SetNoBlend();
	renderstate->Draw();

	// Find the average value:
	for (size_t i = 0; i + 1 < ExposureLevels.size(); i++)
	{
		auto &blevel = ExposureLevels[i];
		auto &next = ExposureLevels[i + 1];

		renderstate->Shader = &ExposureAverage;
		renderstate->Uniforms.Clear();
		renderstate->Viewport = next.Viewport;
		renderstate->SetInputTexture(0, &blevel.Texture, PPFilterMode::Linear);
		renderstate->SetOutputTexture(&next.Texture);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	// Combine average value with current camera exposure:
	renderstate->Shader = &ExposureCombine;
	renderstate->Uniforms.Set(combineUniforms);
	renderstate->Viewport.left = 0;
	renderstate->Viewport.top = 0;
	renderstate->Viewport.width = 1;
	renderstate->Viewport.height = 1;
	renderstate->SetInputTexture(0, &ExposureLevels.back().Texture, PPFilterMode::Linear);
	renderstate->SetOutputTexture(&CameraTexture);
	if (!FirstExposureFrame)
		renderstate->SetAlphaBlend();
	else
		renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();

	FirstExposureFrame = false;
}

void PPCameraExposure::UpdateTextures(int width, int height)
{
	int firstwidth = MAX(width / 2, 1);
	int firstheight = MAX(height / 2, 1);

	if (ExposureLevels.size() > 0 && ExposureLevels[0].Viewport.width == firstwidth && ExposureLevels[0].Viewport.height == firstheight)
	{
		return;
	}

	ExposureLevels.clear();

	int i = 0;
	do
	{
		width = MAX(width / 2, 1);
		height = MAX(height / 2, 1);

		PPExposureLevel blevel;
		blevel.Viewport.left = 0;
		blevel.Viewport.top = 0;
		blevel.Viewport.width = width;
		blevel.Viewport.height = height;
		blevel.Texture = { blevel.Viewport.width, blevel.Viewport.height, PixelFormat::R32f };
		ExposureLevels.push_back(std::move(blevel));

		i++;

	} while (width > 1 || height > 1);

	FirstExposureFrame = true;
}

/////////////////////////////////////////////////////////////////////////////

void PPColormap::Render(PPRenderState *renderstate, int fixedcm)
{
	if (fixedcm < CM_FIRSTSPECIALCOLORMAP || fixedcm >= CM_MAXCOLORMAP)
	{
		return;
	}

	FSpecialColormap *scm = &SpecialColormaps[fixedcm - CM_FIRSTSPECIALCOLORMAP];
	float m[] = { scm->ColorizeEnd[0] - scm->ColorizeStart[0],
		scm->ColorizeEnd[1] - scm->ColorizeStart[1], scm->ColorizeEnd[2] - scm->ColorizeStart[2], 0.f };

	ColormapUniforms uniforms;
	uniforms.MapStart = { scm->ColorizeStart[0], scm->ColorizeStart[1], scm->ColorizeStart[2], 0.f };
	uniforms.MapRange = m;

	renderstate->PushGroup("colormap");

	renderstate->Clear();
	renderstate->Shader = &Colormap;
	renderstate->Uniforms.Set(uniforms);
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetInputCurrent(0);
	renderstate->SetOutputNext();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////

void PPTonemap::UpdateTextures()
{
	if (gl_tonemap == Palette && !PaletteTexture.Data)
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

		PaletteTexture = { 512, 512, PixelFormat::Rgba8, data };
	}
}

void PPTonemap::Render(PPRenderState *renderstate)
{
	if (gl_tonemap == 0)
	{
		return;
	}

	UpdateTextures();

	PPShader *shader = nullptr;
	switch (gl_tonemap)
	{
	default:
	case Linear:		shader = &LinearShader; break;
	case Reinhard:		shader = &ReinhardShader; break;
	case HejlDawson:	shader = &HejlDawsonShader; break;
	case Uncharted2:	shader = &Uncharted2Shader; break;
	case Palette:		shader = &PaletteShader; break;
	}

	renderstate->PushGroup("tonemap");

	renderstate->Clear();
	renderstate->Shader = shader;
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetInputCurrent(0);
	if (gl_tonemap == Palette)
		renderstate->SetInputTexture(1, &PaletteTexture);
	renderstate->SetOutputNext();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////

PPAmbientOcclusion::PPAmbientOcclusion()
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

		AmbientRandomTexture[quality] = { 4, 4, PixelFormat::Rgba16_snorm, data };
	}
}

void PPAmbientOcclusion::CreateShaders()
{
	if (gl_ssao == LastQuality)
		return;

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

	LinearDepth = { "shaders/glsl/lineardepth.fp", "", LinearDepthUniforms::Desc() };
	LinearDepthMS = { "shaders/glsl/lineardepth.fp", "#define MULTISAMPLE\n", LinearDepthUniforms::Desc() };
	AmbientOcclude = { "shaders/glsl/ssao.fp", defines, SSAOUniforms::Desc() };
	AmbientOccludeMS = { "shaders/glsl/ssao.fp", defines + "\n#define MULTISAMPLE\n", SSAOUniforms::Desc() };
	BlurVertical = { "shaders/glsl/depthblur.fp", "#define BLUR_VERTICAL\n", DepthBlurUniforms::Desc() };
	BlurHorizontal = { "shaders/glsl/depthblur.fp", "#define BLUR_HORIZONTAL\n", DepthBlurUniforms::Desc() };
	Combine = { "shaders/glsl/ssaocombine.fp", "", AmbientCombineUniforms::Desc() };
	CombineMS = { "shaders/glsl/ssaocombine.fp", "#define MULTISAMPLE\n", AmbientCombineUniforms::Desc() };

	LastQuality = gl_ssao;
}

void PPAmbientOcclusion::UpdateTextures(int width, int height)
{
	if ((width <= 0 || height <= 0) || (width == LastWidth && height == LastHeight))
		return;

	AmbientWidth = (width + 1) / 2;
	AmbientHeight = (height + 1) / 2;

	LinearDepthTexture = { AmbientWidth, AmbientHeight, PixelFormat::R32f };
	Ambient0 = { AmbientWidth, AmbientHeight, PixelFormat::Rg16f };
	Ambient1 = { AmbientWidth, AmbientHeight, PixelFormat::Rg16f };

	LastWidth = width;
	LastHeight = height;
}

void PPAmbientOcclusion::Render(PPRenderState *renderstate, float m5, int sceneWidth, int sceneHeight)
{
	if (gl_ssao == 0 || sceneWidth == 0 || sceneHeight == 0)
	{
		return;
	}

	CreateShaders();
	UpdateTextures(sceneWidth, sceneHeight);

	float bias = gl_ssao_bias;
	float aoRadius = gl_ssao_radius;
	const float blurAmount = gl_ssao_blur;
	float aoStrength = gl_ssao_strength;

	//float tanHalfFovy = tan(fovy * (M_PI / 360.0f));
	float tanHalfFovy = 1.0f / m5;
	float invFocalLenX = tanHalfFovy * (sceneWidth / (float)sceneHeight);
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
	blurUniforms.PowExponent = gl_ssao_exponent;

	AmbientCombineUniforms combineUniforms;
	combineUniforms.SampleCount = gl_multisample;
	combineUniforms.Scale = screen->SceneScale();
	combineUniforms.Offset = screen->SceneOffset();
	combineUniforms.DebugMode = gl_ssao_debug;

	IntRect ambientViewport;
	ambientViewport.left = 0;
	ambientViewport.top = 0;
	ambientViewport.width = AmbientWidth;
	ambientViewport.height = AmbientHeight;

	renderstate->PushGroup("ssao");

	// Calculate linear depth values
	renderstate->Clear();
	renderstate->Shader = gl_multisample > 1 ? &LinearDepthMS : &LinearDepth;
	renderstate->Uniforms.Set(linearUniforms);
	renderstate->Viewport = ambientViewport;
	renderstate->SetInputSceneDepth(0);
	renderstate->SetInputSceneColor(1);
	renderstate->SetOutputTexture(&LinearDepthTexture);
	renderstate->SetNoBlend();
	renderstate->Draw();

	// Apply ambient occlusion
	renderstate->Clear();
	renderstate->Shader = gl_multisample > 1 ? &AmbientOccludeMS : &AmbientOcclude;
	renderstate->Uniforms.Set(ssaoUniforms);
	renderstate->Viewport = ambientViewport;
	renderstate->SetInputTexture(0, &LinearDepthTexture);
	renderstate->SetInputSceneNormal(1);
	renderstate->SetInputTexture(2, &AmbientRandomTexture[randomTexture], PPFilterMode::Nearest, PPWrapMode::Repeat);
	renderstate->SetOutputTexture(&Ambient0);
	renderstate->SetNoBlend();
	renderstate->Draw();

	// Blur SSAO texture
	if (gl_ssao_debug < 2)
	{
		renderstate->Clear();
		renderstate->Shader = &BlurHorizontal;
		renderstate->Uniforms.Set(blurUniforms);
		renderstate->Viewport = ambientViewport;
		renderstate->SetInputTexture(0, &Ambient0);
		renderstate->SetOutputTexture(&Ambient1);
		renderstate->SetNoBlend();
		renderstate->Draw();

		renderstate->Clear();
		renderstate->Shader = &BlurVertical;
		renderstate->Uniforms.Set(blurUniforms);
		renderstate->Viewport = ambientViewport;
		renderstate->SetInputTexture(0, &Ambient1);
		renderstate->SetOutputTexture(&Ambient0);
		renderstate->SetNoBlend();
		renderstate->Draw();
	}

	// Add SSAO back to scene texture:
	renderstate->Clear();
	renderstate->Shader = gl_multisample > 1 ? &CombineMS : &Combine;
	renderstate->Uniforms.Set(combineUniforms);
	renderstate->Viewport = screen->mSceneViewport;
	if (gl_ssao_debug < 4)
		renderstate->SetInputTexture(0, &Ambient0, PPFilterMode::Linear);
	else
		renderstate->SetInputSceneNormal(0, PPFilterMode::Linear);
	renderstate->SetInputSceneFog(1);
	renderstate->SetOutputSceneColor();
	if (gl_ssao_debug != 0)
		renderstate->SetNoBlend();
	else
		renderstate->SetAlphaBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////

PPPresent::PPPresent()
{
	static const float data[64] =
	{
			.0078125, .2578125, .1328125, .3828125, .0234375, .2734375, .1484375, .3984375,
			.7578125, .5078125, .8828125, .6328125, .7734375, .5234375, .8984375, .6484375,
			.0703125, .3203125, .1953125, .4453125, .0859375, .3359375, .2109375, .4609375,
			.8203125, .5703125, .9453125, .6953125, .8359375, .5859375, .9609375, .7109375,
			.0390625, .2890625, .1640625, .4140625, .0546875, .3046875, .1796875, .4296875,
			.7890625, .5390625, .9140625, .6640625, .8046875, .5546875, .9296875, .6796875,
			.1015625, .3515625, .2265625, .4765625, .1171875, .3671875, .2421875, .4921875,
			.8515625, .6015625, .9765625, .7265625, .8671875, .6171875, .9921875, .7421875,
	};

	std::shared_ptr<void> pixels(new float[64], [](void *p) { delete[](float*)p; });
	memcpy(pixels.get(), data, 64 * sizeof(float));
	Dither = { 8, 8, PixelFormat::R32f, pixels };
}

/////////////////////////////////////////////////////////////////////////////

void PPShadowMap::Update(PPRenderState *renderstate)
{
	ShadowMapUniforms uniforms;
	uniforms.ShadowmapQuality = (float)gl_shadowmap_quality;
	uniforms.NodesCount = screen->mShadowMap.NodesCount();

	renderstate->PushGroup("shadowmap");

	renderstate->Clear();
	renderstate->Shader = &ShadowMap;
	renderstate->Uniforms.Set(uniforms);
	renderstate->Viewport = { 0, 0, gl_shadowmap_quality, 1024 };
	renderstate->SetShadowMapBuffers(true);
	renderstate->SetOutputShadowMap();
	renderstate->SetNoBlend();
	renderstate->Draw();

	renderstate->PopGroup();
}

/////////////////////////////////////////////////////////////////////////////

CVAR(Bool, gl_custompost, true, 0)

void PPCustomShaders::Run(PPRenderState *renderstate, FString target)
{
	if (!gl_custompost)
		return;

	CreateShaders();

	for (auto &shader : mShaders)
	{
		if (shader->Desc->Target == target && shader->Desc->Enabled)
		{
			shader->Run(renderstate);
		}
	}
}

void PPCustomShaders::CreateShaders()
{
	if (mShaders.size() == PostProcessShaders.Size())
		return;

	mShaders.clear();

	for (unsigned int i = 0; i < PostProcessShaders.Size(); i++)
	{
		mShaders.push_back(std::make_unique<PPCustomShaderInstance>(&PostProcessShaders[i]));
	}
}

/////////////////////////////////////////////////////////////////////////////

PPCustomShaderInstance::PPCustomShaderInstance(PostProcessShader *desc) : Desc(desc)
{
	// Build an uniform block to be used as input
	TMap<FString, PostProcessUniformValue>::Iterator it(Desc->Uniforms);
	TMap<FString, PostProcessUniformValue>::Pair *pair;
	size_t offset = 0;
	while (it.NextPair(pair))
	{
		FString type;
		FString name = pair->Key;

		switch (pair->Value.Type)
		{
		case PostProcessUniformType::Float: AddUniformField(offset, name, UniformType::Float, sizeof(float)); break;
		case PostProcessUniformType::Int: AddUniformField(offset, name, UniformType::Int, sizeof(int)); break;
		case PostProcessUniformType::Vec2: AddUniformField(offset, name, UniformType::Vec2, sizeof(float) * 2); break;
		case PostProcessUniformType::Vec3: AddUniformField(offset, name, UniformType::Vec3, sizeof(float) * 3, sizeof(float) * 4); break;
		default: break;
		}
	}
	UniformStructSize = ((int)offset + 15) / 16 * 16;

	// Build the input textures
	FString uniformTextures;
	uniformTextures += "layout(binding=0) uniform sampler2D InputTexture;\n";

	TMap<FString, FString>::Iterator itTextures(Desc->Textures);
	TMap<FString, FString>::Pair *pairTextures;
	int binding = 1;
	while (itTextures.NextPair(pairTextures))
	{
		uniformTextures.AppendFormat("layout(binding=%d) uniform sampler2D %s;\n", binding++, pairTextures->Key.GetChars());
	}

	// Setup pipeline
	FString pipelineInOut;
	if (screen->IsVulkan())
	{
		pipelineInOut += "layout(location=0) in vec2 TexCoord;\n";
		pipelineInOut += "layout(location=0) out vec4 FragColor;\n";
	}
	else 
	{
		pipelineInOut += "in vec2 TexCoord;\n";
		pipelineInOut += "out vec4 FragColor;\n";
	}

	FString prolog;
	prolog += uniformTextures;
	prolog += pipelineInOut;

	Shader = PPShader(Desc->ShaderLumpName, prolog, Fields);
}

void PPCustomShaderInstance::Run(PPRenderState *renderstate)
{
	renderstate->PushGroup(Desc->Name);

	renderstate->Clear();
	renderstate->Shader = &Shader;
	renderstate->Viewport = screen->mScreenViewport;
	renderstate->SetNoBlend();
	renderstate->SetOutputNext();
	//renderstate->SetDebugName(Desc->ShaderLumpName.GetChars());

	SetTextures(renderstate);
	SetUniforms(renderstate);

	renderstate->Draw();

	renderstate->PopGroup();
}

void PPCustomShaderInstance::SetTextures(PPRenderState *renderstate)
{
	renderstate->SetInputCurrent(0, PPFilterMode::Linear);

	int textureIndex = 1;
	TMap<FString, FString>::Iterator it(Desc->Textures);
	TMap<FString, FString>::Pair *pair;
	while (it.NextPair(pair))
	{
		FString name = pair->Value;
		FTexture *tex = TexMan.GetTexture(TexMan.CheckForTexture(name, ETextureType::Any), true);
		if (tex && tex->isValid())
		{
			// Why does this completely circumvent the normal way of handling textures?
			// This absolutely needs fixing because it will also circumvent any potential caching system that may get implemented.
			//
			// To do: fix the above problem by adding PPRenderState::SetInput(FTexture *tex)

			auto &pptex = Textures[tex];
			if (!pptex)
			{
				auto buffer = tex->CreateTexBuffer(0);

				std::shared_ptr<void> data(new uint32_t[buffer.mWidth * buffer.mHeight], [](void *p) { delete[](uint32_t*)p; });

				int count = buffer.mWidth * buffer.mHeight;
				uint8_t *pixels = (uint8_t *)data.get();
				for (int i = 0; i < count; i++)
				{
					int pos = i << 2;
					pixels[pos] = buffer.mBuffer[pos + 2];
					pixels[pos + 1] = buffer.mBuffer[pos + 1];
					pixels[pos + 2] = buffer.mBuffer[pos];
					pixels[pos + 3] = buffer.mBuffer[pos + 3];
				}

				pptex = std::make_unique<PPTexture>(buffer.mWidth, buffer.mHeight, PixelFormat::Rgba8, data);
			}

			renderstate->SetInputTexture(textureIndex, pptex.get(), PPFilterMode::Linear, PPWrapMode::Repeat);
			textureIndex++;
		}
	}
}

void PPCustomShaderInstance::SetUniforms(PPRenderState *renderstate)
{
	TArray<uint8_t> uniforms;
	uniforms.Resize(UniformStructSize);

	TMap<FString, PostProcessUniformValue>::Iterator it(Desc->Uniforms);
	TMap<FString, PostProcessUniformValue>::Pair *pair;
	while (it.NextPair(pair))
	{
		auto it2 = FieldOffset.find(pair->Key);
		if (it2 != FieldOffset.end())
		{
			uint8_t *dst = &uniforms[it2->second];
			float fValues[4];
			int iValues[4];
			switch (pair->Value.Type)
			{
			case PostProcessUniformType::Float:
				fValues[0] = (float)pair->Value.Values[0];
				memcpy(dst, fValues, sizeof(float));
				break;
			case PostProcessUniformType::Int:
				iValues[0] = (int)pair->Value.Values[0];
				memcpy(dst, iValues, sizeof(int));
				break;
			case PostProcessUniformType::Vec2:
				fValues[0] = (float)pair->Value.Values[0];
				fValues[1] = (float)pair->Value.Values[1];
				memcpy(dst, fValues, sizeof(float) * 2);
				break;
			case PostProcessUniformType::Vec3:
				fValues[0] = (float)pair->Value.Values[0];
				fValues[1] = (float)pair->Value.Values[1];
				fValues[2] = (float)pair->Value.Values[2];
				memcpy(dst, fValues, sizeof(float) * 3);
				break;
			default:
				break;
			}
		}
	}

	renderstate->Uniforms.Data = uniforms;
}

void PPCustomShaderInstance::AddUniformField(size_t &offset, const FString &name, UniformType type, size_t fieldsize, size_t alignment)
{
	if (alignment == 0) alignment = fieldsize;
	offset = (offset + alignment - 1) / alignment * alignment;

	FieldOffset[name] = offset;

	auto name2 = std::make_unique<FString>(name);
	auto chars = name2->GetChars();
	FieldNames.push_back(std::move(name2));
	Fields.push_back({ chars, type, offset });
	offset += fieldsize;

	if (fieldsize != alignment) // Workaround for buggy OpenGL drivers that does not do std140 layout correctly for vec3
	{
		name2 = std::make_unique<FString>(name + "_F39350FF12DE_padding");
		chars = name2->GetChars();
		FieldNames.push_back(std::move(name2));
		Fields.push_back({ chars, UniformType::Float, offset });
		offset += alignment - fieldsize;
	}
}
