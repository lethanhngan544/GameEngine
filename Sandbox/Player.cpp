#include <SandBox_Player.h>

namespace sndbx
{
	Player::Player(bool visible)
	{
		if(visible)
			mModel = eg::Data::StaticModelCache::load("models/DamagedHelmet.glb");
	}
}