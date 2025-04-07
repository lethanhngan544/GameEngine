
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
#include <chrono>
#include <thread>


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

	using Clock = std::chrono::high_resolution_clock;
	using TimePoint = std::chrono::time_point<Clock>;

	constexpr double TICK_RATE = 66.666;
	constexpr double TICK_INTERVAL = 1.0 / TICK_RATE;
	constexpr int MAX_FPS = 120;
	constexpr double FRAME_DURATION_CAP = 1.0 / MAX_FPS;
	TimePoint lastTime = Clock::now();
	double accumulator = 0.0;
	
	Logger::create(std::make_unique<VisualStudioLogger>());
	try
	{
		Window::create(1600, 900, "Sandbox");
		Input::Keyboard::create(Window::getHandle());
		Input::Mouse::create(Window::getHandle());
		Renderer::create(1600, 900);
		Data::StaticModelRenderer::create();
		Data::LightRenderer::create();
		Physics::create();
		

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
		


			while (!Window::shouldClose())
			{
				TimePoint now = Clock::now();
				double deltaTime = std::chrono::duration<double>(now - lastTime).count();
				lastTime = now;
				if (deltaTime > 0.25) deltaTime = 0.25;
				accumulator += deltaTime;
				while (accumulator >= TICK_INTERVAL) {
					
					Physics::update(TICK_INTERVAL);
					gameObjManager.update(TICK_INTERVAL);
					accumulator -= TICK_INTERVAL;
				}
				//Update

				
				Input::Keyboard::update();
				Input::Mouse::update();

				//Render
				auto cmd = Renderer::begin(vk::Rect2D(vk::Offset2D{ 0, 0 }, vk::Extent2D{1600, 900}));

				ImGui::Begin("Debug");
				ImGui::End();


				Renderer::getDefaultRenderPass().begin(cmd);
				//Subpass 0, gBuffer generation
				Data::StaticModelRenderer::begin(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS0_GBUFFER);

				//Subpass 1, lighting
				cmd.nextSubpass(vk::SubpassContents::eInline);
				Data::LightRenderer::renderAmbient(cmd);
				Data::LightRenderer::beginPointLight(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS1_POINTLIGHT);

				//Subpass 2, debug draw
				//cmd.nextSubpass(vk::SubpassContents::eInline);



				Renderer::end();
				Window::poll();

				TimePoint frameEnd = Clock::now();
				double frameTime = std::chrono::duration<double>(frameEnd - now).count();
				if (frameTime < FRAME_DURATION_CAP) {
					std::this_thread::sleep_for(std::chrono::duration<double>(FRAME_DURATION_CAP - frameTime));
				}
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