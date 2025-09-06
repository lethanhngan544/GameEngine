#include <Renderer.h>
#include <Core.h>

#include <shaderc/shaderc.h>

namespace eg::Renderer::Postprocessing
{
	struct BloomPS {
		float threshold;
		float knee;
		float rendersScale;
	};

	vk::RenderPass mRenderPass;
	vk::Framebuffer mFramebuffer;
	std::optional<Image2D> mFinalDrawImage, mDrawImage, mBloomImage, mBloomBlurImage, mLuminance;
	vk::Format mDrawImageFormat;
	uint32_t mLuminanceMipLevels = 1;
	vk::ImageView mLuminanceFramebufferView;
	std::optional<CPUBuffer> mLuminanceBuffer;
	float mLuminanceValue;
	vk::Sampler mSampler;

	//Bloom pipeline
	std::string mBloomFragShaderPath = "shaders/postprocess_bloom.glsl";
	vk::Pipeline mBloomPipeline;
	vk::PipelineLayout mBloomLayout;
	vk::DescriptorSetLayout mBloomDescLayout;
	vk::DescriptorSet mBloomSet;

	//Bloom blur
	struct BloomBlurPushConstant
	{
		glm::vec2 direction;	// e.g. (1.0, 0.0) or (0.0, 1.0)
		float radius;			// blur radius in texels
		int32_t pass;			// 0 = horizontal, 1 = vertical
	};
	std::string mBloomBlurFragShaderPath = "shaders/postprocess_bloom_blur.glsl";
	vk::Pipeline mBloomBlurHorizontalPipeline, mBloomBlurVerticalPipeline;
	vk::PipelineLayout mBloomBlurLayout;
	vk::DescriptorSetLayout mBloomBlurDescLayout;
	vk::DescriptorSet mBloomBlurHorizontalSet, mBloomBlurVerticalSet;

	//Compose
	struct ComposePS
	{
		float exposure;
		float saturation;
		float gamma;
	};
	std::string mComposeFragShaderPath = "shaders/postprocess_compose.glsl";
	vk::Pipeline mComposePipeline;
	vk::PipelineLayout mComposeLayout;
	vk::DescriptorSetLayout mComposeDescLayout;
	vk::DescriptorSet mComposeSet;

	Command::Var* mWidthCVar;
	Command::Var* mHeightCVar;
	Command::Var* mRenderScaleCVar;
	Command::Var* mBloomRadiusCVar;
	Command::Var* mBloomThresholdCVar;
	Command::Var* mBloomKneeCvar;
	Command::Var* mExposureCVar;
	Command::Var* mSaturationCVar;
	Command::Var* mGammaCVar;

	void createRenderPass();
	void createBloomPipeline();
	void createBloomBlurPipeline();
	void createComposePipeline();
	void destroyPipeline();


