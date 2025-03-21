
#include <GLFW/glfw3.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION

#include <imgui.h>

#include <vector>
#include <memory>

#include <Sandbox_Player.h>
#include <SandBox_MapObject.h>
#include <Physics.h>
#include <Network.h>
#include <Window.h>
#include <Data.h>
#include <Input.h>


class VisualStudioLogger  final : public eg::Logger
{
public:
	void trace(const std::string& message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}
	void info(const std::string& message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Info").c_str());
	}
	void warn(const std::string& message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Warn").c_str());
	}
	void error(const std::string& message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Error").c_str());
	}
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	using namespace eg;


	Logger::create(std::make_unique<VisualStudioLogger>());
	try
	{

		
		Window::create(1600, 900, "Sandbox");
		Renderer::create(1600, 900);
		Data::StaticModelRenderer::create();
		Data::LightRenderer::create();
		Physics::create();
		Input::Keyboard::create(Window::getHandle());
		Input::Mouse::create(Window::getHandle());

		{
			Data::GameObjectManager gameObjManager;
			
			auto player1 = std::make_unique<sndbx::Player>(true);
			Renderer::setCamera(&player1->getCamera());
			gameObjManager.addGameObject(std::move(player1));

			auto mo1 = std::make_unique<sndbx::MapObject>("models/sponza.glb");
			mo1->getTransform().mScale.x = 0.01f;
			mo1->getTransform().mScale.y = 0.01f;
			mo1->getTransform().mScale.z = 0.01f;
			gameObjManager.addGameObject(std::move(mo1));
			

			std::array<Data::PointLight, 2> pointLights = 
			{
				Data::PointLight{},
				Data::PointLight{}
			};


			while (!Window::shouldClose())
			{
				//Update
				gameObjManager.update(0.016f);

				
				Input::Keyboard::update();
				Input::Mouse::update();

				//Render
				auto cmd = Renderer::begin();

				ImGui::Begin("Debug");

				ImGui::DragFloat3("Light1 position", &pointLights[0].mUniformBuffer.position.x, 0.1f);
				ImGui::DragFloat3("Light2 position", &pointLights[1].mUniformBuffer.position.x, 0.1f);


				ImGui::End();


				//Subpass 0, gBuffer generation
				Renderer::getDefaultRenderPass().begin(cmd, vk::Rect2D({ 0, 0 }, {1600, 900}));
				Data::StaticModelRenderer::begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }));
				gameObjManager.render(cmd);

				//Subpass 1
				cmd.nextSubpass(vk::SubpassContents::eInline);
				Data::LightRenderer::renderAmbient(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet());
				Data::LightRenderer::renderPointLights(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet(), pointLights.data(), pointLights.size());

				Renderer::end();
				Window::poll();
			}

			Renderer::waitIdle();
		}
		Data::StaticModelCache::clear();
		Data::LightRenderer::destroy();
		Data::StaticModelRenderer::destroy();
		Renderer::destory();
		Window::destroy();
		Physics::destroy();
	}
	catch (std::exception& e)
	{
		MessageBox(nullptr, e.what(), "Standard exception", MB_ICONEXCLAMATION);
	}

	catch (...)
	{
		MessageBox(nullptr, "TODO", "Unknown exception", MB_ICONEXCLAMATION);
	}
	return 0;
}