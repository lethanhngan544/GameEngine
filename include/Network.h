#pragma once


#include <vector>
#include <deque>
#include <thread>
#include <memory>

#include <asio.hpp>
#include "Logger.h"

namespace eg::Network
{
	enum class PacketType : uint32_t
	{
		ServerPing = 0,

		ClientAccepted,
		ClientAssignID,
		ClientRegister,
		ClientUnregister,

		GameAddPlayer,
		GameRemovePlayer,
		GameUpdatePlayer,

		PacketTypeEnd,
	};
	class Connection;
	struct Packet
	{
		uint32_t id{};
		uint32_t size{};
		std::shared_ptr<Connection> connection = nullptr; // To be use by IServer
		std::vector<uint8_t> data{};

		static size_t headerSize()
		{
			return sizeof(id) + sizeof(size) + sizeof(connection);
		}

		size_t getSize() const
		{
			return headerSize() + data.size();
		}

		//Push POD data 
		template<typename T>
		friend Packet& operator<<(Packet& packet, const T& data)
		{
			static_assert(std::is_standard_layout<T>::value, "Data is too complex.");

			size_t i = packet.data.size();
			packet.data.resize(packet.data.size() + sizeof(T));
			std::memcpy(packet.data.data() + i, &data, sizeof(T));

			packet.size = static_cast<uint32_t>(packet.data.size());

			return packet;
		}

		template<typename T>
		friend Packet& operator>>(Packet& packet, T& data)
		{
			static_assert(std::is_standard_layout<T>::value, "Data is too complex.");

			size_t i = packet.data.size() - sizeof(T);

			std::memcpy(&data, packet.data.data() + i, sizeof(T));

			packet.data.resize(i);


			packet.size = static_cast<uint32_t>(packet.data.size());

			return packet;
		}
	};

	class NetQueue
	{
	private:
		std::deque<Packet> mQueue;
		std::mutex mMux;
	public:
		NetQueue() = default;
		NetQueue(const NetQueue&) = delete;
		~NetQueue() { clear(); }

		const Packet& front()
		{
			std::scoped_lock lock(mMux);
			return mQueue.front();
		}

		const Packet& back()
		{
			std::scoped_lock lock(mMux);
			return mQueue.back();
		}

		void pop_front()
		{
			std::scoped_lock lock(mMux);
			mQueue.pop_front();
		}

		Packet pop_front_return()
		{
			std::scoped_lock lock(mMux);
			auto t = std::move(mQueue.front());
			mQueue.pop_front();
			return t;
		}

		void pop_back()
		{
			std::scoped_lock lock(mMux);
			mQueue.pop_back();
		}

		Packet pop_back_return()
		{
			std::scoped_lock lock(mMux);
			auto t = std::move(mQueue.back());
			mQueue.pop_back();
			return t;
		}

		void push_front(const Packet& p)
		{
			std::scoped_lock lock(mMux);
			mQueue.push_front(p);
		}

		void push_back(const Packet& p)
		{
			std::scoped_lock lock(mMux);
			mQueue.push_back(p);
		}


		bool empty()
		{
			std::scoped_lock lock(mMux);
			return mQueue.empty();
		}

		void clear()
		{
			std::scoped_lock lock(mMux);
			mQueue.clear();
		}
	};

	class Connection : public std::enable_shared_from_this<Connection>
	{
	private:
		asio::io_context& mContext;
		asio::ip::tcp::socket mSocket;
		NetQueue& mQueueIn;// from the server
		NetQueue mQueueOut; // To the client
		uint64_t mId = 0;
		uint64_t mHandShakeOut = 0;
		uint64_t mHandShakeIn = 0;
		Packet mTempPacket;
	public:
		Connection(asio::io_context& context, asio::ip::tcp::socket socket, NetQueue& inQueue, uint64_t handShakeOut) :
			mContext(context), mQueueIn(inQueue), mSocket(std::move(socket)), mHandShakeOut(handShakeOut)
		{
		}
		virtual ~Connection() = default;
	public:
		void disconnect()
		{
			if (isConnected())
			{
				asio::post(mContext, [this]() { mSocket.close(); });
			}
		}
		bool isConnected() const
		{
			return mSocket.is_open();
		}
		bool connectToClient(uint64_t id)
		{
			if (mSocket.is_open())
			{
				mId = id;
				asyncWriteValidation();
				asyncReadValidation();
			}
			return mSocket.is_open();
		}

		void sendToClient(const Packet& packet)
		{
			asio::post(mContext, [this, packet]
				{
					bool writingMessage = !mQueueOut.empty();
					mQueueOut.push_back(packet);
					if (!writingMessage)
						asyncWriteHeader();
				});
		}
	private:

