
#include <GLFW/glfw3.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION

#include <imgui.h>

#include <vector>
#include <memory>

#include <Sandbox_Player.h>
#include <SandBox_PlayerControlled.h>
#include <SandBox_MapObject.h>
#include <SandBox_MapPhysicsObject.h>
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

	
	
	Logger::create(std::make_unique<VisualStudioLogger>());
	try
	{
		Window::create(1600, 900, "Sandbox");
		Input::Keyboard::create(Window::getHandle());
		Input::Mouse::create(Window::getHandle());
		Renderer::create(1600, 900);
		Data::StaticModelRenderer::create();
		Data::LightRenderer::create();
		Data::DebugRenderer::create();
		Data::ParticleRenderer::create();
		Physics::create();
		

		{
			Data::GameObjectManager gameObjManager;
			
			auto player1 = std::make_unique<sndbx::PlayerControlled>();
			auto player2 = std::make_unique<sndbx::Player>(true);
			Renderer::setCamera(&player1->getCamera());
			gameObjManager.addGameObject(std::move(player1));
			gameObjManager.addGameObject(std::move(player2));

			auto mo1 = std::make_unique<sndbx::MapObject>("models/map1.glb");
			gameObjManager.addGameObject(std::move(mo1));


			//for (uint32_t i = 0; i < 10; i++)
			//{
			//	//Generate random position from 10 to 10

			//	glm::vec3 position = { (float)(rand() % 10), 10.0f, (float)(rand() % 10)};
			//	auto mo2 = std::make_unique<sndbx::MapPhysicsObject>("models/box.glb", position);
			//	gameObjManager.addGameObject(std::move(mo2));

			//}
			glm::vec3 position = {0.0f, 10.0f, 0.0f };
			auto mo2 = std::make_unique<sndbx::MapPhysicsObject>("models/box.glb", position);
			gameObjManager.addGameObject(std::move(mo2));

			/*auto mo2 = std::make_unique<sndbx::MapObject>("models/VC.glb");
			gameObjManager.addGameObject(std::move(mo2));*/

		
			using Clock = std::chrono::high_resolution_clock;
			using TimePoint = std::chrono::time_point<Clock>;

			constexpr double TICK_RATE = 128;
			constexpr double TICK_INTERVAL = 1.0 / TICK_RATE;
			constexpr int MAX_FPS = 60;
			constexpr double FRAME_DURATION_CAP = 1.0 / MAX_FPS;
			TimePoint lastTime = Clock::now();
			double accumulator = 0.0;

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
					Input::Keyboard::update();
					accumulator -= TICK_INTERVAL;
				}
				//Update
				Input::Mouse::update();

				//Render 100 random lines
				
				Data::DebugRenderer::recordLine({ 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 0 });
				Data::DebugRenderer::recordLine({ 0, 0, 0 }, { 0, 1, 0 }, { 0, 1, 0 });
				Data::DebugRenderer::recordLine({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 0, 1 });

				//Render
				auto cmd = Renderer::begin(vk::Rect2D(vk::Offset2D{ 0, 0 }, vk::Extent2D{1600, 900}));
				Physics::render();


				Data::DebugRenderer::updateVertexBuffers(cmd);
				Renderer::getDefaultRenderPass().begin(cmd);
				//Subpass 0, gBuffer generation
				Data::StaticModelRenderer::begin(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS0_GBUFFER);

				//Subpass 1, lighting
				cmd.nextSubpass(vk::SubpassContents::eInline);
				Data::LightRenderer::renderAmbient(cmd);
				Data::LightRenderer::beginPointLight(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS1_POINTLIGHT);
				Data::DebugRenderer::render(cmd);


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
		Data::DebugRenderer::destroy();
		Data::ParticleRenderer::destroy();
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