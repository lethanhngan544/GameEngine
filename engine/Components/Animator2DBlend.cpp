#include "Components.h"


namespace eg::Components
{
	void Animator2DBlend::processCurrentAnimation(float delta)
	{
		struct BlendedNode
		{
			int32_t targetJoint = -1;
			glm::vec3 translation = { 0, 0, 0 };
			glm::quat rotation = { 1, 0, 0, 0 };
			glm::vec3 scale = { 1, 1, 1 };
		};
		mCurrentTime += delta;

		
		
		std::vector<BlendedNode> blendedNodes;
		blendedNodes.resize(mAnimationNodes.size());

		//Process each channel
		float length2 = mState.x * mState.x + mState.y * mState.y;
		if (length2 != 0.0f)
		{
			mState = glm::normalize(mState); // Ensure state is normalized
		}
		size_t i = 0;
		for (const auto& animation : mAnimations)
		{
			float blendFactor = 0.0f;
			switch (i)
			{
			case 0: // Forward
				blendFactor = mState.y; // Vertical
				break;
			case 1: // Backward
				blendFactor = -mState.y; // Vertical
				break;
			case 2: // Left
				blendFactor = -mState.x; // Horizontal
				break;
			case 3: // Right
				blendFactor = mState.x; // Horizontal
				break;
			case 4: // Idle
				blendFactor = 1.0 - length2;
				break;
			}

			
			if (mCurrentTime > animation->getDuration())
			{
				mCurrentTime = 0.0f; // Loop the animation
			}
			if (blendFactor < 0.0f)
				blendFactor = 0.0f;

			for (const auto& channel : animation->getChannels())
			{
				auto& blendedNode = blendedNodes.at(channel.targetJoint);
				blendedNode.targetJoint = channel.targetJoint;
				//Find the keyframe for the current time
				size_t keyframeIndex = 0;
				for (size_t i = 0; i < channel.keyTimes.size(); ++i)
				{
					if (channel.keyTimes[i] > mCurrentTime)
					{
						keyframeIndex = i == 0 ? 0 : i - 1; // Use previous keyframe if current time is before first keyframe
						break;
					}
				}
				if (keyframeIndex + 1 >= channel.data.size())
					continue; // Or reuse last keyframe, or reset

				//Interpolate
				const auto& keyData = channel.data[keyframeIndex];
				const auto& nextKeyData = channel.data[keyframeIndex + 1];
				float t = (mCurrentTime - channel.keyTimes[keyframeIndex]) / (channel.keyTimes[keyframeIndex + 1] - channel.keyTimes[keyframeIndex]);
				switch (channel.path)
				{
				case Animation::Channel::Path::Translation:
					auto interpolatedTranslation = glm::mix(keyData.translation, nextKeyData.translation, t);
					blendedNode.translation = glm::mix(blendedNode.translation, interpolatedTranslation, blendFactor);
					break;
				case Animation::Channel::Path::Rotation:
					auto interpolatedRotation = glm::slerp(keyData.rotation, nextKeyData.rotation, t);
					blendedNode.rotation = glm::slerp(blendedNode.rotation, interpolatedRotation, blendFactor);
					break;
				case Animation::Channel::Path::Scale:
					auto interpolatedScale = glm::mix(keyData.scale, nextKeyData.scale, t);
					blendedNode.scale = glm::mix(blendedNode.scale, interpolatedScale, blendFactor);
					break;
				default:
					throw std::runtime_error("Unknown animation channel path");
				}			
			}

			i++;
		}

		for(const auto& blendedNode : blendedNodes)
		{
			if (blendedNode.targetJoint != -1)
			{
				auto& currentNode = mAnimationNodes.at(blendedNode.targetJoint);
				currentNode->position = blendedNode.translation;
				currentNode->rotation = blendedNode.rotation;
				currentNode->scale = blendedNode.scale;
			}
		}

	}
}