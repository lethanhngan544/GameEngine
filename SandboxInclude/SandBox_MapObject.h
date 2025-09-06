#pragma once

#include <World.h>
#include <Components.h>

namespace sndbx
{
	class MapObject : public eg::World::IGameObject
	{
	private:
		std::shared_ptr<eg::Components::StaticModel> mModel = nullptr;
		eg::Components::RigidBody mBody;
	public:
		MapObject() = default;

		void update(float delta, float alpha) override {}
		void prePhysicsUpdate(float delta) override {}
		void fixedUpdate(float delta) override {}
		void render(vk::CommandBuffer cmd, float alpha, eg::Renderer::RenderStage stage) override;

		const char* getType() const override { return "MapObject"; }

		nlohmann::json toJson() const override
		{
			return {
				{"type", getType()},
				{"model", mModel->toJson()},
				{"body", mBody.toJson()}
			};
		}

		void fromJson(const nlohmann::json& json) override;
	};
}