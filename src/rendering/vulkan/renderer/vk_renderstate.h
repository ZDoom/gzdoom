
#pragma once

#include "vulkan/system/vk_buffers.h"
#include "vulkan/shaders/vk_shader.h"
#include "vulkan/renderer/vk_renderpass.h"

#include "name.h"

#include "hwrenderer/scene/hw_drawstructs.h"
#include "hwrenderer/scene/hw_renderstate.h"
#include "hwrenderer/textures/hw_material.h"

class VkRenderPassSetup;

class VkRenderState : public FRenderState
{
public:
	VkRenderState();

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
	void EnableDrawBufferAttachments(bool on) override;
	void SetStencil(int offs, int op, int flags = -1) override;
	void SetCulling(int mode) override;
	void EnableClipDistance(int num, bool state) override;
	void Clear(int targets) override;
	void EnableStencil(bool on) override;
	void SetScissor(int x, int y, int w, int h) override;
	void SetViewport(int x, int y, int w, int h) override;
	void EnableDepthTest(bool on) override;
	void EnableMultisampling(bool on) override;
	void EnableLineSmooth(bool on) override;

	void Bind(int bindingpoint, uint32_t offset);
	void EndRenderPass();

private:
	void Apply(int dt);
	void ApplyRenderPass(int dt);
	void ApplyScissor();
	void ApplyViewport();
	void ApplyStreamData();
	void ApplyMatrices();
	void ApplyPushConstants();
	void ApplyDynamicSet();
	void ApplyVertexBuffers();
	void ApplyMaterial();

	bool mLastDepthClamp = true;
	VulkanCommandBuffer *mCommandBuffer = nullptr;
	VkRenderPassKey mRenderPassKey = {};
	bool mDynamicSetChanged = true;

	int mScissorX = 0, mScissorY = 0, mScissorWidth = -1, mScissorHeight = -1;
	int mViewportX = 0, mViewportY = 0, mViewportWidth = -1, mViewportHeight = -1;
	float mViewportDepthMin = 0.0f, mViewportDepthMax = 1.0f;
	bool mScissorChanged = true;
	bool mViewportChanged = true;

	bool mDepthTest = false;
	bool mDepthWrite = false;

	MatricesUBO mMatrices = {};
	StreamData mStreamData = {};
	PushConstants mPushConstants = {};

	uint32_t mLastViewpointOffset = 0xffffffff;
	uint32_t mLastLightBufferOffset = 0xffffffff;
	uint32_t mViewpointOffset = 0;
	uint32_t mLightBufferOffset = 0;
	uint32_t mMatricesOffset = 0;
	uint32_t mDataIndex = -1;
	uint32_t mStreamDataOffset = 0;

	VSMatrix mIdentityMatrix;

	IVertexBuffer *mLastVertexBuffer = nullptr;
	IIndexBuffer *mLastIndexBuffer = nullptr;

	bool mLastGlowEnabled = true;
	bool mLastGradientEnabled = true;
	bool mLastSplitEnabled = true;
	bool mLastModelMatrixEnabled = true;
	bool mLastTextureMatrixEnabled = true;
};
