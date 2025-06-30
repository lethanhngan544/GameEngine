#include <Components.h>

namespace eg::Components::ModelCache
{
	static std::unordered_map<std::string, std::shared_ptr<StaticModel>> sCache;
	static std::unordered_map<std::string, std::shared_ptr<AnimatedModel>> sAnimatedCache;

	std::shared_ptr<StaticModel> loadStaticModel(const std::string& filePath)
	{
		auto it = sCache.find(filePath);
		if (it != sCache.end())
		{
			return it->second;
		}
		auto model = std::make_shared<StaticModel>(filePath);
		sCache[filePath] = model;
		return model;
	}
	std::shared_ptr<StaticModel> loadStaticModelFromJson(const nlohmann::json& json)
	{
		if (json.contains("model_path")) {
			auto modelPath = json["model_path"].get<std::string>();
			std::shared_ptr<StaticModel> model = loadStaticModel(modelPath);
			model->setFilePath(modelPath); // Ensure the file path is set correctly
			return model;
		}
		return nullptr;
	}
	std::shared_ptr<AnimatedModel> loadAnimatedModel(const std::string& filePath)
	{
		auto it = sAnimatedCache.find(filePath);
		if (it != sAnimatedCache.end())
		{
			return it->second;
		}
		auto model = std::make_shared<AnimatedModel>(filePath);
		sAnimatedCache[filePath] = model;
		return model;
	}
	std::shared_ptr<AnimatedModel> loadAnimatedModelFromJson(const nlohmann::json& json)
	{
		if (json.contains("model_path")) {
			auto modelPath = json["model_path"].get<std::string>();
			std::shared_ptr<AnimatedModel> model = loadAnimatedModel(modelPath);
			model->setFilePath(modelPath); // Ensure the file path is set correctly
			return model;
		}
		return nullptr;
	}
	void clearCache()
	{
		sCache.clear();
		sAnimatedCache.clear();
		Logger::gInfo("Model cache cleared.");
	}
}