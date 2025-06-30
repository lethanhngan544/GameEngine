
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
#include <SandBox_Debugger.h>
#include <Physics.h>
#include <Network.h>
#include <Window.h>
#include <Data.h>
#include <Input.h>
#include <World.h>
#include <chrono>
#include <thread>

#include <stb_image.h>
#include <ImGuiFileDialog.h>


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
		Renderer::create(1600, 900, 8192);
		Data::StaticModelRenderer::create();
		Data::AnimatedModelRenderer::create();
		Data::LightRenderer::create();
		Data::DebugRenderer::create();
		Data::ParticleRenderer::create();
		Physics::create();

		{
			sndbx::Debugger debugger;
			World::GameObjectManager gameObjManager;
			Components::DirectionalLight directionalLight;
			directionalLight.mUniformBuffer.intensity = 1.0f;
			directionalLight.mUniformBuffer.direction = { 1.01f, -1.0f, 0.0f };

			Renderer::setDirectionalLight(&directionalLight);
			
		
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
		
			using Clock = std::chrono::high_resolution_clock;
			using TimePoint = std::chrono::time_point<Clock>;

			constexpr double TICK_RATE = 60;
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
					gameObjManager.fixedUpdate(TICK_INTERVAL);
					accumulator -= TICK_INTERVAL;
				}
				//Update
				debugger.checkKeyboardInput();
				gameObjManager.update(deltaTime);
				Input::Keyboard::update();
				Input::Mouse::update();


				//Render
				//Retrieve with and height of the glfw window
				auto cmd = Renderer::begin();
				debugger.drawPhysicsDebuggerDialog();
				debugger.drawLightDebuggerDialog(directionalLight);
				debugger.drawWorldDebuggerDialog(gameObjManager, jsonDispatcher);
				

				
				Data::DebugRenderer::updateVertexBuffers(cmd);
				Data::ParticleRenderer::updateBuffers(cmd);


				Renderer::getShadowRenderPass().begin(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SHADOW);

				cmd.endRenderPass();

				Renderer::getDefaultRenderPass().begin(cmd);
				//Subpass 0, gBuffer generation
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS0_GBUFFER);

				//Subpass 1, lighting
				cmd.nextSubpass(vk::SubpassContents::eInline);
				Data::LightRenderer::renderAmbient(cmd);

				directionalLight.update();
				Data::LightRenderer::renderDirectionalLight(cmd, directionalLight);
				Data::LightRenderer::beginPointLight(cmd);
				gameObjManager.render(cmd, Renderer::RenderStage::SUBPASS1_POINTLIGHT);
				Data::ParticleRenderer::render(cmd);
				Data::DebugRenderer::render(cmd);


				Renderer::end();
				Window::poll();
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

	
	Data::DebugRenderer::destroy();
	Data::ParticleRenderer::destroy();
	Data::LightRenderer::destroy();
	Data::StaticModelRenderer::destroy();
	Data::AnimatedModelRenderer::destroy();
	Renderer::destory();
	Window::destroy();
	Physics::destroy();

	return 0;
}