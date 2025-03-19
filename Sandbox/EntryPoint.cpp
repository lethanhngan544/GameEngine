
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION

#include <imgui.h>

#include <vector>
#include <memory>

#include <Sandbox_Player.h>
#include <Network.h>
#include <Window.h>
#include <Data.h>



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

		{
			Data::GameObjectManager gameObjManager;
			

			Data::Camera camera;
			camera.mFov = 70.0f;
			camera.mFar = 1000.0f;

			auto player1 = std::make_unique<sndbx::Player>(true);
			player1->getTransform().mPosition.x += 20;
			player1->getTransform().mScale = { 0.01f, 0.01f, 0.01f };
			gameObjManager.addGameObject(std::move(player1));
			

			std::array<Data::PointLight, 2> pointLights = 
			{
				Data::PointLight{},
				Data::PointLight{}
			};


			while (!Window::shouldClose())
			{
				gameObjManager.update(0.016f);
				auto cmd = Renderer::begin(camera);

				ImGui::Begin("Debug");
				ImGui::DragFloat3("Camera position", &camera.mPosition.x, 0.1f);
				ImGui::DragFloat("Camera pitch", &camera.mPitch, 0.5f);
				ImGui::DragFloat("Camera yaw", &camera.mYaw, 0.5f);

				ImGui::DragFloat3("Light1 position", &pointLights[0].mUniformBuffer.position.x, 0.1f);
				ImGui::DragFloat3("Light2 position", &pointLights[1].mUniformBuffer.position.x, 0.1f);


				ImGui::End();


				//Subpass 0, gBuffer generation
				Renderer::getDefaultRenderPass().begin(cmd, vk::Rect2D({ 0, 0 }, {1600, 900}));
				Data::StaticModelRenderer::begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }));
				//client.renderAllPlayers(cmd, staticModelRenderer);

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