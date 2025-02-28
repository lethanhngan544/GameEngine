#pragma once

#include <Data.h>

namespace sndbx
{
	class Player
	{
	private:
		eg::Data::Transform mTransform;
		std::shared_ptr<eg::Data::StaticModel> mModel;
	public:
		Player();
	};
}