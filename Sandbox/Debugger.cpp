#include <Sandbox_Debugger.h>

#include <Logger.h>
#include <Input.h>
#include <Data.h>
#include <Window.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <ImGuiFileDialog.h>
#include <Physics.h>
#include <Jolt/Jolt.h>
#include <Jolt/Renderer/DebugRendererSimple.h>

namespace sndbx
{
	class DebugRenderer : public JPH::DebugRendererSimple
	{
	public:
		virtual void DrawLine(JPH::RVec3Arg inFrom, JPH::RVec3Arg inTo, JPH::ColorArg inColor) override
		{
			eg::Data::DebugRenderer::recordLine(glm::vec3(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ()),
				glm::vec3(inTo.GetX(), inTo.GetY(), inTo.GetZ()),
				glm::vec4(inColor.r, inColor.g, inColor.b, inColor.a));
		}
		virtual void DrawText3D(JPH::RVec3Arg inPosition, const std::string_view& inString, JPH::ColorArg inColor, float inHeight) override
		{

		}

	};

	void Debugger::checkKeyboardInput()
	{
		if (eg::Input::Keyboard::isKeyDownOnce(GLFW_KEY_F11))
		{
			mEnabled = !mEnabled;

			glfwSetInputMode(eg::Window::getHandle(), GLFW_CURSOR, mEnabled ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
			if (mEnabled)
			{
				eg::Logger::gInfo("Debugger enabled");
			}
			else
			{
				eg::Logger::gInfo("Debugger disabled");
			}
		}
	}

	void Debugger::drawLightDebuggerDialog(eg::Components::DirectionalLight& directionalLight)
	{
		if (!mEnabled) return;
		ImGui::Begin("Light");
		if (ImGui::DragFloat3("Direction", &directionalLight.mUniformBuffer.direction.x, 0.01f))
		{
			directionalLight.mUniformBuffer.direction = glm::normalize(directionalLight.mUniformBuffer.direction);
		}
		ImGui::End();
	}
	void Debugger::drawWorldDebuggerDialog(eg::World::GameObjectManager& gameObjManager,
		eg::World::JsonToIGameObjectDispatcher dispatcherFn)
	{
		if (!mEnabled) return;

		static std::string worldName = "Blank";

		ImGui::Begin("World Debugger");

		if (ImGui::InputText("World Name", &worldName))
		{
			gameObjManager.setWorldName(worldName);
		}
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

				
				gameObjManager.load(filePath, dispatcherFn);
				worldName = gameObjManager.getWorldName();
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

				gameObjManager.save(filePath);
			}
			ImGuiFileDialog::Instance()->Close();
		}
		ImGui::End();

	}

	void Debugger::drawPhysicsDebuggerDialog()
	{
		if (!mEnabled) return;
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
	}
}