#pragma once

#include "vulkaninstance.h"

class VulkanSurface;

class VulkanCompatibleDevice
{
public:
	VulkanPhysicalDevice* Device = nullptr;

	int GraphicsFamily = -1;
	int PresentFamily = -1;

	bool GraphicsTimeQueries = false;

	std::set<std::string> EnabledDeviceExtensions;
	VulkanDeviceFeatures EnabledFeatures;
};
