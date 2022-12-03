
#include "vulkaninstance.h"
#include "vulkanbuilders.h"
#include <mutex>
#include <set>
#include <string>
#include <cstring>

VulkanInstance::VulkanInstance(std::vector<uint32_t> apiVersionsToTry, std::set<std::string> requiredExtensions, std::set<std::string> optionalExtensions, bool wantDebugLayer)
	: ApiVersionsToTry(std::move(apiVersionsToTry)), RequiredExtensions(std::move(requiredExtensions)), OptionalExtensions(std::move(optionalExtensions)), WantDebugLayer(wantDebugLayer)
{
	try
	{
		ShaderBuilder::Init();
		InitVolk();
		CreateInstance();
	}
	catch (...)
	{
		ReleaseResources();
		throw;
	}
}

VulkanInstance::~VulkanInstance()
{
	ReleaseResources();
}

void VulkanInstance::ReleaseResources()
{
	if (debugMessenger)
		vkDestroyDebugUtilsMessengerEXT(Instance, debugMessenger, nullptr);
	debugMessenger = VK_NULL_HANDLE;

	if (Instance)
		vkDestroyInstance(Instance, nullptr);
	Instance = nullptr;
}

void VulkanInstance::InitVolk()
{
	if (volkInitialize() != VK_SUCCESS)
	{
		VulkanError("Unable to find Vulkan");
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		VulkanError("Vulkan not supported");
	}
}

