
#pragma once

#include "hwrenderer/postprocessing/hw_postprocess.h"
#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;

class VkPPShader : public PPShaderBackend
{
public:
	VkPPShader(VulkanFrameBuffer* fb, PPShader *shader);
	~VkPPShader();

	void Reset();

	VulkanFrameBuffer* fb = nullptr;
	std::list<VkPPShader*>::iterator it;

	std::unique_ptr<VulkanShader> VertexShader;
	std::unique_ptr<VulkanShader> FragmentShader;

private:
	FString LoadShaderCode(const FString &lumpname, const FString &defines, int version);
};
