#pragma once

#include <Data.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>


namespace sndbx
{
	class MapObject : public eg::Data::IGameObject
	{
	private:
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
		JPH::BodyID mBody;

	public:
		MapObject(const std::string& modelPath);

		void update(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;
	};
}