#include <Data.h>
#include <Core.h>
#include <Components.h>
#include <Window.h>
#include <GLFW/glfw3.h>

#include <limits>
#include <vector>
#include <optional>
#include <shaderc/shaderc.hpp>
#include <glm/glm.hpp>
#define VMA_IMPLEMENTATION
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <thread>
#include <mutex>
#include <condition_variable>

namespace eg::Renderer
{

	std::vector<const char*> gEnabledExtensions =
	{
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,

	};
	std::vector<const char*> gDeviceEnabledExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	std::vector<const char*> gEnabledLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

	struct FrameData
	{
		vk::Semaphore presentSemaphore, renderSemaphore;
		vk::Fence renderFence;
		vk::CommandBuffer commandBuffer;
		uint32_t swapchainIndex = 0;
	};
	static Components::Camera gDummyCamera;
	static const Components::Camera* gCamera = &gDummyCamera;

	static shaderc::Compiler gShaderCompiler;
	static shaderc::CompileOptions gShaderCompilerOptions;
	static Command::Var* gScreenWidth = nullptr;
	static Command::Var* gScreenHeight = nullptr;
	static Command::Var* gScreenRenderScale = nullptr;

	static vk::Instance gInstance;
	static vk::DebugUtilsMessengerEXT gDbgMsg;
	static vk::SurfaceKHR gSurface;
	static vk::PhysicalDevice gPhysicalDevice;
	static uint32_t gGraphicsQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
	static vk::Device gDevice;
	static vma::Allocator gAllocator;

	static vk::Queue gMainQueue;
	static vk::PresentModeKHR gPresentMode = vk::PresentModeKHR::eImmediate;
	static vk::SurfaceFormatKHR gSurfaceFormat = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	static vk::SwapchainKHR gSwapchain;
	static std::vector<vk::Image> gSwapchainImages;
	static std::vector<vk::ImageView> gSwapchainImageViews;
	//Double buffer setup

	static vk::CommandPool gCommandPool;
	static FrameData gFrameData[MAX_FRAMES_IN_FLIGHT];
	static uint32_t gCurrentFrame = 0;

	static vk::DescriptorPool gDescriptorPool;
	static std::optional<GlobalUniformBuffer> gGlobalUniformBuffer;

	//Default resources
	static std::optional<CombinedImageSampler2D> gDefaultWhiteImage;
	static std::optional<CombinedImageSampler2D> gDefaultCheckerboardImage;


	//ImGUI's stuff
	static vk::RenderPass gImGuiRenderPass;
	static vk::Framebuffer gImGuiFramebuffer;

	//Render passes
	static std::optional<DefaultRenderPass> gDefaultRenderPass;
	static std::optional<PostprocessingRenderPass> gPostprocessingRenderPass;

	//Render functions
	static RenderFn dummyRenderFn = [](vk::CommandBuffer cmd) {
		//This is a dummy render function that does nothing
	};

	static RenderFn gShadowRenderFn = dummyRenderFn;
	static RenderFn gBufferRenderFn = dummyRenderFn;
	static RenderFn gLightRenderFn = dummyRenderFn;
	static RenderFn gDebugRenderFn = dummyRenderFn;

	//Multi threading
	static std::mutex gMainThreadMutex;
	static std::condition_variable gMainCV;

	static std::mutex gShadowMutex;
	static std::condition_variable gShadowCV;
	static bool gShadowThreadReady = false;
	static bool gShadowThreadRunning = true;
	static vk::CommandBuffer gShadowCmdBuffers[MAX_FRAMES_IN_FLIGHT];

	static std::mutex gBufferMutex;
	static std::condition_variable gBufferCV;
	static bool gBufferThreadReady = false;
	static bool gBufferThreadRunning = true;
	static vk::CommandBuffer gBufferCmdBuffers[MAX_FRAMES_IN_FLIGHT];

	static std::unique_ptr<std::thread> gShadowThread;
	static std::unique_ptr<std::thread> gBufferThread;

