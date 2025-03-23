#pragma once

#include <Data.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace sndbx
{
	class Player : public eg::Data::IGameObject
	{
	private:
		eg::Data::Camera mCamera;
		eg::Data::PointLight mLight;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
		JPH::BodyID mBody;

		float mYaw = 0.0f;
		float mPitch = 0.0f;
		const glm::vec3 mGlobalForward = { 0.0f, 0.0f, -1.0f };
		glm::vec3 mForwardVector = mGlobalForward;
		float mCameraDistance = 5.0f;
		float mPlayerSpeed = 1.0f;
		float mMouseSensitivity = 0.2f;
	public:
		Player(bool visible);
		~Player();

		void update(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;

		const eg::Data::Camera& getCamera() const { return mCamera; }
	};
}