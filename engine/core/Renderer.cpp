#include <Core.h>

#include <GLFW/glfw3.h>

#include <limits>
#include <vector>


VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace eg::Renderer
{
	static vk::Instance gInstance;
	static vk::DebugUtilsMessengerEXT gDbgMsg;
	static vk::SurfaceKHR gSurface;
	static vk::PhysicalDevice gPhysicalDevice;
	static uint32_t gGraphicsQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
	static vk::Device gDevice;
	static vk::Queue gMainQueue;
	static vk::SwapchainKHR gSwapchain;
	static std::vector<vk::Image> gSwapchainImages;

	static VKAPI_ATTR VkBool32 VKAPI_CALL gDebugCallbackFn(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{

		Logger::gInfo(std::string("Validation layer: ") + pCallbackData->pMessage);

		return VK_FALSE;
	}

	void create(uint32_t width, uint32_t height)
	{
		VULKAN_HPP_DEFAULT_DISPATCHER.init();
		//Create instance
		vk::ApplicationInfo instanceAI{};
		instanceAI.setPApplicationName("EnGAGE")
			.setApplicationVersion(VK_MAKE_API_VERSION(1, 1, 0, 0))
			.setPEngineName("EnGAGE")
			.setEngineVersion(VK_MAKE_API_VERSION(1, 1, 0, 0))
			.setApiVersion(VK_API_VERSION_1_3);
		vk::InstanceCreateInfo instanceCI{};
		instanceCI.setPApplicationInfo(&instanceAI);

		std::vector<const char*> enabledExtensions =
		{
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME
		};
		std::vector<const char*> deviceEnabledExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
		std::vector<const char*> enabledLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};


		//Load glfw required extensions
		Logger::gInfo("Loading GLFW extensions for vulkan !");
		{
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;

			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			for (size_t i = 0; i < glfwExtensionCount; i++)
			{
				Logger::gInfo(std::string("GLFW Extension: ") + glfwExtensions[i]);
				enabledExtensions.push_back(glfwExtensions[i]);
			}
		}


		instanceCI.setPEnabledExtensionNames(enabledExtensions);
		instanceCI.setPEnabledLayerNames(enabledLayers);

		gInstance = vk::createInstance(instanceCI);
		VULKAN_HPP_DEFAULT_DISPATCHER.init(gInstance);

		//Create debug messenger
#ifndef NDEBUG
		{
			vk::DebugUtilsMessengerCreateInfoEXT dbgMsgCI{};
			dbgMsgCI.messageSeverity =
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
				vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
			dbgMsgCI.messageType =
				vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
				vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
				vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
			dbgMsgCI.pfnUserCallback = gDebugCallbackFn;
			dbgMsgCI.pUserData = nullptr; // Optional

			vk::Result result = gInstance.createDebugUtilsMessengerEXT(&dbgMsgCI, nullptr, &gDbgMsg);
			if (result != vk::Result::eSuccess)
			{
				throw std::runtime_error("Failed to create debug messenger !");
			}
			Logger::gInfo("Debug messenger created !");
		}
#endif

		//Create window surface
		glfwCreateWindowSurface(gInstance, Window::getHandle(), nullptr, reinterpret_cast<VkSurfaceKHR*>(&gSurface));

		//Get physical device
		auto physicalDevices = gInstance.enumeratePhysicalDevices();
		if (physicalDevices.size() == 0)
		{
			throw std::runtime_error("No physical devices found !");
		}

		//Pick suitable physical device
		for (const auto& device : physicalDevices)
		{
			auto properties = device.getProperties();
			auto features = device.getFeatures();
			auto queueFamilies = device.getQueueFamilyProperties();

			//Select device with geometry shader and presentation support
			for (uint32_t i = 0; i < queueFamilies.size(); i++)
			{
				if (features.geometryShader && device.getSurfaceSupportKHR(i, gSurface))
				{
					gPhysicalDevice = device;
					break;
				}
			}
		}

		if (!gPhysicalDevice)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		//Select queue family
		auto queueFamilies = gPhysicalDevice.getQueueFamilyProperties();
		for (uint32_t i = 0; i < queueFamilies.size(); i++)
		{
			if (queueFamilies[i].queueFlags & vk::QueueFlagBits::eGraphics)
			{
				gGraphicsQueueFamilyIndex = i;
			}
		}
		if (gGraphicsQueueFamilyIndex == UINT32_MAX)
		{
			throw std::runtime_error("Failed to find suitable queue family !");
		}

		//Create logical device
		const float queuePriorities[] = {1.0f};
		std::vector<vk::DeviceQueueCreateInfo> queueCIs =
		{
			vk::DeviceQueueCreateInfo{

				{},
				gGraphicsQueueFamilyIndex,
				1,
				queuePriorities
			}
		};

		vk::PhysicalDeviceFeatures features;
		vk::DeviceCreateInfo deviceCI{};
		deviceCI.setQueueCreateInfos(queueCIs)
			.setPEnabledFeatures(&features)
			.setPEnabledExtensionNames(deviceEnabledExtensions);

#ifndef NDEBUG
		deviceCI.setPEnabledLayerNames(enabledLayers);
#endif

		gDevice = gPhysicalDevice.createDevice(deviceCI);
		if (!gDevice)
		{
			throw std::runtime_error("Failed to create logical device !");
		}
		Logger::gInfo("Logical device created !");


		//Get main queue
		gMainQueue = gDevice.getQueue(gGraphicsQueueFamilyIndex, 0);

		//Create swapchain
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = gPhysicalDevice.getSurfaceCapabilitiesKHR(gSurface);
		vk::SurfaceFormatKHR selectedSurfaceFormat = vk::SurfaceFormatKHR
		{
			vk::Format::eB8G8R8A8Unorm,
			vk::ColorSpaceKHR::eSrgbNonlinear
		};
		vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eFifo;
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		{
			imageCount = surfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchainCI{};
		swapchainCI.setSurface(gSurface)
			.setMinImageCount(imageCount)
			.setImageFormat(selectedSurfaceFormat.format)
			.setImageColorSpace(selectedSurfaceFormat.colorSpace)
			.setImageExtent(vk::Extent2D(width, height))
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(vk::SharingMode::eExclusive)
			.setPreTransform(surfaceCapabilities.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(selectedPresentMode)
			.setClipped(true)
			.setQueueFamilyIndices({ gGraphicsQueueFamilyIndex })
			.setPreTransform(surfaceCapabilities.currentTransform)	
			.setClipped(true)
			.setOldSwapchain(nullptr);
		gSwapchain = gDevice.createSwapchainKHR(swapchainCI);
		if (!gSwapchain)
		{
			throw std::runtime_error("Failed to create swapchain !");
		}
		Logger::gInfo("Swapchain created !");
		gSwapchainImages = gDevice.getSwapchainImagesKHR(gSwapchain);
		
	}
	void destory()
	{
		gDevice.destroySwapchainKHR(gSwapchain);
		gDevice.destroy();
		gInstance.destroySurfaceKHR(gSurface);
		gInstance.destroyDebugUtilsMessengerEXT(gDbgMsg);
		gInstance.destroy();
	}


	vk::Instance getInstance()
	{
		return gInstance;
	}
}