void VulkanInstance::CreateInstance()
{
	AvailableLayers = GetAvailableLayers();
	AvailableExtensions = GetExtensions();
	EnabledExtensions = RequiredExtensions;

	std::string debugLayer = "VK_LAYER_KHRONOS_validation";
	bool debugLayerFound = false;
	if (WantDebugLayer)
	{
		for (const VkLayerProperties& layer : AvailableLayers)
		{
			if (layer.layerName == debugLayer)
			{
				EnabledValidationLayers.insert(layer.layerName);
				EnabledExtensions.insert(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
				debugLayerFound = true;
				break;
			}
		}
	}

	// Enable optional instance extensions we are interested in
	for (const auto& ext : AvailableExtensions)
	{
		if (OptionalExtensions.find(ext.extensionName) != OptionalExtensions.end())
		{
			EnabledExtensions.insert(ext.extensionName);
		}
	}

	std::vector<const char*> enabledValidationLayersCStr;
	for (const std::string& layer : EnabledValidationLayers)
		enabledValidationLayersCStr.push_back(layer.c_str());

	std::vector<const char*> enabledExtensionsCStr;
	for (const std::string& ext : EnabledExtensions)
		enabledExtensionsCStr.push_back(ext.c_str());

	// Try get the highest vulkan version we can get
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;
	for (uint32_t apiVersion : ApiVersionsToTry)
	{
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "VulkanDrv";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "VulkanDrv";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = apiVersion;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = (uint32_t)EnabledExtensions.size();
		createInfo.enabledLayerCount = (uint32_t)enabledValidationLayersCStr.size();
		createInfo.ppEnabledLayerNames = enabledValidationLayersCStr.data();
		createInfo.ppEnabledExtensionNames = enabledExtensionsCStr.data();

		result = vkCreateInstance(&createInfo, nullptr, &Instance);
		if (result >= VK_SUCCESS)
		{
			ApiVersion = apiVersion;
			break;
		}
	}
	CheckVulkanError(result, "Could not create vulkan instance");

	volkLoadInstance(Instance);

	if (debugLayerFound)
	{
		VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo = {};
		dbgCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		dbgCreateInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		dbgCreateInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		dbgCreateInfo.pfnUserCallback = DebugCallback;
		dbgCreateInfo.pUserData = this;
		result = vkCreateDebugUtilsMessengerEXT(Instance, &dbgCreateInfo, nullptr, &debugMessenger);
		CheckVulkanError(result, "vkCreateDebugUtilsMessengerEXT failed");

		DebugLayerActive = true;
	}

	PhysicalDevices = GetPhysicalDevices(Instance, ApiVersion);
}

std::vector<VulkanPhysicalDevice> VulkanInstance::GetPhysicalDevices(VkInstance instance, uint32_t apiVersion)
{
	uint32_t deviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (result == VK_ERROR_INITIALIZATION_FAILED) // Some drivers return this when a card does not support vulkan
		return {};
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed");
	if (deviceCount == 0)
		return {};

	std::vector<VkPhysicalDevice> devices(deviceCount);
	result = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	CheckVulkanError(result, "vkEnumeratePhysicalDevices failed (2)");

	std::vector<VulkanPhysicalDevice> devinfo(deviceCount);
	for (size_t i = 0; i < devices.size(); i++)
	{
		auto& dev = devinfo[i];
		dev.Device = devices[i];

		vkGetPhysicalDeviceMemoryProperties(dev.Device, &dev.MemoryProperties);
		vkGetPhysicalDeviceProperties(dev.Device, &dev.Properties);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, nullptr);
		dev.QueueFamilies.resize(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(dev.Device, &queueFamilyCount, dev.QueueFamilies.data());

		uint32_t deviceExtensionCount = 0;
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, nullptr);
		dev.Extensions.resize(deviceExtensionCount);
		vkEnumerateDeviceExtensionProperties(dev.Device, nullptr, &deviceExtensionCount, dev.Extensions.data());

		auto checkForExtension = [&](const char* name)
		{
			for (const auto& ext : dev.Extensions)
			{
				if (strcmp(ext.extensionName, name) == 0)
					return true;
			}
			return false;
		};

		if (apiVersion != VK_API_VERSION_1_0)
		{
			VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

			void** next = const_cast<void**>(&deviceFeatures2.pNext);
			if (checkForExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
			{
				*next = &dev.Features.BufferDeviceAddress;
				next = &dev.Features.BufferDeviceAddress.pNext;
			}
			if (checkForExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
			{
				*next = &dev.Features.AccelerationStructure;
				next = &dev.Features.AccelerationStructure.pNext;
			}
			if (checkForExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME))
			{
				*next = &dev.Features.RayQuery;
				next = &dev.Features.RayQuery.pNext;
			}
			if (checkForExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME))
			{
				*next = &dev.Features.DescriptorIndexing;
				next = &dev.Features.DescriptorIndexing.pNext;
			}

			vkGetPhysicalDeviceFeatures2(dev.Device, &deviceFeatures2);
			dev.Features.Features = deviceFeatures2.features;
			dev.Features.BufferDeviceAddress.pNext = nullptr;
			dev.Features.AccelerationStructure.pNext = nullptr;
			dev.Features.RayQuery.pNext = nullptr;
			dev.Features.DescriptorIndexing.pNext = nullptr;
		}
		else
		{
			vkGetPhysicalDeviceFeatures(dev.Device, &dev.Features.Features);
		}
	}
	return devinfo;
}

std::vector<VkLayerProperties> VulkanInstance::GetAvailableLayers()
{
	uint32_t layerCount;
	VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	return availableLayers;
}

std::vector<VkExtensionProperties> VulkanInstance::GetExtensions()
{
	uint32_t extensionCount = 0;
	VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	result = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
	return extensions;
}

