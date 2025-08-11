#include <Components.h>
#include <Data.h>

namespace eg::Components
{
	DirectionalLight::DirectionalLight() :
		mBuffer(nullptr, sizeof(UniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer)
	{
		vk::DescriptorSetLayout setLayouts[] =
		{
			eg::Data::LightRenderer::getDirectionalPerDescLayout()
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

	std::array<glm::mat4x4, Renderer::ShadowRenderPass::getCsmCount()>
		DirectionalLight::getDirectionalLightViewProj(const Camera& camera) const
	{
		std::array<glm::mat4x4, Renderer::ShadowRenderPass::getCsmCount()> result;


		auto generateMat = [&](float nearPlane, float farPlane)
			{
				auto aspectRatio = (float)Renderer::getDrawExtent().extent.width / (float)Renderer::getDrawExtent().extent.height;
				glm::mat4 cameraProjection = glm::perspectiveRH_ZO(
					glm::radians(camera.mFov),
					aspectRatio,
					nearPlane,
					farPlane
				);
				cameraProjection[1][1] *= -1;
				glm::mat4 inv = glm::inverse(cameraProjection * camera.buildView());
				std::array<glm::vec4, 8> frustumCorners;
				size_t i = 0;
				for (unsigned int x = 0; x < 2; ++x)
				{
					for (unsigned int y = 0; y < 2; ++y)
					{
						for (unsigned int z = 0; z < 2; ++z)
						{
							const glm::vec4 pt =
								inv * glm::vec4(
									2.0f * x - 1.0f,
									2.0f * y - 1.0f,
									2.0f * z - 1.0f,
									1.0f);
							frustumCorners.at(i++) = pt / pt.w; // Homogeneous divide
						}
					}
				}

				glm::vec3 center = glm::vec3(0, 0, 0);
				for (const auto& v : frustumCorners)
				{
					center += glm::vec3(v);
				}
				center /= frustumCorners.size();

				glm::mat4 lightView = glm::lookAt(
					center - glm::normalize(mUniformBuffer.direction),
					center,
					glm::vec3(0.0f, 1.0f, 0.0f)
				);

				glm::vec3 min = glm::vec3(FLT_MAX);
				glm::vec3 max = glm::vec3(-FLT_MAX);
				for (const auto& corner : frustumCorners) {
					glm::vec3 lightSpaceCorner = glm::vec3(lightView * corner);
					min = glm::min(min, lightSpaceCorner);
					max = glm::max(max, lightSpaceCorner);
				}

				const float zPadding = 305.0f;
				min.z -= zPadding;
				max.z += zPadding;


				glm::mat4 lightProj = glm::orthoRH_ZO(min.x, max.x, min.y, max.y, min.z, max.z);
				lightProj[1][1] *= -1;

				return lightProj * lightView;
			};
		

		result[0] = generateMat(1.0f, 5.0f);
		result[1] = generateMat(5.0f, 20.0f);
		result[2] = generateMat(20.0f, 300.0f);
		return result;
	}
}