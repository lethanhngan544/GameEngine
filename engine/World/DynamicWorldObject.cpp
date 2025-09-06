#include <World.h>

#include <Physics.h>
#include <Data.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>

namespace eg::World
{
	DynamicWorldObject::~DynamicWorldObject()
	{
		const JPH::BodyLockInterface& lockInterface = Physics::getPhysicsSystem().GetBodyLockInterface();
		JPH::BodyLockRead lockRead(lockInterface, mBody.mBodyID);
		bool valid = lockRead.Succeeded();
		lockRead.ReleaseLock();
		if (!valid) return; // Body already removed or invalid
		eg::Physics::getBodyInterface()->RemoveBody(mBody.mBodyID);
		eg::Physics::getBodyInterface()->DestroyBody(mBody.mBodyID);
	}
	void DynamicWorldObject::fromJson(const nlohmann::json& json)
	{
		std::string modelPath = json["model"]["path"].get<std::string>();
		glm::vec3 position = { json["body"]["position"].at(0).get<float>(),
			json["body"]["position"].at(1).get<float>(),
			json["body"]["position"].at(2).get<float>() };

		mModel = eg::Components::ModelCache::loadStaticModel(modelPath);

		//Create rigid body
		{

			JPH::BoxShapeSettings shapeSettings(JPH::Vec3(0.5f, 0.5f, 0.5f));
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(position.x, position.y, position.z),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Dynamic,
				eg::Physics::Layers::MOVING);
			bodySetting.mFriction = 0.2f;
			bodySetting.mMassPropertiesOverride.mMass = 10.0f;
			bodySetting.mLinearDamping = 1.001f;
			bodySetting.mAngularDamping = 0.0f;
			bodySetting.mAllowSleeping = false;
			bodySetting.mMotionQuality = JPH::EMotionQuality::Discrete;
			bodySetting.mRestitution = 0.9f;

			mBody.mBodyID = eg::Physics::getBodyInterface()->CreateAndAddBody(bodySetting, JPH::EActivation::Activate);
			mBody.mMass = 10.0f;
			mBody.mFriction = 0.2f;
			mBody.mRestitution = 0.0f;

		}
	}
	void DynamicWorldObject::update(float delta, float alpha)
	{

	}
	void DynamicWorldObject::fixedUpdate(float delta)
	{

	}
	void DynamicWorldObject::render(vk::CommandBuffer cmd, float alpha, Renderer::RenderStage stage)
	{
		glm::mat4x4 mat = mBody.getBodyMatrix(alpha);

		switch (stage)
		{
		case Renderer::RenderStage::SHADOW:
		{
			mModel->renderShadow(cmd, mat);
			break;
		}
		case Renderer::RenderStage::SUBPASS0_GBUFFER:
		{
			mModel->render(cmd,  mat);
			break;
		}


		default:
			break;
		}
	}
}