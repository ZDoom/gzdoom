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

void VkRaytrace::SetLevelMesh(hwrenderer::LevelMesh* mesh)
{
	if (mesh != Mesh)
	{
		Reset();
		Mesh = mesh;
		if (Mesh)
		{
			if (GetVulkanFrameBuffer()->device->SupportsDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
			{
				CreateVulkanObjects();
			}
		}
	}
}

VulkanAccelerationStructure* VkRaytrace::GetAccelStruct()
{
	if (tlAccelStruct)
		return tlAccelStruct.get();

	// We need a dummy accel struct to keep vulkan happy:
	hwrenderer::LevelMesh dummy;

	dummy.MeshVertices.Push({ -1.0f, -1.0f, -1.0f });
	dummy.MeshVertices.Push({  1.0f, -1.0f, -1.0f });
	dummy.MeshVertices.Push({  1.0f,  1.0f, -1.0f });

	dummy.MeshVertices.Push({ -1.0f, -1.0f, -1.0f });
	dummy.MeshVertices.Push({ -1.0f,  1.0f, -1.0f });
	dummy.MeshVertices.Push({  1.0f,  1.0f, -1.0f });

	dummy.MeshVertices.Push({ -1.0f, -1.0f,  1.0f });
	dummy.MeshVertices.Push({  1.0f, -1.0f,  1.0f });
	dummy.MeshVertices.Push({  1.0f,  1.0f,  1.0f });

	dummy.MeshVertices.Push({ -1.0f, -1.0f,  1.0f });
	dummy.MeshVertices.Push({ -1.0f,  1.0f,  1.0f });
	dummy.MeshVertices.Push({  1.0f,  1.0f,  1.0f });

	for (int i = 0; i < 3 * 4; i++)
		dummy.MeshElements.Push(i);

	Mesh = &dummy;
	CreateVulkanObjects();
	Mesh = nullptr;
	return tlAccelStruct.get();
}

void VkRaytrace::Reset()
{
	auto fb = GetVulkanFrameBuffer();
	if (fb)
	{
		auto& deletelist = fb->GetCommands()->FrameDeleteList;
		deletelist.Buffers.push_back(std::move(vertexBuffer));
		deletelist.Buffers.push_back(std::move(indexBuffer));
		deletelist.Buffers.push_back(std::move(transferBuffer));

		deletelist.Buffers.push_back(std::move(blScratchBuffer));
		deletelist.Buffers.push_back(std::move(blAccelStructBuffer));
		deletelist.AccelStructs.push_back(std::move(blAccelStruct));

		deletelist.Buffers.push_back(std::move(tlTransferBuffer));
		deletelist.Buffers.push_back(std::move(tlScratchBuffer));
		deletelist.Buffers.push_back(std::move(tlInstanceBuffer));
		deletelist.Buffers.push_back(std::move(tlAccelStructBuffer));
		deletelist.AccelStructs.push_back(std::move(tlAccelStruct));
	}
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

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(transferbuffersize);
	transferBuffer = tbuilder.create(GetVulkanFrameBuffer()->device);
	transferBuffer->SetDebugName("transferBuffer");
	uint8_t* data = (uint8_t*)transferBuffer->Map(0, transferbuffersize);
	memcpy(data + vertexoffset, Mesh->MeshVertices.Data(), vertexbuffersize);
	memcpy(data + indexoffset, Mesh->MeshElements.Data(), indexbuffersize);
	transferBuffer->Unmap();

	BufferBuilder vbuilder;
	vbuilder.setUsage(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	vbuilder.setSize(vertexbuffersize);
	vertexBuffer = vbuilder.create(GetVulkanFrameBuffer()->device);
	vertexBuffer->SetDebugName("vertexBuffer");

	BufferBuilder ibuilder;
	ibuilder.setUsage(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	ibuilder.setSize(indexbuffersize);
	indexBuffer = ibuilder.create(GetVulkanFrameBuffer()->device);
	indexBuffer->SetDebugName("indexBuffer");

	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->copyBuffer(transferBuffer.get(), vertexBuffer.get(), vertexoffset);
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->copyBuffer(transferBuffer.get(), indexBuffer.get(), indexoffset);

	// Finish transfer before using it for building
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VkRaytrace::CreateBottomLevelAccelerationStructure()
{
	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = vertexBuffer->buffer;
	VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(GetVulkanFrameBuffer()->device->device, &info);

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = indexBuffer->buffer;
	VkDeviceAddress indexAddress = vkGetBufferDeviceAddress(GetVulkanFrameBuffer()->device->device, &info);

	VkAccelerationStructureGeometryTrianglesDataKHR triangles = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress = vertexAddress;
	triangles.vertexStride = sizeof(FVector3);
	triangles.indexType = VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress = indexAddress;
	triangles.maxVertex = Mesh->MeshVertices.Size() - 1;

	VkAccelerationStructureGeometryKHR accelStructBLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	accelStructBLDesc.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	accelStructBLDesc.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
	accelStructBLDesc.geometry.triangles = triangles;

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = Mesh->MeshElements.Size() / 3;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructBLDesc;

	uint32_t maxPrimitiveCount = rangeInfo.primitiveCount;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(GetVulkanFrameBuffer()->device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxPrimitiveCount, &sizeInfo);

	BufferBuilder blbufbuilder;
	blbufbuilder.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	blbufbuilder.setSize(sizeInfo.accelerationStructureSize);
	blAccelStructBuffer = blbufbuilder.create(GetVulkanFrameBuffer()->device);
	blAccelStructBuffer->SetDebugName("blAccelStructBuffer");

	VkAccelerationStructureKHR blAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	createInfo.buffer = blAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(GetVulkanFrameBuffer()->device->device, &createInfo, nullptr, &blAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	blAccelStruct = std::make_unique<VulkanAccelerationStructure>(GetVulkanFrameBuffer()->device, blAccelStructHandle);
	blAccelStruct->SetDebugName("blAccelStruct");

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	sbuilder.setSize(sizeInfo.buildScratchSize);
	blScratchBuffer = sbuilder.create(GetVulkanFrameBuffer()->device);
	blScratchBuffer->SetDebugName("blScratchBuffer");

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = blScratchBuffer->buffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(GetVulkanFrameBuffer()->device->device, &info);

	buildInfo.dstAccelerationStructure = blAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = scratchAddress;
	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building before using it as input to a toplevel accel structure
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->pipelineBarrier(VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);
}

void VkRaytrace::CreateTopLevelAccelerationStructure()
{
	VkAccelerationStructureDeviceAddressInfoKHR addressInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	addressInfo.accelerationStructure = blAccelStruct->accelstruct;
	VkDeviceAddress blAccelStructAddress = vkGetAccelerationStructureDeviceAddressKHR(GetVulkanFrameBuffer()->device->device, &addressInfo);

	VkAccelerationStructureInstanceKHR instance = {};
	instance.transform.matrix[0][0] = 1.0f;
	instance.transform.matrix[1][1] = 1.0f;
	instance.transform.matrix[2][2] = 1.0f;
	instance.instanceCustomIndex = 0;
	instance.accelerationStructureReference = blAccelStructAddress;
	instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
	instance.mask = 0xff;
	instance.instanceShaderBindingTableRecordOffset = 0;

	BufferBuilder tbuilder;
	tbuilder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	tbuilder.setSize(sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer = tbuilder.create(GetVulkanFrameBuffer()->device);
	tlTransferBuffer->SetDebugName("tlTransferBuffer");
	auto data = (uint8_t*)tlTransferBuffer->Map(0, sizeof(VkAccelerationStructureInstanceKHR));
	memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
	tlTransferBuffer->Unmap();

	BufferBuilder instbufbuilder;
	instbufbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
	instbufbuilder.setSize(sizeof(VkAccelerationStructureInstanceKHR));
	tlInstanceBuffer = instbufbuilder.create(GetVulkanFrameBuffer()->device);
	tlInstanceBuffer->SetDebugName("tlInstanceBuffer");

	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->copyBuffer(tlTransferBuffer.get(), tlInstanceBuffer.get());

	// Finish transfering before using it as input
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->pipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	VkBufferDeviceAddressInfo info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = tlInstanceBuffer->buffer;
	VkDeviceAddress instanceBufferAddress = vkGetBufferDeviceAddress(GetVulkanFrameBuffer()->device->device, &info);

	VkAccelerationStructureGeometryInstancesDataKHR instances = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instances.data.deviceAddress = instanceBufferAddress;

	VkAccelerationStructureGeometryKHR accelStructTLDesc = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	accelStructTLDesc.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelStructTLDesc.geometry.instances = instances;

	VkAccelerationStructureBuildRangeInfoKHR rangeInfo = {};
	rangeInfo.primitiveCount = 1;

	VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildInfo.geometryCount = 1;
	buildInfo.pGeometries = &accelStructTLDesc;
	buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

	uint32_t maxInstanceCount = 1;

	VkAccelerationStructureBuildSizesInfoKHR sizeInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(GetVulkanFrameBuffer()->device->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &maxInstanceCount, &sizeInfo);

	BufferBuilder tlbufbuilder;
	tlbufbuilder.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
	tlbufbuilder.setSize(sizeInfo.accelerationStructureSize);
	tlAccelStructBuffer = tlbufbuilder.create(GetVulkanFrameBuffer()->device);
	tlAccelStructBuffer->SetDebugName("tlAccelStructBuffer");

	VkAccelerationStructureKHR tlAccelStructHandle = {};
	VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	createInfo.buffer = tlAccelStructBuffer->buffer;
	createInfo.size = sizeInfo.accelerationStructureSize;
	VkResult result = vkCreateAccelerationStructureKHR(GetVulkanFrameBuffer()->device->device, &createInfo, nullptr, &tlAccelStructHandle);
	if (result != VK_SUCCESS)
		throw std::runtime_error("vkCreateAccelerationStructureKHR failed");
	tlAccelStruct = std::make_unique<VulkanAccelerationStructure>(GetVulkanFrameBuffer()->device, tlAccelStructHandle);

	BufferBuilder sbuilder;
	sbuilder.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	sbuilder.setSize(sizeInfo.buildScratchSize);
	tlScratchBuffer = sbuilder.create(GetVulkanFrameBuffer()->device);
	tlScratchBuffer->SetDebugName("tlScratchBuffer");

	info = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
	info.buffer = tlScratchBuffer->buffer;
	VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(GetVulkanFrameBuffer()->device->device, &info);

	buildInfo.dstAccelerationStructure = tlAccelStruct->accelstruct;
	buildInfo.scratchData.deviceAddress = scratchAddress;

	VkAccelerationStructureBuildRangeInfoKHR* rangeInfos[] = { &rangeInfo };
	GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands()->buildAccelerationStructures(1, &buildInfo, rangeInfos);

	// Finish building the accel struct before using as input in a fragment shader
	PipelineBarrier finishbuildbarrier;
	finishbuildbarrier.addMemory(VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR, VK_ACCESS_SHADER_READ_BIT);
	finishbuildbarrier.execute(GetVulkanFrameBuffer()->GetCommands()->GetTransferCommands(), VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}
