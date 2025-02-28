#include <SandBox_Player.h>

namespace sndbx
{
	Player::Player()
	{
		mModel = eg::Data::StaticModelCache::load("assets/models/box/box.gltf");
	}
}