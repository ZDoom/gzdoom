
#pragma once

#include "zvulkan/vulkanobjects.h"
#include <list>

class VulkanRenderDevice;
class VkHardwareBuffer;
class VkHardwareDataBuffer;
class IBuffer;
class VkRSBuffers;
struct FVertexBufferAttribute;

class VkBufferManager
{
public:
	VkBufferManager(VulkanRenderDevice* fb);
	~VkBufferManager();

	void Init();
	void Deinit();

	IBuffer* CreateVertexBuffer(int numBindingPoints, int numAttributes, size_t stride, const FVertexBufferAttribute* attrs);
	IBuffer* CreateIndexBuffer();

	void AddBuffer(VkHardwareBuffer* buffer);
	void RemoveBuffer(VkHardwareBuffer* buffer);

	VkRSBuffers* GetRSBuffers(int threadIndex) { return RSBuffers[threadIndex].get(); }

	struct
	{
		std::unique_ptr<VkHardwareDataBuffer> Nodes;
		std::unique_ptr<VkHardwareDataBuffer> Lines;
		std::unique_ptr<VkHardwareDataBuffer> Lights;
	} Shadowmap;

	std::unique_ptr<IBuffer> FanToTrisIndexBuffer;

private:
	void CreateFanToTrisIndexBuffer();

	VulkanRenderDevice* fb = nullptr;

	std::list<VkHardwareBuffer*> Buffers;
	std::vector<std::unique_ptr<VkRSBuffers>> RSBuffers;
};
