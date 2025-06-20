#include <Components.h>

#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

namespace eg::Components
{

	glm::mat4x4 RigidBody::getBodyMatrix() const
	{
		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBodyID);
		glm::mat4x4 glmMatrix;
		std::memcpy(&glmMatrix[0][0], &matrix, sizeof(glmMatrix));

		return glmMatrix;
	}
	nlohmann::json RigidBody::toJson() const
	{
		nlohmann::json json;
		
		/*const auto &lockInterface = eg::Physics::getPhysicsSystem().GetBodyLockInterface();
		JPH::BodyLockRead lock(lockInterface, body);
		if (!lock.Succeeded())
		{
			return json;
		}
		const JPH::Body& bodyClass = lock.GetBody();*/


		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBodyID);
		glm::mat4x4 glmMatrix;
		std::memcpy(&glmMatrix[0][0], &matrix, sizeof(glmMatrix));

		//Extract position and rotation from the matrix
		glm::vec3 position = { matrix.GetTranslation().GetX(), matrix.GetTranslation().GetY(), matrix.GetTranslation().GetZ() };
		glm::quat rotation = glm::quat_cast(glm::mat3(glmMatrix));

		//Fill the JSON object with the rigid body data
		json["position"] = { position.x, position.y, position.z };
		json["rotation"] = { rotation.x, rotation.y, rotation.z, rotation.w };

		return json;
	}

}