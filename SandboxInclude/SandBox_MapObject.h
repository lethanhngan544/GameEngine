#pragma once

#include <Data.h>


namespace sndbx
{
	class MapObject : public eg::Data::IGameObject
	{
	private:
		eg::Data::Transform mTransform;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
	public:
		MapObject(const std::string& modelPath);

		void update(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;

		const eg::Data::Transform& getTransform() const { return mTransform; }
		eg::Data::Transform& getTransform() { return mTransform; }
	};
}