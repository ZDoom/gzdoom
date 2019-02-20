#pragma once

#include "gl_sysfb.h"
#include "vk_device.h"
#include "vk_objects.h"

class VkSamplerManager;

class VulkanFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;


public:
	VulkanDevice *device;
	VkSamplerManager *mSamplerManager;

	explicit VulkanFrameBuffer() {}
	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();

	void Update();

	void InitializeState() override;

	void CleanForRestart() override;
	void UpdatePalette() override;
	uint32_t GetCaps() override;
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height) override;
	sector_t *RenderView(player_t *player) override;
	FModelRenderer *CreateModelRenderer(int mli) override;
	void UnbindTexUnit(int no) override;
	void TextureFilterChanged() override;
	void BeginFrame() override;
	void BlurScene(float amount) override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo) override;
	IShaderProgram *CreateShaderProgram() override;

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
