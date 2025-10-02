#include <Debug.h>

#include <Renderer.h>
#include <Core.h>
#include <GLFW/glfw3.h>
#include <Input.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <imgui_stdlib.h>
#include <ImGuizmo.h>
#include <ImGuiFileDialog.h>
#include <Window.h>
#include <Data.h>
#include <World.h>

#include <Physics.h>
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

namespace eg::Debug
{
	class DebugRenderer : public JPH::DebugRendererSimple
	{
	public:
		virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
		{
			Data::DebugRenderer::recordLine(glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
				glm::vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
				glm::vec4(inColor.r, inColor.g, inColor.b, inColor.a));
		}
		virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override
		{

		}

	};

	bool gEnabled = false;

	vk::RenderPass gImGuiRenderPass;
	vk::Framebuffer gImGuiFrameBuffer;
	std::optional<Renderer::Image2D> gImGuiFrameBufferImage;
	VkDescriptorSet gViewportImageSet;

	Command::Var* gScreenWidthCvar;
	Command::Var* gScreenHeightCvar;

	World::IGameObject* gSelectedObject = nullptr; // track selection

	void create()
	{
		//Load cvars
		gScreenWidthCvar = Command::findVar("eg::Renderer::ScreenWidth");
		gScreenHeightCvar = Command::findVar("eg::Renderer::ScreenHeight");

		

		//Create render pass
		vk::AttachmentDescription attachments[] =
		{
			//Final Draw image, id = 0
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				Renderer::Postprocessing::getFinalDrawImage().getFormat(),
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eTransferSrcOptimal,
				vk::ImageLayout::eTransferSrcOptimal
			),
		};

