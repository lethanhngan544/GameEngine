#include <Components.h>

#include <Data.h>

namespace eg::Components
{
	Animator::Animator(const std::vector<std::shared_ptr<Animation>>& animations,
		const AnimatedModel& model) :
		mModel(model),
		mUniformBuffer(nullptr, sizeof(mBoneMatrices), vk::BufferUsageFlagBits::eUniformBuffer),
		mCurrentAnimation(nullptr)
	{
		//Init animation map
		
		for (const auto& animation : animations)
		{
			mAnimationMap[animation->getName()] = animation;
		}

		//Copy model nodes to animation nodes
		mAnimationNodes.reserve(model.mNodes.size());
		for (const auto& node : model.mNodes)
		{
			auto animNode = std::make_shared< AnimationNode>();
			animNode->name = node.name;
			animNode->position = node.position;
			animNode->rotation = node.rotation;
			animNode->scale = node.scale;
			animNode->children = node.children;
			animNode->meshIndex = node.meshIndex;
			animNode->modelLocalTransform = glm::mat4x4(1.0f);
			mAnimationNodes.push_back(animNode);
		}

		//Allocate uniform buffer 
		mBoneMatrices.fill(glm::mat4x4(1.0f));

		//Allocate descriptor set
		vk::DescriptorSetLayout setLayouts[] =
		{
			Data::AnimatedModelRenderer::getBoneSetLayout()
		};
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(setLayouts);
		mDescriptorSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorBufferInfo bufferInfo{};
		bufferInfo
			.setOffset(0)
			.setRange(sizeof(mBoneMatrices))
			.setBuffer(mUniformBuffer.getBuffer());

		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mDescriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo, nullptr, nullptr),
			}, {});
	}

	void Animator::setAnimation(const std::string& animationName)
	{
		if (mAnimationMap.find(animationName) != mAnimationMap.end())
		{
			mCurrentAnimation = mAnimationMap.at(animationName);
		}
		else
		{
			mCurrentAnimation = nullptr;
		}
		
	}

	void Animator::processCurrentAnimation(float delta)
	{
		if (!mCurrentAnimation)
			return;
		mCurrentTime += delta;
		if (mCurrentTime > mCurrentAnimation->getDuration())
		{
			mCurrentTime = 0.0f; // Loop the animation
		}

		//Process each channel
		for (const auto& channel : mCurrentAnimation->getChannels())
		{
			auto currentNode = mAnimationNodes.at(channel.targetJoint);
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
				currentNode->position = glm::mix(keyData.translation, nextKeyData.translation, t);
				break;
			case Animation::Channel::Path::Rotation:
				currentNode->rotation = glm::slerp(keyData.rotation, nextKeyData.rotation, t);
				break;
			case Animation::Channel::Path::Scale:
				currentNode->scale = glm::mix(keyData.scale, nextKeyData.scale, t);
				break;
			default:
				throw std::runtime_error("Unknown animation channel path");
			}
		}

	}

	void Animator::update(float deltaTime)
	{
		processCurrentAnimation(deltaTime * mTimeScale);
		//Build current node worlds transform using lambdas
		std::function<void(AnimationNodeVec& nodes, const std::shared_ptr<AnimationNode>& currentNode, glm::mat4x4 accumulatedTransform)> buildNodeModelLocalTransform
			= [&](AnimationNodeVec& nodes, const std::shared_ptr<AnimationNode>& currentNode, glm::mat4x4 accumulatedTransform)
			{

				glm::mat4x4 localTransform = glm::mat4x4(1.0f);
				localTransform = glm::translate(localTransform, currentNode->position);
				localTransform *= glm::mat4_cast(currentNode->rotation);
				localTransform = glm::scale(localTransform, currentNode->scale);


				accumulatedTransform *= localTransform;
				currentNode->modelLocalTransform = accumulatedTransform;


				for (const auto& child : currentNode->children)
				{
					buildNodeModelLocalTransform(nodes, nodes.at(child), accumulatedTransform);
				}
			};

		for (const auto& skin : mModel.mSkins)
		{
			buildNodeModelLocalTransform(mAnimationNodes, mAnimationNodes.at(skin.joints.at(0)), glm::mat4x4(1.0f));

			for (size_t j = 0; j < skin.joints.size(); ++j)
			{
				const auto& joint = skin.joints.at(j);
				glm::mat4x4 inverseBindMatrix = skin.inverseBindMatrices.at(j);

				mBoneMatrices[j] = mAnimationNodes.at(joint)->modelLocalTransform * inverseBindMatrix;
			}
		}

		//Update bone matrices
		mUniformBuffer.write(mBoneMatrices.data(), sizeof(mBoneMatrices));
	}

	Animator::~Animator()
	{
		//Free descriptor set
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mDescriptorSet);
	}

	std::shared_ptr<Animator::AnimationNode> Animator::getAnimationNodeByName(const std::string& name)
	{
		for (const auto& node : mAnimationNodes)
		{
			if (node->name == name)
			{
				return node;
			}
		}

		return nullptr;
	}

}