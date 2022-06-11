
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"

class VkPPRenderPassSetup;
class VkPPShader;
class VkPPTexture;
class VkTextureImage;
class VulkanFrameBuffer;

class VkPPRenderState : public PPRenderState
{
public:
	VkPPRenderState(VulkanFrameBuffer* fb);

	void PushGroup(const FString &name) override;
	void PopGroup() override;

	void Draw() override;

private:
	void RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize, bool stencilTest);

	VulkanFrameBuffer* fb = nullptr;
};
