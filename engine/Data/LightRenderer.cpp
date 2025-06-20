#include <Data.h>

#include <shaderc/shaderc.hpp>

namespace eg::Data::LightRenderer
{
	vk::Pipeline mAmbientPipeline;
	vk::PipelineLayout mAmbientLayout;
	vk::DescriptorSetLayout mAmbientDescLayout;
	vk::DescriptorSet mAmbientSet;

	vk::Pipeline mPointPipeline;
	vk::PipelineLayout mPointLayout;
	vk::DescriptorSetLayout mPointDescLayout;
	vk::DescriptorSetLayout mPointPerDescLayout; //Per light data
	vk::DescriptorSet mPointSet;


	vk::Pipeline mDirectionalPipeline;
	vk::PipelineLayout mDirectionalLayout;
	vk::DescriptorSetLayout mDirectionalDescLayout;
	vk::DescriptorSetLayout mDirectionalPerDescLayout;
	vk::DescriptorSet mDirectionalSet;


	void createAmbientPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout);
	void createPointPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout);
	void createDirectionalPipeline(const Renderer::DefaultRenderPass& renderPass, const Renderer::ShadowRenderPass& shadowPass, vk::DescriptorSetLayout globalSetLayout);

	void create()
	{
		createAmbientPipeline(Renderer::getDefaultRenderPass(), Renderer::getGlobalDescriptorSet());
		createPointPipeline(Renderer::getDefaultRenderPass(), Renderer::getGlobalDescriptorSet());
		createDirectionalPipeline(Renderer::getDefaultRenderPass(), Renderer::getShadowRenderPass(), Renderer::getGlobalDescriptorSet());
	}
	void destroy()
	{
		Renderer::getDevice().destroyPipeline(mAmbientPipeline);
		Renderer::getDevice().destroyPipelineLayout(mAmbientLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(mAmbientDescLayout);
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mAmbientSet);

		Renderer::getDevice().destroyPipeline(mPointPipeline);
		Renderer::getDevice().destroyPipelineLayout(mPointLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(mPointDescLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(mPointPerDescLayout);
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mPointSet);

		Renderer::getDevice().destroyPipeline(mDirectionalPipeline);
		Renderer::getDevice().destroyPipelineLayout(mDirectionalLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(mDirectionalDescLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(mDirectionalPerDescLayout);
		Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), mDirectionalSet);
		

	}

	void renderDirectionalLight(vk::CommandBuffer cmd, const Components::DirectionalLight& light)
	{
		//Update uniform buffer
		
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mDirectionalPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mDirectionalLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mDirectionalSet, light.getSet()},
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getDrawExtent().extent.width),
			static_cast<float>(Renderer::getDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getDrawExtent());
		cmd.draw(3, 1, 0, 0);
	}

	void renderAmbient(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mAmbientPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mAmbientLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mAmbientSet },
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getDrawExtent().extent.width),
			static_cast<float>(Renderer::getDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getDrawExtent());
		cmd.draw(3, 1, 0, 0);
	}


	void beginPointLight(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPointPipeline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mPointLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mPointSet },
			{}
		);

		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getDrawExtent().extent.width),
			static_cast<float>(Renderer::getDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getDrawExtent());
	}

	void renderPointLight(vk::CommandBuffer cmd,
		const Components::PointLight& pointLight)
	{

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mPointLayout,
			2,
			{ pointLight.getDescriptorSet() },
			{}
		);
		cmd.draw(3, 1, 0, 0);

	}

	vk::DescriptorSetLayout getPointLightPerDescLayout()
	{
		return mPointPerDescLayout;
	}

	vk::DescriptorSetLayout getDirectionalPerDescLayout()
	{
		return mDirectionalPerDescLayout;
	}


	void createAmbientPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //mr
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
			vk::DescriptorImageInfo(nullptr, renderPass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

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
	void LightRenderer::createPointPipeline(const Renderer::DefaultRenderPass& renderPass, vk::DescriptorSetLayout globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Mr
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth
		};

		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mPointDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mPointDescLayout);
		mPointSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, renderPass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};


		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mPointSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0]),
			vk::WriteDescriptorSet(mPointSet, 1, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[1]),
			vk::WriteDescriptorSet(mPointSet, 2, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[2]),
			vk::WriteDescriptorSet(mPointSet, 3, 0,
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

		mPointPerDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/point.glsl", shaderc_glsl_fragment_shader);

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
			mPointDescLayout, //Slot 1
			mPointPerDescLayout //Slot 2
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRangeCount(0)
			.setPPushConstantRanges(nullptr);
		mPointLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
		pipelineCI.setLayout(mPointLayout)
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

		auto pipeLineResult = Renderer::getDevice().createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create PointLight pipeline !");
		}
		mPointPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	void createDirectionalPipeline(const Renderer::DefaultRenderPass& renderPass, const Renderer::ShadowRenderPass& shadowPass, vk::DescriptorSetLayout globalSetLayout)
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Mr
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth
		};

		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mDirectionalDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Per light
		vk::DescriptorSetLayoutBinding descLayoutBindings2[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //UniformBuffer

		};
		descLayoutCI.setBindings(descLayoutBindings2);
		mDirectionalPerDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);


		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mDirectionalDescLayout);
		mDirectionalSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, renderPass.getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getAlbedo().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(nullptr, renderPass.getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(shadowPass.getDepthSampler(), shadowPass.getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

	


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
				&imageInfos[4])
			},

		{});
	
		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/directional.glsl", shaderc_glsl_fragment_shader);

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
			mDirectionalPerDescLayout //Slot 2
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
		pipelineCI.setLayout(mDirectionalLayout)
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
	
}