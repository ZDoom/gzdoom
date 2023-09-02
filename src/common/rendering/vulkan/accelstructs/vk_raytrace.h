
#pragma once

#include "zvulkan/vulkanobjects.h"
#include "hw_levelmesh.h"
#include "common/utility/matrix.h"

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

struct SurfaceInfo
{
	FVector3 Normal;
	float Sky;
	float SamplingDistance;
	uint32_t PortalIndex;
	float Padding1, Padding2;
};

struct PortalInfo
{
	VSMatrix transformation;
};

class VkRaytrace
{
public:
	VkRaytrace(VulkanRenderDevice* fb);

	void SetLevelMesh(LevelMesh* mesh);

	VulkanAccelerationStructure* GetAccelStruct() { return tlAccelStruct.get(); }
	VulkanBuffer* GetVertexBuffer() { return vertexBuffer.get(); }
	VulkanBuffer* GetIndexBuffer() { return indexBuffer.get(); }
	VulkanBuffer* GetNodeBuffer() { return nodesBuffer.get(); }
	VulkanBuffer* GetSurfaceIndexBuffer() { return surfaceIndexBuffer.get(); }
	VulkanBuffer* GetSurfaceBuffer() { return surfaceBuffer.get(); }
	VulkanBuffer* GetPortalBuffer() { return portalBuffer.get(); }

private:
	void Reset();
	void CreateVulkanObjects();
	void CreateBuffers();
	void CreateBottomLevelAccelerationStructure();
	void CreateTopLevelAccelerationStructure();

	std::vector<CollisionNode> CreateCollisionNodes();

	VulkanRenderDevice* fb = nullptr;

	bool useRayQuery = true;

	LevelMesh NullMesh;
	LevelMesh* Mesh = nullptr;

	std::unique_ptr<VulkanBuffer> vertexBuffer;
	std::unique_ptr<VulkanBuffer> indexBuffer;
	std::unique_ptr<VulkanBuffer> transferBuffer;
	std::unique_ptr<VulkanBuffer> nodesBuffer;
	std::unique_ptr<VulkanBuffer> surfaceIndexBuffer;
	std::unique_ptr<VulkanBuffer> surfaceBuffer;
	std::unique_ptr<VulkanBuffer> portalBuffer;

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
