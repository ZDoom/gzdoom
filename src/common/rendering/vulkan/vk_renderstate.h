
#pragma once

#include "vulkan/buffers/vk_hwbuffer.h"
#include "vulkan/buffers/vk_rsbuffers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/pipelines/vk_renderpass.h"

#include "name.h"

#include "hw_renderstate.h"
#include "hw_material.h"

class VulkanRenderDevice;
class VkThreadRenderPassSetup;
class VkTextureImage;

class VkRenderState : public FRenderState
{
public:
	VkRenderState(VulkanRenderDevice* fb, int threadIndex);
	virtual ~VkRenderState() = default;

	// Draw commands
	void ClearScreen() override;
	void Draw(int dt, int index, int count, bool apply = true) override;
	void DrawIndexed(int dt, int index, int count, bool apply = true) override;

	// Immediate render state change commands. These only change infrequently and should not clutter the render state.
	bool SetDepthClamp(bool on) override;
	void SetDepthMask(bool on) override;
	void SetDepthFunc(int func) override;
	void SetDepthRange(float min, float max) override;
	void SetColorMask(bool r, bool g, bool b, bool a) override;
	void SetStencil(int offs, int op, int flags = -1) override;
	void SetCulling(int mode) override;
	void Clear(int targets) override;
	void EnableStencil(bool on) override;
	void SetScissor(int x, int y, int w, int h) override;
	void SetViewport(int x, int y, int w, int h) override;
	void EnableDepthTest(bool on) override;
	void EnableLineSmooth(bool on) override;
	void EnableDrawBuffers(int count, bool apply) override;

	// Buffers
	int SetViewpoint(const HWViewpointUniforms& vp) override;
	void SetViewpoint(int index) override;
	void SetModelMatrix(const VSMatrix& matrix, const VSMatrix& normalMatrix) override;
	void SetTextureMatrix(const VSMatrix& matrix) override;
	int UploadLights(const FDynLightData& lightdata) override;
	int UploadBones(const TArray<VSMatrix>& bones) override;

	// Vertices
	std::pair<FFlatVertex*, unsigned int> AllocVertices(unsigned int count) override;
	void SetShadowData(const TArray<FFlatVertex>& vertices, const TArray<uint32_t>& indexes) override;
	void UpdateShadowData(unsigned int index, const FFlatVertex* vertices, unsigned int count) override;
	void ResetVertices() override;

	// Worker threads
	void FlushCommands() override { EndRenderPass(); }

	void BeginFrame();
	void SetRenderTarget(VkTextureImage* image, VulkanImageView* depthStencilView, int width, int height, VkFormat Format, VkSampleCountFlagBits samples);
	void EndRenderPass();
	void EndFrame();

protected:
	void Apply(int dt);
	void ApplyRenderPass(int dt);
	void ApplyStencilRef();
	void ApplyDepthBias();
	void ApplyScissor();
	void ApplyViewport();
	void ApplyStreamData();
	void ApplyMatrices();
	void ApplyPushConstants();
	void ApplyBufferSets();
	void ApplyVertexBuffers();
	void ApplyMaterial();

	void BeginRenderPass(VulkanCommandBuffer *cmdbuffer);
	void WaitForStreamBuffers();

	VkThreadRenderPassSetup* GetRenderPass(const VkRenderPassKey& key);
	VulkanPipelineLayout* GetPipelineLayout(int numLayers);

	VulkanRenderDevice* fb = nullptr;
	int threadIndex = 0;

	VkRSBuffers* mRSBuffers = nullptr;

	bool mDepthClamp = true;
	VulkanCommandBuffer *mCommandBuffer = nullptr;
	VkPipelineKey mPipelineKey = {};
	VkThreadRenderPassSetup* mPassSetup = nullptr;
	int mClearTargets = 0;
	bool mNeedApply = true;

	int mScissorX = 0, mScissorY = 0, mScissorWidth = -1, mScissorHeight = -1;
	int mViewportX = 0, mViewportY = 0, mViewportWidth = -1, mViewportHeight = -1;
	float mViewportDepthMin = 0.0f, mViewportDepthMax = 1.0f;
	bool mScissorChanged = true;
	bool mViewportChanged = true;

	bool mDepthTest = false;
	bool mDepthWrite = false;
	bool mStencilTest = false;

	bool mStencilRefChanged = false;
	int mStencilRef = 0;
	int mStencilOp = 0;
	int mDepthFunc = 0;
	int mColorMask = 15;
	int mCullMode = 0;

	PushConstants mPushConstants = {};

	uint32_t mLastViewpointOffset = 0xffffffff;
	uint32_t mLastMatricesOffset = 0xffffffff;
	uint32_t mLastStreamDataOffset = 0xffffffff;
	uint32_t mLastLightsOffset = 0;
	uint32_t mViewpointOffset = 0;

	VkStreamBufferWriter mStreamBufferWriter;
	VkMatrixBufferWriter mMatrixBufferWriter;

	int mLastVertexOffsets[2] = { 0, 0 };
	IBuffer* mLastVertexBuffer = nullptr;
	IBuffer* mLastIndexBuffer = nullptr;
	bool mIndexBufferNeedsBind = true;

	MatricesUBO mMatrices = {};
	bool mMatricesChanged = true;

	int mApplyCount = 0;

	struct RenderTarget
	{
		VkTextureImage *Image = nullptr;
		VulkanImageView *DepthStencil = nullptr;
		int Width = 0;
		int Height = 0;
		VkFormat Format = VK_FORMAT_R16G16B16A16_SFLOAT;
		VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;
		int DrawBuffers = 1;
	} mRenderTarget;

	std::map<VkRenderPassKey, std::unique_ptr<VkThreadRenderPassSetup>> mRenderPassSetups;
	std::vector<VulkanPipelineLayout*> mPipelineLayouts;
	std::unique_ptr<VulkanCommandBuffer> mThreadCommandBuffer;
};

class VkThreadRenderPassSetup
{
public:
	VkThreadRenderPassSetup(VulkanRenderDevice* fb, const VkRenderPassKey& key);

	VulkanRenderPass* GetRenderPass(int clearTargets);
	VulkanPipeline* GetPipeline(const VkPipelineKey& key);

private:
	VkRenderPassKey PassKey;
	VulkanRenderDevice* fb;
	VulkanRenderPass* RenderPasses[8] = {};
	std::map<VkPipelineKey, VulkanPipeline*> Pipelines;
};

class VkRenderStateMolten : public VkRenderState
{
public:
	using VkRenderState::VkRenderState;

	void Draw(int dt, int index, int count, bool apply = true) override;
};
