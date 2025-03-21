#include <Data.h>

namespace eg::Data::StaticModelCache
{
	std::unordered_map<std::string, std::shared_ptr<StaticModel>> mCache;

	void clear()
	{
		mCache.clear();
	}

	std::shared_ptr<StaticModel> load(const std::string& filePath)
	{
		auto it = mCache.find(filePath);
		if (it != mCache.end())
		{
			return it->second;
		}
		auto model = std::make_shared<StaticModel>(filePath);
		mCache[filePath] = model;
		return model;
	}
}