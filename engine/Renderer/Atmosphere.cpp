#include "Renderer.h"

#include <random>

#include <shaderc/shaderc.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace eg::Renderer
{
	Atmosphere::Atmosphere(const DefaultRenderPass& defaultpass, const vk::DescriptorSetLayout& globalSetLayout, uint32_t shadowMapSize) :
		mDirectionalBuffer(nullptr, sizeof(DirectionalLightUniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer),
		mAmbientBuffer(nullptr, sizeof(AmbientLightUniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer),
		mShadowMapSize(shadowMapSize)
	{
		generateCSMPlanes(500.0f);
		generateSSAOKernel();
		generateSSAONoise();


		createAmbientLightPipeline(defaultpass, globalSetLayout);
		createDirectionalShadowPass(shadowMapSize);
		createDirectionalLightPipeline(defaultpass, globalSetLayout);
		
	}
	Atmosphere::~Atmosphere()
	{
		getDevice().destroySampler(mSSAONoiseSampler);

		getDevice().destroyPipeline(mAmbientPipeline);
		getDevice().destroyPipelineLayout(mAmbientLayout);
		getDevice().destroyDescriptorSetLayout(mAmbientDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mAmbientSet);


		getDevice().destroyPipeline(mDirectionalPipeline);
		getDevice().destroyPipelineLayout(mDirectionalLayout);
		getDevice().destroyDescriptorSetLayout(mDirectionalDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mDirectionalSet);

		getDevice().destroySampler(mDepthSampler);
		getDevice().destroyImageView(mDepthImageView);
		getAllocator().destroyImage(mDepthImage, mDepthAllocation);

		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}


	void Atmosphere::generateCSMMatrices(float fov, const glm::mat4x4& viewMatrix)
	{
		auto generateMat = [&](float nearPlane, float farPlane)
			{
				auto aspectRatio = (float)Renderer::getDrawExtent().extent.width / (float)Renderer::getDrawExtent().extent.height;
				glm::mat4 cameraProjection = glm::perspectiveRH_ZO(
					glm::radians(fov),
					aspectRatio,
					nearPlane,
					farPlane
				);
				cameraProjection[1][1] *= -1;
				glm::mat4 inv = glm::inverse(cameraProjection * viewMatrix);
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
					center - glm::normalize(mDirectionalLightUniformBuffer.direction),
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


				glm::vec3 extents = max - min;
				float worldUnitsPerTexel = extents.x / static_cast<float>(mShadowMapSize);
				// Snap min bounds to nearest texel
				min.x = std::floor(min.x / worldUnitsPerTexel) * worldUnitsPerTexel;
				min.y = std::floor(min.y / worldUnitsPerTexel) * worldUnitsPerTexel;

				max.x = min.x + extents.x;
				max.y = min.y + extents.y;

				const float zPadding = 100.0f;
				min.z -= zPadding;
				max.z += zPadding;


				glm::mat4 lightProj = glm::orthoRH_ZO(min.x, max.x, min.y, max.y, min.z, max.z);
				lightProj[1][1] *= -1;

				return lightProj * lightView;
			};


		for(size_t i = 0; i < MAX_CSM_COUNT; ++i)
		{
			mDirectionalLightUniformBuffer.csmMatrices[i] =
				generateMat(0.1f, mDirectionalLightUniformBuffer.csmPlanes[i]);
		}
	}

	void Atmosphere::updateDirectionalLight()
	{
		mDirectionalBuffer.write(&mDirectionalLightUniformBuffer, sizeof(DirectionalLightUniformBuffer));
	}

	void Atmosphere::updateAmbientLight()
	{
		mAmbientLightUniformBuffer.renderScale = Renderer::getRenderScale();
		mAmbientBuffer.write(&mAmbientLightUniformBuffer, sizeof(AmbientLightUniformBuffer));
	}

	void Atmosphere::beginDirectionalShadowPass(const vk::CommandBuffer& cmd) const
	{
		vk::ClearValue clearValues[] =
		{
			vk::ClearDepthStencilValue(1.0f, 0),
		};

		vk::RenderPassBeginInfo renderPassBI{};
		renderPassBI.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0),
				vk::Extent2D(mShadowMapSize, mShadowMapSize)))
			.setClearValues(clearValues);

		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eSecondaryCommandBuffers);
	}

	void Atmosphere::renderDirectionalLight(const vk::CommandBuffer& cmd) const
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mDirectionalPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mDirectionalLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mDirectionalSet },
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getScaledDrawExtent().extent.width),
			static_cast<float>(Renderer::getScaledDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getScaledDrawExtent());
		cmd.draw(3, 1, 0, 0);
	}

	void Atmosphere::renderAmbientLight(const vk::CommandBuffer& cmd) const
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mAmbientPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mAmbientLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mAmbientSet },
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getScaledDrawExtent().extent.width),
			static_cast<float>(Renderer::getScaledDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getScaledDrawExtent());
		cmd.draw(3, 1, 0, 0);
	}

	void Atmosphere::generateSSAOKernel()
	{
		std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
		std::default_random_engine generator;
		for (size_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
		{
			glm::vec3 sample(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator)
			);
			sample = glm::normalize(sample);
			sample *= randomFloats(generator);
			float scale = (float)i / 64.0;
			scale = glm::mix(0.1f, 1.0f, scale * scale);
			sample *= scale;
			mAmbientLightUniformBuffer.ssaoKernel[i] = glm::vec4(sample, 0.69f);
		}
	}

	void Atmosphere::generateSSAONoise()
	{
		std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
		std::default_random_engine generator;
		std::vector<glm::vec4> ssaoNoise;
		for (unsigned int i = 0; i < 16; i++)
		{
			glm::vec4 noise(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				0.0f, 0.69f);
			ssaoNoise.push_back(noise);
		}

		mSSAONoiseImage.emplace(4, 4, vk::Format::eR32G32B32A32Sfloat,
			vk::ImageUsageFlagBits::eSampled,
			vk::ImageAspectFlagBits::eColor, ssaoNoise.data(), sizeof(glm::vec4) * ssaoNoise.size());

		{

			vk::SamplerCreateInfo ci{};
			ci.setMagFilter(vk::Filter::eLinear)
				.setMinFilter(vk::Filter::eLinear)
				.setMipmapMode(vk::SamplerMipmapMode::eLinear)
				.setAddressModeU(vk::SamplerAddressMode::eRepeat)
				.setAddressModeV(vk::SamplerAddressMode::eRepeat)
				.setAddressModeW(vk::SamplerAddressMode::eRepeat)
				.setMipLodBias(0)
				.setMinLod(-1)
				.setMaxLod(1)
				.setAnisotropyEnable(false)
				.setMaxAnisotropy(0.0f)
				.setCompareEnable(false)
				.setCompareOp(vk::CompareOp::eAlways);

			mSSAONoiseSampler = getDevice().createSampler(ci);

		}

	}

	void Atmosphere::generateCSMPlanes(float maxDistance)
	{
		//Generate CSM planes using linear distribution
		float nearPlane = 5.0f;
		float farPlane = 100.0f;
		float step = 10.0f;
		for (size_t i = 0; i < MAX_CSM_COUNT; ++i)
		{
			float plane = nearPlane + step * i;
			mDirectionalLightUniformBuffer.csmPlanes[i] = plane;
		}
	}

	void Atmosphere::createAmbientLightPipeline(const DefaultRenderPass& defaultpass, const vk::DescriptorSetLayout& globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //mr
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth,
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //Ambient light uniform buffer
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //SSAO noise
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mAmbientDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mAmbientDescLayout);
		mAmbientSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);


		
		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, defaultpass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, defaultpass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, defaultpass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSSAONoiseSampler, defaultpass.getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSSAONoiseSampler, mSSAONoiseImage->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		vk::DescriptorBufferInfo bufferInfo{};
		bufferInfo.setBuffer(mAmbientBuffer.getBuffer())
			.setOffset(0)
			.setRange(sizeof(AmbientLightUniformBuffer));

		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mAmbientSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0]),
			vk::WriteDescriptorSet(mAmbientSet, 1, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[1]),
			vk::WriteDescriptorSet(mAmbientSet, 2, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[2]),

			vk::WriteDescriptorSet(mAmbientSet, 3, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[3]),

			vk::WriteDescriptorSet(mAmbientSet, 4, 0,
				1,
				vk::DescriptorType::eUniformBuffer, {}, &bufferInfo),

			vk::WriteDescriptorSet(mAmbientSet, 5, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler, &imageInfos[4]),

			}, {});



		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/atmosphere_ambient.glsl", shaderc_glsl_fragment_shader, {
			{"SSAO_KERNEL_SIZE", std::to_string(SSAO_KERNEL_SIZE) }}
		);

		//Create shader modules
		vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, fragmentShaderModuleCI{};
		vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
		vertexShaderModuleCI.setPCode(vertexBinary.data());
		fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
		fragmentShaderModuleCI.setPCode(fragmentBinary.data());

		auto vertexShaderModule = Renderer::getDevice().createShaderModule(vertexShaderModuleCI);
		auto fragmentShaderModule = Renderer::getDevice().createShaderModule(fragmentShaderModuleCI);

		//Create pipeline layout
		vk::DescriptorSetLayout setLayouts[] =
		{
			globalSetLayout, // Slot0
			mAmbientDescLayout //Slot 1
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRangeCount(0)
			.setPPushConstantRanges(nullptr);
		mAmbientLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

		//Create graphics pipeline
		vk::PipelineShaderStageCreateInfo shaderStages[] =
		{
			vk::PipelineShaderStageCreateInfo
			{
				vk::PipelineShaderStageCreateFlags{},
				vk::ShaderStageFlagBits::eVertex,
				vertexShaderModule,
				"main"
			},
			vk::PipelineShaderStageCreateInfo
			{
				vk::PipelineShaderStageCreateFlags{},
				vk::ShaderStageFlagBits::eFragment,
				fragmentShaderModule,
				"main"
			}
		};


		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{});

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
		inputAssemblyStateCI.setFlags(vk::PipelineInputAssemblyStateCreateFlags{})
			.setTopology(vk::PrimitiveTopology::eTriangleList)
			.setPrimitiveRestartEnable(false);

		vk::Viewport viewport{};
		viewport.setMinDepth(0.0f)
			.setMaxDepth(1.0f);
		vk::Rect2D scissor{};
		vk::PipelineViewportStateCreateInfo viewportStateCI{};
		viewportStateCI.setFlags(vk::PipelineViewportStateCreateFlags{})
			.setScissors(scissor)
			.setViewports(viewport);

		vk::PipelineRasterizationStateCreateInfo rasterizationStateCI{};
		rasterizationStateCI.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setPolygonMode(vk::PolygonMode::eFill)
			.setLineWidth(1.0f)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthBiasClamp(0.0f)
			.setDepthBiasEnable(false)
			.setDepthBiasConstantFactor(0.0f)
			.setDepthBiasSlopeFactor(0.0f);

		vk::PipelineMultisampleStateCreateInfo multisampleStateCI{};
		multisampleStateCI.setRasterizationSamples(vk::SampleCountFlagBits::e1)
			.setSampleShadingEnable(false)
			.setMinSampleShading(1.0f)
			.setPSampleMask(nullptr)
			.setAlphaToCoverageEnable(false)
			.setAlphaToOneEnable(false);


		vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
		colorBlendAttachmentState
			.setColorWriteMask(
				vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)
			.setBlendEnable(true)
			.setSrcColorBlendFactor(vk::BlendFactor::eOne)
			.setDstColorBlendFactor(vk::BlendFactor::eOne)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOne)
			.setAlphaBlendOp(vk::BlendOp::eAdd);

		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentState)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });


		vk::StencilOpState stencilOpState = {};
		stencilOpState.setFailOp(vk::StencilOp::eKeep)
			.setPassOp(vk::StencilOp::eKeep)
			.setDepthFailOp(vk::StencilOp::eKeep)
			.setCompareOp(vk::CompareOp::eEqual)
			.setCompareMask(0xFF)
			.setWriteMask(0x00)
			.setReference(Renderer::MESH_STENCIL_VALUE);

		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(false)
			.setDepthWriteEnable(false)
			.setDepthCompareOp(vk::CompareOp::eLess)
			.setDepthBoundsTestEnable(false)
			.setStencilTestEnable(true)
			.setFront(stencilOpState)
			.setBack(stencilOpState)
			.setMinDepthBounds(0.0f)
			.setMaxDepthBounds(1.0f);

		std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
		dynamicStateCI.setDynamicStates(dynamicStates);



		vk::GraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.setLayout(mAmbientLayout)
			.setRenderPass(defaultpass.getRenderPass())
			.setSubpass(1)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputStateCI)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setPInputAssemblyState(&inputAssemblyStateCI)
			.setPViewportState(&viewportStateCI)
			.setPRasterizationState(&rasterizationStateCI)
			.setPMultisampleState(&multisampleStateCI)
			.setPDepthStencilState(&depthStencilStateCI)
			.setPColorBlendState(&colorBlendStateCI)
			.setPDynamicState(&dynamicStateCI);

		auto pipeLineResult = Renderer::getDevice().createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create AmbientLight pipeline !");
		}
		mAmbientPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	void Atmosphere::createDirectionalLightPipeline(const DefaultRenderPass& defaultpass, const vk::DescriptorSetLayout& globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Mr
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Shadow map
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, {}), //UniformBuffer

		};

		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mDirectionalDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);


		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mDirectionalDescLayout);
		mDirectionalSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, defaultpass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, defaultpass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, defaultpass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, defaultpass.getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mDepthSampler, mDepthImageView, vk::ImageLayout::eShaderReadOnlyOptimal),
		};
		vk::DescriptorBufferInfo bufferInfo;
		bufferInfo.setBuffer(mDirectionalBuffer.getBuffer())
			.setOffset(0)
			.setRange(sizeof(DirectionalLightUniformBuffer));


		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mDirectionalSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0]),
			vk::WriteDescriptorSet(mDirectionalSet, 1, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[1]),
			vk::WriteDescriptorSet(mDirectionalSet, 2, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[2]),
			vk::WriteDescriptorSet(mDirectionalSet, 3, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[3]),
			vk::WriteDescriptorSet(mDirectionalSet, 4, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[4]),
			vk::WriteDescriptorSet(mDirectionalSet, 5, 0,
				1,
				vk::DescriptorType::eUniformBuffer,
				nullptr,
				&bufferInfo)
			},
			{});

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/atmosphere_directional.glsl", shaderc_glsl_fragment_shader,
			{ {"MAX_CSM_COUNT", std::to_string(MAX_CSM_COUNT)} });

		//Create shader modules
		vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, fragmentShaderModuleCI{};
		vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
		vertexShaderModuleCI.setPCode(vertexBinary.data());
		fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
		fragmentShaderModuleCI.setPCode(fragmentBinary.data());

		auto vertexShaderModule = Renderer::getDevice().createShaderModule(vertexShaderModuleCI);
		auto fragmentShaderModule = Renderer::getDevice().createShaderModule(fragmentShaderModuleCI);

		//Create pipeline layout
		vk::DescriptorSetLayout setLayouts[] =
		{
			globalSetLayout, // Slot0
			mDirectionalDescLayout, //Slot 1
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRangeCount(0)
			.setPPushConstantRanges(nullptr);
		mDirectionalLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

		//Create graphics pipeline
		vk::PipelineShaderStageCreateInfo shaderStages[] =
		{
			vk::PipelineShaderStageCreateInfo
			{
				vk::PipelineShaderStageCreateFlags{},
				vk::ShaderStageFlagBits::eVertex,
				vertexShaderModule,
				"main"
			},
			vk::PipelineShaderStageCreateInfo
			{
				vk::PipelineShaderStageCreateFlags{},
				vk::ShaderStageFlagBits::eFragment,
				fragmentShaderModule,
				"main"
			}
		};


		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{});

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
		inputAssemblyStateCI.setFlags(vk::PipelineInputAssemblyStateCreateFlags{})
			.setTopology(vk::PrimitiveTopology::eTriangleList)
			.setPrimitiveRestartEnable(false);

		vk::Viewport viewport{};
		viewport.setMinDepth(0.0f)
			.setMaxDepth(1.0f);
		vk::Rect2D scissor{};
		vk::PipelineViewportStateCreateInfo viewportStateCI{};
		viewportStateCI.setFlags(vk::PipelineViewportStateCreateFlags{})
			.setScissors(scissor)
			.setViewports(viewport);

		vk::PipelineRasterizationStateCreateInfo rasterizationStateCI{};
		rasterizationStateCI.setDepthClampEnable(false)
			.setRasterizerDiscardEnable(false)
			.setPolygonMode(vk::PolygonMode::eFill)
			.setLineWidth(1.0f)
			.setCullMode(vk::CullModeFlagBits::eNone)
			.setFrontFace(vk::FrontFace::eCounterClockwise)
			.setDepthBiasClamp(0.0f)
			.setDepthBiasEnable(false)
			.setDepthBiasConstantFactor(0.0f)
			.setDepthBiasSlopeFactor(0.0f);

		vk::PipelineMultisampleStateCreateInfo multisampleStateCI{};
		multisampleStateCI.setRasterizationSamples(vk::SampleCountFlagBits::e1)
			.setSampleShadingEnable(false)
			.setMinSampleShading(1.0f)
			.setPSampleMask(nullptr)
			.setAlphaToCoverageEnable(false)
			.setAlphaToOneEnable(false);

		vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
		colorBlendAttachmentState
			.setColorWriteMask(
				vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)
			.setBlendEnable(true)
			.setSrcColorBlendFactor(vk::BlendFactor::eOne)
			.setDstColorBlendFactor(vk::BlendFactor::eOne)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eOne)
			.setAlphaBlendOp(vk::BlendOp::eAdd);


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentState)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		vk::StencilOpState stencilOpState = {};
		stencilOpState.setFailOp(vk::StencilOp::eKeep)
			.setPassOp(vk::StencilOp::eKeep)
			.setDepthFailOp(vk::StencilOp::eKeep)
			.setCompareOp(vk::CompareOp::eEqual)
			.setCompareMask(0xFF)
			.setWriteMask(0x00)
			.setReference(MESH_STENCIL_VALUE);

		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(false)
			.setDepthWriteEnable(false)
			.setDepthCompareOp(vk::CompareOp::eLess)
			.setDepthBoundsTestEnable(false)
			.setStencilTestEnable(true)
			.setFront(stencilOpState)
			.setBack(stencilOpState)
			.setMinDepthBounds(0.0f)
			.setMaxDepthBounds(1.0f);

		std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
		dynamicStateCI.setDynamicStates(dynamicStates);

		vk::GraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.setLayout(mDirectionalLayout)
			.setRenderPass(defaultpass.getRenderPass())
			.setSubpass(1)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputStateCI)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setPInputAssemblyState(&inputAssemblyStateCI)
			.setPViewportState(&viewportStateCI)
			.setPRasterizationState(&rasterizationStateCI)
			.setPMultisampleState(&multisampleStateCI)
			.setPDepthStencilState(&depthStencilStateCI)
			.setPColorBlendState(&colorBlendStateCI)
			.setPDynamicState(&dynamicStateCI);

		auto pipeLineResult = Renderer::getDevice().createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create DirectionalLight pipeline !");
		}
		mDirectionalPipeline = pipeLineResult.value;

		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	void Atmosphere::createDirectionalShadowPass(uint32_t size)
	{
		vk::ImageCreateInfo imageCI{};
		imageCI.setImageType(vk::ImageType::e2D)
			.setExtent({ size, size, 1 })
			.setMipLevels(1)
			.setArrayLayers(static_cast<uint32_t>(MAX_CSM_COUNT))
			.setFormat(mDepthFormat)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(mUsageFlags)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setFlags(vk::ImageCreateFlags{});
		vma::AllocationCreateInfo allocCI{};
		allocCI.setUsage(vma::MemoryUsage::eGpuOnly);
		auto [image, allocation] = getAllocator().createImage(imageCI, allocCI);
		mDepthImage = image;
		mDepthAllocation = allocation;

		vk::ImageViewCreateInfo imageViewCI{};
		imageViewCI.setImage(mDepthImage)
			.setViewType(vk::ImageViewType::e2DArray)
			.setFormat(mDepthFormat)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, static_cast<uint32_t>(MAX_CSM_COUNT) });
		mDepthImageView = getDevice().createImageView(imageViewCI);

		//Sampler
		vk::SamplerCreateInfo ci{};
		ci.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eNearest)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(1)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways);

		mDepthSampler = getDevice().createSampler(ci);


		//Create sampler for depth
		vk::SamplerCreateInfo samplerCI{};
		samplerCI.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
			.setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
			.setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(0)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways)
			.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);

		mDepthSampler = getDevice().createSampler(samplerCI);

		vk::AttachmentDescription attachments[] =
		{
			//Depth, id = 0
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				vk::Format::eD32Sfloat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eDepthStencilReadOnlyOptimal
			),

		};


		vk::AttachmentReference pass0OutputDepthStencilAttachmentRef = vk::AttachmentReference(0, vk::ImageLayout::eDepthStencilAttachmentOptimal); //Depth;

		vk::SubpassDescription subpasses[] = {
			//Subpass 0
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				0, nullptr, //Output
				nullptr, //Resolve
				&pass0OutputDepthStencilAttachmentRef,
				0, nullptr//Preserve
			),


		};

		//vk::SubpassDependency dependencies[] = {
		//	//// External -> Shadow map pass: ensure previous depth reads/writes are done
		//	//vk::SubpassDependency(
		//	//	VK_SUBPASS_EXTERNAL, 0,
		//	//	vk::PipelineStageFlagBits::eFragmentShader,
		//	//	vk::PipelineStageFlagBits::eEarlyFragmentTests,
		//	//	vk::AccessFlagBits::eShaderRead,
		//	//	vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		//	//	vk::DependencyFlagBits::eByRegion
		//	//),


		//	//vk::SubpassDependency(
		//	//	0, VK_SUBPASS_EXTERNAL,
		//	//	vk::PipelineStageFlagBits::eLateFragmentTests,
		//	//	vk::PipelineStageFlagBits::eFragmentShader,
		//	//	vk::AccessFlagBits::eDepthStencilAttachmentWrite,
		//	//	vk::AccessFlagBits::eShaderRead,
		//	//	vk::DependencyFlagBits::eByRegion
		//	//)
		//};
		vk::RenderPassCreateInfo renderPassCI{};
		renderPassCI
			.setAttachments(attachments)
			.setSubpasses(subpasses);

		mRenderPass = getDevice().createRenderPass(renderPassCI);


		vk::ImageView frameBufferAttachments[] =
		{
			mDepthImageView
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(size)
			.setHeight(size)
			.setLayers(MAX_CSM_COUNT);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);
	}
}