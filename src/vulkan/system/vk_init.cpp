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
#ifndef NO_VULKAN
#define VK_USE_PLATFORM_WIN32_KHR
// Note: volk currently doesn't work on macOS. This may need some fixing if that doesn't change until this code is ready for release.
#include <vector>
#include "volk/volk.h"
#endif
#include "version.h"


// This does not report any specific errors. If Vulkan setup fails, the engine will switch to OpenGL instead.
bool InitVulkan()
{
#ifdef NO_VULKAN
	return false
#else
	// Init the library
	if (volkInitialize() != VK_SUCCESS) 
	{
		// Vulkan not found. 
		return false;
	}
	auto iver = volkGetInstanceVersion();
	if (iver == 0)
	{
		// Vulkan not supported.
		return false;
	}
	VkInstance instance;
	
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = GAMENAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(VER_MAJOR, VER_MINOR, VER_REVISION);
	appInfo.pEngineName = "GZDoom";	// not GAMENAME!
	appInfo.engineVersion = VK_MAKE_VERSION(ENG_MAJOR, ENG_MINOR, ENG_REVISION);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	std::vector<const char*> enabledExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	#if defined(_WIN32)
		enabledExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
	#elif defined(__linux__)
		enabledExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
	#endif	
	// todo: macOS
	
	createInfo.enabledExtensionCount = 0;
	createInfo.ppEnabledExtensionNames = nullptr;

	createInfo.enabledLayerCount = 0;

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) 
	{
		return false;
	}
	
	return true;
#endif
}


