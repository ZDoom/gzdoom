#ifndef __GL_FRAMEBUFFER
#define __GL_FRAMEBUFFER

#include "gl_sysfb.h"
#include "m_png.h"

#include <memory>

namespace OpenGLRenderer
{

class FHardwareTexture;
class FGLDebug;

class OpenGLFrameBuffer : public SystemGLFrameBuffer
{
	typedef SystemGLFrameBuffer Super;

	void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc) override;

public:

	explicit OpenGLFrameBuffer() {}
	OpenGLFrameBuffer(void *hMonitor, bool fullscreen) ;
	~OpenGLFrameBuffer();

	void InitializeState() override;
	void Update() override;

	void AmbientOccludeScene(float m5) override;
	void FirstEye() override;
	void NextEye(int eyecount) override;
	void SetSceneRenderTarget(bool useSSAO) override;
	void UpdateShadowMap() override;
	void WaitForCommands(bool finish) override;
	void SetSaveBuffers(bool yes) override;
	void CopyScreenToBuffer(int width, int height, uint8_t* buffer) override;
	bool FlipSavePic() const override { return true; }

	FRenderState* RenderState() override;
	void UpdatePalette() override;
	const char* DeviceName() const override;
	void SetTextureFilterMode() override;
	IHardwareTexture *CreateHardwareTexture(int numchannels) override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	void BeginFrame() override;
	void SetViewportRects(IntRect *bounds) override;
	void BlurScene(float amount) override;
	IVertexBuffer *CreateVertexBuffer() override;
	IIndexBuffer *CreateIndexBuffer() override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) override;

	void InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData) override;

	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) override;

	void Swap();
	bool IsHWGammaActive() const { return HWGammaActive; }

	void SetVSync(bool vsync) override;

	void Draw2D() override;
	void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) override;

	bool HWGammaActive = false;			// Are we using hardware or software gamma?
	std::shared_ptr<FGLDebug> mDebug;	// Debug API

    FTexture *WipeStartScreen() override;
    FTexture *WipeEndScreen() override;

	int camtexcount = 0;
};

}

#endif //__GL_FRAMEBUFFER
