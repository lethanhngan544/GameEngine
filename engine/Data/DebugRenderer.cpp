#include <Data.h>

#include <shaderc/shaderc.hpp>
#include <Renderer.h>

namespace eg::Data::DebugRenderer
{
	constexpr size_t MAX_LINE_COUNT = 65536 * 2;
	std::vector<VertexFormat> gLineVertices;
	std::optional<Renderer::GPUBuffer> gLineVertexBuffer;
	std::optional<Renderer::CPUBuffer> gLineStagingVertexBuffer;
	
	vk::PipelineLayout gLinePipelineLayout;
	vk::Pipeline gLinePipeline;
	vk::DescriptorSetLayout gLineSetLayout;
	vk::DescriptorSet gLineSet;
	void create()
	{
		Logger::gTrace("Creating debug renderer !");

		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		gLineSetLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(gLineSetLayout);
		gLineSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(nullptr, Renderer::getDefaultRenderPass().getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(gLineSet, 0, 0,
				1,
				vk::DescriptorType::eInputAttachment,
				&imageInfos[0])
			}, {});

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/debug_line_vs.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/debug_line_fs.glsl", shaderc_glsl_fragment_shader);

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
			gLineSetLayout, // Slot1
		};

		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts);

		gLinePipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(VertexFormat, color)),
		};

		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{})
			.setVertexAttributeDescriptions(vertexAttributeDescriptions)
			.setVertexBindingDescriptions(vertexBindingDescriptions);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
		inputAssemblyStateCI.setFlags(vk::PipelineInputAssemblyStateCreateFlags{})
			.setTopology(vk::PrimitiveTopology::eLineList)
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
				vk::ColorComponentFlagBits::eB
			),
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
		pipelineCI.setLayout(gLinePipelineLayout)
			.setRenderPass(Renderer::getDefaultRenderPass().getRenderPass())
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
			throw std::runtime_error("Failed to create StaticModel pipeline !");
		}
		gLinePipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);


		//Allocate vertex buffer
		gLineVertexBuffer.emplace(nullptr, sizeof(VertexFormat) * MAX_LINE_COUNT, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst);
		gLineStagingVertexBuffer.emplace(nullptr, sizeof(VertexFormat) * MAX_LINE_COUNT, vk::BufferUsageFlagBits::eTransferSrc);
	}

	void destroy()
	{
		vk::Device dv = Renderer::getDevice();
		dv.destroyPipeline(gLinePipeline);
		dv.destroyPipelineLayout(gLinePipelineLayout);
		gLineVertexBuffer.reset();
		gLineStagingVertexBuffer.reset();
	}

	void recordLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color)
	{
		if (gLineVertices.size() >= MAX_LINE_COUNT)
		{
			return;
		}
		gLineVertices.push_back({ start, color });
		gLineVertices.push_back({ end, color });
	}

	void updateVertexBuffers(vk::CommandBuffer cmd)
	{
		Renderer::immediateSubmit([&](vk::CommandBuffer cmd)
		{
			gLineStagingVertexBuffer->write(gLineVertices.data(), sizeof(VertexFormat) * gLineVertices.size());

			vk::BufferCopy copyRegion{};
			copyRegion.setSize(sizeof(VertexFormat) * gLineVertices.size());
			cmd.copyBuffer(gLineStagingVertexBuffer->getBuffer(), gLineVertexBuffer->getBuffer(), copyRegion);
		});
	}
	void render(vk::CommandBuffer cmd)
	{
		if (gLineVertices.size() > 0)
		{
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				gLinePipelineLayout,
				0,
				{ Renderer::getCurrentFrameGUBODescSet(), gLineSet },
			{});
			cmd.bindVertexBuffers(0, { gLineVertexBuffer->getBuffer() }, { 0 });

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, gLinePipeline);
			
			cmd.draw(gLineVertices.size(), 1, 0, 0);
		}
		gLineVertices.clear();
	}
}