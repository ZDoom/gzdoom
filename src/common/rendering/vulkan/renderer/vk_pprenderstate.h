
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"

class VkPPRenderPassSetup;
class VkPPShader;
class VkPPTexture;
class VkTextureImage;

class VkPPRenderState : public PPRenderState
{
public:
	void PushGroup(const FString &name) override;
	void PopGroup() override;

	void Draw() override;

private:
	void RenderScreenQuad(VkPPRenderPassSetup *passSetup, VulkanDescriptorSet *descriptorSet, VulkanFramebuffer *framebuffer, int framebufferWidth, int framebufferHeight, int x, int y, int width, int height, const void *pushConstants, uint32_t pushConstantsSize, bool stencilTest);

	VulkanDescriptorSet *GetInput(VkPPRenderPassSetup *passSetup, const TArray<PPTextureInput> &textures, bool bindShadowMapBuffers);
	VulkanFramebuffer *GetOutput(VkPPRenderPassSetup *passSetup, const PPOutput &output, bool stencilTest, int &framebufferWidth, int &framebufferHeight);

	VkPPShader *GetVkShader(PPShader *shader);
	VkPPTexture *GetVkTexture(PPTexture *texture);

	VkTextureImage *GetTexture(const PPTextureType &type, PPTexture *tex);
};