	static VKAPI_ATTR VkBool32 VKAPI_CALL gDebugCallbackFn(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData)
	{
		if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			Logger::gError(std::string("Validation layer: ") + pCallbackData->pMessage);
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			Logger::gWarn(std::string("Validation layer: ") + pCallbackData->pMessage);
		else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			Logger::gInfo(std::string("Validation layer: ") + pCallbackData->pMessage);



		return VK_FALSE;
	}


	std::vector<uint32_t> compileShaderFromFile(const std::string& filePath, uint32_t kind,
		std::vector<std::pair<std::string, std::string>> defines)
	{
		Logger::gTrace("Compiling shader: " + filePath);

		class ShaderIncluder : public shaderc::CompileOptions::IncluderInterface
		{
			shaderc_include_result* GetInclude(const char* requestedSource, shaderc_include_type type, const char* requestingSource, size_t includeDepth) override
			{
				std::string msg = std::string(requestingSource);
				msg += std::to_string(type);
				msg += static_cast<char>(includeDepth);

				const std::string name = std::string(requestedSource);
				const std::string contents = ReadFile(name);

				auto container = new std::array<std::string, 2>;
				(*container)[0] = name;
				(*container)[1] = contents;

				auto data = new shaderc_include_result;

				data->user_data = container;

				data->source_name = (*container)[0].data();
				data->source_name_length = (*container)[0].size();

				data->content = (*container)[1].data();
				data->content_length = (*container)[1].size();

				return data;
			}
			void ReleaseInclude(shaderc_include_result* data) override
			{
				delete static_cast<std::array<std::string, 2>*>(data->user_data);
				delete data;
			}
			static std::string ReadFile(const std::string& path)
			{
				std::string sourceCode;
				std::ifstream in(path, std::ios::in | std::ios::binary);
				if (in)
				{
					in.seekg(0, std::ios::end);
					size_t size = in.tellg();
					if (size > 0)
					{
						sourceCode.resize(size);
						in.seekg(0, std::ios::beg);
						in.read(&sourceCode[0], size);
					}
					else
					{
						Logger::gWarn("compileShaderFromFile, file is empty" + path);
					}
				}
				else
				{
					Logger::gWarn("compileShaderFromFile, can't open shader file file" + path);
				}
				return sourceCode;
			}
		};



		std::ifstream file(filePath);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open shader file !");
		}
		std::stringstream ss;
		ss << file.rdbuf();
		std::string data = ss.str();

		for (const auto& define : defines)
		{
			gShaderCompilerOptions.AddMacroDefinition(define.first, define.second);
		}
		gShaderCompilerOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
		gShaderCompilerOptions.SetIncluder(std::make_unique<ShaderIncluder>());

		shaderc::PreprocessedSourceCompilationResult precompileResult 
			= gShaderCompiler.PreprocessGlsl(data, (shaderc_shader_kind)kind, filePath.c_str(), gShaderCompilerOptions);
		if (precompileResult.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			Logger::gError(precompileResult.GetErrorMessage());
			throw std::runtime_error("Failed to preprocess shader !");
		}

		std::string processedData(precompileResult.begin());

		shaderc::SpvCompilationResult module = gShaderCompiler.CompileGlslToSpv(processedData,
			(shaderc_shader_kind)kind,
			filePath.c_str(), gShaderCompilerOptions);

