#include <SandBox_MapPhysicsObject.h>
#include <RenderStages.h>
#include <Data.h>

#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

namespace sndbx
{

	MapPhysicsObject::~MapPhysicsObject()
	{
		const JPH::BodyLockInterface& lockInterface = eg::Physics::getPhysicsSystem().GetBodyLockInterface();
		JPH::BodyLockRead lockRead(lockInterface, mBody.mBodyID);
		bool valid = lockRead.Succeeded();
		lockRead.ReleaseLock();
		if (!valid) return; // Body already removed or invalid
		eg::Physics::getBodyInterface()->RemoveBody(mBody.mBodyID);
		eg::Physics::getBodyInterface()->DestroyBody(mBody.mBodyID);
		
	}
	void MapPhysicsObject::fromJson(const nlohmann::json& json)
	{
		std::string modelPath = json["model"]["model_path"].get<std::string>();
		glm::vec3 position = { json["rigidBody"]["position"].at(0).get<float>(),
			json["rigidBody"]["position"].at(1).get<float>(),
			json["rigidBody"]["position"].at(2).get<float>() };
		glm::quat rotation;
		rotation.x = json["rigidBody"]["rotation"].at(0).get<float>();
		rotation.y = json["rigidBody"]["rotation"].at(1).get<float>();
		rotation.z = json["rigidBody"]["rotation"].at(2).get<float>();
		rotation.w = json["rigidBody"]["rotation"].at(3).get<float>();

		mCuller = std::make_unique<eg::Components::CameraFrustumCuller>(eg::Renderer::getMainCamera());

		mModel = eg::Components::ModelCache::loadStaticModel(modelPath);

		//Create rigid body
		{

			JPH::BoxShapeSettings shapeSettings(JPH::Vec3(1.0f, 1.0f, 1.0f));
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(position.x, position.y, position.z),
				JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w),
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
			mBody.mRestitution = 0.9f;

		}
	}

	void MapPhysicsObject::update(float delta)
	{
		
		
	}
	void MapPhysicsObject::fixedUpdate(float delta)
	{

	}

	void MapPhysicsObject::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		glm::mat4x4 mat = mBody.getBodyMatrix();
		mCuller->updateFrustumPlanes(eg::Renderer::getDrawExtent().extent);

		switch (stage)
		{
		case eg::Renderer::RenderStage::SHADOW:
		{
			eg::Data::StaticModelRenderer::renderShadow(cmd, *mModel, mat);
			break;
		}
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
		{
			if (mCuller->isSphereInFrustum(mat[3], 2.0f))
			{
				eg::Data::StaticModelRenderer::render(cmd, *mModel, mat);
			}
			
			break;
		}


		default:
			break;
		}
	}
}