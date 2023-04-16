
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "hw_levelmesh.h"

class VulkanRenderDevice;

struct CollisionNodeBufferHeader
{
	int root;
	int padding1;
	int padding2;
	int padding3;
};

struct CollisionNode
{
	FVector3 center;
	float padding1;
	FVector3 extents;
	float padding2;
	int left;
	int right;
	int element_index;
	int padding3;
};

class VkRaytrace
{
public:
	VkRaytrace(VulkanRenderDevice* fb);

	void SetLevelMesh(hwrenderer::LevelMesh* mesh);

	VulkanAccelerationStructure* GetAccelStruct() { return tlAccelStruct.get(); }
	VulkanBuffer* GetVertexBuffer() { return vertexBuffer.get(); }
	VulkanBuffer* GetIndexBuffer() { return indexBuffer.get(); }
	VulkanBuffer* GetNodeBuffer() { return nodesBuffer.get(); }

private:
	void Reset();
	void CreateVulkanObjects();
	void CreateVertexAndIndexBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();

	std::vector<CollisionNode> CreateCollisionNodes();

	VulkanRenderDevice* fb = nullptr;

	bool useRayQuery = true;

	hwrenderer::LevelMesh NullMesh;
	hwrenderer::LevelMesh* Mesh = nullptr;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;
	std::unique_ptr<VulkanBuffer> nodesBuffer;

	std::unique_ptr<VulkanBuffer> blScratchBuffer;
	std::unique_ptr<VulkanBuffer> blAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> blAccelStruct;

	std::unique_ptr<VulkanBuffer> tlTransferBuffer;
	std::unique_ptr<VulkanBuffer> tlScratchBuffer;
	std::unique_ptr<VulkanBuffer> tlInstanceBuffer;
	std::unique_ptr<VulkanBuffer> tlAccelStructBuffer;
	std::unique_ptr<VulkanAccelerationStructure> tlAccelStruct;
};

class BufferTransfer
{
public:
	BufferTransfer& AddBuffer(VulkanBuffer* buffer, const void* data, size_t size);
	BufferTransfer& AddBuffer(VulkanBuffer* buffer, const void* data0, size_t size0, const void* data1, size_t size1);
	std::unique_ptr<VulkanBuffer> Execute(VulkanDevice* device, VulkanCommandBuffer* cmdbuffer);

private:
	struct BufferCopy
	{
		VulkanBuffer* buffer;
		const void* data0;
		size_t size0;
		const void* data1;
		size_t size1;
	};
	std::vector<BufferCopy> bufferCopies;
};
