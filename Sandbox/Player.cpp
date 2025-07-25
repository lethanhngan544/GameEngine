#include <SandBox_Player.h>

#include <Input.h>
#include <GLFW/glfw3.h>
#include <Physics.h>
#include <Data.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

namespace sndbx
{

	Player::Player(bool visible) :
		mVisible(visible)
	{

	}

	Player::~Player()
	{
		const JPH::BodyLockInterface& lockInterface = eg::Physics::getPhysicsSystem().GetBodyLockInterface();
		JPH::BodyLockRead lockRead(lockInterface, mBody.mBodyID);
		bool valid = lockRead.Succeeded();
		lockRead.ReleaseLock();
		if (!valid) return; // Body already removed or invalid
		eg::Physics::getBodyInterface()->RemoveBody(mBody.mBodyID);
		eg::Physics::getBodyInterface()->DestroyBody(mBody.mBodyID);
	}

	void Player::update(float delta)
	{
		const JPH::BodyLockInterface& lockInterface = eg::Physics::getPhysicsSystem().GetBodyLockInterface();
		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();

		JPH::Vec3 velocity(0, 0, 0);
		JPH::Vec3 position(0, 0, 0);
		glm::vec3 positionGlm;
		JPH::Vec3 wishDir(mDirection.x, mDirection.y, mDirection.z);

		// Reading body state
		{
			JPH::BodyLockRead lockRead(lockInterface, mBody.mBodyID);
			if (!lockRead.Succeeded()) return;
			const JPH::Body& body = lockRead.GetBody();
			position = body.GetPosition();
			positionGlm = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
			velocity = body.GetLinearVelocity();
		}
		// Grounded check (simple downward ray)
		bool grounded = false;
		{
			JPH::RRayCast ray;
			ray.mOrigin = position;
			ray.mDirection = JPH::Vec3(0, -mHeight, 0); // Short ray down

			JPH::RayCastResult result;
			grounded = eg::Physics::getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, result,
				JPH::BroadPhaseLayerFilter{},
				JPH::ObjectLayerFilter{},
				JPH::IgnoreSingleBodyFilter(mBody.mBodyID));
		}

