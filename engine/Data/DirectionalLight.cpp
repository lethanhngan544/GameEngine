#include <Data.h>

namespace eg::Data
{
	DirectionalLight::DirectionalLight() :
		mBuffer(nullptr, sizeof(UniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer)
	{
		vk::DescriptorSetLayout setLayouts[] =
		{
			LightRenderer::getDirectionalPerDescLayout()
		};
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(setLayouts);
		mSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.setBuffer(mBuffer.getBuffer())
			.setOffset(0)
			.setRange(sizeof(UniformBuffer));



		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mSet, 0, 0,
				1,
				vk::DescriptorType::eUniformBuffer,
				nullptr,
				&bufferInfo)
			},

			{});
	}

	DirectionalLight::~DirectionalLight()
	{
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mSet);
	}


	void DirectionalLight::update()
	{
		mBuffer.write(&mUniformBuffer, sizeof(UniformBuffer));
	}

	glm::mat4x4 DirectionalLight::getDirectionalLightViewProj(const Camera& camera) const
	{
		glm::mat4x4 view = glm::lookAt(camera.mPosition, camera.mPosition + mUniformBuffer.direction * 5.0f, glm::vec3(0, 1, 0));
		glm::mat4x4 proj = glm::orthoRH_ZO(-10.0f, 10.0f, -10.0f, 10.0f, -100.0f, 100.0f);
		proj[1][1] *= -1;
		return proj * view;
	}
}