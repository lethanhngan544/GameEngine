#pragma once

#include <Data.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace sndbx
{
	class MapPhysicsObject : public eg::Data::IGameObject
	{
	private:
		JPH::BodyID mBody;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
		eg::Data::ParticleEmitter mParticleEmitter;
	public:
		MapPhysicsObject(const std::string& modelPath, const glm::vec3& position);

		void update(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;

		inline eg::Data::ParticleEmitter& getParticleEmitter() { return mParticleEmitter; }
	};
}