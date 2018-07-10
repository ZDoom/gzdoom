#pragma once

#include "vk_device.h"

class VulkanFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;


public:
	VulkanDevice *device;

	explicit VulkanFrameBuffer() {}
	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();

	void InitializeState();
	void Update();

	// Color correction
	void SetGamma();

	void CleanForRestart() override;
	void UpdatePalette() override;
	void InitForLevel() override;
	void SetClearColor(int color) override;
	uint32_t GetCaps() override;
	void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV) override;
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height) override;
	sector_t *RenderView(player_t *player) override;
	void SetTextureFilterMode() override;
	IHardwareTexture *CreateHardwareTexture(FTexture *tex) override;
	FModelRenderer *CreateModelRenderer(int mli) override;
	void UnbindTexUnit(int no) override;
	void TextureFilterChanged() override;
	void BeginFrame() override;
	void BlurScene(float amount) override;
    IUniformBuffer *CreateUniformBuffer(size_t size, bool staticuse = false) override;
	IShaderProgram *CreateShaderProgram() override;


	// Retrieves a buffer containing image data for a screenshot.
	// Hint: Pitch can be negative for upside-down images, in which case buffer
	// points to the last row in the buffer, which will be the first row output.
	virtual void GetScreenshotBuffer(const uint8_t *&buffer, int &pitch, ESSType &color_type, float &gamma) override;

	/*
	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	*/

	void Swap();
	void SetVSync(bool vsync);

	void Draw2D() override;

private:
	int camtexcount = 0;
};
