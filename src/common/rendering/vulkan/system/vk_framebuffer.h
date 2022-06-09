#pragma once

#include "gl_sysfb.h"
#include "vk_device.h"
#include "vk_objects.h"

struct FRenderViewpoint;
class VkSamplerManager;
class VkShaderManager;
class VkCommandBufferManager;
class VkDescriptorSetManager;
class VkRenderPassManager;
class VkRaytrace;
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

	VkCommandBufferManager* GetCommands() { return mCommands.get(); }
	VkShaderManager *GetShaderManager() { return mShaderManager.get(); }
	VkSamplerManager *GetSamplerManager() { return mSamplerManager.get(); }
	VkDescriptorSetManager* GetDescriptorSetManager() { return mDescriptorSetManager.get(); }
	VkRenderPassManager *GetRenderPassManager() { return mRenderPassManager.get(); }
	VkRaytrace* GetRaytrace() { return mRaytrace.get(); }
	VkRenderState *GetRenderState() { return mRenderState.get(); }
	VkPostprocess *GetPostprocess() { return mPostprocess.get(); }
	VkRenderBuffers *GetBuffers() { return mActiveRenderBuffers; }
	FRenderState* RenderState() override;

	unsigned int GetLightBufferBlockSize() const;

	VKDataBuffer *ViewpointUBO = nullptr;
	VKDataBuffer *LightBufferSSO = nullptr;
	VkStreamBuffer *MatrixBuffer = nullptr;
	VkStreamBuffer *StreamBuffer = nullptr;

	VKDataBuffer *LightNodes = nullptr;
	VKDataBuffer *LightLines = nullptr;
	VKDataBuffer *LightList = nullptr;

	std::unique_ptr<IIndexBuffer> FanToTrisIndexBuffer;

	VulkanFrameBuffer(void *hMonitor, bool fullscreen, VulkanDevice *dev);
	~VulkanFrameBuffer();
	bool IsVulkan() override { return true; }

	void Update() override;

	void InitializeState() override;
	bool CompileNextShader() override;
	void PrecacheMaterial(FMaterial *mat, int translation) override;
	void UpdatePalette() override;
	const char* DeviceName() const override;
	int Backend() override { return 1; }
	void SetTextureFilterMode() override;
	void StartPrecaching() override;
	void BeginFrame() override;
	void InitLightmap(int LMTextureSize, int LMTextureCount, TArray<uint16_t>& LMTextureData) override;
	void BlurScene(float amount) override;
	void PostProcessScene(bool swscene, int fixedcm, float flash, const std::function<void()> &afterBloomDrawEndScene2D) override;
	void AmbientOccludeScene(float m5) override;
	void SetSceneRenderTarget(bool useSSAO) override;
	void SetLevelMesh(hwrenderer::LevelMesh* mesh) override;
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

	bool GetVSync() { return cur_vsync; }
	void SetVSync(bool vsync) override;

	void Draw2D() override;

	void WaitForCommands(bool finish) override;

private:
	void RenderTextureView(FCanvasTexture* tex, std::function<void(IntRect &)> renderFunc) override;
	void PrintStartupLog();
	void CreateFanToTrisIndexBuffer();
	void CopyScreenToBuffer(int w, int h, uint8_t *data) override;

	std::unique_ptr<VkCommandBufferManager> mCommands;
	std::unique_ptr<VkShaderManager> mShaderManager;
	std::unique_ptr<VkSamplerManager> mSamplerManager;
	std::unique_ptr<VkRenderBuffers> mScreenBuffers;
	std::unique_ptr<VkRenderBuffers> mSaveBuffers;
	std::unique_ptr<VkPostprocess> mPostprocess;
	std::unique_ptr<VkDescriptorSetManager> mDescriptorSetManager;
	std::unique_ptr<VkRenderPassManager> mRenderPassManager;
	std::unique_ptr<VkRaytrace> mRaytrace;
	std::unique_ptr<VkRenderState> mRenderState;

	VkRenderBuffers *mActiveRenderBuffers = nullptr;

	bool cur_vsync = false;
};

inline VulkanFrameBuffer *GetVulkanFrameBuffer() { return static_cast<VulkanFrameBuffer*>(screen); }
