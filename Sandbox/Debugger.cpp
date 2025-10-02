//#include <Sandbox_Debugger.h>
//
//#include <Logger.h>
//#include <Input.h>
//#include <Data.h>
//#include <Window.h>
//#include <Core.h>
//
//#include <GLFW/glfw3.h>
//#include <imgui.h>
//#include <imgui_stdlib.h>
//#include <ImGuiFileDialog.h>
//#include <Physics.h>
//#include <Jolt/Jolt.h>
//#include <Jolt/Renderer/DebugRendererSimple.h>
//
//namespace sndbx
//{
//	class DebugRenderer : public JPH::DebugRendererSimple
//	{
//	public:
//		virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
//		{
//			eg::Data::DebugRenderer::recordLine(glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
//				glm::vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
//				glm::vec4(inColor.r, inColor.g, inColor.b, inColor.a));
//		}
//		virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override
//		{
//
//		}
//
//	};
//
//
//	void Debugger::drawRendererSettingsDialog()
//	{
//		if (!mEnabled) return;
//		ImGui::Begin("Renderer Settings");
//
//		if (ImGui::Button("Reload all pipelines"))
//		{
//			eg::Renderer::waitIdle();
//			eg::Command::execute("eg::Renderer::ReloadAllPipelines");
//		}
//		//Render scale
//		{
//			eg::Command::Var* renderScaleCVar = eg::Command::findVar("eg::Renderer::ScreenRenderScale");
//			auto renderscaleFloat = static_cast<float>(renderScaleCVar->value);
//			if (ImGui::SliderFloat("Render scale", &renderscaleFloat, 0.1f, 1.0f))
//			{
//				renderScaleCVar->value = static_cast<double>(renderscaleFloat);
//			}
//		}
//
//		//Resolution
//		{	
//			static char widthStr[32] = "1600";
//			static char heightStr[32] = "900";
//			ImGui::InputText("Width", widthStr, sizeof(widthStr), ImGuiInputTextFlags_CharsDecimal);
//			ImGui::InputText("Height", heightStr, sizeof(heightStr), ImGuiInputTextFlags_CharsDecimal);
//			if (ImGui::Button("Update resolution"))
//			{
//				eg::Command::execute("eg::Renderer::UpdateResolution " + std::string(widthStr) + " " + std::string(heightStr));
//			}
//		}
//
//		//Max fps
//		{
//			auto* maxFPSCvar = eg::Command::findVar("eg::MaxFPS");
//			auto* maxTPSCvar = eg::Command::findVar("eg::MaxTPS");
//
//			float fps_tps[] =
//			{
//				static_cast<float>(maxFPSCvar->value),
//				static_cast<float>(maxTPSCvar->value)
//			};
//			if (ImGui::DragFloat2("Max fps/tps", fps_tps, 1.0f, 10.0f, 999.0f))
//			{
//				maxFPSCvar->value = static_cast<double>(fps_tps[0]);
//				maxTPSCvar->value = static_cast<double>(fps_tps[1]);
//			}
//		}
//
//
//		ImGui::Separator();
//		/*
//			glm::vec3 direction = { 1, -1, 0 };
//			float intensity = 1.0f;
//			glm::vec4 color = { 1, 1, 1, 1 };
//		*/
//
//		auto& directionalLight = eg::Renderer::Atmosphere::getDirectionalLightUniformBuffer();
//		ImGui::Text("Directional Light Settings");
//		if (ImGui::DragFloat3("Directional Light direction", &directionalLight.direction.x, 0.01f))
//		{
//			directionalLight.direction = glm::normalize(directionalLight.direction);
//		}
//
//		ImGui::DragFloat("Directional Light intensity", &directionalLight.intensity, 0.01f);
//		ImGui::ColorEdit3("Directional Light color", &directionalLight.color.x);
//
//		ImGui::Separator();
//		//Ambient
//		auto& ambientLight = eg::Renderer::Atmosphere::getAmbientLightUniformBuffer();
//		ImGui::Text("Ambient Light Settings");
//		ImGui::ColorEdit3("Ambient Light color", &ambientLight.color.x);
//		ImGui::DragFloat("Ambient Light intensity", &ambientLight.intensity, 0.01f);
//		ImGui::DragFloat2("Ambient Light height", &ambientLight.noiseScale.x, 0.01f, 0.0f);
//		ImGui::DragFloat("Ambient Light radius", &ambientLight.radius, 0.01f, 0.001f, 5.0f);
//
//		ImGui::Separator();
//
//		ImGui::Text("Post process settings");
//		//Bloom
//		{
//			static eg::Command::Var* bloomRadiusCVar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomRadius");
//			static eg::Command::Var* bloomThresholdCVar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomThreshold");
//			static eg::Command::Var* bloomKneeCvar = eg::Command::findVar("eg::Renderer::Postprocessing::BloomKnee");
//
//			float radius = static_cast<float>(bloomRadiusCVar->value);
//			float threshold = static_cast<float>(bloomThresholdCVar->value);
//			float knee = static_cast<float>(bloomKneeCvar->value);
//
//			ImGui::DragFloat("Bloom radius", &radius, 0.1f, 0.0f, 20.0f);
//			ImGui::DragFloat("Bloom threshold", &threshold, 0.1f, 0.0f, 20.0f);
//			ImGui::DragFloat("Bloom knee", &knee, 0.1f, 0.0f, 20.0f);
//
//			bloomKneeCvar->value = static_cast<double>(knee);
//			bloomRadiusCVar->value = static_cast<double>(radius);
//			bloomThresholdCVar->value = static_cast<double>(threshold);
//		}
//
//		//Exposure
//		{
//			static eg::Command::Var* exposureCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Exposure");
//			float exposure = static_cast<float>(exposureCVar->value);
//			if (ImGui::DragFloat("Exposure", &exposure, 0.1f, 0.0f, 20.0f))
//			{
//				exposureCVar->value = static_cast<double>(exposure);
//			}
//		}
//		//Saturation
//		{
//			static eg::Command::Var* saturationCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Saturation");
//			float saturation = static_cast<float>(saturationCVar->value);
//			if (ImGui::DragFloat("Saturation", &saturation, 0.1f, 0.0f, 5.0f))
//			{
//				saturationCVar->value = static_cast<double>(saturation);
//			}
//		}
//		//Gamma
//		{
//			static eg::Command::Var* gammaCVar = eg::Command::findVar("eg::Renderer::Postprocessing::Gamma");
//			float gamma = static_cast<float>(gammaCVar->value);
//			if (ImGui::DragFloat("Gamma", &gamma, 0.1f, 0.1f, 5.0f))
//			{
//				gammaCVar->value = static_cast<double>(gamma);
//			}
//		}
//
//
//		ImGui::End();
//	}
//
//	void Debugger::checkKeyboardInput()
//	{
//		if (eg::Input::Keyboard::isKeyDownOnce(GLFW_KEY_F11))
//		{
//			mEnabled = !mEnabled;
//
//			glfwSetInputMode(eg::Window::getHandle(), GLFW_CURSOR, mEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
//			if (mEnabled)
//			{
//				eg::Logger::gInfo("Debugger enabled");
//			}
//			else
//			{
//				eg::Logger::gInfo("Debugger disabled");
//			}
//		}
//	}
//
//	void Debugger::drawWorldDebuggerDialog(eg::World::GameObjectManager& gameObjManager,
//		eg::World::JsonToIGameObjectDispatcher dispatcherFn)
//	{
//		if (!mEnabled) return;
//
//		static std::string worldName = "Blank";
//
//		ImGui::Begin("World Debugger");
//
//		if (ImGui::InputText("World Name", &worldName))
//		{
//			gameObjManager.setWorldName(worldName);
//		}
//		// Button to open Load dialog
//		if (ImGui::Button("Load File"))
//		{
//			ImGuiFileDialog::Instance()->OpenDialog("LoadFileDlg", "Choose File to Load", ".json,.txt");
//		}
//
//		// Button to open Save dialog
//		if (ImGui::Button("Save File"))
//		{
//			ImGuiFileDialog::Instance()->OpenDialog("SaveFileDlg", "Choose File to Save", ".json,.txt");
//		}
//
//		if(ImGui::Button("Spawn A Dynamic Object"))
//		{
//			auto& camera = eg::Renderer::getMainCamera();
//			auto obj = std::make_unique<eg::World::DynamicWorldObject>();
//			nlohmann::json json = {
//				{"model", { {"path", "models/box.glb"} } },
//				{"body", {
//					{"position", {camera.mPosition.x, camera.mPosition.y, camera.mPosition.z} }
//				} }
//			};
//			obj->fromJson(json);
//			gameObjManager.addGameObject(std::move(obj));
//		}
//
//		if (ImGui::Button("Spawn B Dynamic Object"))
//		{
//			auto& camera = eg::Renderer::getMainCamera();
//			auto obj = std::make_unique<eg::World::DynamicWorldObject>();
//			nlohmann::json json = {
//				{"model", { {"path", "models/DamagedHelmet.glb"} } },
//				{"body", {
//					{"position", {camera.mPosition.x, camera.mPosition.y, camera.mPosition.z} }
//				} }
//			};
//			obj->fromJson(json);
//			gameObjManager.addGameObject(std::move(obj));
//		}
//
//		// Handle Load File dialog
//		if (ImGuiFileDialog::Instance()->Display("LoadFileDlg"))
//		{
//			if (ImGuiFileDialog::Instance()->IsOk())
//			{
//				std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
//				std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
//
//
//				gameObjManager.load(filePath, dispatcherFn);
//				worldName = gameObjManager.getWorldName();
//			}
//			ImGuiFileDialog::Instance()->Close();
//		}
//
//		// Handle Save File dialog
//		if (ImGuiFileDialog::Instance()->Display("SaveFileDlg"))
//		{
//			if (ImGuiFileDialog::Instance()->IsOk())
//			{
//				std::string filePath = ImGuiFileDialog::Instance()->GetFilePathName();
//				std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
//
//				gameObjManager.save(filePath);
//			}
//			ImGuiFileDialog::Instance()->Close();
//		}
//		ImGui::End();
//
//	}
//
//	void Debugger::drawPhysicsDebuggerDialog()
//	{
//		if (!mEnabled) return;
//		static JPH::BodyManager::DrawSettings settings;
//		static DebugRenderer renderer;
//		ImGui::Begin("Physics Debug Renderer");
//		ImGui::Checkbox("Draw Velocity", &settings.mDrawVelocity);
//		ImGui::Checkbox("Draw Bounding Box", &settings.mDrawBoundingBox);
//		ImGui::Checkbox("Draw Mass and Inertia", &settings.mDrawMassAndInertia);
//		ImGui::Checkbox("Draw Shape", &settings.mDrawShape);
//		ImGui::Checkbox("Draw Shape Wireframe", &settings.mDrawShapeWireframe);
//		ImGui::Checkbox("Draw Center of Mass", &settings.mDrawCenterOfMassTransform);
//		ImGui::Checkbox("Draw Sleep Stat", &settings.mDrawSleepStats);
//		ImGui::End();
//
//		eg::Physics::getPhysicsSystem().DrawBodies(settings, &renderer, nullptr);
//	}
//}