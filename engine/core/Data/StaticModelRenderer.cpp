#include <Data.h>

#include <Renderer.h>
#include <shaderc/shaderc.hpp>


namespace eg::Data
{
	StaticModelRenderer::StaticModelRenderer(vk::Device device,
		vk::RenderPass pass,
		uint32_t subpassIndex,
		vk::DescriptorSetLayout globalDescriptorSetLayout) :
		mDevice(device)
	{
		Logger::gTrace("Creating static model renderer !");
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}) //albedo
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		this->mDescriptorLayout = mDevice.createDescriptorSetLayout(descLayoutCI);

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/static_model_vs.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/static_model_fs.glsl", shaderc_glsl_fragment_shader);

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
			globalDescriptorSetLayout, // Slot0
			mDescriptorLayout, //Slot 1
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertexPushConstant))
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRanges);

		mPipelineLayout = mDevice.createPipelineLayout(pipelineLayoutCI);

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
			vk::VertexInputBindingDescription(0, sizeof(VertexFormat), vk::VertexInputRate::eVertex)
		};
		vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexFormat, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexFormat, normal)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(VertexFormat, uv))
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

		vk::PipelineColorBlendAttachmentState attachments[] = {

			//Position
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
			.setRenderPass(pass)
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

		auto pipeLineResult = mDevice.createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create StaticModel pipeline !");
		}
		this->mPipeline = pipeLineResult.value;



		//Destroy shader modules
		mDevice.destroyShaderModule(vertexShaderModule);
		mDevice.destroyShaderModule(fragmentShaderModule);
	}
	StaticModelRenderer::~StaticModelRenderer()
	{
		mDevice.destroyPipeline(mPipeline);
		mDevice.destroyPipelineLayout(mPipelineLayout);
		mDevice.destroyDescriptorSetLayout(mDescriptorLayout);
	}

	void StaticModelRenderer::render(vk::CommandBuffer cmd,
		vk::Rect2D drawExtent,
		vk::DescriptorSet globalSet,
		const Entity* filteredEntities, size_t entityCount) const
	{
		if (entityCount <= 0) return;

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mPipelineLayout,
			0,
			globalSet,
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(drawExtent.extent.width),
			static_cast<float>(drawExtent.extent.height),
			0.0f, 1.0f } });
		cmd.setScissor(0, drawExtent);

		for (size_t i = 0; i < entityCount; i++)
		{
			const Entity& e = filteredEntities[i];
			if (!e.has<StaticModel>()) continue;

			//Build model matrix
			VertexPushConstant ps{};
			ps.model = glm::mat4(1.0f);
			ps.model = glm::scale(ps.model, e.mScale);
			ps.model *= glm::mat4_cast(e.mRotation);
			ps.model = glm::translate(ps.model, e.mPosition);

			const StaticModel& model = e.get<StaticModel>();
			cmd.pushConstants(mPipelineLayout,
				vk::ShaderStageFlagBits::eVertex, 0, sizeof(ps), &ps);
			for (const auto& rawMesh : model.getRawMeshes())
			{
				cmd.bindVertexBuffers(0, { rawMesh.vertexBuffer.getBuffer() }, { 0 });
				cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
					mPipelineLayout,
					1, { model.getMaterials().at(rawMesh.materialIndex).mSet }, {});
				cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
			}
		}
	}
}