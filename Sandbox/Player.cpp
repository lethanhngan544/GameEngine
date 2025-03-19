#include <SandBox_Player.h>

namespace sndbx
{
	Player::Player(bool visible)
	{
		if(visible)
			mModel = eg::Data::StaticModelCache::load("models/Sponza.glb");
		
	}

	void Player::render(vk::CommandBuffer cmd)
	{
		eg::Data::StaticModelRenderer::render(cmd, *mModel, mTransform.build());
	}
}