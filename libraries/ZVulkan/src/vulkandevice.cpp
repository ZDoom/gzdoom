
#include "vulkandevice.h"
#include "vulkanobjects.h"
#include "vulkancompatibledevice.h"
#include <algorithm>
#include <set>
#include <string>

VulkanDevice::VulkanDevice(std::shared_ptr<VulkanInstance> instance, std::shared_ptr<VulkanSurface> surface, const VulkanCompatibleDevice& selectedDevice) : Instance(instance), Surface(surface)
{
	PhysicalDevice = *selectedDevice.Device;
	EnabledDeviceExtensions = selectedDevice.EnabledDeviceExtensions;
	EnabledFeatures = selectedDevice.EnabledFeatures;

	GraphicsFamily = selectedDevice.GraphicsFamily;
	PresentFamily = selectedDevice.PresentFamily;
	GraphicsTimeQueries = selectedDevice.GraphicsTimeQueries;

	try
	{
		CreateDevice();
		CreateAllocator();
	}
	catch (...)
	{
		ReleaseResources();
		throw;
	}
}

VulkanDevice::~VulkanDevice()
{
	ReleaseResources();
}

bool VulkanDevice::SupportsExtension(const char* ext) const
{
	return
		EnabledDeviceExtensions.find(ext) != EnabledDeviceExtensions.end() ||
		Instance->EnabledExtensions.find(ext) != Instance->EnabledExtensions.end();
}

void VulkanDevice::CreateAllocator()
{
	VmaAllocatorCreateInfo allocinfo = {};
	allocinfo.vulkanApiVersion = Instance->ApiVersion;
	if (SupportsExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) && SupportsExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
		allocinfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
	if (SupportsExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
		allocinfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocinfo.physicalDevice = PhysicalDevice.Device;
	allocinfo.device = device;
	allocinfo.instance = Instance->Instance;
	allocinfo.preferredLargeHeapBlockSize = 64 * 1024 * 1024;
	if (vmaCreateAllocator(&allocinfo, &allocator) != VK_SUCCESS)
		VulkanError("Unable to create allocator");
}

void VulkanDevice::CreateDevice()
{
	float queuePriority = 1.0f;
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	std::set<int> neededFamilies;
	if (GraphicsFamily != -1)
		neededFamilies.insert(GraphicsFamily);
	if (PresentFamily != -1)
		neededFamilies.insert(PresentFamily);

	for (int index : neededFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = index;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	std::vector<const char*> extensionNames;
	extensionNames.reserve(EnabledDeviceExtensions.size());
	for (const auto& name : EnabledDeviceExtensions)
		extensionNames.push_back(name.c_str());

	VkDeviceCreateInfo deviceCreateInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceCreateInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = (uint32_t)extensionNames.size();
	deviceCreateInfo.ppEnabledExtensionNames = extensionNames.data();
	deviceCreateInfo.enabledLayerCount = 0;

	VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
	deviceFeatures2.features = EnabledFeatures.Features;

	void** next = const_cast<void**>(&deviceCreateInfo.pNext);
	if (SupportsExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
	{
		*next = &deviceFeatures2;
		next = &deviceFeatures2.pNext;
	}
	else // vulkan 1.0 specified features in a different way
	{
		deviceCreateInfo.pEnabledFeatures = &deviceFeatures2.features;
	}

	if (SupportsExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
	{
		*next = &EnabledFeatures.BufferDeviceAddress;
		next = &EnabledFeatures.BufferDeviceAddress.pNext;
	}
	if (SupportsExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
	{
		*next = &EnabledFeatures.AccelerationStructure;
		next = &EnabledFeatures.AccelerationStructure.pNext;
	}
	if (SupportsExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
	{
		*next = &EnabledFeatures.RayQuery;
		next = &EnabledFeatures.RayQuery.pNext;
	}
	if (SupportsExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
	{
		*next = &EnabledFeatures.DescriptorIndexing;
		next = &EnabledFeatures.DescriptorIndexing.pNext;
	}

	VkResult result = vkCreateDevice(PhysicalDevice.Device, &deviceCreateInfo, nullptr, &device);
	CheckVulkanError(result, "Could not create vulkan device");

	volkLoadDevice(device);

	if (GraphicsFamily != -1)
		vkGetDeviceQueue(device, GraphicsFamily, 0, &GraphicsQueue);
	if (PresentFamily != -1)
		vkGetDeviceQueue(device, PresentFamily, 0, &PresentQueue);
}

void VulkanDevice::ReleaseResources()
{
	if (device)
		vkDeviceWaitIdle(device);

	if (allocator)
		vmaDestroyAllocator(allocator);

	if (device)
		vkDestroyDevice(device, nullptr);
	device = nullptr;
}

void VulkanDevice::SetObjectName(const char* name, uint64_t handle, VkObjectType type)
{
	if (!DebugLayerActive) return;

	VkDebugUtilsObjectNameInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	info.objectHandle = handle;
	info.objectType = type;
	info.pObjectName = name;
	vkSetDebugUtilsObjectNameEXT(device, &info);
}
