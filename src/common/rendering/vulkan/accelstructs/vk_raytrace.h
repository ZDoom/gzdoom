
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
	int32_t TextureIndex;
	float Alpha;
};

struct SurfaceVertex
{
	FVector4 pos;
	FVector2 uv;
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

	VulkanAccelerationStructure* GetAccelStruct() { return TopLevelAS.AccelStruct.get(); }
	VulkanBuffer* GetVertexBuffer() { return VertexBuffer.get(); }
	VulkanBuffer* GetIndexBuffer() { return IndexBuffer.get(); }
	VulkanBuffer* GetNodeBuffer() { return NodeBuffer.get(); }
	VulkanBuffer* GetSurfaceIndexBuffer() { return SurfaceIndexBuffer.get(); }
	VulkanBuffer* GetSurfaceBuffer() { return SurfaceBuffer.get(); }
	VulkanBuffer* GetPortalBuffer() { return PortalBuffer.get(); }

private:
	void Reset();
	void CreateVulkanObjects();
	void CreateBuffers();
	void CreateStaticBLAS();
	void CreateDynamicBLAS();
	void CreateTopLevelAS();

	std::vector<CollisionNode> CreateCollisionNodes();

	VulkanRenderDevice* fb = nullptr;

	bool useRayQuery = true;

	LevelMesh NullMesh;
	LevelMesh* Mesh = nullptr;

	std::unique_ptr<VulkanBuffer> VertexBuffer;
	std::unique_ptr<VulkanBuffer> IndexBuffer;
	std::unique_ptr<VulkanBuffer> SurfaceIndexBuffer;
	std::unique_ptr<VulkanBuffer> SurfaceBuffer;
	std::unique_ptr<VulkanBuffer> PortalBuffer;

	std::unique_ptr<VulkanBuffer> NodeBuffer;

	struct
	{
		std::unique_ptr<VulkanBuffer> ScratchBuffer;
		std::unique_ptr<VulkanBuffer> AccelStructBuffer;
		std::unique_ptr<VulkanAccelerationStructure> AccelStruct;
	} StaticBLAS;

	struct
	{
		std::unique_ptr<VulkanBuffer> ScratchBuffer;
		std::unique_ptr<VulkanBuffer> AccelStructBuffer;
		std::unique_ptr<VulkanAccelerationStructure> AccelStruct;
	} DynamicBLAS;

	struct
	{
		std::unique_ptr<VulkanBuffer> TransferBuffer;
		std::unique_ptr<VulkanBuffer> ScratchBuffer;
		std::unique_ptr<VulkanBuffer> InstanceBuffer;
		std::unique_ptr<VulkanBuffer> AccelStructBuffer;
		std::unique_ptr<VulkanAccelerationStructure> AccelStruct;
	} TopLevelAS;
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
