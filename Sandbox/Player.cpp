#include <SandBox_Player.h>

#include <Input.h>
#include <GLFW/glfw3.h>
#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

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
			bodySetting.mFriction = 0.8f;
			bodySetting.mMassPropertiesOverride.mMass = mMass;
			bodySetting.mLinearDamping = 0.9f;
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
		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Vec3 position = bodyInterface->GetPosition(mBody);
		glm::vec3 postionGlm = glm::vec3(position.GetX(), position.GetY(), position.GetZ());
		JPH::Vec3 velocity = bodyInterface->GetLinearVelocity(mBody);
		JPH::Vec3 wishDir = JPH::Vec3(mDirection.x, mDirection.y, mDirection.z);
		
		// Simple grounded check (raycast down)
		JPH::RRayCast ray;
		ray.mOrigin = position;
		ray.mDirection = JPH::Vec3(0, -1, 0);

		JPH::RayCastResult result;
		bool grounded = eg::Physics::getPhysicsSystem().GetNarrowPhaseQuery().CastRay(ray, result,
			JPH::BroadPhaseLayerFilter{},
			JPH::ObjectLayerFilter{},
			JPH::IgnoreSingleBodyFilter(mBody));


		float currentSpeed = velocity.Dot(wishDir);
		if (grounded) {
			float addSpeed = mGroundMaxSpeed - currentSpeed;
			if (addSpeed <= 0.0f) addSpeed = 0.0f;
			float accelSpeed = mGroundAccel * delta * mGroundMaxSpeed;
			if (accelSpeed > addSpeed)
				accelSpeed = addSpeed;

			velocity += wishDir * accelSpeed;

			if (mJumpRequested) {
				velocity.SetY(mJumpStrength);
			}
		}
		else {
			float addSpeed = mAirMaxSpeed - currentSpeed;
			if (addSpeed <= 0.0f) addSpeed = 0.0f;
			float accelSpeed = mAirAccel * delta * mAirMaxSpeed;
			if (accelSpeed > addSpeed)
				accelSpeed = addSpeed;

			velocity += wishDir * accelSpeed;
			velocity.SetY(velocity.GetY() - 9.8f * delta); // Gravity
		}

		velocity.SetY(velocity.GetY() - 9.8f * delta);

		bodyInterface->SetLinearVelocity(mBody, velocity);
		bodyInterface->SetRotation(mBody, JPH::Quat::sIdentity(), JPH::EActivation::Activate);
		//Rotate body based on yaw
		bodyInterface->SetRotation(mBody, JPH::Quat::sRotation(JPH::Vec3(0, 1, 0), glm::radians(mYaw)), JPH::EActivation::Activate);
		

		mLight.mUniformBuffer.position = glm::vec4(postionGlm + glm::vec3(0.0f, 0.5f, 0.0f), 0.0f);
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