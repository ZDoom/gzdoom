// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2018 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
// Uniform buffer management


#include "vk_uniformbuffer.h"


//==========================================================================
//
//
//
//==========================================================================

void VkUniformBuffer::Destroy()
{
	mapcnt = 1;
	Unmap();
	if (uniformBuffer != VK_NULL_HANDLE)
		vmaDestroyBuffer(vDevice->vkAllocator, uniformBuffer, uniformBufferAllocation);
	uniformBuffer = VK_NULL_HANDLE;
	uniformBuffer = VK_NULL_HANDLE;
}

VkResult VkUniformBuffer::Create(size_t size)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;	// or maybe VK_BUFFER_USAGE_STORAGE_BUFFER_BIT???
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	
	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;	// we want this to be mappable.
	return vmaCreateBuffer(vDevice->vkAllocator, &bufferInfo, &allocInfo, &uniformBuffer, &uniformBufferAllocation, nullptr);
	
}
void *VkUniformBuffer::Map()
{
	mapcnt++;
	if (map) return map;
	auto res = vmaMapMemory(vDevice->vkAllocator, uniformBufferAllocation, &map);
	if (res == VK_SUCCESS) return map;
	mapcnt--;
	return nullptr;
}
	
void VkUniformBuffer::Unmap()
{
	mapcnt--;
	if (mapcnt <= 0 && map)
	{
		vmaUnmapMemory(vDevice->vkAllocator, uniformBufferAllocation);
	}	
	map = nullptr;
}
