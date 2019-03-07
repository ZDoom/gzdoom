#pragma once

#include "gl_sysfb.h"
#include "vk_device.h"
#include "vk_objects.h"

struct FRenderViewpoint;
class VkSamplerManager;
class VkShaderManager;
class VkRenderPassManager;
class VkRenderState;
class VKDataBuffer;
class VkHardwareTexture;
class VkRenderBuffers;
class VkPostprocess;
class SWSceneDrawer;

class VulkanFrameBuffer : public SystemBaseFrameBuffer
{
	typedef SystemBaseFrameBuffer Super;


public:
	VulkanDevice *device;

	VulkanCommandBuffer *GetUploadCommands();
	VulkanCommandBuffer *GetDrawCommands();
	VkShaderManager *GetShaderManager() { return mShaderManager.get(); }
	VkSamplerManager *GetSamplerManager() { return mSamplerManager.get(); }
	VkRenderPassManager *GetRenderPassManager() { return mRenderPassManager.get(); }
	VkRenderState *GetRenderState() { return mRenderState.get(); }
	VkPostprocess *GetPostprocess() { return mPostprocess.get(); }
	VkRenderBuffers *GetBuffers() { return mActiveRenderBuffers; }

	unsigned int GetLightBufferBlockSize() const;

	template<typename T>
	int UniformBufferAlignedSize() const { return (sizeof(T) + uniformblockalignment - 1) / uniformblockalignment * uniformblockalignment; }

	VKDataBuffer *ViewpointUBO = nullptr;
	VKDataBuffer *LightBufferSSO = nullptr;
	VKDataBuffer *MatricesUBO = nullptr;
	VKDataBuffer *StreamUBO = nullptr;

	std::vector<std::unique_ptr<VulkanBuffer>> mFrameDeleteList;

	std::unique_ptr<SWSceneDrawer> swdrawer;

	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();

	void Update();

	void InitializeState() override;

	void CleanForRestart() override;
	void UpdatePalette() override;
	uint32_t GetCaps() override;
	void WriteSavePic(player_t *player, FileWriter *file, int width, int height) override;
	sector_t *RenderView(player_t *player) override;
	void UnbindTexUnit(int no) override;
	void TextureFilterChanged() override;
	void BeginFrame() override;
	void BlurScene(float amount) override;
	void PostProcessScene(int fixedcm, const std::function<void()> &afterBloomDrawEndScene2D) override;

	IHardwareTexture *CreateHardwareTexture() override;
	FModelRenderer *CreateModelRenderer(int mli) override;
	IShaderProgram *CreateShaderProgram() override;
	IVertexBuffer *CreateVertexBuffer() override;
	IIndexBuffer *CreateIndexBuffer() override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo) override;

	/*
	bool WipeStartScreen(int type);
	void WipeEndScreen();
	bool WipeDo(int ticks);
	void WipeCleanup();
	*/

	void SetVSync(bool vsync);

	void Draw2D() override;

private:
	sector_t *RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void DrawScene(HWDrawInfo *di, int drawmode);
	void PrintStartupLog();

	std::unique_ptr<VkShaderManager> mShaderManager;
	std::unique_ptr<VkSamplerManager> mSamplerManager;
	std::unique_ptr<VkRenderBuffers> mScreenBuffers;
	std::unique_ptr<VkRenderBuffers> mSaveBuffers;
	std::unique_ptr<VkPostprocess> mPostprocess;
	std::unique_ptr<VkRenderPassManager> mRenderPassManager;
	std::unique_ptr<VulkanCommandPool> mGraphicsCommandPool;
	std::unique_ptr<VulkanCommandBuffer> mUploadCommands;
	std::unique_ptr<VulkanCommandBuffer> mDrawCommands;
	std::unique_ptr<VulkanSemaphore> mUploadSemaphore;
	std::unique_ptr<VkRenderState> mRenderState;

	VkRenderBuffers *mActiveRenderBuffers = nullptr;

	int camtexcount = 0;

	int lastSwapWidth = 0;
	int lastSwapHeight = 0;
};

inline VulkanFrameBuffer *GetVulkanFrameBuffer() { return static_cast<VulkanFrameBuffer*>(screen); }
