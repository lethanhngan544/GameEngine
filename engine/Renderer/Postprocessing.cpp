#include <Renderer.h>
#include <Core.h>

#include <shaderc/shaderc.h>

namespace eg::Renderer::Postprocessing
{
	vk::Sampler mSampler;
	vk::Pipeline mPipeline;
	vk::PipelineLayout mLayout;
	vk::DescriptorSetLayout mDescLayout;
	vk::DescriptorSet mSet;
	FragmentPushConstants mPushConstants;

	Command::Var* mWidthCVar;
	Command::Var* mHeightCVar;
	Command::Var* mRenderScaleCVar;

	void createPipeline();
	void destroyPipeline();

	void create()
	{
		Command::registerFn("eg::Renderer::ReloadAllPipelines",
			[](size_t, char* []) {
				destroyPipeline();
				createPipeline();
			});

		mWidthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		mHeightCVar = Command::findVar("eg::Renderer::ScreenHeight");
		mRenderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");

		//Create sampler

		vk::SamplerCreateInfo samplerCI{};
		samplerCI.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
			.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(1)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways);
		mSampler = Renderer::getDevice().createSampler(samplerCI);

		createPipeline();
	}
	void destroy()
	{
		destroyPipeline();
		//Free sampler
		Renderer::getDevice().destroySampler(mSampler);
	}
	FragmentPushConstants& getPushConstants()
	{
		return mPushConstants;
	}
	void createPipeline()
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}),//Draw image from default render pass
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Previous frame image
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Depth image from default render pass
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Normal image from default render pass
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}) //MetalicRoughness image from default render pass
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here

		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mDescLayout);
		mSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(mSampler, DefaultRenderPass::getDrawImage().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSampler, getPostprocessingRenderPass().getPrevDrawImage().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSampler, DefaultRenderPass::getDepth().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSampler, DefaultRenderPass::getNormal().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::DescriptorImageInfo(mSampler, DefaultRenderPass::getMr().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};


		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mSet, 0, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[0]),

			vk::WriteDescriptorSet(mSet, 1, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[1]),
			vk::WriteDescriptorSet(mSet, 2, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[2]),

			vk::WriteDescriptorSet(mSet, 3, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[3]),
			vk::WriteDescriptorSet(mSet, 4, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[4]),
			}, {});



		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/postprocessing_test.glsl", shaderc_glsl_fragment_shader);

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
			getGlobalDescriptorSet(), // Slot0
			mDescLayout //Slot 1
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange{vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPushConstants)} //Render scale
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRangeCount(0)
			.setPushConstantRanges(pushConstantRanges);
		mLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
		pipelineCI.setLayout(mLayout)
			.setRenderPass(getPostprocessingRenderPass().getRenderPass())
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
			throw std::runtime_error("Failed to create AmbientLight pipeline !");
		}
		mPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
	void destroyPipeline()
	{
		getDevice().destroyPipeline(mPipeline);
		getDevice().destroyPipelineLayout(mLayout);
		getDevice().destroyDescriptorSetLayout(mDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mSet);
	}
	void render(const vk::CommandBuffer& cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mSet },
			{}
		);

		float width = static_cast<float>(mWidthCVar->value);
		float height = static_cast<float>(mHeightCVar->value);
		float renderScale = static_cast<float>(mRenderScaleCVar->value);

		mPushConstants.renderScale = renderScale;
		mPushConstants.scaledWidth = static_cast<int>(width * renderScale);
		mPushConstants.scaledHeight = static_cast<int>(height * renderScale);

		cmd.pushConstants(mLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPushConstants), &mPushConstants);


		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, width, height,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }));
		cmd.draw(3, 1, 0, 0);
	}
}