
#pragma once

#include "zvulkan/vulkanobjects.h"
#include <list>

class VulkanRenderDevice;
class VkHardwareBuffer;
class VkHardwareDataBuffer;
class VkStreamBuffer;
class IBuffer;
struct FVertexBufferAttribute;
struct HWViewpointUniforms;
struct FFlatVertex;

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

	struct
	{
		std::unique_ptr<VulkanBuffer> VertexBuffer;
		int VertexFormat = 0;
		FFlatVertex* Vertices = nullptr;
		unsigned int ShadowDataSize = 0;
		unsigned int CurIndex = 0;
		static const unsigned int BUFFER_SIZE = 2000000;
		static const unsigned int BUFFER_SIZE_TO_USE = BUFFER_SIZE - 500;
		std::unique_ptr<VulkanBuffer> IndexBuffer;
	} Flatbuffer;

	struct
	{
		int UploadIndex = 0;
		int BlockAlign = 0;
		int Count = 1000;
		std::unique_ptr<VulkanBuffer> UBO;
		void* Data = nullptr;
	} Viewpoint;

	struct
	{
		int UploadIndex = 0;
		int Count = 80000;
		std::unique_ptr<VulkanBuffer> UBO;
		void* Data = nullptr;
	} Lightbuffer;

	struct
	{
		int UploadIndex = 0;
		int Count = 80000;
		std::unique_ptr<VulkanBuffer> SSO;
		void* Data = nullptr;
	} Bonebuffer;

	struct
	{
		std::unique_ptr<VkHardwareDataBuffer> Nodes;
		std::unique_ptr<VkHardwareDataBuffer> Lines;
		std::unique_ptr<VkHardwareDataBuffer> Lights;
	} Shadowmap;

	std::unique_ptr<VkStreamBuffer> MatrixBuffer;
	std::unique_ptr<VkStreamBuffer> StreamBuffer;

	std::unique_ptr<IBuffer> FanToTrisIndexBuffer;

private:
	void CreateFanToTrisIndexBuffer();

	VulkanRenderDevice* fb = nullptr;

	std::list<VkHardwareBuffer*> Buffers;

	friend class VkStreamBuffer;
};

class VkStreamBuffer
{
public:
	VkStreamBuffer(VkBufferManager* buffers, size_t structSize, size_t count);
	~VkStreamBuffer();

	uint32_t NextStreamDataBlock();
	void Reset() { mStreamDataOffset = 0; }

	std::unique_ptr<VulkanBuffer> UBO;
	void* Data = nullptr;

private:
	uint32_t mBlockSize = 0;
	uint32_t mStreamDataOffset = 0;
};
