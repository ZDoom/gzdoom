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
