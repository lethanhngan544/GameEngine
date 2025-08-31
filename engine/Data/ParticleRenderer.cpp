#include <Data.h>
#include <Core.h>

#include <shaderc/shaderc.hpp>

#include <Data.h>

#include <shaderc/shaderc.hpp>

namespace eg::Data::ParticleRenderer
{
	static constexpr uint32_t MAX_PARTICLES = 10000; // Max particles to render

	struct ParticleVertex {
		glm::vec2 position;
		glm::vec2 uv;
	};
	std::vector<ParticleVertex> particleVertices = {
		{{-0.5f, -0.5f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.5f}, {1.0f, 0.0f}},
		{{-0.5f,  0.5f}, {0.0f, 1.0f}},
		{{ 0.5f,  0.5f}, {1.0f, 1.0f}},
	};

	struct ParticleAtlas
	{
		glm::uvec2 size; // Size of the atlas
		std::vector<Components::ParticleInstance> instances;
		std::optional<Renderer::GPUBuffer> buffer;
	};


	std::unordered_map<VkDescriptorSet, ParticleAtlas> gParticleMap;

	vk::Pipeline gPipeline;
	vk::PipelineLayout gPipelineLayout;
	vk::DescriptorSetLayout gDescLayout;
	std::optional<Renderer::GPUBuffer> gVertexBuffer;
	std::optional<Renderer::CPUBuffer> gInstanceBufferCPU;

	Command::Var* mRenderScaleCVar;
	Command::Var* mWidthCVar;
	Command::Var* mHeightCVar;


	void createPipeline();

	void create()
	{
		mRenderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");
		mWidthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		mHeightCVar = Command::findVar("eg::Renderer::ScreenHeight");

		Command::registerFn("eg::Renderer::ReloadAllPipelines", [](size_t, char* []) {
			Renderer::getDevice().destroyPipeline(gPipeline);
			Renderer::getDevice().destroyPipelineLayout(gPipelineLayout);
			Renderer::getDevice().destroyDescriptorSetLayout(gDescLayout);
			createPipeline();
		});

		gVertexBuffer.emplace(particleVertices.data(), particleVertices.size() * sizeof(ParticleVertex), vk::BufferUsageFlagBits::eVertexBuffer);
		gInstanceBufferCPU.emplace(nullptr, sizeof(Components::ParticleInstance) * MAX_PARTICLES, vk::BufferUsageFlagBits::eTransferSrc);
		createPipeline();
	}


	void recordParticle(const Components::ParticleInstance instance, vk::DescriptorSet textureAtlas, glm::uvec2 atlasSize)
	{
		gParticleMap[textureAtlas].size = atlasSize;
		gParticleMap[textureAtlas].instances.push_back(instance);
	}


	vk::DescriptorSetLayout getTextureAtlasDescLayout()
	{
		return gDescLayout;
	}

	void updateBuffers()
	{
		for (auto& [set, atlas] : gParticleMap)
		{
			if (!atlas.buffer.has_value())
			{
				atlas.buffer.emplace(nullptr,
					sizeof(Components::ParticleInstance) * MAX_PARTICLES,
					vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);
			}

			//Update buffer
			if (atlas.instances.size() == 0)
			{
				continue;
			}
			if (atlas.instances.size() > MAX_PARTICLES)
			{
				atlas.instances.resize(MAX_PARTICLES);
			}
			if (atlas.instances.size() > 0)
			{
				Renderer::immediateSubmit([&](vk::CommandBuffer cmd)
					{
						gInstanceBufferCPU->write(atlas.instances.data(), atlas.instances.size() * sizeof(Components::ParticleInstance));

						vk::BufferCopy copyRegion{};
						copyRegion.setSize(sizeof(Components::ParticleInstance) * atlas.instances.size());
						cmd.copyBuffer(gInstanceBufferCPU->getBuffer(), atlas.buffer->getBuffer(), copyRegion);
					});
			}
		}	
	}
	void render(vk::CommandBuffer cmd)
	{
		
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, gPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			gPipelineLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet() },
			{}
		);
		float scaledWidth = static_cast<float>(mWidthCVar->value * mRenderScaleCVar->value);
		float scaledHeight = static_cast<float>(mHeightCVar->value * mRenderScaleCVar->value);

		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, scaledWidth, scaledHeight,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(scaledWidth), static_cast<uint32_t>(scaledHeight) }));
		

		for (auto& [set, atlas] : gParticleMap)
		{
			VertexPushConstant ps;
			ps.atlasSize = atlas.size;
			cmd.pushConstants(gPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertexPushConstant), &ps);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				gPipelineLayout,
				1,
				{ set },
				{}
			);

			cmd.bindVertexBuffers(0, { gVertexBuffer->getBuffer(), atlas.buffer->getBuffer() }, { 0, 0 });
			cmd.draw(4, static_cast<uint32_t>(atlas.instances.size()), 0, 0);
		}


		for (auto& [set, atlas] : gParticleMap)
		{
			atlas.instances.clear();
		}
	}
	
	void destroy()
	{
		Renderer::getDevice().destroyPipeline(gPipeline);
		Renderer::getDevice().destroyPipelineLayout(gPipelineLayout);
		Renderer::getDevice().destroyDescriptorSetLayout(gDescLayout);
		gVertexBuffer.reset();
		gInstanceBufferCPU.reset();
		gParticleMap.clear();
	}


	void createPipeline()
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Texture atlas
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		gDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);


		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/particle_vs.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/particle_fs.glsl", shaderc_glsl_fragment_shader);

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
			gDescLayout, // Slot 1
		};

		//Push constant
		vk::PushConstantRange pushConstantRange{};
		pushConstantRange.setOffset(0)
			.setSize(sizeof(VertexPushConstant))
			.setStageFlags(vk::ShaderStageFlagBits::eVertex);
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRange);

		gPipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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

		vk::VertexInputBindingDescription vertexInputBindingDescriptions[] = {
			vk::VertexInputBindingDescription(0, sizeof(ParticleVertex), vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(1, sizeof(Components::ParticleInstance), vk::VertexInputRate::eInstance),
		};

		vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(ParticleVertex, position)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(ParticleVertex, uv)),
			vk::VertexInputAttributeDescription(2, 1, vk::Format::eR32G32B32A32Sfloat, offsetof(Components::ParticleInstance, positionSize)),
			vk::VertexInputAttributeDescription(3, 1, vk::Format::eR32G32Uint, offsetof(Components::ParticleInstance, frameIndex)),
		};



		vk::PipelineVertexInputStateCreateInfo vertexInputStateCI{};
		vertexInputStateCI.setFlags(vk::PipelineVertexInputStateCreateFlags{})
			.setVertexBindingDescriptions(vertexInputBindingDescriptions)
			.setVertexAttributeDescriptions(vertexInputAttributeDescriptions);

		vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI{};
		inputAssemblyStateCI.setFlags(vk::PipelineInputAssemblyStateCreateFlags{})
			.setTopology(vk::PrimitiveTopology::eTriangleStrip)
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
		depthStencilStateCI.setDepthTestEnable(true)
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
		pipelineCI.setLayout(gPipelineLayout)
			.setRenderPass(Renderer::DefaultRenderPass::getRenderPass())
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
		gPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
}