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
	std::unique_ptr<VulkanSwapChain> swapChain;
	uint32_t presentImageIndex = 0xffffffff;

	VulkanCommandBuffer *GetTransferCommands();
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

	VKDataBuffer *LightNodes = nullptr;
	VKDataBuffer *LightLines = nullptr;
	VKDataBuffer *LightList = nullptr;

	std::unique_ptr<IIndexBuffer> FanToTrisIndexBuffer;

	class DeleteList
	{
	public:
		std::vector<std::unique_ptr<VulkanImage>> Images;
		std::vector<std::unique_ptr<VulkanImageView>> ImageViews;
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> Descriptors;
	} FrameDeleteList;

	std::unique_ptr<SWSceneDrawer> swdrawer;

	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();

	void Update();

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
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo) override;

	FTexture *WipeStartScreen() override;
	FTexture *WipeEndScreen() override;

	TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) override;

	void SetVSync(bool vsync);

	void Draw2D() override;

private:
	sector_t *RenderViewpoint(FRenderViewpoint &mainvp, AActor * camera, IntRect * bounds, float fov, float ratio, float fovratio, bool mainview, bool toscreen);
	void RenderTextureView(FCanvasTexture *tex, AActor *Viewpoint, double FOV);
	void DrawScene(HWDrawInfo *di, int drawmode);
	void PrintStartupLog();
	void CreateFanToTrisIndexBuffer();
	void SubmitCommands(bool finish);
	void CopyScreenToBuffer(int w, int h, void *data);
	void UpdateShadowMap();
	void DeleteFrameObjects();

	std::unique_ptr<VkShaderManager> mShaderManager;
	std::unique_ptr<VkSamplerManager> mSamplerManager;
	std::unique_ptr<VkRenderBuffers> mScreenBuffers;
	std::unique_ptr<VkRenderBuffers> mSaveBuffers;
	std::unique_ptr<VkPostprocess> mPostprocess;
	std::unique_ptr<VkRenderPassManager> mRenderPassManager;
	std::unique_ptr<VulkanCommandPool> mCommandPool;
	std::unique_ptr<VulkanCommandBuffer> mTransferCommands;
	std::unique_ptr<VulkanCommandBuffer> mDrawCommands;
	std::unique_ptr<VulkanSemaphore> mTransferSemaphore;
	std::unique_ptr<VkRenderState> mRenderState;

	std::unique_ptr<VulkanSemaphore> mSwapChainImageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> mRenderFinishedSemaphore;
	std::unique_ptr<VulkanFence> mRenderFinishedFence;

	VkRenderBuffers *mActiveRenderBuffers = nullptr;
};

inline VulkanFrameBuffer *GetVulkanFrameBuffer() { return static_cast<VulkanFrameBuffer*>(screen); }
