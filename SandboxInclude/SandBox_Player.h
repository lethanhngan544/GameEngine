#pragma once

#include <World.h>
#include <Components.h>
 

namespace sndbx
{


	class Player : public eg::World::IGameObject
	{
	protected:

		eg::Components::PointLight mLight;
		std::shared_ptr<eg::Components::StaticModel> mModel = nullptr;
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
	public:
		Player(bool visible = true);
		~Player();

		void update(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;

		const char* getType() const override { return "Player"; }

		nlohmann::json toJson() const override
		{
			return {
				{ "light", mLight.toJson() },
				{ "model", mModel->toJson() },
				{ "body", mBody.toJson()},
				{ "visible", mVisible },
				{ "height", mHeight },
				{ "radius", mRadius },
				{ "mass", mMass },
				{ "groundAccel", mGroundAccel },
				{ "airAccel", mAirAccel },
				{ "jumpStrength", mJumpStrength },
				{ "groundMaxSpeed", mGroundMaxSpeed },
				{ "airMaxSpeed", mAirMaxSpeed },
				{ "groundDamping", mGroundDamping },
				{ "airDamping", mAirDamping },
				{ "yaw", mYaw },
				{ "pitchClamp", mPitchClamp },
				{ "pitch", mPitch },
				{ "cameraDistance", mCameraDistance },
				{ "playerSpeed", mPlayerSpeed },
				{ "mouseSensitivity", mMouseSensitivity},
			};
		}

		void fromJson(const nlohmann::json& json)
		{
			if (json.contains("light")) mLight.fromJson(json["light"]);
			if (json.contains("model")) mModel = eg::Components::StaticModel::loadFromJson(json["model"]);
			if (json.contains("visible")) mVisible = json["visible"];
			if (json.contains("height")) mHeight = json["height"];
			if (json.contains("radius")) mRadius = json["radius"];
			if (json.contains("mass")) mMass = json["mass"];
			if (json.contains("groundAccel")) mGroundAccel = json["groundAccel"];
			if (json.contains("airAccel")) mAirAccel = json["airAccel"];
			if (json.contains("jumpStrength")) mJumpStrength = json["jumpStrength"];
			if (json.contains("groundMaxSpeed")) mGroundMaxSpeed = json["groundMaxSpeed"];
			if (json.contains("airMaxSpeed")) mAirMaxSpeed = json["airMaxSpeed"];
			if (json.contains("groundDamping")) mGroundDamping = json["groundDamping"];
			if (json.contains("airDamping")) mAirDamping = json["airDamping"];
			if (json.contains("yaw")) mYaw = json["yaw"];
			if (json.contains("pitchClamp")) mPitchClamp = json["pitchClamp"];
			if (json.contains("pitch")) mPitch = json["pitch"];
			if (json.contains("cameraDistance")) mCameraDistance = json["cameraDistance"];
			if (json.contains("playerSpeed")) mPlayerSpeed = json["playerSpeed"];
			if (json.contains("mouseSensitivity")) mMouseSensitivity = json["mouseSensitivity"];
		}
	};

	
}