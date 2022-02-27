#pragma once

#include "gl_sysfb.h"
#include "vk_device.h"
#include "vk_objects.h"

struct FRenderViewpoint;
class VkSamplerManager;
class VkShaderManager;
class VkRenderPassManager;
class VkRenderState;
class VkStreamBuffer;
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
	bool cur_vsync;

	VulkanCommandBuffer *GetTransferCommands();
	VulkanCommandBuffer *GetDrawCommands();
	VkShaderManager *GetShaderManager() { return mShaderManager.get(); }
	VkSamplerManager *GetSamplerManager() { return mSamplerManager.get(); }
	VkRenderPassManager *GetRenderPassManager() { return mRenderPassManager.get(); }
	VkRenderState *GetRenderState() { return mRenderState.get(); }
	VkPostprocess *GetPostprocess() { return mPostprocess.get(); }
	VkRenderBuffers *GetBuffers() { return mActiveRenderBuffers; }
	FRenderState* RenderState() override;

	void FlushCommands(bool finish, bool lastsubmit = false, bool uploadOnly = false);

	unsigned int GetLightBufferBlockSize() const;

	VKDataBuffer *ViewpointUBO = nullptr;
	VKDataBuffer *LightBufferSSO = nullptr;
	VkStreamBuffer *MatrixBuffer = nullptr;
	VkStreamBuffer *StreamBuffer = nullptr;

	VKDataBuffer *LightNodes = nullptr;
	VKDataBuffer *LightLines = nullptr;
	VKDataBuffer *LightList = nullptr;

	std::unique_ptr<IIndexBuffer> FanToTrisIndexBuffer;

	class DeleteList
	{
	public:
		std::vector<std::unique_ptr<VulkanImage>> Images;
		std::vector<std::unique_ptr<VulkanImageView>> ImageViews;
		std::vector<std::unique_ptr<VulkanFramebuffer>> Framebuffers;
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		std::vector<std::unique_ptr<VulkanDescriptorSet>> Descriptors;
		std::vector<std::unique_ptr<VulkanDescriptorPool>> DescriptorPools;
		std::vector<std::unique_ptr<VulkanCommandBuffer>> CommandBuffers;
	} FrameDeleteList;

	struct
	{
		std::vector<std::unique_ptr<VulkanBuffer>> Buffers;
		size_t TotalSize = 0;
	} FrameTextureUpload;

	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();
	bool IsVulkan() override { return true; }

	void Update() override;

	void InitializeState() override;

	void PrecacheMaterial(FMaterial *mat, int translation) override;
	void UpdatePalette() override;
	const char* DeviceName() const override;
	int Backend() override { return 1; }
	void SetTextureFilterMode() override;
	void StartPrecaching() override;
	void BeginFrame() override;
	void InitLightmap(FLevelLocals* Level) override;
	void BlurScene(float amount) override;
	void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) override;
	void AmbientOccludeScene(float m5) override;
	void SetSceneRenderTarget(bool useSSAO) override;
	void UpdateShadowMap() override;
	void SetSaveBuffers(bool yes) override;
	void ImageTransitionScene(bool unknown) override;
	void SetActiveRenderTarget() override;

	IHardwareTexture *CreateHardwareTexture(int numchannels) override;
	FMaterial* CreateMaterial(FGameTexture* tex, int scaleflags) override;
	IVertexBuffer *CreateVertexBuffer() override;
	IIndexBuffer *CreateIndexBuffer() override;
	IDataBuffer *CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize) override;

	FTexture *WipeStartScreen() override;
	FTexture *WipeEndScreen() override;

	TArray<uint8_t> GetScreenshotBuffer(int &pitch, ESSType &color_type, float &gamma) override;

	void SetVSync(bool vsync) override;

	void Draw2D() override;

	void WaitForCommands(bool finish) override { WaitForCommands(finish, false); }
	void WaitForCommands(bool finish, bool uploadOnly);

	void PushGroup(const FString &name);
	void PopGroup();
	void UpdateGpuStats();
	IntRect SetupTextureView(FCanvasTexture* tex);
	void FinishTextureView(FCanvasTexture* tex);

private:
	void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc) override;
	void PrintStartupLog();
	void CreateFanToTrisIndexBuffer();
	void CopyScreenToBuffer(int w, int h, uint8_t *data) override;
	void DeleteFrameObjects(bool uploadOnly = false);
	void FlushCommands(VulkanCommandBuffer **commands, size_t count, bool finish, bool lastsubmit);

	std::unique_ptr<VkShaderManager> mShaderManager;
	std::unique_ptr<VkSamplerManager> mSamplerManager;
	std::unique_ptr<VkRenderBuffers> mScreenBuffers;
	std::unique_ptr<VkRenderBuffers> mSaveBuffers;
	std::unique_ptr<VkPostprocess> mPostprocess;
	std::unique_ptr<VkRenderPassManager> mRenderPassManager;
	std::unique_ptr<VulkanCommandPool> mCommandPool;
	std::unique_ptr<VulkanCommandBuffer> mTransferCommands;
	std::unique_ptr<VkRenderState> mRenderState;

	std::unique_ptr<VulkanCommandBuffer> mDrawCommands;

	enum { maxConcurrentSubmitCount = 8};
	std::unique_ptr<VulkanSemaphore> mSubmitSemaphore[maxConcurrentSubmitCount];
	std::unique_ptr<VulkanFence> mSubmitFence[maxConcurrentSubmitCount];
	VkFence mSubmitWaitFences[maxConcurrentSubmitCount];
	int mNextSubmit = 0;

	std::unique_ptr<VulkanSemaphore> mSwapChainImageAvailableSemaphore;
	std::unique_ptr<VulkanSemaphore> mRenderFinishedSemaphore;

	VkRenderBuffers *mActiveRenderBuffers = nullptr;

	struct TimestampQuery
	{
		FString name;
		uint32_t startIndex;
		uint32_t endIndex;
	};

	enum { MaxTimestampQueries = 100 };
	std::unique_ptr<VulkanQueryPool> mTimestampQueryPool;
	int mNextTimestampQuery = 0;
	std::vector<size_t> mGroupStack;
	std::vector<TimestampQuery> timeElapsedQueries;
};

inline VulkanFrameBuffer *GetVulkanFrameBuffer() { return static_cast<VulkanFrameBuffer*>(screen); }