		//Check if velocity is zero
		glm::vec3 forwardVector = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 rightVector = glm::vec3(1.0f, 0.0f, 0.0f);
		//Rotate forward vector by yaw
		glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(mYaw), glm::vec3(0.0f, 1.0f, 0.0f));
		forwardVector = glm::vec3(rotationMatrix * glm::vec4(forwardVector, 1.0f));
		rightVector = glm::vec3(rotationMatrix * glm::vec4(rightVector, 1.0f));

		// Dot between forward vector and velocity
		float forwardDot = glm::dot(forwardVector, glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ()));
		float rightDot = glm::dot(rightVector, glm::vec3(velocity.GetX(), velocity.GetY(), velocity.GetZ()));


		mAnimator->setTimeScale(velocity.LengthSq() / (5.0f * 5.0f));
		mAnimator->setAnimation("models/ybot_idle.glb");

		if (glm::abs(forwardDot) >= glm::abs(rightDot))
		{
			if (forwardDot > 0.01f)
			{
				mAnimator->setAnimation("models/ybot_jog_forward.glb");
			}
			else if (forwardDot < -0.01f)
			{
				mAnimator->setAnimation("models/ybot_jog_backward.glb");
			}
		}
		else
		{
			if (rightDot > 0.01f)
			{
				mAnimator->setAnimation("models/ybot_jog_right.glb");
			}
			else if (rightDot < -0.01f)
			{
				mAnimator->setAnimation("models/ybot_jog_left.glb");
			}
		}

		if (!grounded)
		{
			mAnimator->setAnimation("models/ybot_falling.glb");
		}
		

		

		// Separate horizontal and vertical velocity
		JPH::Vec3 flatVelocity = velocity;
		flatVelocity.SetY(0.0f);

		float flatSpeed = flatVelocity.Length();
		float wishSpeed = grounded ? mGroundMaxSpeed : mAirMaxSpeed;

		// Normalize wishdir safely
		if (wishDir.LengthSq() > 0.0f)
			wishDir = wishDir.Normalized();

		// Calculate acceleration
		float addSpeed = wishSpeed - flatVelocity.Dot(wishDir);
		if (addSpeed > 0.0f)
		{
			float accel = (grounded ? mGroundAccel : mAirAccel) * delta * wishSpeed;
			if (accel > addSpeed)
				accel = addSpeed;
			flatVelocity += wishDir * accel;
		}

		// Jumping
		if (mJumpRequested && grounded)
		{
			velocity.SetY(mJumpStrength); // Launch upward
		}
		else
		{
			// Gravity (if not jumping newly)
			velocity.SetY(velocity.GetY() - 9.8f * delta);
		}

		// Combine updated horizontal and vertical movement
		velocity.SetX(flatVelocity.GetX());
		velocity.SetZ(flatVelocity.GetZ());

		// Writing body state
		{
			JPH::BodyLockWrite lockWrite(lockInterface, mBody.mBodyID);
			if (!lockWrite.Succeeded()) return;
			JPH::Body& body = lockWrite.GetBody();
			body.SetLinearVelocity(velocity);
			body.GetMotionProperties()->SetLinearDamping(grounded ? mGroundDamping : mAirDamping);
		}

		// Set character facing direction
		bodyInterface->SetRotation(mBody.mBodyID, JPH::Quat::sRotation(JPH::Vec3(0, 1, 0), glm::radians(mYaw)), JPH::EActivation::Activate);

		mLight.mUniformBuffer.position = glm::vec4(positionGlm + glm::vec3(0.0f, 0.5f, 0.0f), 0.0f);
		mAnimator->update(delta);
	}

	void Player::fixedUpdate(float delta)
	{

	}

	void Player::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		if (!mModel)
			return;

		glm::mat4x4 glmMatrix = mBody.getBodyMatrix();
		glmMatrix *= mModelOffsetMatrix;

		switch (stage)
		{

		case eg::Renderer::RenderStage::SHADOW:
		{
			eg::Data::AnimatedModelRenderer::renderShadow(cmd, *mModel, *mAnimator, glmMatrix);
			break;
		}
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
		{
			if (mVisible)
			{
				eg::Data::AnimatedModelRenderer::render(cmd, *mModel, *mAnimator, glmMatrix);
			}
			break;
		}
		default:
			break;
		}
	}

	nlohmann::json Player::toJson() const
	{
		return {
			{ "light", mLight.toJson() },
			{ "model", mModel->toJson() },
			{ "body", mBody.toJson()},
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

	void Player::fromJson(const nlohmann::json& json)
	{
		mLight.fromJson(json["light"]);
		mModel = eg::Components::ModelCache::loadAnimatedModelFromJson(json["model"]);

		mHeight = json["height"];
		mRadius = json["radius"];
		mMass = json["mass"];
		mGroundAccel = json["groundAccel"];
		mAirAccel = json["airAccel"];
		mJumpStrength = json["jumpStrength"];
		mGroundMaxSpeed = json["groundMaxSpeed"];
		mAirMaxSpeed = json["airMaxSpeed"];
		mGroundDamping = json["groundDamping"];
		mAirDamping = json["airDamping"];
		mYaw = json["yaw"];
		mPitchClamp = json["pitchClamp"];
		mPitch = json["pitch"];
		mCameraDistance = json["cameraDistance"];
		mPlayerSpeed = json["playerSpeed"];
		mMouseSensitivity = json["mouseSensitivity"];


		std::vector<std::shared_ptr<eg::Components::Animation>> animations;
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_idle.glb"));
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_jog_forward.glb"));
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_jog_backward.glb"));
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_jog_left.glb"));
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_jog_right.glb"));
		animations.push_back(eg::Components::ModelCache::loadAnimation("models/ybot_falling.glb"));
		mAnimator = std::make_unique<eg::Components::Animator>(animations, *mModel);

		glm::vec3 position = { json["body"]["position"].at(0).get<float>(),
			json["body"]["position"].at(1).get<float>(),
			json["body"]["position"].at(2).get<float>() };
		glm::quat rotation;
		rotation.x = json["body"]["rotation"].at(0).get<float>();
		rotation.y = json["body"]["rotation"].at(1).get<float>();
		rotation.z = json["body"]["rotation"].at(2).get<float>();
		rotation.w = json["body"]["rotation"].at(3).get<float>();

		//Create rigid body
		{

			JPH::CapsuleShapeSettings shapeSettings(mHeight * 0.5f, mRadius);
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(position.x, position.y, position.z),
				JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
				JPH::EMotionType::Dynamic,
				eg::Physics::Layers::MOVING);
			bodySetting.mFriction = 0.0f;
			bodySetting.mMassPropertiesOverride.mMass = mMass;
			bodySetting.mLinearDamping = 0.0f;
			bodySetting.mAngularDamping = 0.0f;
			bodySetting.mAllowSleeping = false;
			bodySetting.mMotionQuality = JPH::EMotionQuality::LinearCast;
			bodySetting.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX |
				JPH::EAllowedDOFs::TranslationY |
				JPH::EAllowedDOFs::TranslationZ;

			mBody.mBodyID = eg::Physics::getBodyInterface()->CreateAndAddBody(bodySetting, JPH::EActivation::Activate);
		}


		mModelOffsetMatrix = glm::rotate(glm::mat4x4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		mModelOffsetMatrix = glm::translate(mModelOffsetMatrix, glm::vec3(0.0f, -mHeight * 0.5f - 0.1f, 0.0f));
	}
}