		if (module.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			Logger::gError(module.GetErrorMessage());
			throw std::runtime_error("Failed to compile shader !");
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

	static void gShadowThreadFn()
	{
		eg::Logger::gInfo("Shadow thread started !");

		//Allocate command pool and command buffer for gShadow rendering
		vk::CommandPoolCreateInfo cmdPoolCI{};
		cmdPoolCI.setQueueFamilyIndex(eg::Renderer::getGraphicsQueueFamilyIndex())
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		auto cmdPool = gDevice.createCommandPool(cmdPoolCI);
		vk::CommandBufferAllocateInfo cmdAI{};
		cmdAI.setCommandPool(cmdPool)
			.setLevel(vk::CommandBufferLevel::eSecondary)
			.setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
		auto buffers = gDevice.allocateCommandBuffers(cmdAI);
		for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			gShadowCmdBuffers[i] = buffers[i];
		}

		while (gShadowThreadRunning)
		{
			std::unique_lock lk(gShadowMutex);
			gShadowCV.wait(lk, [] {
				return gShadowThreadReady;
				});

			//Reset commandbuffer

			//Record commands
			vk::CommandBufferInheritanceInfo cmdInheritanceInfo{};
			cmdInheritanceInfo.setRenderPass(Atmosphere::getRenderPass())
				.setSubpass(0)
				.setFramebuffer(Atmosphere::getFramebuffer());

			auto cmd = gShadowCmdBuffers[gCurrentFrame];

			vk::CommandBufferBeginInfo cmdBeginInfo{};
			cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse)
				.setPInheritanceInfo(&cmdInheritanceInfo);
			cmd.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
			cmd.begin(cmdBeginInfo);
			gShadowRenderFn(cmd);
			cmd.end();

			//Notify main thread that gShadow commands are ready
			{
				std::lock_guard lk(gMainThreadMutex);
				gShadowThreadReady = false;
				gMainCV.notify_one();
			}
		}

		//Delete command buffer and command pool
		gDevice.destroyCommandPool(cmdPool);

