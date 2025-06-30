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

		//Process animation based on velocity
		{
			mAnimator->setAnimation(velocity.LengthSq() > 2.0f ? "Fucking" : "");
		}

		// Grounded check (simple downward ray)
		bool grounded = false;
		{
			JPH::RRayCast ray;
			ray.mOrigin = position;
			ray.mDirection = JPH::Vec3(0, -mHeight, 0)  ; // Short ray down

			JPH::RayCastResult result;
			grounded = eg::Physics::getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, result,
				JPH::BroadPhaseLayerFilter{},
				JPH::ObjectLayerFilter{},
				JPH::IgnoreSingleBodyFilter(mBody.mBodyID));
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
		glm::mat4x4 glmMatrix = mBody.getBodyMatrix();
		glmMatrix = glm::translate(glmMatrix, glm::vec3(0.0f, -mHeight * 0.5f -0.1f, 0.0f));

		if (!mModel)
			return;

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
		case eg::Renderer::RenderStage::SUBPASS1_POINTLIGHT:
			//mLight.update();
			//eg::Data::LightRenderer::renderPointLight(cmd, mLight);
			break;
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

	void Player::fromJson(const nlohmann::json& json)
	{
		if (json.contains("light")) mLight.fromJson(json["light"]);
		if (json.contains("model"))
		{
			mModel = eg::Components::ModelCache::loadAnimatedModelFromJson(json["model"]);
			mAnimator = std::make_unique<eg::Components::Animator>(*mModel);
		}
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

	}
}