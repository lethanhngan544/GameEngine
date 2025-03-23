#include <SandBox_Player.h>

#include <Input.h>
#include <GLFW/glfw3.h>
#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

namespace sndbx
{
	Player::Player(bool visible)
	{
		mModel = eg::Data::StaticModelCache::load("models/DamagedHelmet.glb");
		//Create rigid body
		{
			JPH::SphereShapeSettings shapeSettings(0.5f);
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(0.0f, 10.0f, 0.0f),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Dynamic,
				eg::Physics::Layers::MOVING);
			bodySetting.mRestitution = 0.0f;
			bodySetting.mFriction = 0.3f;
			
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

		mYaw -= eg::Input::Mouse::getDeltaX() * mMouseSensitivity;
		mPitch -= eg::Input::Mouse::getDeltaY() * mMouseSensitivity;
		mCamera.mPitch = -mPitch;
		mCamera.mYaw = -mYaw;

		glm::vec3 mDirection = { 0.0f,0.0f,0.0f };
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_W))
		{
			mDirection.x += glm::cos(glm::radians(mYaw + 90.0f));
			mDirection.z -= glm::sin(glm::radians(mYaw + 90.0f));
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_S))
		{
			mDirection.x -= glm::cos(glm::radians(mYaw + 90.0f));
			mDirection.z += glm::sin(glm::radians(mYaw + 90.0f));
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_D))
		{
			mDirection.x += glm::cos(glm::radians(mYaw));
			mDirection.z -= glm::sin(glm::radians(mYaw));
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_A))
		{
			mDirection.x -= glm::cos(glm::radians(mYaw));
			mDirection.z += glm::sin(glm::radians(mYaw));
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_SPACE))
		{
			mDirection.y += 1.0f;
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_LEFT_SHIFT))
		{
			mDirection.y -= 1.0f;
		}


		if (glm::dot(mDirection, mDirection) != 0.0f)
		{
			mDirection = glm::normalize(mDirection);
		}

		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		float YVel = bodyInterface->GetLinearVelocity(mBody).GetY();
		bodyInterface->SetLinearVelocity(mBody, JPH::RVec3(mDirection.x, YVel, mDirection.z) * mPlayerSpeed);

		JPH::RVec3 position = bodyInterface->GetCenterOfMassPosition(mBody);
		glm::vec3 postionGlm = glm::vec3(position.GetX(), position.GetY(), position.GetZ());

		//Rotate forward vector by pitch and yaw in degrees
		glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mYaw), { 0.0f, 1.0f, 0.0f });
		glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mPitch), { 1.0f, 0.0f, 0.0f });
		mForwardVector = glm::mat3(yawRotation) * glm::mat3(pitchRotation) * mGlobalForward;

		mCamera.mPosition = postionGlm - mForwardVector * mCameraDistance;
		mLight.mUniformBuffer.position = glm::vec4(postionGlm + glm::vec3(0.0f, 0.5f, 0.0f), 0.0f);


	}

	void Player::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
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
		case eg::Renderer::RenderStage::SUBPASS1_POINTLIGHT:
			eg::Data::LightRenderer::renderPointLight(cmd, mLight);
			break;
		default:
			break;
		}
	}
}