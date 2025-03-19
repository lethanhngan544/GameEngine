#pragma once

#include <Data.h>

namespace sndbx
{
	class Player : public eg::Data::IGameObject
	{
	private:
		eg::Data::Transform mTransform;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
	public:
		Player(bool visible);

		void update(float delta) override {}
		void render(vk::CommandBuffer cmd) override;

		const eg::Data::Transform& getTransform() const { return mTransform;  }
		eg::Data::Transform& getTransform() { return mTransform; }
	};
}