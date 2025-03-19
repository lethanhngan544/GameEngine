#pragma once

#include <Data.h>


namespace sndbx
{
	class Player
	{
	private:
		eg::Data::Transform mTransform;
		std::shared_ptr<eg::Data::StaticModel> mModel = nullptr;
	public:
		Player(bool visible);

		const eg::Data::Transform& getTransform() const { return mTransform;  }
		eg::Data::Transform& getTransform() { return mTransform; }
		const eg::Data::StaticModel* getModel() const { return mModel.get(); }
	};
}