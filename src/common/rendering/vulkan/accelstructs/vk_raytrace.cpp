/*
**  Vulkan backend
**  Copyright (c) 2016-2020 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include "vk_raytrace.h"
#include "zvulkan/vulkanbuilders.h"
#include "vulkan/vk_renderdevice.h"
#include "vulkan/commands/vk_commandbuffer.h"
#include "hw_levelmesh.h"
#include "hw_material.h"
#include "texturemanager.h"

VkRaytrace::VkRaytrace(VulkanRenderDevice* fb) : fb(fb)
{
	useRayQuery = fb->GetDevice()->SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

	NullMesh.MeshVertices.Push({ -1.0f, -1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ 1.0f, -1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ 1.0f,  1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ -1.0f, -1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ -1.0f,  1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ 1.0f,  1.0f, -1.0f });
	NullMesh.MeshVertices.Push({ -1.0f, -1.0f,  1.0f });
	NullMesh.MeshVertices.Push({ 1.0f, -1.0f,  1.0f });
	NullMesh.MeshVertices.Push({ 1.0f,  1.0f,  1.0f });
	NullMesh.MeshVertices.Push({ -1.0f, -1.0f,  1.0f });
	NullMesh.MeshVertices.Push({ -1.0f,  1.0f,  1.0f });
	NullMesh.MeshVertices.Push({ 1.0f,  1.0f,  1.0f });

	NullMesh.MeshVertexUVs.Resize(NullMesh.MeshVertices.Size());

	for (int i = 0; i < 3 * 4; i++)
		NullMesh.MeshElements.Push(i);

	NullMesh.Collision = std::make_unique<TriangleMeshShape>(NullMesh.MeshVertices.Data(), NullMesh.MeshVertices.Size(), NullMesh.MeshElements.Data(), NullMesh.MeshElements.Size());

	SetLevelMesh(nullptr);
}

void VkRaytrace::SetLevelMesh(LevelMesh* mesh)
{
	if (!mesh)
		mesh = &NullMesh;

	Reset();
	Mesh = mesh;
	CreateVulkanObjects();
}

void VkRaytrace::Reset()
{
	auto deletelist = fb->GetCommands()->DrawDeleteList.get();
	deletelist->Add(std::move(VertexBuffer));
	deletelist->Add(std::move(IndexBuffer));
	deletelist->Add(std::move(NodeBuffer));
	deletelist->Add(std::move(SurfaceBuffer));
	deletelist->Add(std::move(SurfaceIndexBuffer));
	deletelist->Add(std::move(PortalBuffer));
	deletelist->Add(std::move(StaticBLAS.ScratchBuffer));
	deletelist->Add(std::move(StaticBLAS.AccelStructBuffer));
	deletelist->Add(std::move(StaticBLAS.AccelStruct));
	deletelist->Add(std::move(TopLevelAS.TransferBuffer));
	deletelist->Add(std::move(TopLevelAS.ScratchBuffer));
	deletelist->Add(std::move(TopLevelAS.InstanceBuffer));
	deletelist->Add(std::move(TopLevelAS.AccelStructBuffer));
	deletelist->Add(std::move(TopLevelAS.AccelStruct));
}

void VkRaytrace::CreateVulkanObjects()
{
	CreateBuffers();
	if (useRayQuery)
	{
		CreateStaticBLAS();
		CreateDynamicBLAS();
		CreateTopLevelAS();
	}
}

void VkRaytrace::CreateBuffers()
{
	std::vector<CollisionNode> nodes = CreateCollisionNodes();

	// std430 alignment rules forces us to convert the vec3 to a vec4
	std::vector<SurfaceVertex> vertices;
	vertices.reserve(Mesh->MeshVertices.Size());
	for (int i = 0, count = Mesh->MeshVertices.Size(); i < count; ++i)
		vertices.push_back({ { Mesh->MeshVertices[i], 1.0f }, Mesh->MeshVertexUVs[i], float(i), i + 10000.0f});

	CollisionNodeBufferHeader nodesHeader;
	nodesHeader.root = Mesh->Collision->get_root();

	TArray<PortalInfo> portalInfo;
	for (auto& portal : Mesh->Portals)
	{
		PortalInfo info;
		info.transformation = portal.transformation;
		portalInfo.Push(info);
	}

	TArray<SurfaceInfo> surfaceInfo;
	for (int i = 0, count = Mesh->GetSurfaceCount(); i < count; i++)
	{
		LevelMeshSurface* surface = Mesh->GetSurface(i);

		SurfaceInfo info;
		info.Normal = surface->plane.XYZ();
		info.PortalIndex = surface->portalIndex;
		info.SamplingDistance = (float)surface->sampleDimension;
		info.Sky = surface->bSky;

		if (surface->texture.isValid())
		{
			auto mat = FMaterial::ValidateTexture(TexMan.GetGameTexture(surface->texture), 0);
			info.TextureIndex = fb->GetBindlessTextureIndex(mat, CLAMP_NONE, 0);
		}
		else
		{
			info.TextureIndex = -1;
		}

		info.Alpha = surface->alpha;

		surfaceInfo.Push(info);
	}

	VertexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			(useRayQuery ?
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0) |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(vertices.size() * sizeof(SurfaceVertex))
		.DebugName("VertexBuffer")
		.Create(fb->GetDevice());

	IndexBuffer = BufferBuilder()
		.Usage(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
			VK_BUFFER_USAGE_TRANSFER_DST_BIT |
			(useRayQuery ?
				VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
				VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR : 0) |
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size((size_t)Mesh->MeshElements.Size() * sizeof(uint32_t))
		.DebugName("IndexBuffer")
		.Create(fb->GetDevice());

	NodeBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(sizeof(CollisionNodeBufferHeader) + nodes.size() * sizeof(CollisionNode))
		.DebugName("NodeBuffer")
		.Create(fb->GetDevice());

	SurfaceIndexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(Mesh->MeshSurfaceIndexes.Size() * sizeof(int))
		.DebugName("SurfaceBuffer")
		.Create(fb->GetDevice());

	SurfaceBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(surfaceInfo.Size() * sizeof(SurfaceInfo))
		.DebugName("SurfaceBuffer")
		.Create(fb->GetDevice());

	PortalBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(portalInfo.Size() * sizeof(PortalInfo))
		.DebugName("PortalBuffer")
		.Create(fb->GetDevice());

	auto transferBuffer = BufferTransfer()
		.AddBuffer(VertexBuffer.get(), vertices.data(), vertices.size() * sizeof(SurfaceVertex))
		.AddBuffer(IndexBuffer.get(), Mesh->MeshElements.Data(), (size_t)Mesh->MeshElements.Size() * sizeof(uint32_t))
		.AddBuffer(NodeBuffer.get(), &nodesHeader, sizeof(CollisionNodeBufferHeader), nodes.data(), nodes.size() * sizeof(CollisionNode))
		.AddBuffer(SurfaceIndexBuffer.get(), Mesh->MeshSurfaceIndexes.Data(), Mesh->MeshSurfaceIndexes.Size() * sizeof(int))
		.AddBuffer(SurfaceBuffer.get(), surfaceInfo.Data(), surfaceInfo.Size() * sizeof(SurfaceInfo))
		.AddBuffer(PortalBuffer.get(), portalInfo.Data(), portalInfo.Size() * sizeof(PortalInfo))
		.Execute(fb->GetDevice(), fb->GetCommands()->GetTransferCommands());

	fb->GetCommands()->TransferDeleteList->Add(std::move(transferBuffer));

	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, useRayQuery ? VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR : VK_ACCESS_SHADER_READ_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, useRayQuery ? VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VkRaytrace::CreateStaticBLAS()
{
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructBLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	accelStructBLDesc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	accelStructBLDesc.geometry.triangles.vertexData.deviceAddress = VertexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.vertexStride = sizeof(SurfaceVertex);
	accelStructBLDesc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelStructBLDesc.geometry.triangles.indexData.deviceAddress = IndexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.maxVertex = Mesh->MeshVertices.Size() - 1;

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.ppGeometries = geometries;

	uint32_t maxPrimitiveCount = Mesh->MeshElements.Size() / 3;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(fb->GetDevice()->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	StaticBLAS.AccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("StaticBLAS.AccelStructBuffer")
		.Create(fb->GetDevice());

	StaticBLAS.AccelStruct = AccelerationStructureBuilder()
		.Type(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
		.Buffer(StaticBLAS.AccelStructBuffer.get(), sizeInfo.accelerationStructureSize)
		.DebugName("StaticBLAS.AccelStruct")
		.Create(fb->GetDevice());

	StaticBLAS.ScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.MinAlignment(fb->GetDevice()->PhysicalDevice.Properties.AccelerationStructure.minAccelerationStructureScratchOffsetAlignment)
		.DebugName("StaticBLAS.ScratchBuffer")
		.Create(fb->GetDevice());

	buildInfo.dstAccelerationStructure = StaticBLAS.AccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = StaticBLAS.ScratchBuffer->GetDeviceAddress();
	rangeInfo.primitiveCount = maxPrimitiveCount;

	fb->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building before using it as input to a toplevel accel structure
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
}

void VkRaytrace::CreateDynamicBLAS()
{
#ifdef USE_DYNAMIC_BLAS
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructBLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	accelStructBLDesc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
	accelStructBLDesc.geometry.triangles.vertexData.deviceAddress = VertexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.vertexStride = sizeof(SurfaceVertex);
	accelStructBLDesc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelStructBLDesc.geometry.triangles.indexData.deviceAddress = IndexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.maxVertex = Mesh->MeshVertices.Size() - 1;

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.ppGeometries = geometries;

	uint32_t maxPrimitiveCount = Mesh->MeshElements.Size() / 3;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(fb->GetDevice()->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	DynamicBLAS.AccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("DynamicBLAS.AccelStructBuffer")
		.Create(fb->GetDevice());

	DynamicBLAS.AccelStruct = AccelerationStructureBuilder()
		.Type(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR)
		.Buffer(DynamicBLAS.AccelStructBuffer.get(), sizeInfo.accelerationStructureSize)
		.DebugName("DynamicBLAS.AccelStruct")
		.Create(fb->GetDevice());

	DynamicBLAS.ScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.MinAlignment(fb->GetDevice()->PhysicalDevice.Properties.AccelerationStructure.minAccelerationStructureScratchOffsetAlignment)
		.DebugName("DynamicBLAS.ScratchBuffer")
		.Create(fb->GetDevice());

	buildInfo.dstAccelerationStructure = DynamicBLAS.AccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = DynamicBLAS.ScratchBuffer->GetDeviceAddress();
	rangeInfo.primitiveCount = maxPrimitiveCount;

	fb->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building before using it as input to a toplevel accel structure
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
#endif
}

void VkRaytrace::CreateTopLevelAS()
{
	VkAccelerationStructureInstanceKHR instances[2] = {};
	instances[0].transform.matrix[0][0] = 1.0f;
	instances[0].transform.matrix[1][1] = 1.0f;
	instances[0].transform.matrix[2][2] = 1.0f;
	instances[0].mask = 0xff;
	instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instances[0].accelerationStructureReference = StaticBLAS.AccelStruct->GetDeviceAddress();

#ifdef USE_DYNAMIC_BLAS
	instances[1].transform.matrix[0][0] = 1.0f;
	instances[1].transform.matrix[1][1] = 1.0f;
	instances[1].transform.matrix[2][2] = 1.0f;
	instances[1].mask = 0xff;
	instances[1].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instances[1].accelerationStructureReference = DynamicBLAS.AccelStruct->GetDeviceAddress();
#endif

	TopLevelAS.TransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(sizeof(VkAccelerationStructureInstanceKHR) * 2)
		.DebugName("TopLevelAS.TransferBuffer")
		.Create(fb->GetDevice());

	auto data = (uint8_t*)TopLevelAS.TransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR) * 2);
	memcpy(data, instances, sizeof(VkAccelerationStructureInstanceKHR) * 2);
	TopLevelAS.TransferBuffer->Unmap();

	TopLevelAS.InstanceBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(sizeof(VkAccelerationStructureInstanceKHR) * 2)
		.DebugName("TopLevelAS.InstanceBuffer")
		.Create(fb->GetDevice());

	fb->GetCommands()->GetTransferCommands()->copyBuffer(TopLevelAS.TransferBuffer.get(), TopLevelAS.InstanceBuffer.get());

	// Finish transfering before using it as input
	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructTLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructTLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
#ifdef USE_DYNAMIC_BLAS
	buildInfo.geometryCount = 2;
#else
	buildInfo.geometryCount = 1;
#endif
	buildInfo.ppGeometries = geometries;

	accelStructTLDesc.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelStructTLDesc.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	accelStructTLDesc.geometry.instances.data.deviceAddress = TopLevelAS.InstanceBuffer->GetDeviceAddress();

	uint32_t maxInstanceCount = 2;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(fb->GetDevice()->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxInstanceCount, &sizeInfo);

	TopLevelAS.AccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("TopLevelAS.AccelStructBuffer")
		.Create(fb->GetDevice());

	TopLevelAS.AccelStruct = AccelerationStructureBuilder()
		.Type(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR)
		.Buffer(TopLevelAS.AccelStructBuffer.get(), sizeInfo.accelerationStructureSize)
		.DebugName("TopLevelAS.AccelStruct")
		.Create(fb->GetDevice());

	TopLevelAS.ScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.MinAlignment(fb->GetDevice()->PhysicalDevice.Properties.AccelerationStructure.minAccelerationStructureScratchOffsetAlignment)
		.DebugName("TopLevelAS.ScratchBuffer")
		.Create(fb->GetDevice());

	buildInfo.dstAccelerationStructure = TopLevelAS.AccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = TopLevelAS.ScratchBuffer->GetDeviceAddress();
	rangeInfo.primitiveCount = 1;

	fb->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building the accel struct before using as input in a fragment shader
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_SHADER_READ_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

std::vector<CollisionNode> VkRaytrace::CreateCollisionNodes()
{
	std::vector<CollisionNode> nodes;
	nodes.reserve(Mesh->Collision->get_nodes().size());
	for (const auto& node : Mesh->Collision->get_nodes())
	{
		CollisionNode info;
		info.center = node.aabb.Center;
		info.extents = node.aabb.Extents;
		info.left = node.left;
		info.right = node.right;
		info.element_index = node.element_index;
		nodes.push_back(info);
	}
	if (nodes.empty()) // vulkan doesn't support zero byte buffers
		nodes.push_back(CollisionNode());
	return nodes;
}


/////////////////////////////////////////////////////////////////////////////

BufferTransfer& BufferTransfer::AddBuffer(VulkanBuffer* buffer, const void* data, size_t size)
{
	bufferCopies.push_back({ buffer, data, size, nullptr, 0 });
	return *this;
}

BufferTransfer& BufferTransfer::AddBuffer(VulkanBuffer* buffer, const void* data0, size_t size0, const void* data1, size_t size1)
{
	bufferCopies.push_back({ buffer, data0, size0, data1, size1 });
	return *this;
}

std::unique_ptr<VulkanBuffer> BufferTransfer::Execute(VulkanDevice* device, VulkanCommandBuffer* cmdbuffer)
{
	size_t transferbuffersize = 0;
	for (const auto& copy : bufferCopies)
		transferbuffersize += copy.size0 + copy.size1;

	if (transferbuffersize == 0)
		return nullptr;

	auto transferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(transferbuffersize)
		.DebugName("BufferTransfer.transferBuffer")
		.Create(device);

	uint8_t* data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	size_t pos = 0;
	for (const auto& copy : bufferCopies)
	{
		memcpy(data + pos, copy.data0, copy.size0);
		pos += copy.size0;
		memcpy(data + pos, copy.data1, copy.size1);
		pos += copy.size1;
	}
	transferBuffer->Unmap();

	pos = 0;
	for (const auto& copy : bufferCopies)
	{
		if (copy.size0 > 0)
			cmdbuffer->copyBuffer(transferBuffer.get(), copy.buffer, pos, 0, copy.size0);
		pos += copy.size0;

		if (copy.size1 > 0)
			cmdbuffer->copyBuffer(transferBuffer.get(), copy.buffer, pos, copy.size0, copy.size1);
		pos += copy.size1;
	}

	return transferBuffer;
}
