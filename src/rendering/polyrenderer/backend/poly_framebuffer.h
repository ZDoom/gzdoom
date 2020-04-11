#pragma once

#include "gl_sysfb.h"
#include "rendering/swrenderer/r_memory.h"
#include "rendering/swrenderer/drawers/r_thread.h"
#include "rendering/polyrenderer/drawers/poly_triangle.h"

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

	std::unique_ptr<SWSceneDrawer> swdrawer;

	PolyFrameBuffer(void *hMonitor, bool fullscreen);
	~PolyFrameBuffer();

	void Update();

	bool IsPoly() override { return true; }

	void InitializeState() override;

	void CleanForRestart() override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	void UpdatePalette() override;
	uint32_t GetCaps() override;
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height) override;
	sector_t *RenderView(player_t *player) override;
	void SetTextureFilterMode() override;
	void TextureFilterChanged() override;
	void StartPrecaching() override;
	void BeginFrame() override;
	void BlurScene(float amount) override;
	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D) override;

	IHardwareTexture *CreateHardwareTexture() override;
	FModelRenderer *CreateModelRenderer(int mli) override;
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
	sector_t *RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV);
	void DrawScene(HWDrawInfo *di, int drawmode);
	void UpdateShadowMap();

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
