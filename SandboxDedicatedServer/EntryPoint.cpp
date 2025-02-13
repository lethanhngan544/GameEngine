#include <Network.h>

#include <glm/glm.hpp>

struct PlayerInfo
{
	uint64_t id;

	glm::vec3 position;
	float pitch, yaw;

};


class Server final : public eg::Network::IServer
{
private:
	std::unordered_map<uint64_t, std::pair<PlayerInfo, std::shared_ptr<eg::Network::Connection>>> mPlayers; // Map id to player info
public:
	Server() :
		eg::Network::IServer(1234)
	{
	};

protected:
	void onClientConnect(std::shared_ptr<eg::Network::Connection> client) final 
	{
		eg::Network::Packet packet;
		packet.id = static_cast<uint32_t>(eg::Network::PacketType::ClientAccepted);
		client->sendToClient(packet);
	}
	void onClientDisconnect(std::shared_ptr<eg::Network::Connection> client) final 
	{
	}
	void onMessage(std::shared_ptr<eg::Network::Connection> client, eg::Network::Packet& p) final
	{
		switch (p.id)
		{
		case (uint32_t)eg::Network::PacketType::ClientRegister:
		{
			PlayerInfo info;
			p >> info;
			eg::Network::Packet packet;
			packet.id = static_cast<uint32_t>(eg::Network::PacketType::GameAddPlayer);
			packet << info;
			messageAllClient(packet, client);
			break;
		}
		case (uint32_t)eg::Network::PacketType::GameUpdatePlayer:
		{
			PlayerInfo info;
			p >> info;
			eg::Network::Packet packet;
			packet.id = static_cast<uint32_t>(eg::Network::PacketType::GameUpdatePlayer);
			packet << info;
			messageAllClient(packet, client);
			break;
		}
		}
	}

};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	
	eg::Logger::create(std::make_unique<eg::FileLogger>("output.txt"));
	Server server;
	server.start();

	while (1)
	{
		server.update();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}


}