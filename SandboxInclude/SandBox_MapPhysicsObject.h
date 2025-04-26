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
	public:
		MapPhysicsObject(const std::string& modelPath, const glm::vec3& position);

		void update(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;
	};
}