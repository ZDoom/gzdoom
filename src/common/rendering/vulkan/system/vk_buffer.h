
#pragma once

#include "vulkan/system/vk_objects.h"
#include <list>

class VulkanFrameBuffer;
class VkHardwareBuffer;
class VkHardwareDataBuffer;
class VkStreamBuffer;
class IIndexBuffer;
class IVertexBuffer;
class IDataBuffer;

class VkBufferManager
{
public:
	VkBufferManager(VulkanFrameBuffer* fb);
	~VkBufferManager();

	void Init();
	void Deinit();

	IVertexBuffer* CreateVertexBuffer();
	IIndexBuffer* CreateIndexBuffer();
	IDataBuffer* CreateDataBuffer(int bindingpoint, bool ssbo, bool needsresize);

	void AddBuffer(VkHardwareBuffer* buffer);
	void RemoveBuffer(VkHardwareBuffer* buffer);

	VkHardwareDataBuffer* ViewpointUBO = nullptr;
	VkHardwareDataBuffer* LightBufferSSO = nullptr;
	VkHardwareDataBuffer* LightNodes = nullptr;
	VkHardwareDataBuffer* LightLines = nullptr;
	VkHardwareDataBuffer* LightList = nullptr;

	std::unique_ptr<VkStreamBuffer> MatrixBuffer;
	std::unique_ptr<VkStreamBuffer> StreamBuffer;

	std::unique_ptr<IIndexBuffer> FanToTrisIndexBuffer;

private:
	void CreateFanToTrisIndexBuffer();

	VulkanFrameBuffer* fb = nullptr;

	std::list<VkHardwareBuffer*> Buffers;
};

class VkStreamBuffer
{
public:
	VkStreamBuffer(VkBufferManager* buffers, size_t structSize, size_t count);
	~VkStreamBuffer();

	uint32_t NextStreamDataBlock();
	void Reset() { mStreamDataOffset = 0; }

	VkHardwareDataBuffer* UniformBuffer = nullptr;

private:
	uint32_t mBlockSize = 0;
	uint32_t mStreamDataOffset = 0;
};
