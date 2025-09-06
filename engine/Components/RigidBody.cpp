#include <Components.h>

#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>


namespace eg::Components
{

	void RigidBody::updatePrevState()
	{
		mPreviousMatrix = mCurrentMatrix;
	}
	glm::mat4x4 RigidBody::getBodyMatrix(float alpha) const
	{
		// Fast decompose lambda function
		auto decomposeTRS = [](const glm::mat4& m) -> std::tuple<glm::vec3, glm::quat, glm::vec3>
			{
				// Extract translation
				glm::vec3 translation = glm::vec3(m[3]);

				// Extract scale (length of basis vectors)
				glm::vec3 col0 = glm::vec3(m[0]);
				glm::vec3 col1 = glm::vec3(m[1]);
				glm::vec3 col2 = glm::vec3(m[2]);

				glm::vec3 scale;
				scale.x = glm::length(col0);
				scale.y = glm::length(col1);
				scale.z = glm::length(col2);

				// Normalize the rotation columns
				if (scale.x != 0.0f) col0 /= scale.x;
				if (scale.y != 0.0f) col1 /= scale.y;
				if (scale.z != 0.0f) col2 /= scale.z;

				// Build rotation matrix
				glm::mat3 rotMat(col0, col1, col2);

				// Convert to quaternion
				glm::quat rotation = glm::quat_cast(rotMat);

				return std::make_tuple(translation, rotation, scale);
			};

		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBodyID);
		std::memcpy(&mCurrentMatrix[0][0], &matrix, sizeof(mCurrentMatrix));
	

		// Decompose both matrices
		glm::vec3 scale1, translation1;
		glm::quat rotation1;
		std::tie(translation1, rotation1, scale1) = decomposeTRS(mPreviousMatrix);

		glm::vec3 scale2, translation2;
		glm::quat rotation2;
		std::tie(translation2, rotation2, scale2) = decomposeTRS(mCurrentMatrix);

		// Interpolate components
		glm::vec3 scale = glm::mix(scale1, scale2, alpha);
		glm::vec3 translation = glm::mix(translation1, translation2, alpha);
		glm::quat rotation = glm::slerp(rotation1, rotation2, alpha);

		// Recompose
		glm::mat4 result = glm::translate(glm::mat4(1.0f), translation)
			* glm::mat4_cast(rotation)
			* glm::scale(glm::mat4(1.0f), scale);

		return result;
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