	void create(vk::Format format)
	{
		Command::registerFn("eg::Renderer::ReloadAllPipelines",
			[](size_t, char* []) {
				destroyPipeline();
				createBloomPipeline();
				createBloomBlurPipeline();
				createComposePipeline();
			});

		mWidthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		mHeightCVar = Command::findVar("eg::Renderer::ScreenHeight");
		mRenderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");
		mBloomRadiusCVar = Command::findVar("eg::Renderer::Postprocessing::BloomRadius");
		mBloomRadiusCVar->value = 3.0f;
		mBloomThresholdCVar = Command::findVar("eg::Renderer::Postprocessing::BloomThreshold");
		mBloomThresholdCVar->value = 1.0f;
		mBloomKneeCvar = Command::findVar("eg::Renderer::Postprocessing::BloomKnee");
		mBloomKneeCvar->value = 0.5f;
		mExposureCVar = Command::findVar("eg::Renderer::Postprocessing::Exposure");
		mExposureCVar->value = 1.6f;
		mSaturationCVar = Command::findVar("eg::Renderer::Postprocessing::Saturation");
		mSaturationCVar->value = 1.8f;
		mGammaCVar = Command::findVar("eg::Renderer::Postprocessing::Gamma");
		mGammaCVar->value = 1.0f;


		mDrawImageFormat = format;
		mFinalDrawImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), format,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mDrawImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), format,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mBloomImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), format,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mBloomBlurImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), format,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);	
		mLuminanceMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(mWidthCVar->value, mHeightCVar->value)))) + 1;
		mLuminance.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), vk::Format::eR16Sfloat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment |
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
			vk::ImageAspectFlagBits::eColor, mLuminanceMipLevels);
		vk::ImageViewCreateInfo mipViewCI{};
		mipViewCI.setImage(mLuminance->getImage())
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vk::Format::eR16Sfloat)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		mLuminanceFramebufferView = Renderer::getDevice().createImageView(mipViewCI);
		mLuminanceBuffer.emplace(nullptr, sizeof(uint16_t), vk::BufferUsageFlagBits::eTransferDst);

		vk::SamplerCreateInfo samplerCI{};
		samplerCI.setMagFilter(vk::Filter::eLinear)
			.setMinFilter(vk::Filter::eLinear)
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

		createRenderPass();
		createBloomPipeline();
		createBloomBlurPipeline();
		createComposePipeline();
	}

	void createRenderPass()
	{
		vk::AttachmentDescription attachments[] =
		{
			//Final Draw image, id = 0
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				mDrawImageFormat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal // For copying to swapchain image
			),
			//Draw image, id = 1
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				mDrawImageFormat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal // For copying to swapchain image
			),
			//Bloom image, id = 2
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				mDrawImageFormat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal
			),

			//Bloom blur, id = 3
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				mDrawImageFormat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal
			),
			//Luminance, id = 4
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				vk::Format::eR16Sfloat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferSrcOptimal
			)
		};


		vk::AttachmentReference pass0OutputAttachmentRef[] =
		{
			vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal), //Draw
			vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal), //Bloom
			vk::AttachmentReference(4, vk::ImageLayout::eColorAttachmentOptimal), //Luminance
		};

		vk::AttachmentReference pass1InputAttachmentRef[] =
		{
			vk::AttachmentReference(2, vk::ImageLayout::eShaderReadOnlyOptimal), //Bloom
		};

		vk::AttachmentReference pass1OutputAttachmentRef[] =
		{
			vk::AttachmentReference(3, vk::ImageLayout::eColorAttachmentOptimal), //Bloom blur
		};

		vk::AttachmentReference pass2InputAttachmentRef[] =
		{
			vk::AttachmentReference(3, vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		vk::AttachmentReference pass2OutputAttachmentRef[] =
		{
			vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal),
		};

		vk::AttachmentReference pass3InputAttachmentRef[] =
		{
			vk::AttachmentReference(1, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentReference(2, vk::ImageLayout::eShaderReadOnlyOptimal),
		};

		vk::AttachmentReference pass3OutputAttachmentRef[] =
		{
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
		};

		uint32_t pass1Preserve[] = { 1, 4 }; // preserve scene (1) and luminance (4) while doing blur
		uint32_t pass2Preserve[] = { 1, 4 }; // preserve scene and luminance while doing second blur

		vk::SubpassDescription subpasses[] = {
			//Subpass 0, bloom generation
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				sizeof(pass0OutputAttachmentRef) / sizeof(pass0OutputAttachmentRef[0]), pass0OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				0, nullptr//Preserve
			),

			//Subpass 1, Blur horizontal
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				sizeof(pass1InputAttachmentRef) / sizeof(pass1InputAttachmentRef[0]), pass1InputAttachmentRef, // Input
				sizeof(pass1OutputAttachmentRef) / sizeof(pass1OutputAttachmentRef[0]), pass1OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				sizeof(pass1Preserve) / sizeof(pass1Preserve[0]), pass1Preserve
			),

			//Subpass 2, Blur vertical
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				sizeof(pass2InputAttachmentRef) / sizeof(pass2InputAttachmentRef[0]), pass2InputAttachmentRef, // Input
				sizeof(pass2OutputAttachmentRef) / sizeof(pass2OutputAttachmentRef[0]), pass2OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				sizeof(pass2Preserve) / sizeof(pass2Preserve[0]), pass2Preserve
			),

			//Subpass 3, Composite
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				sizeof(pass3InputAttachmentRef) / sizeof(pass3InputAttachmentRef[0]), pass3InputAttachmentRef, // Input
				sizeof(pass3OutputAttachmentRef) / sizeof(pass3OutputAttachmentRef[0]), pass3OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				0, nullptr//Preserve
			),
		};

		vk::SubpassDependency dependencies[] = {
			// External -> Subpass 0 : ensure prior external work is finished before writing attachments
		   vk::SubpassDependency(
			   VK_SUBPASS_EXTERNAL, 0,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   (vk::AccessFlagBits)0,
			   vk::AccessFlagBits::eColorAttachmentWrite,
			   vk::DependencyFlagBits::eByRegion
		   ),

		   // 0 -> 1 : bloom written -> read for blur
		   vk::SubpassDependency(
			   0, 1,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   vk::PipelineStageFlagBits::eFragmentShader,
			   vk::AccessFlagBits::eColorAttachmentWrite,
			   vk::AccessFlagBits::eInputAttachmentRead,
			   vk::DependencyFlagBits::eByRegion
		   ),

		   // 1 -> 2 : horizontal blur write -> vertical blur read
		   vk::SubpassDependency(
			   1, 2,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   vk::PipelineStageFlagBits::eFragmentShader,
			   vk::AccessFlagBits::eColorAttachmentWrite,
			   vk::AccessFlagBits::eInputAttachmentRead,
			   vk::DependencyFlagBits::eByRegion
		   ),

		   // 2 -> 3 : blurred bloom written -> composite read
		   vk::SubpassDependency(
			   2, 3,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   vk::PipelineStageFlagBits::eFragmentShader,
			   vk::AccessFlagBits::eColorAttachmentWrite,
			   vk::AccessFlagBits::eInputAttachmentRead,
			   vk::DependencyFlagBits::eByRegion
		   ),

		   // Subpass 3 -> External : ensure writes are finished before external ops (present/copy)
		   vk::SubpassDependency(
			   3, VK_SUBPASS_EXTERNAL,
			   vk::PipelineStageFlagBits::eColorAttachmentOutput,
			   vk::PipelineStageFlagBits::eBottomOfPipe,
			   vk::AccessFlagBits::eColorAttachmentWrite,
			   (vk::AccessFlagBits)0,
			   vk::DependencyFlagBits::eByRegion
		   )
		};

		vk::RenderPassCreateInfo renderPassCI{};
		renderPassCI
			.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		mRenderPass = getDevice().createRenderPass(renderPassCI);

		vk::ImageView frameBufferAttachments[] =
		{
			mFinalDrawImage->getImageView(),
			mDrawImage->getImageView(),
			mBloomImage->getImageView(),
			mBloomBlurImage->getImageView(),    
			mLuminanceFramebufferView,
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(static_cast<uint32_t>(mWidthCVar->value))
			.setHeight(static_cast<uint32_t>(mHeightCVar->value))
			.setLayers(1);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);
	}

	void createBloomBlurPipeline()
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}),
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mBloomBlurDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mBloomBlurDescLayout);
		mBloomBlurHorizontalSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);
		mBloomBlurVerticalSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfo[1];
		imageInfo[0] = vk::DescriptorImageInfo(mSampler, mBloomBlurImage->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mBloomBlurHorizontalSet, 0, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				imageInfo),
			}, {});
		imageInfo[0] = vk::DescriptorImageInfo(mSampler, mBloomImage->getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mBloomBlurVerticalSet, 0, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				imageInfo),
			}, {});


		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile(mBloomBlurFragShaderPath.c_str(), shaderc_glsl_fragment_shader);

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
			mBloomBlurDescLayout
		};

		// Push constant for blur direction
		vk::PushConstantRange pushConstantRange{};
		pushConstantRange.setOffset(0)
			.setSize(sizeof(BloomBlurPushConstant))
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRange);
		mBloomBlurLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);


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


		vk::PipelineColorBlendAttachmentState colorBlendAttachmentStates[] = {
			vk::PipelineColorBlendAttachmentState(
				false,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			),
		};


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentStates)
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
		pipelineCI.setLayout(mBloomBlurLayout)
			.setRenderPass(mRenderPass)
			.setSubpass(2)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputStateCI)
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
			throw std::runtime_error("Failed to create mBloomBlurHorizontalPipeline pipeline !");
		}
		mBloomBlurHorizontalPipeline = pipeLineResult.value;

		pipelineCI.setSubpass(1);
		pipeLineResult = Renderer::getDevice().createGraphicsPipeline(nullptr, pipelineCI);
		if (pipeLineResult.result != vk::Result::eSuccess)
		{
			throw std::runtime_error("Failed to create mBloomBlurVerticalPipeline pipeline !");
		}
		mBloomBlurVerticalPipeline = pipeLineResult.value;




		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	void createComposePipeline()
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(
				0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
			vk::DescriptorSetLayoutBinding(
				1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mComposeDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mComposeDescLayout);
		mComposeSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo sceneInputInfo{
			{},  // no sampler
			mDrawImage->getImageView(), // from your render pass attachment 0
			vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::DescriptorImageInfo bloomInputInfo{
			{},  // no sampler
			mBloomImage->getImageView(), // from your render pass attachment 1
			vk::ImageLayout::eShaderReadOnlyOptimal
		};

		vk::WriteDescriptorSet writes[] = {
			vk::WriteDescriptorSet(mComposeSet, 0, 0, 1, vk::DescriptorType::eInputAttachment, &sceneInputInfo),
			vk::WriteDescriptorSet(mComposeSet, 1, 0, 1, vk::DescriptorType::eInputAttachment, &bloomInputInfo),
		};

		Renderer::getDevice().updateDescriptorSets(writes, {});

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile(mComposeFragShaderPath.c_str(), shaderc_glsl_fragment_shader);

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
			mComposeDescLayout //Slot 1
		};

		//Setup push constant range
		vk::PushConstantRange pushConstantRange{};
		pushConstantRange.setOffset(0)
			.setSize(sizeof(ComposePS))
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRange);
		mComposeLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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


		vk::PipelineColorBlendAttachmentState colorBlendAttachmentStates[] = {
			vk::PipelineColorBlendAttachmentState( // for draw image
				false,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			),
		};


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentStates)
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
		pipelineCI.setLayout(mComposeLayout)
			.setRenderPass(mRenderPass)
			.setSubpass(3)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputStateCI)
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
			throw std::runtime_error("Failed to create mComposePipeline pipeline !");
		}
		mComposePipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	
	void createBloomPipeline()
	{
		//Define shader layout
		vk::DescriptorSetLayoutBinding descLayoutBindings[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //Draw image from default render pass
		};
		vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
		descLayoutCI.setBindings(descLayoutBindings);

		mBloomDescLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

		//Allocate descriptor set right here
		vk::DescriptorSetAllocateInfo ai{};
		ai.setDescriptorPool(Renderer::getDescriptorPool())
			.setDescriptorSetCount(1)
			.setSetLayouts(mBloomDescLayout);
		mBloomSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

		vk::DescriptorImageInfo imageInfos[] = {
			vk::DescriptorImageInfo(mSampler, DefaultRenderPass::getDrawImage().getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal),
		};


		Renderer::getDevice().updateDescriptorSets({
			vk::WriteDescriptorSet(mBloomSet, 0, 0,
				1,
				vk::DescriptorType::eCombinedImageSampler,
				&imageInfos[0]),
			}, {});



		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/fullscreen_quad.glsl", shaderc_glsl_vertex_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile(mBloomFragShaderPath.c_str(), shaderc_glsl_fragment_shader);

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
			mBloomDescLayout //Slot 1
		};

		//Setup push constant range
		vk::PushConstantRange pushConstantRange{};
		pushConstantRange.setOffset(0)
			.setSize(sizeof(BloomPS))
			.setStageFlags(vk::ShaderStageFlagBits::eFragment);

		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRange);

		mBloomLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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


		vk::PipelineColorBlendAttachmentState colorBlendAttachmentStates[] = {
			vk::PipelineColorBlendAttachmentState( // for draw image
				true,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			),

			vk::PipelineColorBlendAttachmentState( // for bloom image
				true,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			),
			vk::PipelineColorBlendAttachmentState( // for bloom image
				true,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::BlendFactor::eOne, vk::BlendFactor::eOne, vk::BlendOp::eAdd,
				vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
			),
		};


		vk::PipelineColorBlendStateCreateInfo colorBlendStateCI{};
		colorBlendStateCI.setLogicOpEnable(false)
			.setLogicOp(vk::LogicOp::eCopy)
			.setAttachments(colorBlendAttachmentStates)
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
		pipelineCI.setLayout(mBloomLayout)
			.setRenderPass(mRenderPass)
			.setSubpass(0)
			.setBasePipelineHandle(nullptr)
			.setBasePipelineIndex(-1)
			.setStages(shaderStages)
			.setPVertexInputState(&vertexInputStateCI)
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
		mBloomPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	void compose(const vk::CommandBuffer& cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mComposePipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mComposeLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mComposeSet },
			{}
		);
		float width = static_cast<float>(mWidthCVar->value);
		float height = static_cast<float>(mHeightCVar->value);
		float renderScale = static_cast<float>(mRenderScaleCVar->value);

		ComposePS pushConstant{};
		pushConstant.exposure = 0.1f / (mLuminanceValue + 1e-4f) + static_cast<float>(mExposureCVar->value);
		pushConstant.saturation = static_cast<float>(mSaturationCVar->value);
		pushConstant.gamma = static_cast<float>(mGammaCVar->value);

		cmd.pushConstants(mComposeLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(ComposePS), &pushConstant);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, width, height,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }));
		cmd.draw(3, 1, 0, 0);
	}

	void destroyPipeline()
	{
		//Destroy bloom pipeline

		getDevice().destroyPipeline(mBloomPipeline);
		getDevice().destroyPipelineLayout(mBloomLayout);
		getDevice().destroyDescriptorSetLayout(mBloomDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mBloomSet);

		//Destroy bloom blur pipeline
		getDevice().destroyPipeline(mBloomBlurHorizontalPipeline);
		getDevice().destroyPipeline(mBloomBlurVerticalPipeline);
		getDevice().destroyPipelineLayout(mBloomBlurLayout);
		getDevice().destroyDescriptorSetLayout(mBloomBlurDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mBloomBlurHorizontalSet);
		getDevice().freeDescriptorSets(getDescriptorPool(), mBloomBlurVerticalSet);
		//Destroy compose pipeline
		getDevice().destroyPipeline(mComposePipeline);
		getDevice().destroyPipelineLayout(mComposeLayout);
		getDevice().destroyDescriptorSetLayout(mComposeDescLayout);
		getDevice().freeDescriptorSets(getDescriptorPool(), mComposeSet);

	}
	void generateBloomBlur(const vk::CommandBuffer& cmd, bool vertical)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, vertical ?  mBloomBlurVerticalPipeline : mBloomBlurHorizontalPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mBloomBlurLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), vertical ? mBloomBlurVerticalSet : mBloomBlurHorizontalSet },
			{}
		);
		float width = static_cast<float>(mWidthCVar->value);
		float height = static_cast<float>(mHeightCVar->value);
		float renderScale = static_cast<float>(mRenderScaleCVar->value);
		/*mPushConstants.renderScale = renderScale;
		mPushConstants.scaledWidth = static_cast<int>(width * renderScale);
		mPushConstants.scaledHeight = static_cast<int>(height * renderScale);
		cmd.pushConstants(mLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPushConstants), &mPushConstants);*/

		BloomBlurPushConstant pushConstant{};
		pushConstant.direction.x = vertical ? 0.0f : 1.0f;
		pushConstant.direction.y = vertical ? 1.0f : 0.0f;
		pushConstant.pass = vertical ? 0 : 1;
		pushConstant.radius = static_cast<float>(mBloomRadiusCVar->value);

		cmd.pushConstants(mBloomBlurLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomBlurPushConstant), &pushConstant);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, width, height,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }));
		cmd.draw(3, 1, 0, 0);

	}
	void generateBloom(const vk::CommandBuffer& cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, mBloomPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			mBloomLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), mBloomSet },
			{}
		);

		float width = static_cast<float>(mWidthCVar->value);
		float height = static_cast<float>(mHeightCVar->value);
		float renderScale = static_cast<float>(mRenderScaleCVar->value);


		BloomPS pushConstant{};
		pushConstant.threshold = static_cast<float>(mBloomThresholdCVar->value);
		pushConstant.knee = static_cast<float>(mBloomKneeCvar->value);
		pushConstant.rendersScale = static_cast<float>(mRenderScaleCVar->value);
		/*mPushConstants.renderScale = renderScale;
		mPushConstants.scaledWidth = static_cast<int>(width * renderScale);
		mPushConstants.scaledHeight = static_cast<int>(height * renderScale);

		cmd.pushConstants(mLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(FragmentPushConstants), &mPushConstants);*/

		cmd.pushConstants(mBloomLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(BloomPS), &pushConstant);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, width, height,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(width), static_cast<uint32_t>(height) }));
		cmd.draw(3, 1, 0, 0);
	}

	

	
	void destroy()
	{
		destroyPipeline();
		//Free sampler
		Renderer::getDevice().destroySampler(mSampler);
		Renderer::getDevice().destroyImageView(mLuminanceFramebufferView);
		mFinalDrawImage.reset();
		mLuminanceBuffer.reset();
		mDrawImage.reset();
		mBloomImage.reset();
		mBloomBlurImage.reset();
		mLuminance.reset();
		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}

	void resize(uint32_t width, uint32_t height)
	{
		//Destroy framebuffer, images
		getDevice().destroyFramebuffer(mFramebuffer);
		mFinalDrawImage.reset();
		mDrawImage.reset();
		mBloomImage.reset();
		mBloomBlurImage.reset();
		mLuminance.reset();
		//Recreate images


		mFinalDrawImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), mDrawImageFormat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mDrawImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), mDrawImageFormat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mBloomImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), mDrawImageFormat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mBloomBlurImage.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), mDrawImageFormat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mLuminanceMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		mLuminance.emplace(static_cast<uint32_t>(mWidthCVar->value), static_cast<uint32_t>(mHeightCVar->value), vk::Format::eR16Sfloat,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eInputAttachment |
			vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
			vk::ImageAspectFlagBits::eColor,
			mLuminanceMipLevels);
		vk::ImageViewCreateInfo mipViewCI{};
		mipViewCI.setImage(mLuminance->getImage())
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(vk::Format::eR16Sfloat)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		mLuminanceFramebufferView = Renderer::getDevice().createImageView(mipViewCI);

		vk::ImageView frameBufferAttachments[] =
		{
			mFinalDrawImage->getImageView(),
			mDrawImage->getImageView(),
			mBloomImage->getImageView(),
			mBloomBlurImage->getImageView(),
			mLuminanceFramebufferView,
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(width)
			.setHeight(height)
			.setLayers(1);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);
	}



	void begin(const vk::CommandBuffer& cmd)
	{
		vk::ClearValue clearValues[] =
		{
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
		};

		uint32_t width = static_cast<uint32_t>(mWidthCVar->value);
		uint32_t height = static_cast<uint32_t>(mHeightCVar->value);

		vk::RenderPassBeginInfo renderPassBI{};
		renderPassBI.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)))
			.setClearValues(clearValues);


		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eInline);
	}

	void processLuminance(const vk::CommandBuffer& cmd, float delta)
	{
		// Transition luminance image other mip to transfer dst
		vk::ImageMemoryBarrier otherMipsBarrier(
			{}, vk::AccessFlagBits::eTransferWrite,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			mLuminance->getImage(),
			{ vk::ImageAspectFlagBits::eColor, 1, mLuminanceMipLevels - 1, 0, 1 }
		);

		cmd.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{}, nullptr, nullptr, otherMipsBarrier
		);

		//Downsample to first mip level
		int32_t mipWidth = static_cast<int32_t>(mWidthCVar->value);
		int32_t mipHeight = static_cast<int32_t>(mHeightCVar->value);
		
		for (uint32_t i = 1; i < mLuminanceMipLevels; i++)
		{
			vk::ImageBlit blit{};
			blit.setSrcSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, static_cast<uint32_t>(i - 1), 0, 1 })
				.setSrcOffsets({ vk::Offset3D{0, 0, 0},
					vk::Offset3D{ static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1 } });
			mipWidth = std::max(1, mipWidth / 2);
			mipHeight = std::max(1, mipHeight / 2);
			blit.setDstSubresource(vk::ImageSubresourceLayers{ vk::ImageAspectFlagBits::eColor, static_cast<uint32_t>(i), 0, 1 })
				.setDstOffsets({ vk::Offset3D{0, 0, 0},
					vk::Offset3D{ static_cast<int32_t>(mipWidth), static_cast<int32_t>(mipHeight), 1 } });
			cmd.blitImage(
				mLuminance->getImage(), vk::ImageLayout::eTransferSrcOptimal,
				mLuminance->getImage(), vk::ImageLayout::eTransferDstOptimal,
				{ blit },
				vk::Filter::eLinear
			);

			// Transition previous mip to src layout for the next blit
			vk::ImageMemoryBarrier mipBarrier(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eTransferRead,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eTransferSrcOptimal,
				VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
				mLuminance->getImage(),
				{ vk::ImageAspectFlagBits::eColor, i, 1, 0, 1 }
			);

			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eTransfer,
				{}, nullptr, nullptr, mipBarrier
			);
		}
		vk::BufferImageCopy copyRegion{};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0; // tightly packed
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copyRegion.imageSubresource.mipLevel = mLuminanceMipLevels - 1;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset = vk::Offset3D{ 0, 0, 0 };
		copyRegion.imageExtent = vk::Extent3D{ 1, 1, 1 };

		cmd.copyImageToBuffer(
			mLuminance->getImage(),
			vk::ImageLayout::eTransferSrcOptimal,
			mLuminanceBuffer->getBuffer(),
			copyRegion
		);

		auto* data = reinterpret_cast<uint16_t*>(mLuminanceBuffer->getInfo().pMappedData);
		auto halfToFloat = [](uint16_t h) {
			uint32_t sign = (h & 0x8000) << 16;
			uint32_t exponent = (h >> 10) & 0x1F;
			uint32_t mantissa = h & 0x3FF;

			uint32_t bits;
			if (exponent == 0) {
				if (mantissa == 0) {
					bits = sign; // Zero
				}
				else {
					// Subnormal -> normalize
					exponent = 1;
					while ((mantissa & 0x400) == 0) {
						mantissa <<= 1;
						exponent--;
					}
					mantissa &= 0x3FF;
					bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
				}
			}
			else if (exponent == 31) {
				bits = sign | 0x7F800000 | (mantissa << 13); // Inf/NaN
			}
			else {
				bits = sign | ((exponent + 112) << 23) | (mantissa << 13);
			}

			float f;
			std::memcpy(&f, &bits, sizeof(f));
			return f;
		};
		auto value = halfToFloat(*data);
		mLuminanceValue = glm::mix(mLuminanceValue, value, delta * 5.0f); // smooth
	}

	vk::RenderPass getRenderPass() { return mRenderPass; }
	vk::Framebuffer getFramebuffer() { return mFramebuffer; }
	Image2D& getFinalDrawImage() { return *mFinalDrawImage; }
	Image2D& getDrawImage() { return *mDrawImage; }
	Image2D& getBloomImage() { return *mBloomImage; }
	Image2D& getBloomBlurImage() { return *mBloomBlurImage; }
}