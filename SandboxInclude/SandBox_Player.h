#pragma once

#include <World.h>
#include <Components.h>
 

namespace sndbx
{
	class Player : public eg::World::IGameObject
	{
	protected:
		std::shared_ptr<eg::Components::AnimatedModel> mModel = nullptr;
		std::unique_ptr<eg::Components::Animator2DBlend> mAnimator;
		eg::Components::RigidBody mBody;

		bool mVisible;
		float mHeight = 1.8f;
		float mRadius = 0.2f;
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

		glm::mat4x4 mModelOffsetMatrix;
	public:
		Player(bool visible = true);
		~Player();

		void update(float delta, float alpha) override;
		void prePhysicsUpdate(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, float alpha, eg::Renderer::RenderStage stage) override;

		const char* getType() const override { return "Player"; }

		nlohmann::json toJson() const override;

		void fromJson(const nlohmann::json& json);
	};

	
}