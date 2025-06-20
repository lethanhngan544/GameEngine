#include <Data.h>

#include <Renderer.h>
#include <shaderc/shaderc.hpp>


#define SUBPASS_INDEX 0

namespace eg::Data::StaticModelRenderer
{

	vk::Pipeline mPipeline;
	vk::PipelineLayout mPipelineLayout;
	vk::DescriptorSetLayout mDescriptorLayout;

	vk::Pipeline mShadowPipeline;
	vk::PipelineLayout mShadowPipelineLayout;

	static void createStaticModelPipeline();
	static void createStaticModelShadowPipeline();
	

	void create()
	{
		createStaticModelPipeline();
		createStaticModelShadowPipeline();
	}
	void destroy()
	{
		vk::Device dv = Renderer::getDevice();
		dv.destroyPipeline(mPipeline);
		dv.destroyPipelineLayout(mPipelineLayout);
		dv.destroyDescriptorSetLayout(mDescriptorLayout);
		dv.destroyPipeline(mShadowPipeline);
		dv.destroyPipelineLayout(mShadowPipelineLayout);
	}

	void begin(vk::CommandBuffer cmd)
	{

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mPipelineLayout,
			0,
			Renderer::getCurrentFrameGUBODescSet(),
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getDrawExtent().extent.width),
			static_cast<float>(Renderer::getDrawExtent().extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, Renderer::getDrawExtent());



	}

	void render(vk::CommandBuffer cmd,
		const Components::StaticModel& model,
		glm::mat4x4 worldTransform)
	{
		//Build model matrix
		VertexPushConstant ps{};
		ps.model = worldTransform;
		cmd.pushConstants(mPipelineLayout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry, 0, sizeof(ps), &ps);
		for (const auto& rawMesh : model.getRawMeshes())
		{
			cmd.bindVertexBuffers(0, { rawMesh.positionBuffer.getBuffer(), rawMesh.normalBuffer.getBuffer(), rawMesh.uvBuffer.getBuffer() }, { 0, 0, 0 });
			cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				mPipelineLayout,
				1, { model.getMaterials().at(rawMesh.materialIndex).mSet }, {});
			cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
		}
	}

	void beginShadow(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mShadowPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mShadowPipelineLayout,
			0,
			Renderer::getCurrentFrameGUBODescSet(),
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::getShadowMapResolution()),
			static_cast<float>(Renderer::getShadowMapResolution()),
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0),
			 vk::Extent2D(Renderer::getShadowMapResolution(), Renderer::getShadowMapResolution())));

	}

	void renderShadow(vk::CommandBuffer cmd,
		const Components::StaticModel& model,
		glm::mat4x4 worldTransform)
	{
		//Build model matrix
		VertexPushConstant ps{};
		ps.model = worldTransform;
		cmd.pushConstants(mShadowPipelineLayout,
			vk::ShaderStageFlagBits::eVertex , 0, sizeof(ps), &ps);
		for (const auto& rawMesh : model.getRawMeshes())
		{
			cmd.bindVertexBuffers(0, { rawMesh.positionBuffer.getBuffer() }, { 0, });
			cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
			cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
		}
	}

	vk::DescriptorSetLayout getMaterialSetLayout()
	{
		return mDescriptorLayout;
	}

	void createStaticModelPipeline()
	{
		Logger::gTrace("Creating static model renderer !");
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //uniform buffer
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //albedo
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //normal
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}) //mr
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mDescriptorLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/static_model_vs.glsl", shaderc_glsl_vertex_shader);
		auto geometryBinary = Renderer::compileShaderFromFile("shaders/static_model_gs.glsl", shaderc_glsl_geometry_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/static_model_fs.glsl", shaderc_glsl_fragment_shader);

		//Create shader modules
		vk::ShaderModuleCreateInfo vertexShaderModuleCI{}, geometryShaderModuleCI{}, fragmentShaderModuleCI{};
		vertexShaderModuleCI.setCodeSize(vertexBinary.size() * sizeof(uint32_t));
		vertexShaderModuleCI.setPCode(vertexBinary.data());

		geometryShaderModuleCI.setCodeSize(geometryBinary.size() * sizeof(uint32_t));
		geometryShaderModuleCI.setPCode(geometryBinary.data());

		fragmentShaderModuleCI.setCodeSize(fragmentBinary.size() * sizeof(uint32_t));
		fragmentShaderModuleCI.setPCode(fragmentBinary.data());

		auto vertexShaderModule = Renderer::getDevice().createShaderModule(vertexShaderModuleCI);
		auto geometryShaderModule = Renderer::getDevice().createShaderModule(geometryShaderModuleCI);
		auto fragmentShaderModule = Renderer::getDevice().createShaderModule(fragmentShaderModuleCI);


		//Create pipeline layout
		vk::DescriptorSetLayout setLayouts[] =
		{
			Renderer::getGlobalDescriptorSet(), // Slot0
			mDescriptorLayout, //Slot 1
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry, 0, sizeof(VertexPushConstant))
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRanges);

		mPipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
				vk::ShaderStageFlagBits::eGeometry,
				geometryShaderModule,
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

		//Create vertex layout
		vk::VertexInputBindingDescription vertexBindingDescriptions[] = {
			vk::VertexInputBindingDescription(0, sizeof(float) * 3, vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(1, sizeof(float) * 3, vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(2, sizeof(float) * 2, vk::VertexInputRate::eVertex)
		};
		vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(2, 2, vk::Format::eR32G32Sfloat, 0)
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{})
			.setVertexAttributeDescriptions(vertexAttributeDescriptions)
			.setVertexBindingDescriptions(vertexBindingDescriptions);

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
			.setCullMode(vk::CullModeFlagBits::eBack)
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

		vk::PipelineColorBlendAttachmentState attachments[] = {
			//Normal
			vk::PipelineColorBlendAttachmentState(false,
				vk::BlendFactor::eOne,
				vk::BlendFactor::eZero,
				vk::BlendOp::eAdd,
				vk::BlendFactor::eOne,
				vk::BlendFactor::eOne,
				vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR |
				vk::ColorComponentFlagBits::eG |
				vk::ColorComponentFlagBits::eB |
				vk::ColorComponentFlagBits::eA),

				//Albedo
				vk::PipelineColorBlendAttachmentState(false,
					vk::BlendFactor::eOne,
					vk::BlendFactor::eZero,
					vk::BlendOp::eAdd,
					vk::BlendFactor::eOne,
					vk::BlendFactor::eOne,
					vk::BlendOp::eAdd,
					vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG |
					vk::ColorComponentFlagBits::eB |
					vk::ColorComponentFlagBits::eA),

				//Mr
				vk::PipelineColorBlendAttachmentState(false,
					vk::BlendFactor::eOne,
					vk::BlendFactor::eZero,
					vk::BlendOp::eAdd,
					vk::BlendFactor::eOne,
					vk::BlendFactor::eOne,
					vk::BlendOp::eAdd,
					vk::ColorComponentFlagBits::eR |
					vk::ColorComponentFlagBits::eG |
					vk::ColorComponentFlagBits::eB |
					vk::ColorComponentFlagBits::eA)
		};


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(attachments)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
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
		pipelineCI.setLayout(mPipelineLayout)
			.setRenderPass(Renderer::getDefaultRenderPass().getRenderPass())
			.setSubpass(0)
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
			throw std::runtime_error("Failed to create StaticModel pipeline !");
		}
		mPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(geometryShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
	void createStaticModelShadowPipeline()
	{
		Logger::gTrace("Creating depth only static model renderer !");

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/static_model_shadow_vs.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/static_model_shadow_fs.glsl", shaderc_glsl_fragment_shader);

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
			Renderer::getGlobalDescriptorSet(), // Slot0
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertexPushConstant))
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRanges);

		mShadowPipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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

		//Create vertex layout
		vk::VertexInputBindingDescription vertexBindingDescriptions[] = {
			vk::VertexInputBindingDescription(0, sizeof(float) * 3, vk::VertexInputRate::eVertex),
		};
		vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{})
			.setVertexAttributeDescriptions(vertexAttributeDescriptions)
			.setVertexBindingDescriptions(vertexBindingDescriptions);

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
			.setCullMode(vk::CullModeFlagBits::eFront)
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

		//vk::PipelineColorBlendAttachmentState attachments[] = {
		//	//Normal
		//	vk::PipelineColorBlendAttachmentState(false,
		//		vk::BlendFactor::eOne,
		//		vk::BlendFactor::eZero,
		//		vk::BlendOp::eAdd,
		//		vk::BlendFactor::eOne,
		//		vk::BlendFactor::eOne,
		//		vk::BlendOp::eAdd,
		//		vk::ColorComponentFlagBits::eR |
		//		vk::ColorComponentFlagBits::eG |
		//		vk::ColorComponentFlagBits::eB |
		//		vk::ColorComponentFlagBits::eA),

		//		//Albedo
		//		vk::PipelineColorBlendAttachmentState(false,
		//			vk::BlendFactor::eOne,
		//			vk::BlendFactor::eZero,
		//			vk::BlendOp::eAdd,
		//			vk::BlendFactor::eOne,
		//			vk::BlendFactor::eOne,
		//			vk::BlendOp::eAdd,
		//			vk::ColorComponentFlagBits::eR |
		//			vk::ColorComponentFlagBits::eG |
		//			vk::ColorComponentFlagBits::eB |
		//			vk::ColorComponentFlagBits::eA),

		//		//Mr
		//		vk::PipelineColorBlendAttachmentState(false,
		//			vk::BlendFactor::eOne,
		//			vk::BlendFactor::eZero,
		//			vk::BlendOp::eAdd,
		//			vk::BlendFactor::eOne,
		//			vk::BlendFactor::eOne,
		//			vk::BlendOp::eAdd,
		//			vk::ColorComponentFlagBits::eR |
		//			vk::ColorComponentFlagBits::eG |
		//			vk::ColorComponentFlagBits::eB |
		//			vk::ColorComponentFlagBits::eA)
		//};


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });
		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
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
		pipelineCI.setLayout(mShadowPipelineLayout)
			.setRenderPass(Renderer::getShadowRenderPass().getRenderPass())
			.setSubpass(0)
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
			throw std::runtime_error("Failed to create depth only StaticModel pipeline !");
		}
		mShadowPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
}