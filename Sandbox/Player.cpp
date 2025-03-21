#include <SandBox_Player.h>

#include <Input.h>
#include <GLFW/glfw3.h>

namespace sndbx
{
	Player::Player(bool visible)
	{
		mModel = eg::Data::StaticModelCache::load("models/DamagedHelmet.glb");
		
	}

	void Player::update(float delta)
	{
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_W))
		{
			mTransform.mPosition.z -= mPlayerSpeed * delta;
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_S))
		{
			mTransform.mPosition.z += mPlayerSpeed * delta;
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_D))
		{
			mTransform.mPosition.x += mPlayerSpeed * delta;
		}
		if (eg::Input::Keyboard::isKeyDown(GLFW_KEY_A))
		{
			mTransform.mPosition.x -= mPlayerSpeed * delta;
		}

		mYaw += eg::Input::Mouse::getDeltaX();
		mPitch -= eg::Input::Mouse::getDeltaY();

		//Rotate forward vector by pitch and yaw in degrees
		glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mYaw), { 0.0f, 1.0f, 0.0f });
		glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mPitch), { 1.0f, 0.0f, 0.0f });
		mForwardVector = glm::mat3(yawRotation) * glm::mat3(pitchRotation) * mGlobalForward;

		mCamera.mPosition = mTransform.mPosition - mForwardVector * mCameraDistance;
	}

	void Player::render(vk::CommandBuffer cmd)
	{
		eg::Data::StaticModelRenderer::render(cmd, *mModel, mTransform.build());
	}
}