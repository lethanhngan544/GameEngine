#include <SandBox_Player.h>

#include <Input.h>
#include <GLFW/glfw3.h>
#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

namespace sndbx
{
	
	Player::Player(bool visible) 
	{
		if(visible)
			mModel = eg::Data::StaticModelCache::load("models/DamagedHelmet.glb");
		//Create rigid body
		{

			JPH::CapsuleShapeSettings shapeSettings(mHeight * 0.5f - mRadius, mRadius);
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(0.0f, 10.0f, 0.0f),
				JPH::Quat::sIdentity(),
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
			
			mBody = eg::Physics::getBodyInterface()->CreateAndAddBody(bodySetting, JPH::EActivation::Activate);
			eg::Physics::getBodyInterface()->SetLinearVelocity(mBody, { 0.0f, 10.0f, 0.0f });
		}
	}

	Player::~Player()
	{
		eg::Physics::getBodyInterface()->RemoveBody(mBody);
		eg::Physics::getBodyInterface()->DestroyBody(mBody);
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
			JPH::BodyLockRead lockRead(lockInterface, mBody);
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
			ray.mDirection = JPH::Vec3(0, -1.5f, 0); // Short ray down

			JPH::RayCastResult result;
			grounded = eg::Physics::getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, result,
				JPH::BroadPhaseLayerFilter{},
				JPH::ObjectLayerFilter{},
				JPH::IgnoreSingleBodyFilter(mBody));
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
			JPH::BodyLockWrite lockWrite(lockInterface, mBody);
			if (!lockWrite.Succeeded()) return;
			JPH::Body& body = lockWrite.GetBody();
			body.SetLinearVelocity(velocity);
			body.GetMotionProperties()->SetLinearDamping(grounded ? mGroundDamping : mAirDamping);
		}

		// Set character facing direction
		bodyInterface->SetRotation(mBody, JPH::Quat::sRotation(JPH::Vec3(0, 1, 0), glm::radians(mYaw)), JPH::EActivation::Activate);

		mLight.mUniformBuffer.position = glm::vec4(positionGlm + glm::vec3(0.0f, 0.5f, 0.0f), 0.0f);
	}

	void Player::fixedUpdate(float delta)
	{
	
	}

	void Player::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		switch (stage)
		{
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
		{
			if (mModel)
			{
				JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
				JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBody);
				glm::mat4x4 glmMatrix;
				std::memcpy(&glmMatrix[0][0], &matrix, sizeof(glmMatrix));

				eg::Data::StaticModelRenderer::render(cmd, *mModel, glmMatrix);
			}
			break;
		}
		case eg::Renderer::RenderStage::SUBPASS1_POINTLIGHT:
			eg::Data::LightRenderer::renderPointLight(cmd, mLight);
			break;
		default:
			break;
		}
	}
}