VkBool32 VulkanInstance::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData)
{
	VulkanInstance* instance = (VulkanInstance*)userData;

	static std::mutex mtx;
	static std::set<std::string> seenMessages;
	static int totalMessages;

	std::unique_lock<std::mutex> lock(mtx);

	std::string msg = callbackData->pMessage;

	// Attempt to parse the string because the default formatting is totally unreadable and half of what it writes is totally useless!
	auto parts = SplitString(msg, " | ");
	if (parts.size() == 3)
	{
		msg = parts[2];
		size_t pos = msg.find(" The Vulkan spec states:");
		if (pos != std::string::npos)
			msg = msg.substr(0, pos);

		if (callbackData->objectCount > 0)
		{
			msg += " (";
			for (uint32_t i = 0; i < callbackData->objectCount; i++)
			{
				if (i > 0)
					msg += ", ";
				if (callbackData->pObjects[i].pObjectName)
					msg += callbackData->pObjects[i].pObjectName;
				else
					msg += "<noname>";
			}
			msg += ")";
		}
	}

	bool found = seenMessages.find(msg) != seenMessages.end();
	if (!found)
	{
		if (totalMessages < 20)
		{
			totalMessages++;
			seenMessages.insert(msg);

			const char* typestr;
			bool showcallstack = false;
			if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			{
				typestr = "vulkan error";
				showcallstack = true;
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			{
				typestr = "vulkan warning";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			{
				typestr = "vulkan info";
			}
			else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
			{
				typestr = "vulkan verbose";
			}
			else
			{
				typestr = "vulkan";
			}

			VulkanPrintLog(typestr, msg);
		}
	}

	return VK_FALSE;
}

std::vector<std::string> VulkanInstance::SplitString(const std::string& s, const std::string& seperator)
{
	std::vector<std::string> output;
	std::string::size_type prev_pos = 0, pos = 0;

	while ((pos = s.find(seperator, pos)) != std::string::npos)
	{
		std::string substring(s.substr(prev_pos, pos - prev_pos));

		output.push_back(substring);

		pos += seperator.length();
		prev_pos = pos;
	}

	output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word
	return output;
}

std::string VkResultToString(VkResult result)
{
	switch (result)
	{
	case VK_SUCCESS: return "success";
	case VK_NOT_READY: return "not ready";
	case VK_TIMEOUT: return "timeout";
	case VK_EVENT_SET: return "event set";
	case VK_EVENT_RESET: return "event reset";
	case VK_INCOMPLETE: return "incomplete";
	case VK_ERROR_OUT_OF_HOST_MEMORY: return "out of host memory";
	case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "out of device memory";
	case VK_ERROR_INITIALIZATION_FAILED: return "initialization failed";
	case VK_ERROR_DEVICE_LOST: return "device lost";
	case VK_ERROR_MEMORY_MAP_FAILED: return "memory map failed";
	case VK_ERROR_LAYER_NOT_PRESENT: return "layer not present";
	case VK_ERROR_EXTENSION_NOT_PRESENT: return "extension not present";
	case VK_ERROR_FEATURE_NOT_PRESENT: return "feature not present";
	case VK_ERROR_INCOMPATIBLE_DRIVER: return "incompatible driver";
	case VK_ERROR_TOO_MANY_OBJECTS: return "too many objects";
	case VK_ERROR_FORMAT_NOT_SUPPORTED: return "format not supported";
	case VK_ERROR_FRAGMENTED_POOL: return "fragmented pool";
	case VK_ERROR_OUT_OF_POOL_MEMORY: return "out of pool memory";
	case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "invalid external handle";
	case VK_ERROR_SURFACE_LOST_KHR: return "surface lost";
	case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "native window in use";
	case VK_SUBOPTIMAL_KHR: return "suboptimal";
	case VK_ERROR_OUT_OF_DATE_KHR: return "out of date";
	case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "incompatible display";
	case VK_ERROR_VALIDATION_FAILED_EXT: return "validation failed";
	case VK_ERROR_INVALID_SHADER_NV: return "invalid shader";
	case VK_ERROR_FRAGMENTATION_EXT: return "fragmentation";
	case VK_ERROR_NOT_PERMITTED_EXT: return "not permitted";
	case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: return "full screen exclusive mode lost";
	case VK_THREAD_IDLE_KHR: return "thread idle";
	case VK_THREAD_DONE_KHR: return "thread done";
	case VK_OPERATION_DEFERRED_KHR: return "operation deferred";
	case VK_OPERATION_NOT_DEFERRED_KHR: return "operation not deferred";
	case VK_PIPELINE_COMPILE_REQUIRED_EXT: return "pipeline compile required";
	default: break;
	}
	return "vkResult " + std::to_string((int)result);
}
