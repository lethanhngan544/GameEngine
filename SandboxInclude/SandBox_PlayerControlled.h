#pragma once

#include "SandBox_Player.h"

namespace sndbx
{
	class PlayerControlled : public Player
	{
	private:
		eg::Components::Camera mCamera;
		std::shared_ptr<eg::Components::Animator::AnimationNode> mHeadNode;
		glm::vec2 mAnimState;
	public:
		PlayerControlled() : Player(true) {}
		~PlayerControlled() = default;

		void update(float delta) override;
		void fixedUpdate(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;


		const char* getType() const override { return "PlayerControlled"; }

		nlohmann::json toJson() const override
		{
			return {
				{"type", getType()},
				{"camera", mCamera.toJson()},
				{"player", Player::toJson()}
			};
		}

		void fromJson(const nlohmann::json& json) override
		{
			mCamera.fromJson(json["camera"]);
			Player::fromJson(json["player"]);
			mHeadNode = mAnimator->getAnimationNodeByName("mixamorig:Head");
			mAnimState = { 0, 0 };
		}


		const eg::Components::Camera& getCamera() const { return mCamera; }
	};


}