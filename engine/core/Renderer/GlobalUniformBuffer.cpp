#include <Renderer.h>

namespace eg::Renderer
{
	GlobalUniformBuffer::GlobalUniformBuffer(size_t frameCount)
	{
		//Define layout
		vk::DescriptorSetLayoutBinding bindings[] = {
			{0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, {}}
		};


		vk::DescriptorSetLayoutCreateInfo layoutCI{};
		layoutCI.setBindings(bindings);

		this->mLayout = getDevice().createDescriptorSetLayout(layoutCI);


		for (size_t i = 0; i < frameCount; i++)
		{
			this->mDesc.push_back(std::make_unique<DescriptorSet>(this->mLayout));
		}

	}

	GlobalUniformBuffer::~GlobalUniformBuffer()
	{
		getDevice().destroyDescriptorSetLayout(this->mLayout);
	}

	void GlobalUniformBuffer::update(const Data& data, size_t frameIndex)
	{
		this->mDesc.at(frameIndex)->getCPUBuffer().write(&data, sizeof(Data));
	}

	const GlobalUniformBuffer::DescriptorSet& GlobalUniformBuffer::getDescriptorSet(size_t currentFrame)
	{
		return *this->mDesc.at(currentFrame);
	}
}