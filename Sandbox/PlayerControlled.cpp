#include <SandBox_PlayerControlled.h>


#include <Input.h>
#include <Physics.h>
#include <GLFW/glfw3.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>


namespace sndbx
{
	void PlayerControlled::fixedUpdate(float delta)
	{
		Player::fixedUpdate(delta);
	}
	void PlayerControlled::update(float delta)
	{
		mDirection = { 0.0f, 0.0f, 0.0f };
		mJumpRequested = false;
		mGrabOject = false;
		mYaw -= eg::Input::Mouse::getDeltaX() * mMouseSensitivity;
		mPitch -= eg::Input::Mouse::getDeltaY() * mMouseSensitivity;

		if (mPitch > mPitchClamp)
		{
			mPitch = mPitchClamp;
		}
		else if (mPitch < -mPitchClamp)
		{
			mPitch = -mPitchClamp;
		}

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
		if (eg::Input::Keyboard::isKeyDownOnce(GLFW_KEY_SPACE))
		{
			mJumpRequested = true;
		}

		if (glm::dot(mDirection, mDirection) != 0.0f)
		{
			mDirection = glm::normalize(mDirection);
		}
		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Vec3 position = bodyInterface->GetPosition(mBody);
		glm::vec3 postionGlm = glm::vec3(position.GetX(), position.GetY(), position.GetZ());

		mCamera.mPosition = postionGlm + glm::vec3{ 0.0f, 0.8f, 0.0f };
		mCamera.mPitch = -mPitch;
		mCamera.mYaw = -mYaw;

		Player::update(delta);
	}

	void PlayerControlled::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		Player::render(cmd, stage);
	}
}