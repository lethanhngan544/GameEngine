#include <World.h>
#include <Components.h>
#include <Logger.h>
#include <Physics.h>

#include <fstream>

namespace eg::World
{

	GameObjectManager::~GameObjectManager()
	{
		cleanup();
	}
	void GameObjectManager::addGameObject(std::unique_ptr<IGameObject> gameobject)
	{
		mGameObjects.push_back(std::move(gameobject));
	}
	void GameObjectManager::removeGameObject(const IGameObject* gameObject)
	{
		//TODO
	}
	void GameObjectManager::update(float delta)
	{
		for (const auto& gameObject : mGameObjects)
		{
			gameObject->update(delta);
		}
	}

	void GameObjectManager::fixedUpdate(float delta)
	{
		for (const auto& gameObject : mGameObjects)
		{
			gameObject->fixedUpdate(delta);
		}
	}
	void GameObjectManager::render(vk::CommandBuffer cmd, Renderer::RenderStage stage)
	{
		for (const auto& gameObject : mGameObjects)
		{
			gameObject->render(cmd, stage);
		}
	}

	void GameObjectManager::save(const std::string& filename) const
	{
		nlohmann::json mainJson;
		mainJson["worldName"] = mWorldName;
		nlohmann::json gameObjectsJson = nlohmann::json::array();
		for (const auto& gameObject : mGameObjects)
		{
			nlohmann::json objJson = gameObject->toJson();
			objJson["type"] = std::string(gameObject->getType()); // Ensure type is included
			gameObjectsJson.push_back(objJson);
		}
		mainJson["gameObjects"] = gameObjectsJson;
		std::ofstream file(filename);
		if (file.is_open())
		{
			file << mainJson.dump(4); // Pretty print with 4 spaces
			file.close();
		}
		else
		{
			eg::Logger::gError("Failed to open file for saving game objects: " + filename);
		}
	}

	void GameObjectManager::cleanup()
	{
		//Clean up 
		Renderer::waitIdle();
		Components::ParticleEmitter::clearAtlasTextures();
		Components::ModelCache::clearCache();
		mGameObjects.clear();
		Physics::reset();
	}

	void GameObjectManager::load(const std::string& filename, JsonToIGameObjectDispatcher dispatcher)
	{
		cleanup();
		//Load json file
		std::ifstream file(filename);
		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file for loading game objects: " + filename);
		}

		nlohmann::json mainJson;
		file >> mainJson;
		file.close();
		if (!mainJson.contains("gameObjects") || !mainJson["gameObjects"].is_array())
		{
			throw std::runtime_error("Invalid game object data in file: " + filename);
		}
		mWorldName = mainJson.at("worldName").get<std::string>();
		for (const auto& objJson : mainJson["gameObjects"])
		{
			if (!objJson.contains("type") || !objJson["type"].is_string())
			{
				eg::Logger::gError("Game object type is missing or invalid in JSON: " + objJson.dump());
				continue;
			}
			std::string type = objJson["type"];
			std::unique_ptr<IGameObject> gameObject = nullptr;
			try
			{
				gameObject = dispatcher(objJson, type);
				gameObject->fromJson(objJson);
			}
			catch (const nlohmann::detail::exception& e)
			{
				std::string errorMsg = "JSON parsing error for game object type '" + type + "': " + e.what();
				eg::Logger::gError(errorMsg);
				continue; // Skip this object if an error occurs
			}
			catch (const std::exception& e)
			{
				eg::Logger::gError(e.what());
				continue; // Skip this object if an error occurs
			}
			addGameObject(std::move(gameObject));
		}
	}
}