		vk::AttachmentReference pass0OutputAttachmentRef[] =
		{
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal), //FInal draw
		};

		vk::SubpassDescription subpasses[] = {
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				sizeof(pass0OutputAttachmentRef) / sizeof(pass0OutputAttachmentRef[0]), pass0OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				0, nullptr//Preserve
			),

		};

		vk::SubpassDependency dependencies[] = {
			vk::SubpassDependency(
				VK_SUBPASS_EXTERNAL,                  // srcSubpass (external)
				0,                                    // dstSubpass (this subpass)
				vk::PipelineStageFlagBits::eColorAttachmentOutput, // srcStageMask
				vk::PipelineStageFlagBits::eColorAttachmentOutput, // dstStageMask
				vk::AccessFlagBits::eColorAttachmentWrite,         // srcAccessMask
				vk::AccessFlagBits::eColorAttachmentRead |
				vk::AccessFlagBits::eColorAttachmentWrite,         // dstAccessMask
				vk::DependencyFlagBits::eByRegion
			)
		};

		vk::RenderPassCreateInfo renderPassCI{};
		renderPassCI
			.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		gImGuiRenderPass = Renderer::getDevice().createRenderPass(renderPassCI);

		//Create framebuffer image
		gImGuiFrameBufferImage.emplace(
			static_cast<uint32_t>(gScreenWidthCvar->value),
			static_cast<uint32_t>(gScreenHeightCvar->value),
			Renderer::Postprocessing::getFinalDrawImage().getFormat(),
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		//Create framebuffer
		vk::ImageView attachmentsViews[] = {
			gImGuiFrameBufferImage->getImageView(),
		};
		vk::FramebufferCreateInfo fbCI{};
		fbCI.setRenderPass(gImGuiRenderPass)
			.setAttachments(attachmentsViews)
			.setWidth(static_cast<uint32_t>(gScreenWidthCvar->value))
			.setHeight(static_cast<uint32_t>(gScreenHeightCvar->value))
			.setLayers(1);
		gImGuiFrameBuffer = Renderer::getDevice().createFramebuffer(fbCI);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
		
		ImGui::StyleColorsDark();
		ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0f;
		ImGui_ImplGlfw_InitForVulkan(Window::getHandle(), true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = Renderer::getInstance();
		init_info.PhysicalDevice = Renderer::getPhysicalDevice();
		init_info.Device = Renderer::getDevice();
		init_info.QueueFamily = Renderer::getMainQueueFamilyIndex();
		init_info.Queue = Renderer::getMainQueue();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = Renderer::getDescriptorPool();
		init_info.RenderPass = gImGuiRenderPass;
		init_info.Subpass = 0;
		init_info.MinImageCount = Renderer::getSurfaceCapabilities().minImageCount;
		init_info.ImageCount = Renderer::getImageCount();
		init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.CheckVkResultFn =
			[](VkResult result) {
			if (result != 0)
				Logger::gError("[vulkan] Error: VkResult = %d\n");
			};
		ImGui_ImplVulkan_Init(&init_info);

		gViewportImageSet = ImGui_ImplVulkan_AddTexture(Renderer::getDefaultWhiteImage().getSampler(),
			Renderer::Postprocessing::getFinalDrawImage().getImageView(),
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	void destroy()
	{	
		gImGuiFrameBufferImage.reset();
		Renderer::getDevice().destroyFramebuffer(gImGuiFrameBuffer);
		Renderer::getDevice().destroyRenderPass(gImGuiRenderPass);
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), (vk::DescriptorSet)gViewportImageSet);
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}
	void checkForKeyboardInput()
	{
		static bool prevState = false;
		int state = glfwGetKey(Window::getHandle(), GLFW_KEY_F3);
		bool pressed = (state == GLFW_PRESS);

		// emulate "isKeyDownOnce"
		if (pressed && !prevState)
		{
			gEnabled = !gEnabled;
			glfwSetInputMode(Window::getHandle(), GLFW_CURSOR,
				gEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);

			if (gEnabled)
				eg::Logger::gInfo("Debugger enabled");
			else
				eg::Logger::gInfo("Debugger disabled");
		}
		prevState = pressed;
	}

	void render(vk::CommandBuffer cmd)
	{
		if(!gEnabled)
			return;

		static glm::mat4x4 matrix(1.0f);
		static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
		static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
		ImGui::DockSpaceOverViewport();

		ImGui::Begin("GameObjects");
		uint32_t i = 0;
		for (auto& obj : World::getGameObjects()) {
			ImGui::PushID(i++);
			std::string label = std::to_string(i) + " | " + obj->getType();
			if (ImGui::Selectable(label.c_str(), obj.get() == gSelectedObject)) {
				gSelectedObject = obj.get();
			}
			ImGui::PopID();
		}
		ImGui::End();

		ImGui::Begin("Inspector");
		if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
			mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
			mCurrentGizmoOperation = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
			mCurrentGizmoOperation = ImGuizmo::SCALE;
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(&matrix[0][0], matrixTranslation, matrixRotation, matrixScale);
		ImGui::InputFloat3("Tr", matrixTranslation);
		ImGui::InputFloat3("Rt", matrixRotation);
		ImGui::InputFloat3("Sc", matrixScale);
		ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, &matrix[0][0]);
		if (mCurrentGizmoOperation != ImGuizmo::SCALE)
		{
			if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
				mCurrentGizmoMode = ImGuizmo::LOCAL;
			ImGui::SameLine();
			if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
				mCurrentGizmoMode = ImGuizmo::WORLD;
		}
		if (gSelectedObject) {
			gSelectedObject->onInspector();
		}
		else {
			ImGui::Text("No object selected");
		}
		ImGui::End();

		ImGui::Begin("Viewport");
		
		

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 windowPos = ImGui::GetWindowPos();
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(windowPos.x, windowPos.y, windowSize.x, windowSize.y);
		glm::mat4x4 cameraView = Renderer::getMainCamera().buildView();
		glm::mat4x4 cameraProjection = Renderer::getMainCamera().buildProjection(vk::Extent2D(
			gScreenWidthCvar->value,
			gScreenHeightCvar->value));
		//Flip y
		cameraProjection[1][1] *= -1;
		ImGui::Image((ImTextureID)gViewportImageSet,
			ImGui::GetContentRegionAvail());
		ImGuizmo::Manipulate(&cameraView[0][0], &cameraProjection[0][0], mCurrentGizmoOperation, mCurrentGizmoMode, &matrix[0][0], NULL, NULL);
		
		ImGui::End();
		ImGui::Begin("World Debugger");


		// Button to open Load dialog
		if (ImGui::Button("Load File"))
		{
			ImGuiFileDialog::Instance()->OpenDialog("LoadFileDlg", "Choose File to Load", ".json,.txt");
		}

		// Button to open Save dialog
		if (ImGui::Button("Save File"))
		{
			ImGuiFileDialog::Instance()->OpenDialog("SaveFileDlg", "Choose File to Save", ".json,.txt");
		}

	
		// Handle Load File dialog
		if (ImGuiFileDialog::Instance()->Display("LoadFileDlg"))
		{
			if (ImGuiFileDialog::Instance()->IsOk())
			{
				std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
				std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();


				
			}
			ImGuiFileDialog::Instance()->Close();
		}

		// Handle Save File dialog
		if (ImGuiFileDialog::Instance()->Display("SaveFileDlg"))
		{
			if (ImGuiFileDialog::Instance()->IsOk())
			{
				std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
				std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();

				
			}
			ImGuiFileDialog::Instance()->Close();
		}
		ImGui::End();

		ImGui::Begin("Renderer Settings");

		if (ImGui::Button("Reload all pipelines"))
		{
			eg::Renderer::waitIdle();
			eg::Command::execute("eg::Renderer::ReloadAllPipelines");
		}
		//Render scale
		{
			eg::Command::Var* renderScaleCVar = eg::Command::findVar("eg::Renderer::ScreenRenderScale");
			auto renderscaleFloat = static_cast<float>(renderScaleCVar->value);
			if (ImGui::SliderFloat("Render scale", &renderscaleFloat, 0.1f, 1.0f))
			{
				renderScaleCVar->value = static_cast<double>(renderscaleFloat);
			}
		}

		//Resolution
		{
			static char widthStr[32] = "1600";
			static char heightStr[32] = "900";
			ImGui::InputText("Width", widthStr, sizeof(widthStr), ImGuiInputTextFlags_CharsDecimal);
			ImGui::InputText("Height", heightStr, sizeof(heightStr), ImGuiInputTextFlags_CharsDecimal);
			if (ImGui::Button("Update resolution"))
			{
				eg::Command::execute("eg::Renderer::UpdateResolution " + std::string(widthStr) + " " + std::string(heightStr));
			}
		}

		//Max fps
		{
			auto* maxFPSCvar = eg::Command::findVar("eg::MaxFPS");
			auto* maxTPSCvar = eg::Command::findVar("eg::MaxTPS");

			float fps_tps[] =
			{
				static_cast<float>(maxFPSCvar->value),
				static_cast<float>(maxTPSCvar->value)
			};
			if (ImGui::DragFloat2("Max fps/tps", fps_tps, 1.0f, 10.0f, 999.0f))
			{
				maxFPSCvar->value = static_cast<double>(fps_tps[0]);
				maxTPSCvar->value = static_cast<double>(fps_tps[1]);
			}
		}


		ImGui::Separator();
		/*
			glm::vec3 direction = { 1, -1, 0 };
			float intensity = 1.0f;
			glm::vec4 color = { 1, 1, 1, 1 };
		*/

		auto& directionalLight = eg::Renderer::Atmosphere::getDirectionalLightUniformBuffer();
		ImGui::Text("Directional Light Settings");
		if (ImGui::DragFloat3("Directional Light direction", &directionalLight.direction.x, 0.01f))
		{
			directionalLight.direction = glm::normalize(directionalLight.direction);
		}

		ImGui::DragFloat("Directional Light intensity", &directionalLight.intensity, 0.01f);
		ImGui::ColorEdit3("Directional Light color", &directionalLight.color.x);

		ImGui::Separator();
		//Ambient
		auto& ambientLight = eg::Renderer::Atmosphere::getAmbientLightUniformBuffer();
		ImGui::Text("Ambient Light Settings");
		ImGui::ColorEdit3("Ambient Light color", &ambientLight.color.x);
		ImGui::DragFloat("Ambient Light intensity", &ambientLight.intensity, 0.01f);
		ImGui::DragFloat2("Ambient Light height", &ambientLight.noiseScale.x, 0.01f, 0.0f);
		ImGui::DragFloat("Ambient Light radius", &ambientLight.radius, 0.01f, 0.001f, 5.0f);

		ImGui::Separator();

		ImGui::Text("Post process settings");
		//Bloom
		{
			static eg::Command::Var* bloomRadiusCVar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomRadius");
			static eg::Command::Var* bloomThresholdCVar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomThreshold");
			static eg::Command::Var* bloomKneeCvar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomKnee");

			float radius = static_cast<float>(bloomRadiusCVar->value);
			float threshold = static_cast<float>(bloomThresholdCVar->value);
			float knee = static_cast<float>(bloomKneeCvar->value);

			ImGui::DragFloat("Bloom radius", &radius, 0.1f, 0.0f, 20.0f);
			ImGui::DragFloat("Bloom threshold", &threshold, 0.1f, 0.0f, 20.0f);
			ImGui::DragFloat("Bloom knee", &knee, 0.1f, 0.0f, 20.0f);

			bloomKneeCvar->value = static_cast<double>(knee);
			bloomRadiusCVar->value = static_cast<double>(radius);
			bloomThresholdCVar->value = static_cast<double>(threshold);
		}

		//Exposure
		{
			static eg::Command::Var* exposureCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Exposure");
			float exposure = static_cast<float>(exposureCVar->value);
			if (ImGui::DragFloat("Exposure", &exposure, 0.1f, 0.0f, 20.0f))
			{
				exposureCVar->value = static_cast<double>(exposure);
			}
		}
		//Saturation
		{
			static eg::Command::Var* saturationCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Saturation");
			float saturation = static_cast<float>(saturationCVar->value);
			if (ImGui::DragFloat("Saturation", &saturation, 0.1f, 0.0f, 5.0f))
			{
				saturationCVar->value = static_cast<double>(saturation);
			}
		}
		//Gamma
		{
			static eg::Command::Var* gammaCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Gamma");
			float gamma = static_cast<float>(gammaCVar->value);
			if (ImGui::DragFloat("Gamma", &gamma, 0.1f, 0.1f, 5.0f))
			{
				gammaCVar->value = static_cast<double>(gamma);
			}
		}


		ImGui::End();

		static JPH::BodyManager::DrawSettings settings;
		static DebugRenderer renderer;
		ImGui::Begin("Physics Debug Renderer");
		ImGui::Checkbox("Draw Velocity", &settings.mDrawVelocity);
		ImGui::Checkbox("Draw Bounding Box", &settings.mDrawBoundingBox);
		ImGui::Checkbox("Draw Mass and Inertia", &settings.mDrawMassAndInertia);
		ImGui::Checkbox("Draw Shape", &settings.mDrawShape);
		ImGui::Checkbox("Draw Shape Wireframe", &settings.mDrawShapeWireframe);
		ImGui::Checkbox("Draw Center of Mass", &settings.mDrawCenterOfMassTransform);
		ImGui::Checkbox("Draw Sleep Stat", &settings.mDrawSleepStats);
		ImGui::End();

		eg::Physics::getPhysicsSystem().DrawBodies(settings, &renderer, nullptr);

		ImGui::Render();

		//Begin render pass
		vk::ClearValue clearValues[1];
		clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 1.0f }));
		vk::RenderPassBeginInfo rpBI{};
		rpBI.setRenderPass(gImGuiRenderPass)
			.setFramebuffer(gImGuiFrameBuffer)
			.setRenderArea(vk::Rect2D({ 0, 0 },
				{ static_cast<uint32_t>(gScreenWidthCvar->value), static_cast<uint32_t>(gScreenHeightCvar->value) }))
			.setClearValueCount(1)
			.setPClearValues(clearValues);
		cmd.beginRenderPass(rpBI, vk::SubpassContents::eInline);

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
		cmd.endRenderPass();

	}
	bool enabled()
	{
		return gEnabled;
	}

	Renderer::Image2D& getImage()
	{
		return *gImGuiFrameBufferImage;
	}
}