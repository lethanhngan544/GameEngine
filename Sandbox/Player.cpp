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

		mTransform.mPosition += mDirection * mPlayerSpeed * delta;

		//Rotate forward vector by pitch and yaw in degrees
		glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mYaw), { 0.0f, 1.0f, 0.0f });
		glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(mPitch), { 1.0f, 0.0f, 0.0f });
		mForwardVector = glm::mat3(yawRotation) * glm::mat3(pitchRotation) * mGlobalForward;

		mCamera.mPosition = mTransform.mPosition - mForwardVector * mCameraDistance;
		mLight.mUniformBuffer.position = glm::vec4(mTransform.mPosition + glm::vec3(0.0f, 2.0f, 0.0f), 0.0f);
	}

	void Player::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		switch (stage)
		{
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
			eg::Data::StaticModelRenderer::render(cmd, *mModel, mTransform.build());
			break;
		case eg::Renderer::RenderStage::SUBPASS1_POINTLIGHT:
			eg::Data::LightRenderer::renderPointLights(cmd, &mLight, 1);
			break;
		default:
			break;
		}		
	}
}