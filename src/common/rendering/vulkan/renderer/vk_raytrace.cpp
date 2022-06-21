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
#include "vulkan/system/vk_builders.h"
#include "vulkan/system/vk_framebuffer.h"
#include "vulkan/system/vk_commandbuffer.h"
#include "doom_levelmesh.h"

VkRaytrace::VkRaytrace(VulkanFrameBuffer* fb) : fb(fb)
{
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
	for (int i = 0; i < 3 * 4; i++)
		NullMesh.MeshElements.Push(i);

	SetLevelMesh(nullptr);
}

void VkRaytrace::SetLevelMesh(hwrenderer::LevelMesh* mesh)
{
	if (!mesh)
		mesh = &NullMesh;

	if (mesh != Mesh)
	{
		Reset();
		Mesh = mesh;
		if (fb->RaytracingEnabled())
		{
			CreateVulkanObjects();
		}
	}
}

void VkRaytrace::Reset()
{
	auto deletelist = fb->GetCommands()->DrawDeleteList.get();
	deletelist->Add(std::move(vertexBuffer));
	deletelist->Add(std::move(indexBuffer));
	deletelist->Add(std::move(transferBuffer));
	deletelist->Add(std::move(blScratchBuffer));
	deletelist->Add(std::move(blAccelStructBuffer));
	deletelist->Add(std::move(blAccelStruct));
	deletelist->Add(std::move(tlTransferBuffer));
	deletelist->Add(std::move(tlScratchBuffer));
	deletelist->Add(std::move(tlInstanceBuffer));
	deletelist->Add(std::move(tlAccelStructBuffer));
	deletelist->Add(std::move(tlAccelStruct));
}

void VkRaytrace::CreateVulkanObjects()
{
	CreateVertexAndIndexBuffers();
	CreateBottomLevelAccelerationStructure();
	CreateTopLevelAccelerationStructure();
}

void VkRaytrace::CreateVertexAndIndexBuffers()
{
	static_assert(sizeof(FVector3) == 3 * 4, "sizeof(FVector3) is not 12 bytes!");

	size_t vertexbuffersize = (size_t)Mesh->MeshVertices.Size() * sizeof(FVector3);
	size_t indexbuffersize = (size_t)Mesh->MeshElements.Size() * sizeof(uint32_t);
	size_t transferbuffersize = vertexbuffersize + indexbuffersize;
	size_t vertexoffset = 0;
	size_t indexoffset = vertexoffset + vertexbuffersize;

	transferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(transferbuffersize)
		.DebugName("transferBuffer")
		.Create(fb->device);

	uint8_t* data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	memcpy(data + vertexoffset, Mesh->MeshVertices.Data(), vertexbuffersize);
	memcpy(data + indexoffset, Mesh->MeshElements.Data(), indexbuffersize);
	transferBuffer->Unmap();

	vertexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(vertexbuffersize)
		.DebugName("vertexBuffer")
		.Create(fb->device);

	indexBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(indexbuffersize)
		.DebugName("indexBuffer")
		.Create(fb->device);

	fb->GetCommands()->GetTransferCommands()->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	fb->GetCommands()->GetTransferCommands()->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);

	// Finish transfer before using it for building
	PipelineBarrier()
		.AddMemory(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
}

void VkRaytrace::CreateBottomLevelAccelerationStructure()
{
	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructBLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.ppGeometries = geometries;

	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	accelStructBLDesc.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	accelStructBLDesc.geometry.triangles.vertexData.deviceAddress = vertexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.vertexStride = sizeof(FVector3);
	accelStructBLDesc.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
	accelStructBLDesc.geometry.triangles.indexData.deviceAddress = indexBuffer->GetDeviceAddress();
	accelStructBLDesc.geometry.triangles.maxVertex = Mesh->MeshVertices.Size() - 1;
	//accelStructBLDesc.geometry.triangles.transformData = transformAddress; // optional; VkTransformMatrixKHR transfered to a buffer?

	rangeInfo.primitiveCount = Mesh->MeshElements.Size() / 3;
	uint32_t maxPrimitiveCount = rangeInfo.primitiveCount;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(fb->device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	blAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("blAccelStructBuffer")
		.Create(fb->device);

	VkAccelerationStructureKHR blAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.buffer = blAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(fb->device->device, &createInfo, nullptr, &blAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	blAccelStruct = std::make_unique<VulkanAccelerationStructure>(fb->device, blAccelStructHandle);
	blAccelStruct->SetDebugName("blAccelStruct");

	blScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("blScratchBuffer")
		.Create(fb->device);

	buildInfo.dstAccelerationStructure = blAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = blScratchBuffer->GetDeviceAddress();

	fb->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building before using it as input to a toplevel accel structure
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR);
}

void VkRaytrace::CreateTopLevelAccelerationStructure()
{
	VkAccelerationStructureInstanceKHR instance = {};
	instance.transform.matrix[0][0] = 1.0f;
	instance.transform.matrix[1][1] = 1.0f;
	instance.transform.matrix[2][2] = 1.0f;
	instance.mask = 0xff;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.accelerationStructureReference = blAccelStruct->GetDeviceAddress();

	tlTransferBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY)
		.Size(sizeof(VkAccelerationStructureInstanceKHR))
		.DebugName("tlTransferBuffer")
		.Create(fb->device);

	auto data = (uint8_t*)tlTransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR));
	memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer->Unmap();

	tlInstanceBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT)
		.Size(sizeof(VkAccelerationStructureInstanceKHR))
		.DebugName("tlInstanceBuffer")
		.Create(fb->device);

	fb->GetCommands()->GetTransferCommands()->copyBuffer(tlTransferBuffer.get(), tlInstanceBuffer.get());

	// Finish transfering before using it as input
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	fb->GetCommands()->GetTransferCommands()->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	VkAccelerationStructureGeometryKHR accelStructTLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	VkAccelerationStructureGeometryKHR* geometries[] = { &accelStructTLDesc };
	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };

	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.ppGeometries = geometries;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	accelStructTLDesc.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelStructTLDesc.geometry.instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	accelStructTLDesc.geometry.instances.data.deviceAddress = tlInstanceBuffer->GetDeviceAddress();

	uint32_t maxInstanceCount = 1;
	rangeInfo.primitiveCount = 1;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(fb->device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxInstanceCount, &sizeInfo);

	tlAccelStructBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
		.Size(sizeInfo.accelerationStructureSize)
		.DebugName("tlAccelStructBuffer")
		.Create(fb->device);

	VkAccelerationStructureKHR tlAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.buffer = tlAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(fb->device->device, &createInfo, nullptr, &tlAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	tlAccelStruct = std::make_unique<VulkanAccelerationStructure>(fb->device, tlAccelStructHandle);

	tlScratchBuffer = BufferBuilder()
		.Usage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)
		.Size(sizeInfo.buildScratchSize)
		.DebugName("tlScratchBuffer")
		.Create(fb->device);

	buildInfo.dstAccelerationStructure = tlAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = tlScratchBuffer->GetDeviceAddress();

	fb->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building the accel struct before using as input in a fragment shader
	PipelineBarrier()
		.AddMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_SHADER_READ_BIT)
		.Execute(fb->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}
