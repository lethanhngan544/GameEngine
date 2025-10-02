
#include <GLFW/glfw3.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION

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
#include <World.h>
#include <Debug.h>
#include <Core.h>
#include <chrono>
#include <thread>

#include <stb_image.h>
#include <ImGuiFileDialog.h>


class VisualStudioLogger  final : public eg::Logger
{
public:
	void trace(const std::string message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Trace").c_str());
	}
	void info(const std::string message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Info").c_str());
	}
	void warn(const std::string message) final
	{
		OutputDebugStringA(Logger::formatMessage(message, "Engine", "Warn").c_str());
	}
	void error(const std::string message) final
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
		Command::Var* maxFPSCvar = Command::findVar("eg::MaxFPS");
		maxFPSCvar->value = 120.0;
		Command::Var* maxTPSCvar = Command::findVar("eg::MaxTPS");
		maxTPSCvar->value = 60.0;

		Physics::create();
		Window::create(1600, 900, "Sandbox");
		Input::Keyboard::create(Window::getHandle());
		Input::Mouse::create(Window::getHandle());
		Renderer::create(1600, 900, 2048);
		Debug::create();
		Data::LightRenderer::create();
		Data::SkyRenderer::create();
		Data::DebugRenderer::create();
		Data::ParticleRenderer::create();
		Components::StaticModel::create();
		Components::AnimatedModel::create();
		World::create();

		{

			World::JsonToIGameObjectDispatcher jsonDispatcher =
				[](const nlohmann::json& jsonObj, const std::string& type) -> std::unique_ptr<World::IGameObject>
				{
					if (type == "PlayerControlled")
					{
						auto player = std::make_unique<sndbx::PlayerControlled>();
						Renderer::setCamera(&player->getCamera());
						return player;
					}
					else if (type == "Player")
					{
						auto player = std::make_unique<sndbx::Player>();
						return player;
					}
					else if (type == "MapObject")
					{
						auto mapObject = std::make_unique<sndbx::MapObject>();
						return mapObject;
					}
					else if (type == "MapPhysicsObject")
					{
						auto mapPhysicsObject = std::make_unique<sndbx::MapPhysicsObject>();
						return mapPhysicsObject;
					}
					throw World::JsonToIGameObjectException(type, "Unknown game object type");
					return nullptr;
				};
			Renderer::setDebugRenderFunction(
				[&](vk::CommandBuffer cmd, float alpha)
				{
					
				});
			Renderer::setShadowRenderFunction(
				[&](vk::CommandBuffer cmd, float alpha)
				{
					World::render(cmd, alpha,Renderer::RenderStage::SHADOW);
				});
			Renderer::setGBufferRenderFunction(
				[&](vk::CommandBuffer cmd, float alpha)
				{
					World::render(cmd, alpha, Renderer::RenderStage::SUBPASS0_GBUFFER);
				});
			Renderer::setLightRenderFunction(
				[&](vk::CommandBuffer cmd, float alpha)
				{
					World::render(cmd, alpha, Renderer::RenderStage::SUBPASS1_POINTLIGHT);
				});

			World::load("world.json", jsonDispatcher);

			using Clock = std::chrono::high_resolution_clock;
			using TimePoint = std::chrono::time_point<Clock>;

		
			TimePoint lastTime = Clock::now();
			double accumulator = 0.0;

			while (!Window::shouldClose())
			{
				double frameTime = 1.0 / maxFPSCvar->value;
				double tickInterval = 1.0 / maxTPSCvar->value;

				TimePoint now = Clock::now();
				double deltaTime = std::chrono::duration<double>(now - lastTime).count();
				lastTime = now;
				deltaTime = std::min(deltaTime, 0.25);
				accumulator += deltaTime;
				while (accumulator >= tickInterval) {
					World::prePhysicsUpdate(tickInterval);
					Physics::update(tickInterval);
					World::fixedUpdate(tickInterval);
					accumulator -= tickInterval;
				}

				//Update
				float alpha = static_cast<float>(accumulator / tickInterval);
				eg::Debug::checkForKeyboardInput();
				World::update(deltaTime, alpha);
				Input::Keyboard::update();
				Input::Mouse::update();

				//Render
				Renderer::render(alpha, deltaTime);
				Window::poll();

				// Frame pacing
				TimePoint frameEnd = Clock::now();
				double currentTime = std::chrono::duration<double>(frameEnd - now).count();
				if (currentTime < frameTime) {
					auto sleepDuration = std::chrono::duration<double>(frameTime - currentTime);
					std::this_thread::sleep_for(sleepDuration);
				}


			}
			Renderer::waitIdle();

			
		}

	}
	catch (std::exception e)
	{
		MessageBox(nullptr, e.what(), "Standard exception", MB_ICONEXCLAMATION);
	}

	catch (...)
	{
		MessageBox(nullptr, "TODO", "Unknown exception", MB_ICONEXCLAMATION);
	}

	World::destroy();
	Components::AnimatedModel::destroy();
	Components::StaticModel::destroy();
	Data::DebugRenderer::destroy();
	Data::ParticleRenderer::destroy();
	Data::SkyRenderer::destroy();
	Data::LightRenderer::destroy();
	Debug::destroy();
	Renderer::destory();
	Window::destroy();
	Physics::destroy();

	return 0;
}