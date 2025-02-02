#include <Data.h>

#include <shaderc/shaderc.hpp>

namespace eg::Data
{
	LightRenderer::LightRenderer(
		vk::Device device,
		vk::DescriptorPool pool,
		const Renderer::DefaultRenderPass& renderpass,
		vk::DescriptorSetLayout globalDescriptorSetLayout) :
		mDevice(device),
		mPool(pool)
	{
		createAmbientPipeline(renderpass, globalDescriptorSetLayout);
		createPointPipeline(renderpass, globalDescriptorSetLayout);
	}
	LightRenderer::~LightRenderer()
	{
		mDevice.destroyPipeline(mAmbientPipeline);
		mDevice.destroyPipelineLayout(mAmbientLayout);
		mDevice.destroyDescriptorSetLayout(mAmbientDescLayout);
		mDevice.freeDescriptorSets(mPool, mAmbientSet);

		mDevice.destroyPipeline(mPointPipeline);
		mDevice.destroyPipelineLayout(mPointLayout);
		mDevice.destroyDescriptorSetLayout(mPointDescLayout);
		mDevice.destroyDescriptorSetLayout(mPointPerDescLayout);
		mDevice.freeDescriptorSets(mPool, mPointSet);

	}

	void LightRenderer::renderAmbient(vk::CommandBuffer cmd,
		vk::Rect2D drawExtent,
		vk::DescriptorSet globalSet) const
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mAmbientPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mAmbientLayout,
			0,
			{ globalSet, mAmbientSet },
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(drawExtent.extent.width),
			static_cast<float>(drawExtent.extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, drawExtent);
		cmd.draw(3, 1, 0, 0);
	}
	void LightRenderer::renderPointLights(vk::CommandBuffer cmd,
		vk::Rect2D drawExtent,
		vk::DescriptorSet globalSet,
		const PointLight* pointLights, size_t pointLightCount) const
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPointPipeline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mPointLayout,
			0,
			{globalSet, this->mPointSet},
			{}
		);
		
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(drawExtent.extent.width),
			static_cast<float>(drawExtent.extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, drawExtent);

		for (size_t i = 0; i < pointLightCount; i++)
		{
			//Copy data to uniform buffer
			std::memcpy(pointLights[i].getBuffer().getInfo().pMappedData, &pointLights[i].mUniformBuffer, sizeof(Data::PointLight::UniformBuffer));

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				mPointLayout,
				2,
				{ pointLights[i].getDescriptorSet() },
				{}
			);
			cmd.draw(3, 1, 0, 0);
		}
	}
	void LightRenderer::createAmbientPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //position
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //mr
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		this->mAmbientDescLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(mPool)
			.setDescriptorSetCount(1)
			.setSetLayouts(this->mAmbientDescLayout);
		this->mAmbientSet = mDevice.allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, renderPass.getPosition().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getPosition().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		mDevice.updateDescriptorSets({
			vk::WriteDescriptorSet(this->mAmbientSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0]),
			vk::WriteDescriptorSet(this->mAmbientSet, 1, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[1]),
			vk::WriteDescriptorSet(this->mAmbientSet, 2, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[2]),

			vk::WriteDescriptorSet(this->mAmbientSet, 3, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[3]),
			}, {});


		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/ambient.glsl", shaderc_glsl_fragment_shader);

		//Create shader modules
		vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, fragmentShaderModuleCI{};
		vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
		vertexShaderModuleCI.setPCode(vertexBinary.data());
		fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
		fragmentShaderModuleCI.setPCode(fragmentBinary.data());

		auto vertexShaderModule = mDevice.createShaderModule(vertexShaderModuleCI);
		auto fragmentShaderModule = mDevice.createShaderModule(fragmentShaderModuleCI);

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
		mAmbientLayout = mDevice.createPipelineLayout(pipelineLayoutCI);

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

		/*vk::VertexInputBindingDescription vertexBindingDescriptions[] = {
			vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex)
		};*/
		/*vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, uv))
		};*/

		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{});
		/*.setVertexAttributeDescriptions(vertexAttributeDescriptions)
		.setVertexBindingDescriptions(vertexBindingDescriptions);*/

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
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)
			.setBlendEnable(true)
			.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
			.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
			.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
			.setAlphaBlendOp(vk::BlendOp::eAdd);

		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentState)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(false)
			.setDepthWriteEnable(false)
			.setDepthCompareOp(vk::CompareOp::eLess)
			.setDepthBoundsTestEnable(false)
			.setStencilTestEnable(false)
			.setFront({})
			.setBack({})
			.setMinDepthBounds(0.0f)
			.setMaxDepthBounds(1.0f);

		std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
		dynamicStateCI.setDynamicStates(dynamicStates);



		vk::GraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.setLayout(mAmbientLayout)
			.setRenderPass(renderPass.getRenderPass())
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

		auto pipeLineResult = mDevice.createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create AmbientLight pipeline !");
		}
		this->mAmbientPipeline = pipeLineResult.value;



		//Destroy shader modules
		mDevice.destroyShaderModule(vertexShaderModule);
		mDevice.destroyShaderModule(fragmentShaderModule);
	}
	void LightRenderer::createPointPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //position
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Mr
		};

		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		this->mPointDescLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(mPool)
			.setDescriptorSetCount(1)
			.setSetLayouts(this->mPointDescLayout);
		this->mPointSet = mDevice.allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, renderPass.getPosition().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};


		mDevice.updateDescriptorSets({
			vk::WriteDescriptorSet(this->mPointSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0]),
			vk::WriteDescriptorSet(this->mPointSet, 1, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[1]),
			vk::WriteDescriptorSet(this->mPointSet, 2, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[2]),
			vk::WriteDescriptorSet(this->mPointSet, 3, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[3])
			}, {});
		//Perlight descriptor set layout
		vk::DescriptorSetLayoutBinding perLightDescLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //position
		};

		descLayoutCI.setBindings(perLightDescLayoutBindings);

		this->mPointPerDescLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/point.glsl", shaderc_glsl_fragment_shader);

		//Create shader modules
		vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, fragmentShaderModuleCI{};
		vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
		vertexShaderModuleCI.setPCode(vertexBinary.data());
		fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
		fragmentShaderModuleCI.setPCode(fragmentBinary.data());

		auto vertexShaderModule = mDevice.createShaderModule(vertexShaderModuleCI);
		auto fragmentShaderModule = mDevice.createShaderModule(fragmentShaderModuleCI);

		//Create pipeline layout
		vk::DescriptorSetLayout setLayouts[] =
		{
			globalSetLayout, // Slot0
			mPointDescLayout, //Slot 1
			mPointPerDescLayout //Slot 2
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRangeCount(0)
			.setPPushConstantRanges(nullptr);
		this->mPointLayout = mDevice.createPipelineLayout(pipelineLayoutCI);

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
			.setColorWriteMask(vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA)
			.setBlendEnable(true)
			.setSrcColorBlendFactor(vk::BlendFactor::eOne)
			.setDstColorBlendFactor(vk::BlendFactor::eOne)
			.setColorBlendOp(vk::BlendOp::eAdd)
			.setSrcAlphaBlendFactor(vk::BlendFactor::eZero)
			.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
			.setAlphaBlendOp(vk::BlendOp::eAdd);

		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentState)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(false)
			.setDepthWriteEnable(false)
			.setDepthCompareOp(vk::CompareOp::eLess)
			.setDepthBoundsTestEnable(false)
			.setStencilTestEnable(false)
			.setFront({})
			.setBack({})
			.setMinDepthBounds(0.0f)
			.setMaxDepthBounds(1.0f);

		std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
		vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
		dynamicStateCI.setDynamicStates(dynamicStates);



		vk::GraphicsPipelineCreateInfo pipelineCI{};
		pipelineCI.setLayout(this->mPointLayout)
			.setRenderPass(renderPass.getRenderPass())
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

		auto pipeLineResult = mDevice.createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create PointLight pipeline !");
		}
		this->mPointPipeline = pipeLineResult.value;



		//Destroy shader modules
		mDevice.destroyShaderModule(vertexShaderModule);
		mDevice.destroyShaderModule(fragmentShaderModule);
	}

	//void LightRenderer::createDirectionalPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout)
	//{
	//	//Define shader layout
	//	vk::DescriptorSetLayoutBinding descLayoutBindings[] =
	//	{
	//		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //position
	//		vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
	//		vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
	//		vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Mr
	//	};

	//	vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
	//	descLayoutCI.setBindings(descLayoutBindings);

	//	this->mDirectionalDescLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

	//	//Allocate descriptor set right here

	//	vk::DescriptorSetAllocateInfo ai{};
	//	ai.setDescriptorPool(mPool)
	//		.setDescriptorSetCount(1)
	//		.setSetLayouts(this->mDirectionalDescLayout);
	//	this->mDirectionalSet = mDevice.allocateDescriptorSets(ai).at(0);

	//	vk::DescriptorImageInfo imageInfos[] = {
	//		vk::DescriptorImageInfo(nullptr, renderPass.getPosition().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
	//		vk::DescriptorImageInfo(nullptr, renderPass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
	//		vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
	//		vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
	//	};


	//	mDevice.updateDescriptorSets({
	//		vk::WriteDescriptorSet(this->mPointSet, 0, 0,
	//			1,
	//			vk::DescriptorType::eInputAttachment,
	//			&imageInfos[0]),
	//		vk::WriteDescriptorSet(this->mPointSet, 1, 0,
	//			1,
	//			vk::DescriptorType::eInputAttachment,
	//			&imageInfos[1]),
	//		vk::WriteDescriptorSet(this->mPointSet, 2, 0,
	//			1,
	//			vk::DescriptorType::eInputAttachment,
	//			&imageInfos[2])
	//		}, {});
	//	//Perlight descriptor set layout
	//	vk::DescriptorSetLayoutBinding perLightDescLayoutBindings[] =
	//	{
	//		vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //position
	//	};

	//	descLayoutCI.setBindings(perLightDescLayoutBindings);

	//	this->mPointPerDescLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

	//	//Load shaders
	//	auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
	//	auto fragmentBinary = Renderer::compileShaderFromFile("shaders/point.glsl", shaderc_glsl_fragment_shader);

	//	//Create shader modules
	//	vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, fragmentShaderModuleCI{};
	//	vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
	//	vertexShaderModuleCI.setPCode(vertexBinary.data());
	//	fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
	//	fragmentShaderModuleCI.setPCode(fragmentBinary.data());

	//	auto vertexShaderModule = mDevice.createShaderModule(vertexShaderModuleCI);
	//	auto fragmentShaderModule = mDevice.createShaderModule(fragmentShaderModuleCI);

	//	//Create pipeline layout
	//	vk::DescriptorSetLayout setLayouts[] =
	//	{
	//		globalSetLayout, // Slot0
	//		mPointDescLayout, //Slot 1
	//		mPointPerDescLayout //Slot 2
	//	};
	//	vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
	//	pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
	//		.setSetLayouts(setLayouts)
	//		.setPushConstantRangeCount(0)
	//		.setPPushConstantRanges(nullptr);
	//	this->mPointLayout = mDevice.createPipelineLayout(pipelineLayoutCI);

	//	//Create graphics pipeline
	//	vk::PipelineShaderStageCreateInfo shaderStages[] =
	//	{
	//		vk::PipelineShaderStageCreateInfo
	//		{
	//			vk::PipelineShaderStageCreateFlags{},
	//			vk::ShaderStageFlagBits::eVertex,
	//			vertexShaderModule,
	//			"main"
	//		},
	//		vk::PipelineShaderStageCreateInfo
	//		{
	//			vk::PipelineShaderStageCreateFlags{},
	//			vk::ShaderStageFlagBits::eFragment,
	//			fragmentShaderModule,
	//			"main"
	//		}
	//	};


	//	vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
	//	vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{});

	//	vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
	//	inputAssemblyStateCI.setFlags(vk::PipelineInputAssemblyStateCreateFlags{})
	//		.setTopology(vk::PrimitiveTopology::eTriangleList)
	//		.setPrimitiveRestartEnable(false);

	//	vk::Viewport viewport{};
	//	viewport.setMinDepth(0.0f)
	//		.setMaxDepth(1.0f);
	//	vk::Rect2D scissor{};
	//	vk::PipelineViewportStateCreateInfo viewportStateCI{};
	//	viewportStateCI.setFlags(vk::PipelineViewportStateCreateFlags{})
	//		.setScissors(scissor)
	//		.setViewports(viewport);

	//	vk::PipelineRasterizationStateCreateInfo rasterizationStateCI{};
	//	rasterizationStateCI.setDepthClampEnable(false)
	//		.setRasterizerDiscardEnable(false)
	//		.setPolygonMode(vk::PolygonMode::eFill)
	//		.setLineWidth(1.0f)
	//		.setCullMode(vk::CullModeFlagBits::eNone)
	//		.setFrontFace(vk::FrontFace::eCounterClockwise)
	//		.setDepthBiasClamp(0.0f)
	//		.setDepthBiasEnable(false)
	//		.setDepthBiasConstantFactor(0.0f)
	//		.setDepthBiasSlopeFactor(0.0f);

	//	vk::PipelineMultisampleStateCreateInfo multisampleStateCI{};
	//	multisampleStateCI.setRasterizationSamples(vk::SampleCountFlagBits::e1)
	//		.setSampleShadingEnable(false)
	//		.setMinSampleShading(1.0f)
	//		.setPSampleMask(nullptr)
	//		.setAlphaToCoverageEnable(false)
	//		.setAlphaToOneEnable(false);

	//	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState{};
	//	colorBlendAttachmentState
	//		.setColorWriteMask(vk::ColorComponentFlagBits::eR |
	//			vk::ColorComponentFlagBits::eG |
	//			vk::ColorComponentFlagBits::eB |
	//			vk::ColorComponentFlagBits::eA)
	//		.setBlendEnable(true)
	//		.setSrcColorBlendFactor(vk::BlendFactor::eOne)
	//		.setDstColorBlendFactor(vk::BlendFactor::eOne)
	//		.setColorBlendOp(vk::BlendOp::eAdd)
	//		.setSrcAlphaBlendFactor(vk::BlendFactor::eZero)
	//		.setDstAlphaBlendFactor(vk::BlendFactor::eZero)
	//		.setAlphaBlendOp(vk::BlendOp::eAdd);

	//	vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
	//	colorBlendStateCI.setLogicOpEnable(false)
	//		.setLogicOp(vk::LogicOp::eCopy)
	//		.setAttachments(colorBlendAttachmentState)
	//		.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
	//	vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
	//	depthStencilStateCI.setDepthTestEnable(false)
	//		.setDepthWriteEnable(false)
	//		.setDepthCompareOp(vk::CompareOp::eLess)
	//		.setDepthBoundsTestEnable(false)
	//		.setStencilTestEnable(false)
	//		.setFront({})
	//		.setBack({})
	//		.setMinDepthBounds(0.0f)
	//		.setMaxDepthBounds(1.0f);

	//	std::vector<vk::DynamicState> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	//	vk::PipelineDynamicStateCreateInfo dynamicStateCI{};
	//	dynamicStateCI.setDynamicStates(dynamicStates);



	//	vk::GraphicsPipelineCreateInfo pipelineCI{};
	//	pipelineCI.setLayout(this->mPointLayout)
	//		.setRenderPass(renderPass.getRenderPass())
	//		.setSubpass(1)
	//		.setBasePipelineHandle(nullptr)
	//		.setBasePipelineIndex(-1)
	//		.setStages(shaderStages)
	//		.setPVertexInputState(&vertexInputStateCI)
	//		.setBasePipelineHandle(nullptr)
	//		.setBasePipelineIndex(-1)
	//		.setPInputAssemblyState(&inputAssemblyStateCI)
	//		.setPViewportState(&viewportStateCI)
	//		.setPRasterizationState(&rasterizationStateCI)
	//		.setPMultisampleState(&multisampleStateCI)
	//		.setPDepthStencilState(&depthStencilStateCI)
	//		.setPColorBlendState(&colorBlendStateCI)
	//		.setPDynamicState(&dynamicStateCI);

	//	auto pipeLineResult = mDevice.createGraphicsPipeline(nullptr, pipelineCI);
	//	if (pipeLineResult.result != vk::Result::eSuccess)
	//	{
	//		throw std::runtime_error("Failed to create PointLight pipeline !");
	//	}
	//	this->mPointPipeline = pipeLineResult.value;



	//	//Destroy shader modules
	//	mDevice.destroyShaderModule(vertexShaderModule);
	//	mDevice.destroyShaderModule(fragmentShaderModule);
	//}
}