		eg::Logger::gInfo("Shadow thread destroyed !");
	}

	static void gBufferThreadFn()
	{
		eg::Logger::gInfo("GBuffer thread started !");

		//Allocate command pool and command buffer for gShadow rendering
		vk::CommandPoolCreateInfo cmdPoolCI{};
		cmdPoolCI.setQueueFamilyIndex(eg::Renderer::getGraphicsQueueFamilyIndex())
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		auto cmdPool = gDevice.createCommandPool(cmdPoolCI);
		vk::CommandBufferAllocateInfo cmdAI{};
		cmdAI.setCommandPool(cmdPool)
			.setLevel(vk::CommandBufferLevel::eSecondary)
			.setCommandBufferCount(MAX_FRAMES_IN_FLIGHT);
		auto buffers = gDevice.allocateCommandBuffers(cmdAI);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			gBufferCmdBuffers[i] = buffers[i];
		}


		while (gBufferThreadRunning)
		{
			std::unique_lock lk(gBufferMutex);
			gBufferCV.wait(lk, [] {
				return gBufferThreadReady;
				});

			//Reset commandbuffer

			//Record commands
			vk::CommandBufferInheritanceInfo cmdInheritanceInfo{};
			cmdInheritanceInfo.setRenderPass(eg::Renderer::getDefaultRenderPass().getRenderPass())
				.setSubpass(0)
				.setFramebuffer(eg::Renderer::getDefaultRenderPass().getFramebuffer());

			auto cmd = gBufferCmdBuffers[gCurrentFrame];

			vk::CommandBufferBeginInfo cmdBeginInfo{};
			cmdBeginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse)
				.setPInheritanceInfo(&cmdInheritanceInfo);
			cmd.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
			cmd.begin(cmdBeginInfo);
			gBufferRenderFn(cmd);
			cmd.end();

			//Notify main thread that gShadow commands are ready
			{
				std::lock_guard lk(gMainThreadMutex);
				gBufferThreadReady = false;
				gMainCV.notify_one();
			}
		}

		//Delete command buffer and command pool
		gDevice.destroyCommandPool(cmdPool);

		eg::Logger::gInfo("GBuffer thread destroyed !");
	}


	static std::tuple<uint32_t, vk::SurfaceCapabilitiesKHR> createSwapchain(uint32_t width, uint32_t height)
	{
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
			.setImageExtent(vk::Extent2D(width, height))
			.setImageArrayLayers(1)
			.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst)
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

		return { imageCount, surfaceCapabilities };
	}

	void waitIdle()
	{
		gDevice.waitIdle();
	}

	void create(uint32_t width, uint32_t height, uint32_t shadowMapRes)
	{
		//Create cvar for draw extent
		gScreenWidth = Command::registerVar("eg::Renderer::ScreenWidth", "None", static_cast<double>(width));
		gScreenHeight = Command::registerVar("eg::Renderer::ScreenHeight", "None", static_cast<double>(height));
		gScreenRenderScale = Command::registerVar("eg::Renderer::ScreenRenderScale", "None", 1.0);

		Command::registerFn("eg::Renderer::UpdateResolution", [](size_t argc, char* argv[]) {
			if (argc != 3)
			{
				Logger::gWarn("eg::Renderer::UpdateResolution <width> <height>");
				return;
			}
			try
			{
				gDevice.waitIdle();

				//Extract width and height
				int screenWidth = std::stoi(argv[1]);
				int screenHeight = std::stoi(argv[2]);
				int framebufferWidth, framebufferHeight;
				uint32_t changedWidth, changedHeight;

				glfwSetWindowSize(Window::getHandle(), screenWidth, screenHeight);
				glfwGetFramebufferSize(Window::getHandle(), &framebufferWidth, &framebufferHeight);
				changedWidth = static_cast<uint32_t>(framebufferWidth);
				changedHeight = static_cast<uint32_t>(framebufferHeight);


				gDevice.waitIdle();
				gScreenWidth->value = static_cast<double>(framebufferWidth);
				gScreenHeight->value = static_cast<double>(framebufferHeight);


				//We need to rebuild the swapchain
				for (auto imageView : gSwapchainImageViews)
				{
					gDevice.destroyImageView(imageView);
				}
				gSwapchainImageViews.clear();
				gDevice.destroySwapchainKHR(gSwapchain);
				createSwapchain(changedWidth, changedHeight);
				gDefaultRenderPass->resize(changedWidth, changedHeight);
				gPostprocessingRenderPass->resize(changedWidth, changedHeight);

				//Rebuild all pipelines
				eg::Command::execute("eg::Renderer::ReloadAllPipelines");
			} 
			catch (const std::exception& e)
			{
				Logger::gError("Failed to update resolution: " + std::string(e.what()));
			}


		});



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

		//Load glfw required extensions
		Logger::gInfo("Loading GLFW extensions for vulkan !");
		{
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;

			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			for (size_t i = 0; i < glfwExtensionCount; i++)
			{
				Logger::gInfo(std::string("GLFW Extension: ") + glfwExtensions[i]);
				gEnabledExtensions.push_back(glfwExtensions[i]);
			}
		}


		instanceCI.setPEnabledExtensionNames(gEnabledExtensions);
		instanceCI.setPEnabledLayerNames(gEnabledLayers);


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
			dbgMsgCI.pfnUserCallback = (vk::PFN_DebugUtilsMessengerCallbackEXT)gDebugCallbackFn;
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

		vk::PhysicalDeviceSynchronization2Features sync2Features{};
		sync2Features.synchronization2 = true;

		vk::PhysicalDeviceFeatures2 features2{};
		features2.features.geometryShader = true;
		features2.pNext = &sync2Features;


		vk::DeviceCreateInfo deviceCI{};
		deviceCI.setQueueCreateInfos(queueCIs)
			.setPEnabledExtensionNames(gDeviceEnabledExtensions)
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

		const auto[imageCount, surfaceCapabilities] = createSwapchain(width, height);

		//Create command pool
		vk::CommandPoolCreateInfo commandPoolCI{};
		commandPoolCI.setQueueFamilyIndex(gGraphicsQueueFamilyIndex)
			.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
		gCommandPool = gDevice.createCommandPool(commandPoolCI);


		//Create frame data
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			FrameData& frameData = gFrameData[i];
			vk::SemaphoreCreateInfo semaphoreCI{};
			vk::FenceCreateInfo fenceCI{};
			fenceCI.setFlags(vk::FenceCreateFlagBits::eSignaled);
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

		//Create descriptor pool
		vk::DescriptorPoolSize poolSizes[] =
		{
			vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, 4096},
			vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, 4096}
		};
		vk::DescriptorPoolCreateInfo descriptorPoolCI{};
		descriptorPoolCI
			.setPoolSizes(poolSizes)
			.setMaxSets(1024)
			.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
		gDescriptorPool = gDevice.createDescriptorPool(descriptorPoolCI);

		//Create global uniform buffers
		gGlobalUniformBuffer.emplace(MAX_FRAMES_IN_FLIGHT);

		//Create default resources
		std::vector<uint8_t> whitePixels(64 * 64 * 4, 255);
		std::vector<uint8_t> checkerPixels(64 * 64 * 4);
		for (int y = 0; y < 64; ++y) {
			for (int x = 0; x < 64; ++x) {
				int tileX = x / 8;
				int tileY = y / 8;
				bool isPurple = (tileX + tileY) % 2 == 0;

				int index = (y * 64 + x) * 4;
				if (isPurple) {
					checkerPixels[index + 0] = 255; // R
					checkerPixels[index + 1] = 0;   // G
					checkerPixels[index + 2] = 255; // B
					checkerPixels[index + 3] = 255; // A
				}
				else {
					checkerPixels[index + 0] = 0;
					checkerPixels[index + 1] = 0;
					checkerPixels[index + 2] = 0;
					checkerPixels[index + 3] = 255;
				}
			}
		}
		gDefaultWhiteImage.emplace(64, 64, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor, whitePixels.data(), whitePixels.size());
		gDefaultCheckerboardImage.emplace(64, 64, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor, checkerPixels.data(), checkerPixels.size());

		gDefaultRenderPass.emplace(width, height, gSurfaceFormat.format);
		gPostprocessingRenderPass.emplace(width, height, gSurfaceFormat.format);
		Atmosphere::create(shadowMapRes);

		//Init imgui
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForVulkan(Window::getHandle(), true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = gInstance;
		init_info.PhysicalDevice = gPhysicalDevice;
		init_info.Device = gDevice;
		init_info.QueueFamily = gGraphicsQueueFamilyIndex;
		init_info.Queue = gMainQueue;
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = gDescriptorPool;
		init_info.RenderPass = gPostprocessingRenderPass->getRenderPass();
		init_info.Subpass = 0;
		init_info.MinImageCount = surfaceCapabilities.minImageCount;
		init_info.ImageCount = imageCount;
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn =
			[](VkResult result) {
			if (result != 0)
				Logger::gError("[vulkan] Error: VkResult = %d\n");
			};
		ImGui_ImplVulkan_Init(&init_info);


		//Launch threads
		gShadowThread = std::make_unique<std::thread>(gShadowThreadFn);
		gBufferThread = std::make_unique<std::thread>(gBufferThreadFn);


		Command::registerFn("eg::Renderer::ChangeResolution", [](size_t argc, char* argv[]) {});
		
	}

	void setCamera(const Components::Camera* camera)
	{
		if (camera == nullptr)
		{
			gCamera = &gDummyCamera;
			return;
		}
		gCamera = camera;
	}

	const Components::Camera& getMainCamera()
	{
		return *gCamera;
	}


	void render()
	{	
		auto cmd = begin();
		auto& frameData = gFrameData[gCurrentFrame];
		gDebugRenderFn(cmd);
		Data::DebugRenderer::updateVertexBuffers();
		Data::ParticleRenderer::updateBuffers();

		//Give tasks for gBuffer and shadow threads
		{
			std::lock_guard lk(gShadowMutex);
			gShadowThreadReady = true;
			gShadowCV.notify_one();
		}

		{
			std::lock_guard lk(gBufferMutex);
			gBufferThreadReady = true;
			gBufferCV.notify_one();
		}


		//Wait for the shadow thread and gBuffer thread to be ready
		{
			std::unique_lock lk(gMainThreadMutex);
			gMainCV.wait(lk, [] {
				return !gBufferThreadReady && !gShadowThreadReady;
				});
		}

		Atmosphere::generateCSMMatrices(gCamera->mFov, gCamera->buildView());
		Atmosphere::updateDirectionalLight();
		Atmosphere::updateAmbientLight();

		Atmosphere::beginDirectionalShadowPass(cmd);
		cmd.executeCommands(gShadowCmdBuffers[gCurrentFrame]);
		cmd.endRenderPass();

		gDefaultRenderPass->begin(cmd); // subpass 0
		cmd.executeCommands(gBufferCmdBuffers[gCurrentFrame]);
		cmd.nextSubpass(vk::SubpassContents::eInline); //Subpass 1
		Data::SkyRenderer::render(cmd, Data::SkyRenderer::SkySettings{});
		Atmosphere::renderAmbientLight(cmd);
		Atmosphere::renderDirectionalLight(cmd);
		Data::LightRenderer::beginPointLight(cmd);
		gLightRenderFn(cmd);
		Data::ParticleRenderer::render(cmd);
		Data::DebugRenderer::render(cmd);

		cmd.endRenderPass();

		//Copy default render pass image to post processing image
		{
			//At the end of the subpass, the default render pass image is in eTransferSrcOptimal layout so we just need to copy it
			//First transition the post processing image to eTransferDstOptimal
			vk::ImageMemoryBarrier barrier{};
			barrier.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(gPostprocessingRenderPass->getDrawImage().getImage())
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
				.setSrcAccessMask(vk::AccessFlagBits{})
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, {}, {}, { barrier });

			//Now copy the image

			//Calculate scaled extent
			uint32_t scaledWidth = static_cast<uint32_t>(gScreenWidth->value * gScreenRenderScale->value);
			uint32_t scaledHeight = static_cast<uint32_t>(gScreenHeight->value * gScreenRenderScale->value);


			vk::ImageBlit blitRegion{};
			blitRegion.setSrcSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
				.setSrcOffsets({ vk::Offset3D(0, 0, 0) , vk::Offset3D(scaledWidth, scaledHeight, 1) })
				.setDstSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
				.setDstOffsets({ vk::Offset3D(0, 0, 0) , vk::Offset3D(static_cast<uint32_t>(gScreenWidth->value),
					static_cast<uint32_t>(gScreenHeight->value), 1)});

			cmd.blitImage(gDefaultRenderPass->getDrawImage().getImage(), vk::ImageLayout::eTransferSrcOptimal,
				gPostprocessingRenderPass->getDrawImage().getImage(), vk::ImageLayout::eTransferDstOptimal, { blitRegion }, vk::Filter::eLinear);


		}

		//Post processing
		gPostprocessingRenderPass->begin(cmd);


		ImGui::Render();
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		cmd.endRenderPass();


		//Copy the post processing image to the swapchain image
		{
			//Transition swapchain image to transfer destination optimal
			vk::ImageMemoryBarrier barrier{};
			barrier.setOldLayout(vk::ImageLayout::eUndefined)
				.setNewLayout(vk::ImageLayout::eTransferDstOptimal)
				.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
				.setImage(gSwapchainImages[frameData.swapchainIndex])
				.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 })
				.setSrcAccessMask(vk::AccessFlagBits{})
				.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags{}, {}, {}, { barrier });

			vk::ImageCopy blitRegion{};
			blitRegion.setSrcSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
				.setSrcOffset({ 0, 0, 0 })
				.setDstSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, 0, 0, 1 })
				.setDstOffset({ 0, 0, 0 })
				.setExtent({ static_cast<uint32_t>(gScreenWidth->value), static_cast<uint32_t>(gScreenHeight->value), 1 });

			cmd.copyImage(gPostprocessingRenderPass->getDrawImage().getImage(), vk::ImageLayout::eTransferSrcOptimal,
				gSwapchainImages[frameData.swapchainIndex], vk::ImageLayout::eTransferDstOptimal, { blitRegion });


			//Transition swapchain image to present source optimal
			barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal)
				.setNewLayout(vk::ImageLayout::ePresentSrcKHR)
				.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
				.setDstAccessMask(vk::AccessFlagBits::eMemoryRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eBottomOfPipe, vk::DependencyFlags{}, {}, {}, { barrier });
		}

		Renderer::end();
	}

	void destory()
	{
		//Stop threads
		gBufferThreadRunning = false;
		{
			std::lock_guard lk(gBufferMutex);
			gBufferThreadReady = true;
			gBufferCV.notify_one();
		}
		gBufferThread->join();

		gShadowThreadRunning = false;
		{
			std::lock_guard lk(gShadowMutex);
			gShadowThreadReady = true;
			gShadowCV.notify_one();
		}
		gShadowThread->join();

		Atmosphere::destroy();
		gDefaultRenderPass.reset();
		gPostprocessingRenderPass.reset();
		gGlobalUniformBuffer.reset();

		gDefaultWhiteImage.reset();
		gDefaultCheckerboardImage.reset();

		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		gAllocator.destroy();
		gDevice.destroyDescriptorPool(gDescriptorPool);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
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

#ifndef NDEBUG
		gInstance.destroyDebugUtilsMessengerEXT(gDbgMsg);
#endif
		gInstance.destroy();
	}


	vk::CommandBuffer begin()
	{
		auto& frameData = gFrameData[gCurrentFrame];


		uint32_t width = static_cast<uint32_t>(gScreenWidth->value);
		uint32_t height = static_cast<uint32_t>(gScreenHeight->value);

		try
		{
			gDevice.waitForFences(frameData.renderFence, VK_TRUE, 1000000000);
		} 
		catch(const std::exception& e)
		{
			Logger::gError("Failed to wait for fence: " + std::string(e.what()));
		}

		gDevice.resetFences(frameData.renderFence);
		frameData.swapchainIndex = gDevice.acquireNextImageKHR(gSwapchain, 1000000000, frameData.presentSemaphore, nullptr).value;

		//draw imgui
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport(0, 0, ImGuiDockNodeFlags_PassthruCentralNode);

		//Update global uniform buffer
		GlobalUniformBuffer::Data GlobalUBOData;
		GlobalUBOData.mProjection = gCamera->buildProjection(vk::Extent2D(width, height));
		GlobalUBOData.mView = gCamera->buildView();
		GlobalUBOData.mCameraPosition = gCamera->mPosition;

		gGlobalUniformBuffer->update(GlobalUBOData, gCurrentFrame);

		vk::CommandBuffer& cmd = frameData.commandBuffer;
		vk::CommandBufferBeginInfo commandBufferBI{};
		commandBufferBI.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

		cmd.begin(commandBufferBI);



		return cmd;
	}

	void end()
	{
		auto& frameData = gFrameData[gCurrentFrame];
		vk::CommandBuffer& cmd = frameData.commandBuffer;

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
			.setPImageIndices(&frameData.swapchainIndex);

		try
		{
			gMainQueue.presentKHR(presentInfo);
		}
		catch (const std::exception& e)
		{
			Logger::gWarn("Failed to present image: " + std::string(e.what()));
		}
		
		gCurrentFrame = (gCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	uint32_t getCurrentFrameIndex()
	{
		return gCurrentFrame;
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

	vk::DescriptorPool getDescriptorPool()
	{
		return gDescriptorPool;
	}

	const DefaultRenderPass& getDefaultRenderPass()
	{
		return *gDefaultRenderPass;
	}

	vk::DescriptorSet getCurrentFrameGUBODescSet()
	{
		return gGlobalUniformBuffer->getDescriptorSet(gCurrentFrame).getSet();
	}

	vk::DescriptorSetLayout getGlobalDescriptorSet()
	{
		return gGlobalUniformBuffer->getLayout();
	}
	const CombinedImageSampler2D& getDefaultWhiteImage()
	{
		return *gDefaultWhiteImage;
	}

	const CombinedImageSampler2D& getDefaultCheckerboardImage()
	{
		return *gDefaultCheckerboardImage;
	}
	const uint32_t getGraphicsQueueFamilyIndex()
	{
		return gGraphicsQueueFamilyIndex;
	}

	void setShadowRenderFunction(RenderFn&& renderFn)
	{
		gShadowRenderFn = std::move(renderFn);
	}
	void setGBufferRenderFunction(RenderFn&& renderFn)
	{
		gBufferRenderFn = std::move(renderFn);
	}

	void setLightRenderFunction(RenderFn&& renderFn)
	{
		gLightRenderFn = std::move(renderFn);
	}

	void setDebugRenderFunction(RenderFn&& renderFn)
	{
		gDebugRenderFn = std::move(renderFn);
	}
}