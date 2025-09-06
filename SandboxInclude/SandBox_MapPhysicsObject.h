#pragma once

#include <World.h>
#include <Components.h>

namespace sndbx
{
	class MapPhysicsObject : public eg::World::IGameObject
	{
	private:
		eg::Components::RigidBody mBody;
		std::unique_ptr<eg::Components::CameraFrustumCuller> mCuller;
		std::shared_ptr<eg::Components::StaticModel> mModel = nullptr;
	public:
		MapPhysicsObject() = default;
		~MapPhysicsObject();

		void update(float delta, float alpha) override;
		void prePhysicsUpdate(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, float alpha, eg::Renderer::RenderStage stage) override;

		const char* getType() const override { return "MapPhysicsObject"; }

		nlohmann::json toJson() const override
		{
			return {
				{"type", getType()},
				{"model", mModel->toJson()},
				{"rigidBody", mBody.toJson()},
				{"particleEmitter",{}}
			};
		}
		void fromJson(const nlohmann::json& json) override;
	};
}