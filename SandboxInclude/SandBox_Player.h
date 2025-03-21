#pragma once

#include <Data.h>


namespace sndbx
{
	class Player : public eg::Data::IGameObject
	{
	private:
		eg::Data::Transform mTransform;
		eg::Data::Camera mCamera;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;

		float mYaw = 0.0f;
		float mPitch = 0.0f;
		const glm::vec3 mGlobalForward = { 0.0f, 0.0f, -1.0f };
		glm::vec3 mForwardVector = mGlobalForward;
		float mCameraDistance = 5.0f;
		float mPlayerSpeed = 1.0f;
	public:
		Player(bool visible);

		void update(float delta) override;
		void render(vk::CommandBuffer cmd) override;

		const eg::Data::Transform& getTransform() const { return mTransform;  }
		eg::Data::Transform& getTransform() { return mTransform; }
		const eg::Data::Camera& getCamera() const { return mCamera; }
	};
}