		uint64_t scramble(uint64_t id)
		{
			uint64_t out = id ^ 0xDEADBEEFC0DECAFE;
			out = (out & 0xF0F0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}

		void asyncReadHeader()
		{
			asio::async_read(mSocket, asio::buffer(&mTempPacket.id, Packet::headerSize()),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						if (mTempPacket.size > 0)
						{
							mTempPacket.data.resize(mTempPacket.size);
							asyncReadBody();
						}
						else
						{
							mQueueIn.push_back(Packet{ mTempPacket.id, mTempPacket.size, this->shared_from_this(), mTempPacket.data });
							asyncReadHeader();
						}
					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}
		void asyncReadBody()
		{
			asio::async_read(mSocket, asio::buffer(mTempPacket.data.data(), mTempPacket.size),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						mQueueIn.push_back(Packet{ mTempPacket.id, mTempPacket.size, this->shared_from_this(), mTempPacket.data });
						asyncReadHeader();
					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}

		void asyncWriteHeader()
		{
			asio::async_write(mSocket, asio::buffer(&mQueueOut.front().id, Packet::headerSize()),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						if (mQueueOut.front().data.size() > 0)
						{
							asyncWriteBody();
						}
						else
						{
							mQueueOut.pop_front();
							if (!mQueueOut.empty())
							{
								asyncWriteHeader();
							}
						}
					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}

		void asyncWriteBody()
		{
			asio::async_write(mSocket, asio::buffer(mQueueOut.front().data.data(), mQueueOut.front().size),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						mQueueOut.pop_front();
						if (!mQueueOut.empty())
						{
							asyncWriteHeader();
						}
					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}

		void asyncWriteValidation()
		{
			asio::async_write(mSocket, asio::buffer(&mHandShakeOut, sizeof(mHandShakeOut)),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{

					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}

		void asyncReadValidation()
		{
			asio::async_read(mSocket, asio::buffer(&mHandShakeIn, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (mHandShakeIn == scramble(mHandShakeOut))
						{
							Logger::gTrace("Client validated | ID: " + std::to_string(mId));
							asyncReadHeader();
						}
						else
						{
							Logger::gWarn("Client failed to validate | ID: " + std::to_string(mId));
							mSocket.close();
						}
					}
					else
					{
						Logger::gWarn("Connection lost: " + std::to_string(mId));
						mSocket.close();
					}
				});
		}
	};

	class IServer
	{
	private:
		NetQueue mMessageIn;

		std::deque<std::shared_ptr<Connection>> mConnections;

		asio::io_context				mContext;
		std::thread mThreadContext;

		asio::ip::tcp::acceptor mAcceptor;
		uint64_t mIdCounter = 69000;

		uint64_t mHandShakeID = 0;

	public:
		IServer(uint16_t port) :
			mAcceptor(mContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
		{
			mHandShakeID = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count();

		}

		virtual ~IServer()
		{
			mContext.stop();
			mThreadContext.join();
			Logger::gInfo("Server stopped.");
		}




		bool start()
		{
			try
			{
				asyncWaitforClient();
				mThreadContext = std::thread([this]() { mContext.run(); });
			}
			catch (std::exception& e)
			{
				Logger::gError(e.what());
				return false;
			}

			Logger::gInfo("Server started");
			return true;
		}

		void asyncWaitforClient()
		{
			mAcceptor.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket)
				{
					if (!ec)
					{
						Logger::gInfo("New connection: " + socket.remote_endpoint().address().to_string());
						std::shared_ptr<Connection> client = std::make_shared<Connection>(mContext, std::move(socket), mMessageIn, mHandShakeID);
						mConnections.push_back(std::move(client));
						if (mConnections.back()->connectToClient(mIdCounter++))
						{
							Logger::gTrace("Client accepted | ID: " + std::to_string(mIdCounter - 1));
							onClientConnect(client);
						}



					}
					else
					{
						Logger::gError(ec.message());
					}


					asyncWaitforClient();
				});
		}

		void messageClient(std::shared_ptr<Connection> client, const Packet& p)
		{
			if (client && client->isConnected())
			{
				client->sendToClient(p);
			}
			else
			{
				onClientDisconnect(client);
				client.reset();
				mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), client), mConnections.end());
			}
		}

		void messageAllClient(const Packet& p, std::shared_ptr<Connection> ignoreClient = nullptr)
		{
			bool invalidClientExists = false;
			for (auto& client : mConnections)
			{
				if (client && client->isConnected())
				{
					if (client != ignoreClient)
					{
						client->sendToClient(p);
					}
				}
				else
				{
					onClientDisconnect(client);
					client.reset();
					invalidClientExists = true;
				}
			}
			if (invalidClientExists)
			{
				mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), nullptr), mConnections.end());
			}
		}
		void update(size_t maxMessages = -1)
		{

			size_t messageCount = 0;
			while (messageCount < maxMessages && !mMessageIn.empty())
			{
				auto msg = mMessageIn.pop_front_return();
				onMessage(msg.connection, msg);
				messageCount++;
			}
		}
	protected:
		virtual void onClientConnect(std::shared_ptr<Connection> client) {}
		virtual void onClientDisconnect(std::shared_ptr<Connection> client) {}
		virtual void onMessage(std::shared_ptr<Connection> client, Packet& p) {}
	};


	class IClient
	{
	private:
		asio::io_context				mContext;
		std::thread						mThreadContext;
		NetQueue						mQueueIn; // From the server
		NetQueue						mQueueOut; // To the server	
		asio::ip::tcp::socket			mSocket;
		Packet							mTempPacket;

		uint64_t mHandShakeIn = 0;
		uint64_t mHandShakeOut = 0;
	public:
		IClient() : mSocket(mContext) {}
		virtual ~IClient() { disconnect(); }

		void update(size_t maxMessages = -1)
		{
			if (!mSocket.is_open())
				return;

			size_t messageCount = 0;
			while (messageCount < maxMessages && !mQueueIn.empty())
			{
				auto msg = mQueueIn.pop_front_return();
				onMessage(msg);
				messageCount++;
			}
		}



		bool connect(const std::string& host, const uint16_t port)
		{
			try
			{
				asio::ip::tcp::resolver resolver(mContext);
				asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port)); //TODO
				//Connect to server
				asio::async_connect(mSocket, endpoints,
					[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
					{
						if (!ec)
						{
							asyncReadValidation();
						}
						else
						{
							Logger::gError(ec.message());
						}
					});
				//asio::connect(mSocket, endpoints, );

				mThreadContext = std::thread([this]() { mContext.run();});
			}
			catch (std::exception& e)
			{
				Logger::gError(e.what());
				return false;
			}
			return true;
		}

		void disconnect()
		{
			mSocket.close();
			mContext.stop();
			mThreadContext.join();

		}

		bool isConnected() const
		{
			return mSocket.is_open();
		}

		void sendToServer(const Packet& packet)
		{
			asio::post(mContext, [this, packet]
				{
					bool writingMessage = !mQueueOut.empty();
					mQueueOut.push_back(packet);
					if (!writingMessage)
						asyncWriteHeader();
				});
		}

		void asyncReadHeader()
		{
			asio::async_read(mSocket, asio::buffer(&mTempPacket.id, Packet::headerSize()),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						if (mTempPacket.size > 0)
						{
							mTempPacket.data.resize(mTempPacket.size);
							asyncReadBody();
						}
						else
						{
							mQueueIn.push_back(Packet{ mTempPacket.id, mTempPacket.size, nullptr, mTempPacket.data });
							asyncReadHeader();
						}
					}
					else
					{
						Logger::gWarn("Client Connection lost !");
						mSocket.close();
					}
				});
		}
		void asyncReadBody()
		{
			asio::async_read(mSocket, asio::buffer(mTempPacket.data.data(), mTempPacket.size),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						mQueueIn.push_back(Packet{ mTempPacket.id, mTempPacket.size, nullptr, mTempPacket.data });
						asyncReadHeader();
					}
					else
					{
						Logger::gWarn("Client Connection lost !");
						mSocket.close();
					}
				});
		}

		void asyncWriteHeader()
		{
			asio::async_write(mSocket, asio::buffer(&mQueueOut.front().id, Packet::headerSize()),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						if (mQueueOut.front().data.size() > 0)
						{
							asyncWriteBody();
						}
						else
						{
							mQueueOut.pop_front();
							if (!mQueueOut.empty())
							{
								asyncWriteHeader();
							}
						}
					}
					else
					{
						Logger::gWarn("Connection to sever lost !");
						mSocket.close();
					}
				});
		}

		void asyncWriteBody()
		{
			asio::async_write(mSocket, asio::buffer(mQueueOut.front().data.data(), mQueueOut.front().size),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						mQueueOut.pop_front();
						if (!mQueueOut.empty())
						{
							asyncWriteHeader();
						}
					}
					else
					{
						Logger::gWarn("Connection server lost !");
						mSocket.close();
					}
				});
		}

		void asyncWriteValidation()
		{
			asio::async_write(mSocket, asio::buffer(&mHandShakeOut, sizeof(mHandShakeOut)),
				[this](std::error_code ec, size_t length)
				{
					if (!ec)
					{
						asyncReadHeader();
					}
					else
					{
						Logger::gWarn("Connection to server lost !");
						mSocket.close();
					}
				});
		}

		void asyncReadValidation()
		{
			asio::async_read(mSocket, asio::buffer(&mHandShakeIn, sizeof(uint64_t)),
				[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						mHandShakeOut = scramble(mHandShakeIn);
						asyncWriteValidation();
					}
					else
					{
						Logger::gWarn("Connection to server lost !");
						mSocket.close();
					}
				});
		}

		uint64_t scramble(uint64_t id)
		{
			uint64_t out = id ^ 0xDEADBEEFC0DECAFE;
			out = (out & 0xF0F0F0F0F0F0F0F0) >> 4 | (out & 0x0F0F0F0F0F0F0F0F) << 4;
			return out ^ 0xC0DEFACE12345678;
		}

	protected:
		virtual void onMessage(Packet& p) {}
	};


}