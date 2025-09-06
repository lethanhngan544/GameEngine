#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

#include <MyVulkan.h>
#include <RenderStages.h>
#include <Components.h>

namespace eg::World
{
	class IGameObject
	{
	public:
		IGameObject() = default;
		virtual ~IGameObject() = default;

		virtual void update(float delta, float alpha) = 0;
		virtual void prePhysicsUpdate(float delta) = 0;
		virtual void fixedUpdate(float delta) = 0;
		virtual void render(vk::CommandBuffer cmd, float alpha, Renderer::RenderStage stage) = 0;
		virtual nlohmann::json toJson() const = 0;
		virtual void fromJson(const nlohmann::json& json) = 0;
		virtual const char* getType() const = 0;
	};

	class DynamicWorldObject final : public IGameObject
	{
	private:
		std::shared_ptr<Components::StaticModel> mModel = nullptr;
		eg::Components::RigidBody mBody;
	public:
		DynamicWorldObject() = default;
		virtual ~DynamicWorldObject();

		virtual void update(float delta, float alpha) final;
		virtual void prePhysicsUpdate(float delta) final { mBody.updatePrevState(); }
		virtual void fixedUpdate(float delta) final;
		virtual void render(vk::CommandBuffer cmd, float alpha, Renderer::RenderStage stage) final;
		virtual nlohmann::json toJson() const final { return {}; }
		virtual void fromJson(const nlohmann::json& json) final;
		virtual const char* getType() const final { return "DynamicWorldObject"; }
	};


	//Json load dispatcher function pointer
	using JsonToIGameObjectDispatcher = std::function<std::unique_ptr<IGameObject>(const nlohmann::json&, const std::string&)>;

	//To be use in case of an unknown game object type
	class JsonToIGameObjectException : public std::exception
	{
	private:
		std::string mType;
		std::string mMessage;
		mutable std::string mMessageCache;
	public:
		JsonToIGameObjectException(const std::string& type, const std::string& message)
			: mType(type), mMessage(message) {
		}

		const char* what() const noexcept override {
			mMessageCache = "Game object type: " + mType + ". " + mMessage;
			return mMessage.c_str();
		}
	};

	class GameObjectManager
	{
	private:
		std::string mWorldName;
		std::vector<std::unique_ptr<IGameObject>> mGameObjects;
	public:
		GameObjectManager() = default;
		~GameObjectManager();


		void cleanup();
		void addGameObject(std::unique_ptr<IGameObject> gameobject);
		void removeGameObject(const IGameObject* gameObject);

		void update(float delta, float alpha);
		void prePhysicsUpdate(float delta);
		void fixedUpdate(float delta);
		void render(vk::CommandBuffer cmd, float alpha, Renderer::RenderStage stage);

		void save(const std::string& filename) const;
		void load(const std::string& filename, JsonToIGameObjectDispatcher dispatcher);

		inline const std::string& getWorldName() const {
			return mWorldName;
		}
		inline void setWorldName(const std::string& worldName) {
			mWorldName = worldName;
		}
	};

}