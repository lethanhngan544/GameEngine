#include <Data.h>
namespace eg::Data
{
	PointLight::PointLight()  :
		mBuffer(nullptr, sizeof(UniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer)
	{

		vk::DescriptorSetLayout setLayouts[] =
		{
			LightRenderer::getPointLightPerDescLayout()
		};
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(setLayouts);
		this->mSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorBufferInfo buffer(mBuffer.getBuffer(), 0, sizeof(UniformBuffer));


		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mSet, 0, 0,
				1,
				vk::DescriptorType::eUniformBuffer,
				nullptr,
				&buffer,
				nullptr)
			}, {});
	}

	PointLight::~PointLight()
	{
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mSet);
	}
}