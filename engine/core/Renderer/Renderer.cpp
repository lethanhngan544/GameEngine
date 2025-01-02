#include <Core.h>
#include <GLFW/glfw3.h>

#include <limits>
#include <vector>
#include <optional>
#include <shaderc/shaderc.hpp>
#include <glm/glm.hpp>
#define VMA_IMPLEMENTATION
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace eg::Renderer
{
	struct FrameData
	{
		vk::Semaphore presentSemaphore, renderSemaphore;
		vk::Fence renderFence;
		vk::CommandBuffer commandBuffer;
	};

	
	static shaderc::Compiler gShaderCompiler;
	static shaderc::CompileOptions gShaderCompilerOptions;
	static vk::Rect2D gDrawExtent;
	static vk::Instance gInstance;
	static vk::DebugUtilsMessengerEXT gDbgMsg;
	static vk::SurfaceKHR gSurface;
	static vk::PhysicalDevice gPhysicalDevice;
	static uint32_t gGraphicsQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
	static vk::Device gDevice;
	static vma::Allocator gAllocator;

	static vk::Queue gMainQueue;
	static vk::PresentModeKHR gPresentMode = vk::PresentModeKHR::eFifo;
	static vk::SurfaceFormatKHR gSurfaceFormat = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	static vk::SwapchainKHR gSwapchain;
	static std::vector<vk::Image> gSwapchainImages;
	static std::vector<vk::ImageView> gSwapchainImageViews;
	//Double buffer setup

	static vk::CommandPool gCommandPool;
	static constexpr size_t gFrameCount = 2;
	static FrameData gFrameData[gFrameCount];
	static uint32_t gCurrentFrame = 0;

	static std::optional<DefaultPipeline> gDefaultPipeline;

	static std::optional<VertexBuffer> gVertexBuffer;
	static std::optional<IndexBuffer> gIndexBuffer;

	static VKAPI_ATTR VkBool32 VKAPI_CALL gDebugCallbackFn(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{

		Logger::gInfo(std::string("Validation layer: ") + pCallbackData->pMessage);

		return VK_FALSE;
	}

	std::vector<uint32_t> compileShaderFromFile(const std::string& filePath, uint32_t kind)
	{
		Logger::gTrace("Compiling shader: " + filePath);
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open shader file: " + filePath);
		}
		std::stringstream ss;
		ss << file.rdbuf();
		std::string data = ss.str();



		gShaderCompilerOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
		shaderc::SpvCompilationResult module = gShaderCompiler.CompileGlslToSpv(data, 
			(shaderc_shader_kind)kind, 
			filePath.c_str(), gShaderCompilerOptions);

		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			Logger::gError(module.GetErrorMessage());
			throw std::runtime_error("Failed to compile shader: " + filePath);
		}

		return { module.cbegin(), module.cend() };
	}

	void immediateSubmit(std::function<void(vk::CommandBuffer cmd)>&& function)
	{
		vk::CommandBufferAllocateInfo cmdAI{};
		cmdAI.setCommandPool(gCommandPool)
			.setLevel(vk::CommandBufferLevel::ePrimary)
			.setCommandBufferCount(1);
		auto cmdBuffer = gDevice.allocateCommandBuffers(cmdAI)[0];
		vk::CommandBufferBeginInfo cmdBeginInfo{};
		cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		cmdBuffer.begin(cmdBeginInfo);
		function(cmdBuffer);
		cmdBuffer.end();
		vk::SubmitInfo submitInfo{};
		submitInfo.setCommandBufferCount(1)
			.setPCommandBuffers(&cmdBuffer);
		gMainQueue.submit(submitInfo, nullptr);
		gMainQueue.waitIdle();
		gDevice.freeCommandBuffers(gCommandPool, cmdBuffer);
	}

	void create(uint32_t width, uint32_t height)
	{
		gDrawExtent = vk::Rect2D{ {0, 0}, {width, height} };
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
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME,

		};
		std::vector<const char*> deviceEnabledExtensions =
		{
			VK_KHR_SWAPCHAIN_EXTENSION_NAME,
			VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
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
		const float queuePriorities[] = { 1.0f };
		std::vector<vk::DeviceQueueCreateInfo> queueCIs =
		{
			vk::DeviceQueueCreateInfo{

				{},
				gGraphicsQueueFamilyIndex,
				1,
				queuePriorities
			}
		};


		vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
		dynamicRenderingFeatures.setDynamicRendering(true);
		vk::PhysicalDeviceFeatures2 features2{};
		features2.setPNext(&dynamicRenderingFeatures);


		vk::DeviceCreateInfo deviceCI{};
		deviceCI.setQueueCreateInfos(queueCIs)
			.setPEnabledExtensionNames(deviceEnabledExtensions)
			.setPNext(&features2);

		gDevice = gPhysicalDevice.createDevice(deviceCI);
		if (!gDevice)
		{
			throw std::runtime_error("Failed to create logical device !");
		}
		Logger::gInfo("Logical device created !");
		VULKAN_HPP_DEFAULT_DISPATCHER.init(gDevice);


		//Get main queue
		gMainQueue = gDevice.getQueue(gGraphicsQueueFamilyIndex, 0);

		//Create allocator
		vma::AllocatorCreateInfo allocatorCI{};
		vma::VulkanFunctions funcs = vma::functionsFromDispatcher(VULKAN_HPP_DEFAULT_DISPATCHER);
		allocatorCI.setPhysicalDevice(gPhysicalDevice)
			.setDevice(gDevice)
			.setInstance(gInstance)
			.setPVulkanFunctions(&funcs);

		gAllocator = vma::createAllocator(allocatorCI);


		//Create swapchain
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = gPhysicalDevice.getSurfaceCapabilitiesKHR(gSurface);
		uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
		{
			imageCount = surfaceCapabilities.maxImageCount;
		}

		vk::SwapchainCreateInfoKHR swapchainCI{};
		swapchainCI.setSurface(gSurface)
			.setMinImageCount(imageCount)
			.setImageFormat(gSurfaceFormat.format)
			.setImageColorSpace(gSurfaceFormat.colorSpace)
			.setImageExtent(gDrawExtent.extent)
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
			.setImageSharingMode(vk::SharingMode::eExclusive)
			.setPreTransform(surfaceCapabilities.currentTransform)
			.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
			.setPresentMode(gPresentMode)
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

		//Create image views
		gSwapchainImageViews.reserve(gSwapchainImages.size());
		for (size_t i = 0; i < gSwapchainImages.size(); i++)
		{
			vk::ImageViewCreateInfo imageViewCI{};
			imageViewCI.setImage(gSwapchainImages[i])
				.setViewType(vk::ImageViewType::e2D)
				.setFormat(gSurfaceFormat.format)
				.setComponents(vk::ComponentMapping{})
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			gSwapchainImageViews.push_back(gDevice.createImageView(imageViewCI));
		}
		//Create command pool
		vk::CommandPoolCreateInfo commandPoolCI{};
		commandPoolCI.setQueueFamilyIndex(gGraphicsQueueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		gCommandPool = gDevice.createCommandPool(commandPoolCI);


		//Create frame data
		for (size_t i = 0; i < gFrameCount; i++)
		{
			FrameData& frameData = gFrameData[i];
			vk::SemaphoreCreateInfo semaphoreCI{};
			vk::FenceCreateInfo fenceCI{};
			vk::CommandPoolCreateInfo commandPoolCI{};
			vk::CommandBufferAllocateInfo commandBufferAI{};
			frameData.presentSemaphore = gDevice.createSemaphore(semaphoreCI);
			frameData.renderSemaphore = gDevice.createSemaphore(semaphoreCI);
			frameData.renderFence = gDevice.createFence(fenceCI);
			commandPoolCI.setQueueFamilyIndex(gGraphicsQueueFamilyIndex)
				.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
			commandBufferAI.setCommandPool(gCommandPool)
				.setLevel(vk::CommandBufferLevel::ePrimary)
				.setCommandBufferCount(1);
			frameData.commandBuffer = gDevice.allocateCommandBuffers(commandBufferAI)[0];
		}

		gDefaultPipeline.emplace();


		std::vector<DefaultPipeline::Vertex> vertices = {
			{ {0.0f, -0.5f, 0.0f} },
			{ {0.5f, 0.5f, 0.0f} },
			{ {-0.5f, 0.5f, 0.0f} }
		};

		std::vector<uint32_t> indices = { 0, 1, 2 };

		gVertexBuffer.emplace(vertices.data(), sizeof(DefaultPipeline::Vertex) * vertices.size());
		gIndexBuffer.emplace(indices.data(), sizeof(uint32_t) * indices.size());
	}

	void drawTestTriangle()
	{
		auto& frameData = gFrameData[gCurrentFrame];
		if (gDevice.waitForFences(frameData.renderFence, VK_TRUE, 1000000000) != vk::Result::eSuccess)
		{
			Logger::gWarn("Failed to wait for fence !");
		}
		gDevice.resetFences(frameData.renderFence);
		uint32_t imageIndex = gDevice.acquireNextImageKHR(gSwapchain, 1000000000, frameData.presentSemaphore, nullptr).value;

		//Draw test triangle
		vk::CommandBuffer& cmd = frameData.commandBuffer;
		vk::CommandBufferBeginInfo commandBufferBI{};
		commandBufferBI.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

		cmd.begin(commandBufferBI);


		//Transition image layout
		vk::ImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.setOldLayout(vk::ImageLayout::eUndefined)
			.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
			.setImage(gSwapchainImages[imageIndex])
			.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
			.setSrcAccessMask(vk::AccessFlags{})
			.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::DependencyFlags{},
			nullptr, nullptr, imageMemoryBarrier);

		vk::RenderingAttachmentInfo colorAttachmentInfo{};
		colorAttachmentInfo.setImageView(gSwapchainImageViews[imageIndex])
			.setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore);
		vk::ClearValue clearColor = vk::ClearColorValue{ std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f} };
		colorAttachmentInfo.setClearValue(clearColor);

		vk::RenderingInfo rendering{};
		rendering.setLayerCount(1)
			.setColorAttachments(colorAttachmentInfo)
			.setRenderArea(gDrawExtent);
		cmd.beginRendering(rendering);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, gDefaultPipeline->getPipeline());
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(gDrawExtent.extent.width),
			static_cast<float>(gDrawExtent.extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, gDrawExtent);

		cmd.bindVertexBuffers(0, { gVertexBuffer->getBuffer() }, {0});
		cmd.bindIndexBuffer(gIndexBuffer->getBuffer(), 0, vk::IndexType::eUint32);
		cmd.drawIndexed(3, 1, 0, 0, 0);


		cmd.endRendering();

		//Transition to present layout
		imageMemoryBarrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
			.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
			.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
			vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags{},
			nullptr, nullptr, imageMemoryBarrier);

		cmd.end();


		vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
		vk::SubmitInfo submitInfo{};
		submitInfo.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&frameData.presentSemaphore)
			.setPWaitDstStageMask(waitStages)
			.setCommandBufferCount(1)
			.setPCommandBuffers(&frameData.commandBuffer)
			.setSignalSemaphoreCount(1)
			.setPSignalSemaphores(&frameData.renderSemaphore);
		gMainQueue.submit(submitInfo, frameData.renderFence);
		vk::PresentInfoKHR presentInfo{};
		presentInfo.setWaitSemaphoreCount(1)
			.setPWaitSemaphores(&frameData.renderSemaphore)
			.setSwapchainCount(1)
			.setPSwapchains(&gSwapchain)
			.setPImageIndices(&imageIndex);
		if (gMainQueue.presentKHR(presentInfo) != vk::Result::eSuccess)
		{
			Logger::gWarn("Failed to present image !");
		}
		gCurrentFrame = (gCurrentFrame + 1) % gFrameCount;

	}
	void destory()
	{
		gDevice.waitIdle();

		gVertexBuffer.reset();
		gIndexBuffer.reset();

		gAllocator.destroy();

		gDefaultPipeline.reset();
		for (size_t i = 0; i < gFrameCount; i++)
		{
			auto& frameData = gFrameData[i];
			gDevice.destroySemaphore(frameData.presentSemaphore);
			gDevice.destroySemaphore(frameData.renderSemaphore);
			gDevice.destroyFence(frameData.renderFence);
			gDevice.freeCommandBuffers(gCommandPool, frameData.commandBuffer);
			
		}
		gDevice.destroyCommandPool(gCommandPool);
		for (auto imageView : gSwapchainImageViews)
		{
			gDevice.destroyImageView(imageView);
		}
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

	vk::Device getDevice()
	{
		return gDevice;
	}
	
	vma::Allocator getAllocator()
	{
		return gAllocator;
	}

}