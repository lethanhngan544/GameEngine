#include <World.h>
#include <Physics.h>

namespace eg::World
{
	static std::string sWorldName = "Default";
	static std::vector<std::unique_ptr<IGameObject>> sGameObjects;
	static std::vector<JsonToIGameObjectDispatcher> sJsonToIGameObjectDispatchers;

	void create()
	{
		
	}
	void destroy()
	{
		sGameObjects.clear();
		cleanup();
	}

	void cleanup()
	{
		Renderer::waitIdle();
		Components::ParticleEmitter::clearAtlasTextures();
		Components::ModelCache::clearCache();
		sGameObjects.clear();
		Physics::reset();
	}

	void addGameObject(std::unique_ptr<IGameObject> gameobject)
	{
		sGameObjects.push_back(std::move(gameobject));
	}

	void removeGameObject(const IGameObject* gameObject)
	{
		//TODO: Implement
	}

	std::vector<std::unique_ptr<IGameObject>>& getGameObjects()
	{
		return sGameObjects;
	}

	void update(float delta, float alpha)
	{
		for (auto& obj : sGameObjects)
			obj->update(delta, alpha);
	}
	void prePhysicsUpdate(float delta)
	{
		for (auto& obj : sGameObjects)
			obj->prePhysicsUpdate(delta);
	}
	void fixedUpdate(float delta)
	{
		for (auto& obj : sGameObjects)
			obj->fixedUpdate(delta);
	}
	void render(vk::CommandBuffer cmd, float alpha, Renderer::RenderStage stage)
	{
		for (auto& obj : sGameObjects)
			obj->render(cmd, alpha, stage);
	}

	void save(const std::string& filename)
	{
		nlohmann::json mainJson;
		mainJson["worldName"] = sWorldName;
		nlohmann::json gameObjectsJson = nlohmann::json::array();
		for (const auto& gameObject : sGameObjects)
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
	void load(const std::string& filename, JsonToIGameObjectDispatcher dispatcher)
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
		sWorldName = mainJson.at("worldName").get<std::string>();
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