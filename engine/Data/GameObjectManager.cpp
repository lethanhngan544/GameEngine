#include <Data.h>

namespace eg::Data
{
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
	void GameObjectManager::render(vk::CommandBuffer cmd)
	{
		for (const auto& gameObject : mGameObjects)
		{
			gameObject->render(cmd);
		}
	}
}