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

		glm::vec2 desiredDirection = { 0.0f, 0.0f };

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
			desiredDirection.y += 1.0f; // Forward
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_S))
		{
			mDirection.x -= glm::cos(glm::radians(mYaw + 90.0f));
			mDirection.z += glm::sin(glm::radians(mYaw + 90.0f));
			desiredDirection.y -= 1.0f; // Backward
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_D))
		{
			mDirection.x += glm::cos(glm::radians(mYaw));
			mDirection.z -= glm::sin(glm::radians(mYaw));
			desiredDirection.x += 1.0f; // Right
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_A))
		{
			mDirection.x -= glm::cos(glm::radians(mYaw));
			mDirection.z += glm::sin(glm::radians(mYaw));
			desiredDirection.x -= 1.0f; // Left
		}
		if (eg::Input::Keyboard::isKeyDownOnce(GLFW_KEY_SPACE))
		{
			mJumpRequested = true;
		}

		if (glm::dot(mDirection, mDirection) != 0.0f)
		{
			mDirection = glm::normalize(mDirection);
		}
		glm::mat4x4 bodyMatrix = mBody.getBodyMatrix();
		auto headLocalTransform = mHeadNode->localTransform;
		auto headGlobalTransform = bodyMatrix * mModelOffsetMatrix * headLocalTransform;
		auto headGlobalPosition = glm::vec3{ headGlobalTransform[3][0], headGlobalTransform[3][1], headGlobalTransform[3][2] };

		glm::vec3 positionGlm = { bodyMatrix[3][0], bodyMatrix[3][1], bodyMatrix[3][2] };

		//mCamera.mPosition = positionGlm + glm::vec3{ 0.0f, 0.8f, 0.0f };
		mCamera.mPosition = headGlobalPosition;
		mCamera.mPitch = -mPitch;
		mCamera.mYaw = -mYaw;

		//Normalize desiredDirection
		float length2 = desiredDirection.x * desiredDirection.x + desiredDirection.y * desiredDirection.y;
		if (length2 != 0.0f)
		{
			desiredDirection = glm::normalize(desiredDirection); // Ensure desiredDirection is normalized
		}

		mAnimState.x = glm::mix(mAnimState.x, desiredDirection.x, delta * 10.0f);
		mAnimState.y = glm::mix(mAnimState.y, desiredDirection.y, delta * 10.0f);

		mAnimator->setState(mAnimState);

		Player::update(delta);

		
	}

	void PlayerControlled::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		Player::render(cmd, stage);
	}
}