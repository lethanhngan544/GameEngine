
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define VMA_IMPLEMENTATION
#include <Core.h>

#include <imgui.h>

#include <vector>
#include <memory>


struct PlayerInfo
{
	uint64_t id;

	glm::vec3 position;
	float pitch, yaw;

};

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



class Client final : public eg::Network::IClient
{
private:
	uint64_t mId; // Id of this client
	std::unordered_map<uint64_t, std::pair<PlayerInfo, std::unique_ptr<eg::Data::Entity>>> mPlayers; // Map id to player info
public:
	Client(vk::DescriptorSetLayout materialSetLayout) :
		eg::Network::IClient()
	{
		/*	mPlayers[0].first.id = 0;
			mPlayers[0].first.position = { 0.0f, 0.0f, 0.0f };
			mPlayers[0].first.pitch = 0.0f;
			mPlayers[0].first.yaw = 0.0f;
			mPlayers[0].second = std::make_unique<eg::Data::Entity>();
			mPlayers[0].second->add<eg::Data::StaticModel>("models/DamagedHelmet.glb", materialSetLayout);*/

		if (!connect("localhost", 1234))
		{
			throw std::runtime_error("Failed to connect to server !");

		}



	}


	void renderAllPlayers(vk::CommandBuffer cmd, eg::Data::StaticModelRenderer& staticModelRenderer)
	{
		for (const auto& [id, infoPair] : mPlayers)
		{

			infoPair.second->mPosition = infoPair.first.position;

			staticModelRenderer.render(cmd, *(infoPair.second));

		}
	}

	void updatePlayerState()
	{
		eg::Network::Packet packet;
		packet.id = static_cast<uint32_t>(eg::Network::PacketType::GameUpdatePlayer);
		packet << mPlayers.at(mId).first;
		sendToServer(packet);

		eg::Network::IClient::update();
	}
protected:
	void onMessage(eg::Network::Packet& p) final
	{
		switch (p.id)
		{

		case (uint32_t)eg::Network::PacketType::ClientAccepted:
		{
			eg::Network::Packet packet;
			packet.id = static_cast<uint32_t>(eg::Network::PacketType::ClientRegister);
			PlayerInfo info;
			info.position = { 0.0f, 0.0f, 0.0f };
			info.pitch = 0.0f;
			info.yaw = 0.0f;
			packet << info;

			sendToServer(packet);
			break;
		}
		case (uint32_t)eg::Network::PacketType::ClientAssignID:
		{
			p >> mId;
			break;
		}
		case (uint32_t)eg::Network::PacketType::GameAddPlayer:
		{
			PlayerInfo info;

			p >> info;
			this->mPlayers.insert_or_assign(info.id, std::make_pair(info, std::make_unique<eg::Data::Entity>()));
			break;
		}

		case (uint32_t)eg::Network::PacketType::GameRemovePlayer:
		{
			uint64_t removalId;

			p >> removalId;
			this->mPlayers.erase(removalId);

			break;
		}

		case (uint32_t)eg::Network::PacketType::GameUpdatePlayer:
		{
			PlayerInfo info;
			p >> info;
			this->mPlayers.at(info.id).first = info;
			break;


		}
		}
	}
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	using namespace eg;
	Logger::create(std::make_unique<VisualStudioLogger>());
	try
	{
		//entities.at(0).add<Data::StaticModel>("models/DamagedHelmet.glb", staticModelRenderer.getMaterialSetLayout());

		Window::create(1600, 900, "Sandbox");
		Renderer::create(1600, 900);

		{
			Data::LightRenderer lightRenderer(Renderer::getDevice(), Renderer::getDescriptorPool(), Renderer::getDefaultRenderPass(), Renderer::getGlobalDescriptorSet());

			Data::StaticModelRenderer staticModelRenderer(0);
			Data::Camera camera;
			camera.mFov = 70.0f;
			camera.mFar = 1000.0f;
			

			Client client(staticModelRenderer.getMaterialSetLayout());
			

			Data::PointLight pointLights[] = 
			{
				Data::PointLight(lightRenderer.getPointLightPerDescLayout()),
				Data::PointLight(lightRenderer.getPointLightPerDescLayout())
			};


			while (!Window::shouldClose())
			{
				client.updatePlayerState();
				auto cmd = Renderer::begin(camera);

				ImGui::Begin("Debug");
				ImGui::DragFloat3("Camera position", &camera.mPosition.x, 0.1f);
				ImGui::DragFloat("Camera pitch", &camera.mPitch, 0.5f);
				ImGui::DragFloat("Camera yaw", &camera.mYaw, 0.5f);

				if (ImGui::Button("Send message"))
				{
					eg::Network::Packet packet;
					packet.id = static_cast<uint32_t>(eg::Network::PacketType::ServerPing);
					packet << 69;
					client.sendToServer(packet);
				}

				ImGui::DragFloat3("Light1 position", &pointLights[0].mUniformBuffer.position.x, 0.1f);
				ImGui::DragFloat3("Light2 position", &pointLights[1].mUniformBuffer.position.x, 0.1f);


				ImGui::End();


				//Subpass 0, gBuffer generation
				Renderer::getDefaultRenderPass().begin(cmd, vk::Rect2D({ 0, 0 }, {1600, 900}));
				staticModelRenderer.begin(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet());
				client.renderAllPlayers(cmd, staticModelRenderer);

				/*staticModelRenderer.render(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet(),
					entities.data(), entityCount);*/


				//Subpass 1
				cmd.nextSubpass(vk::SubpassContents::eInline);
				lightRenderer.renderAmbient(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet());
				lightRenderer.renderPointLights(cmd, vk::Rect2D({ 0, 0 }, { 1600, 900 }), Renderer::getCurrentFrameGUBODescSet(), pointLights, 2);

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


	return 0;
}