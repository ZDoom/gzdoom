#pragma once

#include "gl_sysfb.h"
#include "r_memory.h"
#include "r_thread.h"
#include "poly_triangle.h"

struct FRenderViewpoint;
class PolyDataBuffer;
class PolyRenderState;
class SWSceneDrawer;

class PolyFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;

public:
	RenderMemory *GetFrameMemory() { return &mFrameMemory; }
	PolyRenderState *GetRenderState() { return mRenderState.get(); }
	DCanvas *GetCanvas() override { return mCanvas.get(); }
	PolyDepthStencil *GetDepthStencil() { return mDepthStencil.get(); }
	PolyCommandBuffer *GetDrawCommands();
	void FlushDrawCommands();

	unsigned int GetLightBufferBlockSize() const;

	PolyFrameBuffer(void *hMonitor, bool fullscreen);
	~PolyFrameBuffer();
	int GetShaderCount() override { return 0; }

	void Update() override;

	bool IsPoly() override { return true; }

	void InitializeState() override;

	FRenderState* RenderState() override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	void UpdatePalette() override;
	void SetTextureFilterMode() override;
	void BeginFrame() override;
	void BlurScene(float amount) override;
	void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) override;
	void AmbientOccludeScene(float m5) override;
	//void SetSceneRenderTarget(bool useSSAO) override;

	IHardwareTexture *CreateHardwareTexture(int numchannels) override;
	IVertexBuffer *CreateVertexBuffer() override;
	IIndexBuffer *CreateIndexBuffer() override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) override;

	FTexture *WipeStartScreen() override;
	FTexture *WipeEndScreen() override;

	TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) override;

	void SetVSync(bool vsync) override;
	void Draw2D() override;

	struct DeleteList
	{
		std::vector<std::vector<uint32_t>> Buffers;
		std::vector<std::unique_ptr<DCanvas>> Images;
	} FrameDeleteList;

private:
	void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc) override;
	void UpdateShadowMap() override;

	void CheckCanvas();

	IDataBuffer *mLightBuffer = nullptr;

	std::unique_ptr<PolyRenderState> mRenderState;
	std::unique_ptr<DCanvas> mCanvas;
	std::unique_ptr<PolyDepthStencil> mDepthStencil;
	std::unique_ptr<PolyCommandBuffer> mDrawCommands;
	RenderMemory mFrameMemory;

	struct ScreenQuadVertex
	{
		float x, y, z;
		float u, v;
		PalEntry color0;

		ScreenQuadVertex() = default;
		ScreenQuadVertex(float x, float y, float u, float v) : x(x), y(y), z(1.0f), u(u), v(v), color0(0xffffffff) { }
	};

	struct ScreenQuad
	{
		IVertexBuffer* VertexBuffer = nullptr;
		IIndexBuffer* IndexBuffer = nullptr;
	} mScreenQuad;

	bool cur_vsync = false;
};

inline PolyFrameBuffer *GetPolyFrameBuffer() { return static_cast<PolyFrameBuffer*>(screen); }

// [GEC] Original code of dpJudas, I add the formulas of gamma, brightness, contrast and saturation.
class CopyAndApplyGammaCommand : public DrawerCommand
{
public:
	CopyAndApplyGammaCommand(void* dest, int destpitch, const void* src, int width, int height, int srcpitch, 
		float gamma, float contrast, float brightness, float saturation) : dest(dest), src(src), destpitch(destpitch), width(width), height(height), srcpitch(srcpitch),
		gamma(gamma), contrast(contrast), brightness(brightness), saturation(saturation)
	{
	}

	void Execute(DrawerThread* thread)
	{
		float Saturation = clamp<float>(saturation, -15.0f, 15.f);

		std::vector<uint8_t> gammatablebuf(256);
		uint8_t* gammatable = gammatablebuf.data();
		InitGammaTable(gammatable);

		int w = width;
		int start = thread->skipped_by_thread(0);
		int count = thread->count_for_thread(0, height);
		int sstep = thread->num_cores * srcpitch;
		int dstep = thread->num_cores * destpitch;
		uint32_t* d = (uint32_t*)dest + start * destpitch;
		const uint32_t* s = (const uint32_t*)src + start * srcpitch;
		for (int y = 0; y < count; y++)
		{
			for (int x = 0; x < w; x++)
			{
				uint32_t red = RPART(s[x]);
				uint32_t green = GPART(s[x]);
				uint32_t blue = BPART(s[x]);
				uint32_t alpha = APART(s[x]);

				if (saturation != 1.0f)
				{
					float NewR = (float)(red / 255.f);
					float NewG = (float)(green / 255.f);
					float NewB = (float)(blue / 255.f);

					// Apply Saturation
					// float luma = dot(In, float3(0.2126729, 0.7151522, 0.0721750));
					// Out =  luma.xxx + Saturation.xxx * (In - luma.xxx);
					//float luma = (NewR * 0.2126729f) + (NewG * 0.7151522f) + (NewB * 0.0721750f); // Rec. 709
					float luma = (NewR * 0.299f) + (NewG * 0.587f) + (NewB * 0.114f); //Rec. 601
					NewR = luma + (Saturation * (NewR - luma));
					NewG = luma + (Saturation * (NewG - luma));
					NewB = luma + (Saturation * (NewB - luma));

					// Clamp All
					NewR = clamp<float>(NewR, 0.0f, 1.f);
					NewG = clamp<float>(NewG, 0.0f, 1.f);
					NewB = clamp<float>(NewB, 0.0f, 1.f);

					red = (uint32_t)(NewR * 255.f);
					green = (uint32_t)(NewG * 255.f);
					blue = (uint32_t)(NewB * 255.f);
				}

				// Apply Contrast / Brightness / Gamma
				red = gammatable[red];
				green = gammatable[green];
				blue = gammatable[blue];

				d[x] = MAKEARGB(alpha, (uint8_t)red, (uint8_t)green, (uint8_t)blue);
			}
			d += dstep;
			s += sstep;
		}
	}

private:
	void InitGammaTable(uint8_t *gammatable)
	{
		float InvGamma = 1.0f / clamp<float>(gamma, 0.1f, 4.f);
		float Brightness = clamp<float>(brightness, -0.8f, 0.8f);
		float Contrast = clamp<float>(contrast, 0.1f, 3.f);

		for (int x = 0; x < 256; x++)
		{
			float ramp = (float)(x / 255.f);

			// Apply Contrast
			// vec4 finalColor = vec4((((originalColor.rgb - vec3(0.5)) * Contrast) + vec3(0.5)), 1.0);
			if (contrast != 1.0f)
				ramp = (((ramp - 0.5f) * Contrast) + 0.5f);

			// Apply Brightness
			// vec4 finalColor = vec4(originalColor.rgb + Brightness, 1.0);
			if (brightness != 0.0f)
				ramp += (Brightness / 2.0f);

			// Apply Gamma
			// FragColor.rgb = pow(fragColor.rgb, vec3(1.0/gamma));
			if (gamma != 1.0f)
				ramp = pow(ramp, InvGamma);

			// Clamp ramp
			ramp = clamp<float>(ramp, 0.0f, 1.f);

			gammatable[x] = (uint8_t)(ramp * 255);
		}
	}

	void* dest;
	const void* src;
	int destpitch;
	int width;
	int height;
	int srcpitch;
	float gamma;
	float contrast;
	float brightness;
	float saturation;
};
