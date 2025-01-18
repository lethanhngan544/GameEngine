
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
			Data::Camera camera;
			camera.mFov = 70.0f;
			camera.mFar = 100000.0f;
			

			std::vector<std::unique_ptr<Data::Entity>> entities;
			entities.push_back(std::make_unique<Data::Entity>());
			entities.push_back(std::make_unique<Data::Entity>());

			entities.at(0)->add<Data::StaticModel>("models/DamagedHelmet.glb");
			entities.at(1)->add<Data::StaticModel>("models/Sponza.glb");

			entities.at(1)->mScale = { 0.01f, 0.01f, 0.01f };

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
				Renderer::getStaticModelPipeline().begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }));


				//Static model rendering system
				for (const auto& e : entities)
				{
					if (!e->has<Data::StaticModel>()) continue;

					//Build model matrix
					Renderer::StaticModelPipeline::VertexPushConstant ps{};
					ps.model = glm::mat4(1.0f);
					ps.model = glm::scale(ps.model, e->mScale);
					ps.model *= glm::mat4_cast(e->mRotation);
					ps.model = glm::translate(ps.model, e->mPosition);

					Data::StaticModel& model = e->get<Data::StaticModel>();
					cmd.pushConstants(Renderer::getStaticModelPipeline().getPipelineLayout(),
						vk::ShaderStageFlagBits::eVertex, 0, sizeof(ps), &ps);
					for (const auto& rawMesh : model.getRawMeshes())
					{
						cmd.bindVertexBuffers(0, { rawMesh.vertexBuffer.getBuffer() }, { 0 });
						cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
						cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, Renderer::getStaticModelPipeline().getPipelineLayout(),
							1, { model.getMaterials().at(rawMesh.materialIndex).mSet }, {});
						cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
					}
				}
				

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