#pragma once

#include "SandBox_Player.h"

namespace sndbx
{
	class PlayerControlled : public Player
	{
	private:
		eg::Data::Camera mCamera;
	public:
		PlayerControlled() : Player(false) {}
		~PlayerControlled() = default;
		void update(float delta) override;
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage) override;


		const eg::Data::Camera& getCamera() const { return mCamera; }
	};


}