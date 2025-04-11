#include <SandBox_MapPhysicsObject.h>


#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

namespace sndbx
{
	MapPhysicsObject::MapPhysicsObject(const std::string& modelPath)
	{
		mModel = eg::Data::StaticModelCache::load(modelPath);

		//Create rigid body
		{

			JPH::BoxShapeSettings shapeSettings(JPH::Vec3(1.0f, 1.0f, 1.0f));
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(0.0f, 10.0f, 0.0f),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Dynamic,
				eg::Physics::Layers::MOVING);
			bodySetting.mFriction = 0.2f;
			bodySetting.mMassPropertiesOverride.mMass = 10.0f;
			bodySetting.mLinearDamping = 0.9f;
			bodySetting.mAngularDamping = 0.0f;
			bodySetting.mAllowSleeping = false;
			bodySetting.mMotionQuality = JPH::EMotionQuality::Discrete;


			mBody = eg::Physics::getBodyInterface()->CreateAndAddBody(bodySetting, JPH::EActivation::Activate);
		}
	}

	void MapPhysicsObject::update(float delta)
	{

	}

	void MapPhysicsObject::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		switch (stage)
		{
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
		{
			JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
			JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBody);
			glm::mat4x4 glmMatrix;
			std::memcpy(&glmMatrix[0][0], &matrix, sizeof(glmMatrix));

			eg::Data::StaticModelRenderer::render(cmd, *mModel, glmMatrix);
			break;
		}
		default:
			break;
		}
	}
}