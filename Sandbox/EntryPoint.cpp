
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION
#include <Core.h>

#include <imgui.h>

#include <vector>
#include <memory>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	using namespace eg;
	Logger::create(std::make_unique<VisualStudioLogger>());
	try
	{
		
		Window::create(1600, 900, "Sandbox");
		Renderer::create(1600, 900);

		{
			Data::StaticModelRenderer staticModelRenderer(Renderer::getDevice(), Renderer::getDefaultRenderPass().getRenderPass(),
				0, Renderer::getGlobalDescriptorSet());
			Data::Camera camera;
			camera.mFov = 70.0f;
			camera.mFar = 1000.0f;
			

			size_t entityCount = 2;
			std::array<Data::Entity, 10> entities;
			entities.at(0).add<Data::StaticModel>("models/DamagedHelmet.glb", staticModelRenderer.getMaterialSetLayout());
			entities.at(1).add<Data::StaticModel>("models/Sponza.glb", staticModelRenderer.getMaterialSetLayout());

			entities.at(1).mScale = { 0.01f, 0.01f, 0.01f };

			Data::PointLight newLight;


			while (!Window::shouldClose())
			{
				auto cmd = Renderer::begin(camera);

				ImGui::Begin("Debug");
				ImGui::DragFloat3("Camera position", &camera.mPosition.x, 0.1f);
				ImGui::DragFloat("Camera pitch", &camera.mPitch, 0.5f);
				ImGui::DragFloat("Camera yaw", &camera.mYaw, 0.5f);

				ImGui::DragFloat3("Light position", &newLight.mUniformBuffer.position.x, 0.5f);
				ImGui::End();


				//Subpass 0, gBuffer generation
				Renderer::getDefaultRenderPass().begin(cmd, vk::Rect2D({ 0, 0 }, {1600, 900}));
				
				staticModelRenderer.render(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet(),
					entities.data(), entityCount);


				//Subpass 1
				cmd.nextSubpass(vk::SubpassContents::eInline);
				Renderer::getAmbientLightPipeline().begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }));
				cmd.draw(3, 1, 0, 0);
				Renderer::getPointLightPipeline().begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }));
				Renderer::getPointLightPipeline().processPointLight(cmd, newLight);

				Renderer::end();
				Window::poll();
			}

			Renderer::waitIdle();
		}

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

	Logger::destroy();

	return 0;
}