#pragma once

#include <Data.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace sndbx
{


	class Player : public eg::Data::IGameObject
	{
	protected:

		eg::Data::PointLight mLight;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
		JPH::BodyID mBody;

		float mHeight = 1.8f;
		float mRadius = 0.3f;
		float mMass = 80.0f;
		float mGroundAccel = 20.0f;
		float mAirAccel = 10.0f;
		float mJumpStrength = 10.5f;
		float mGroundMaxSpeed = 6.0f;
		float mAirMaxSpeed = 6.0f;
		float mGroundDamping = 5.0f;
		float mAirDamping = 0.0f;

		float mYaw = 0.0f;
		float mPitchClamp = 89.5f; // Clamp for pitch to prevent flipping
		float mPitch = 0.0f;
		glm::vec3 mDirection = { 0, 0, 0 };
		glm::vec3 mVelocity = { 0, 0, 0 };
		bool mJumpRequested = false;
		bool mGrabOject = false;
		float mCameraDistance = 5.0f;
		float mPlayerSpeed = 1.0f;
		float mMouseSensitivity = 0.2f;
	public:
		Player(bool visible);
		~Player();

		void update(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;

		
	};

	
}