
#pragma once

#include "vulkan/system/vk_objects.h"
#include "hw_levelmesh.h"

class VulkanFrameBuffer;

class VkRaytrace
{
public:
	VkRaytrace(VulkanFrameBuffer* fb);

	void SetLevelMesh(hwrenderer::LevelMesh* mesh);

	VulkanAccelerationStructure* GetAccelStruct() { return tlAccelStruct.get(); }

private:
	void Reset();
	void CreateVulkanObjects();
	void CreateVertexAndIndexBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();

	VulkanFrameBuffer* fb = nullptr;

	hwrenderer::LevelMesh NullMesh;
	hwrenderer::LevelMesh* Mesh = nullptr;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;

	std::unique_ptr<VulkanBuffer> blScratchBuffer;
	std::unique_ptr<VulkanBuffer> blAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> blAccelStruct;

	std::unique_ptr<VulkanBuffer> tlTransferBuffer;
	std::unique_ptr<VulkanBuffer> tlScratchBuffer;
	std::unique_ptr<VulkanBuffer> tlInstanceBuffer;
	std::unique_ptr<VulkanBuffer> tlAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> tlAccelStruct;
};
