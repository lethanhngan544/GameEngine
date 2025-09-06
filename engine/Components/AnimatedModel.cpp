#include <Components.h>
#include <Data.h>
#include <string>
#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>

namespace eg::Components
{
	vk::Pipeline AnimatedModel::sPipeline;
	vk::PipelineLayout AnimatedModel::sPipelineLayout;
	vk::DescriptorSetLayout AnimatedModel::sMaterialLayout;
	vk::DescriptorSetLayout AnimatedModel::sBoneLayout;
	vk::Pipeline AnimatedModel::sShadowPipeline;
	vk::PipelineLayout AnimatedModel::sShadowPipelineLayout;
	Command::Var* AnimatedModel::sRenderScaleCVar;
	Command::Var* AnimatedModel::sWidthCVar;
	Command::Var* AnimatedModel::sHeightCVar;

	void AnimatedModel::create()
	{
		sRenderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");
		sWidthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		sHeightCVar = Command::findVar("eg::Renderer::ScreenHeight");

		Command::registerFn("eg::Renderer::ReloadAllPipelines", [](size_t, char* []) {
			destroy();
			createPipeline();
			createShadowPipeline();
			});

		createPipeline();
		createShadowPipeline();
	}
	void AnimatedModel::destroy()
	{
		vk::Device dv = Renderer::getDevice();
		dv.destroyPipeline(sPipeline);
		dv.destroyPipelineLayout(sPipelineLayout);
		dv.destroyDescriptorSetLayout(sMaterialLayout);
		dv.destroyDescriptorSetLayout(sBoneLayout);
		dv.destroyPipeline(sShadowPipeline);
		dv.destroyPipelineLayout(sShadowPipelineLayout);
	}
	void AnimatedModel::createPipeline()
	{
		Logger::gTrace("Creating animated model renderer !");
		//Define shader layout
		{

			vk::DescriptorSetLayoutBinding descLayoutBindings[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment, {}), //uniform buffer
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //albedo
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}), //normal
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, {}) //mr
			};
			vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
			descLayoutCI.setBindings(descLayoutBindings);

			sMaterialLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);
		}

		//Create bone layout
		{
			vk::DescriptorSetLayoutBinding descLayoutBindings[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex, {}), //uniform buffer
			};
			vk::DescriptorSetLayoutCreateInfo descLayoutCI{};
			descLayoutCI.setBindings(descLayoutBindings);

			sBoneLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);
		}


		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/animated_model_vs.glsl", shaderc_glsl_vertex_shader,
			{ std::make_pair("MAX_BONES", std::to_string(Components::AnimatedModel::MAX_BONE_COUNT)) });
		auto geometryBinary = Renderer::compileShaderFromFile("shaders/animated_model_gs.glsl", shaderc_glsl_geometry_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/animated_model_fs.glsl", shaderc_glsl_fragment_shader);

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
			sMaterialLayout, //Slot 1
			sBoneLayout, //Slot 2
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry, 0, sizeof(VertexPushConstant))
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRanges);

		sPipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
			vk::VertexInputBindingDescription(2, sizeof(float) * 2, vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(3, sizeof(uint32_t) * 4, vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(4, sizeof(float) * 4, vk::VertexInputRate::eVertex)
		};
		vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(2, 2, vk::Format::eR32G32Sfloat, 0),
			vk::VertexInputAttributeDescription(3, 3, vk::Format::eR32G32B32A32Sint, 0),
			vk::VertexInputAttributeDescription(4, 4, vk::Format::eR32G32B32A32Sfloat, 0)
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

		vk::StencilOpState stencilOpState = {};
		stencilOpState.setFailOp(vk::StencilOp::eReplace)
			.setPassOp(vk::StencilOp::eReplace)
			.setDepthFailOp(vk::StencilOp::eReplace)
			.setCompareOp(vk::CompareOp::eAlways)
			.setCompareMask(0xFF)
			.setWriteMask(0xFF)
			.setReference(Renderer::MESH_STENCIL_VALUE);

		vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI{};
		depthStencilStateCI.setDepthTestEnable(true)
			.setDepthWriteEnable(true)
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
		pipelineCI.setLayout(sPipelineLayout)
			.setRenderPass(Renderer::DefaultRenderPass::getRenderPass())
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
			throw std::runtime_error("Failed to create AnimatedModel pipeline !");
		}
		sPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(geometryShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
	void AnimatedModel::createShadowPipeline()
	{
		Logger::gTrace("Creating depth only animated model renderer !");

		//Load shaders
		std::vector<std::pair<std::string, std::string>> defines;
		defines.emplace_back("MAX_BONES", std::to_string(Components::AnimatedModel::MAX_BONE_COUNT));

		auto vertexBinary = Renderer::compileShaderFromFile("shaders/animated_model_shadow_vs.glsl", shaderc_glsl_vertex_shader, defines);
		auto geometryBinary = Renderer::compileShaderFromFile("shaders/animated_model_shadow_gs.glsl", shaderc_glsl_geometry_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/animated_model_shadow_fs.glsl", shaderc_glsl_fragment_shader);

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
			Renderer::Atmosphere::getDirectionalDescLayout(), // Slot 1
			sBoneLayout, //Slot 2
		};
		vk::PushConstantRange pushConstantRanges[] =
		{
			vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(VertexPushConstant))
		};
		vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
		pipelineLayoutCI.setFlags(vk::PipelineLayoutCreateFlags{})
			.setSetLayouts(setLayouts)
			.setPushConstantRanges(pushConstantRanges);

		sShadowPipelineLayout = Renderer::getDevice().createPipelineLayout(pipelineLayoutCI);

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
			vk::VertexInputBindingDescription(1, sizeof(uint32_t) * 4, vk::VertexInputRate::eVertex),
			vk::VertexInputBindingDescription(2, sizeof(float) * 4, vk::VertexInputRate::eVertex),
		};
		vk::VertexInputAttributeDescription vertexAttributeDescriptions[] = {
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
			vk::VertexInputAttributeDescription(1, 1, vk::Format::eR32G32B32A32Sint, 0),
			vk::VertexInputAttributeDescription(2, 2, vk::Format::eR32G32B32A32Sfloat, 0)
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
		pipelineCI.setLayout(sShadowPipelineLayout)
			.setRenderPass(Renderer::Atmosphere::getRenderPass())
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
			throw std::runtime_error("Failed to create depth only AnimatedModel pipeline !");
		}
		sShadowPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(geometryShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}

	AnimatedModel::AnimatedModel(const std::string& filePath) :
		StaticModel()
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		loader.SetImageLoader(Data::LoadImageData, nullptr);
		std::string err;
		std::string warn;
		bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filePath);
		if (!warn.empty()) {
			Logger::gWarn(warn);
		}

		if (!err.empty()) {
			Logger::gError(err);
		}

		if (!ret) {
			throw std::runtime_error("Failed to load model: " + filePath);
		}

		try
		{
			this->extractNodes(model);
			this->extractMaterials(model);
			this->extractedAnimatedRawMeshes(model);
			this->extractSkins(model);
		}
		catch (...)
		{
			throw std::runtime_error("StaticModel, Error loading model !");
		}

		Logger::gInfo("Animated model: " + filePath + " loaded !");
	}

	AnimatedModel::~AnimatedModel()
	{

	}

	void AnimatedModel::render(vk::CommandBuffer cmd,
		const Animator& animator,
		glm::mat4x4 worldTransform)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, sPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			sPipelineLayout,
			0,
			Renderer::getCurrentFrameGUBODescSet(),
			{}
		);

		float scaledWidth = static_cast<float>(sWidthCVar->value * sRenderScaleCVar->value);
		float scaledHeight = static_cast<float>(sHeightCVar->value * sRenderScaleCVar->value);

		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f, scaledWidth, scaledHeight,
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D({ 0, 0 }, { static_cast<uint32_t>(scaledWidth), static_cast<uint32_t>(scaledHeight) }));

		using Node = Components::Animator::AnimationNode;
		using NodeVec = std::vector<std::shared_ptr<Node>>;


		//Build current node worlds transform using lambdas
		std::function<void(const NodeVec& nodes,
			const std::shared_ptr<Node>& currentNode,
			glm::mat4x4 accumulatedTransform)> renderScreneGraph
			= [&](const NodeVec& nodes,
				const std::shared_ptr<Node>& currentNode,
				glm::mat4x4 accumulatedTransform)
			{
				glm::mat4x4 localTransform = glm::mat4x4(1.0f);
				localTransform = glm::translate(localTransform, currentNode->position);
				localTransform *= glm::mat4_cast(currentNode->rotation);
				localTransform = glm::scale(localTransform, currentNode->scale);
				accumulatedTransform *= localTransform;

				if (currentNode->meshIndex >= 0)
				{
					//Build model matrix	
					VertexPushConstant ps{};
					ps.model = accumulatedTransform;
					cmd.pushConstants(sPipelineLayout,
						vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry, 0, sizeof(ps), &ps);
					const auto& rawMesh = mAnimatedRawMeshes.at(currentNode->meshIndex);
					cmd.bindVertexBuffers(0, {
						rawMesh.positionBuffer.getBuffer(),
						rawMesh.normalBuffer.getBuffer(),
						rawMesh.uvBuffer.getBuffer(),
						rawMesh.boneIdsBuffer.getBuffer(),
						rawMesh.boneWeightsBuffer.getBuffer()
						}, { 0, 0, 0, 0, 0 });
					cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
						sPipelineLayout,
						1, { mMaterials.at(rawMesh.materialIndex).mSet, animator.getDescriptorSet() }, {});
					cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
				}


				for (const auto& child : currentNode->children)
				{
					renderScreneGraph(nodes, nodes.at(child), accumulatedTransform);
				}
			};
		renderScreneGraph(
			animator.getAnimationNodes(),
			animator.getAnimationNodes().at(mRootNodeIndex),
			worldTransform);
	}
	void AnimatedModel::renderShadow(vk::CommandBuffer cmd,
		const Animator& animator,
		glm::mat4x4 worldTransform)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, sShadowPipeline);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
			sShadowPipelineLayout,
			0,
			{ Renderer::getCurrentFrameGUBODescSet(), Renderer::Atmosphere::getDirectionalSet() },
			{}
		);
		cmd.setViewport(0, { vk::Viewport{ 0.0f, 0.0f,
			static_cast<float>(Renderer::Atmosphere::getShadowMapSize()),
			static_cast<float>(Renderer::Atmosphere::getShadowMapSize()),
			0.0f, 1.0f } });
		cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0),
			vk::Extent2D(Renderer::Atmosphere::getShadowMapSize(), Renderer::Atmosphere::getShadowMapSize())));

		using Node = Components::Animator::AnimationNode;
		using NodeVec = std::vector<std::shared_ptr<Node>>;

		//Build current node worlds transform using lambdas
		std::function<void( const NodeVec& nodes,
			const std::shared_ptr<Node>& currentNode,
			glm::mat4x4 accumulatedTransform)> renderScreneGraph
			= [&](const NodeVec& nodes,
				const std::shared_ptr<Node>& currentNode,
				glm::mat4x4 accumulatedTransform)
			{
				glm::mat4x4 localTransform = glm::mat4x4(1.0f);
				localTransform = glm::translate(localTransform, currentNode->position);
				localTransform *= glm::mat4_cast(currentNode->rotation);
				localTransform = glm::scale(localTransform, currentNode->scale);
				accumulatedTransform *= localTransform;

				if (currentNode->meshIndex >= 0)
				{
					//Build model matrix	
					VertexPushConstant ps{};
					ps.model = accumulatedTransform;
					cmd.pushConstants(sShadowPipelineLayout,
						vk::ShaderStageFlagBits::eVertex, 0, sizeof(ps), &ps);
					const auto& rawMesh = mAnimatedRawMeshes.at(currentNode->meshIndex);
					cmd.bindVertexBuffers(0, {
						rawMesh.positionBuffer.getBuffer(),
						rawMesh.boneIdsBuffer.getBuffer(),
						rawMesh.boneWeightsBuffer.getBuffer()
						}, { 0, 0, 0 });
					cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
						sShadowPipelineLayout,
						2, { animator.getDescriptorSet() }, {});
					cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
				}


				for (const auto& child : currentNode->children)
				{
					renderScreneGraph(nodes, nodes.at(child), accumulatedTransform);
				}
			};
		renderScreneGraph(animator.getAnimationNodes(),
			animator.getAnimationNodes().at(mRootNodeIndex),
			worldTransform);
	}


	void AnimatedModel::extractNodes(const tinygltf::Model& model)
	{
		//Get root node index
		mRootNodeIndex = model.scenes.at(model.defaultScene).nodes.at(0);
		//Extract node data
		mNodes.reserve(model.nodes.size());
		for (const auto& gltfNode : model.nodes)
		{
			glm::vec3 translation = glm::vec3(0.0f);
			glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			glm::vec3 scale = glm::vec3(1.0f);
			//Check if the node has model matrix
			if (gltfNode.matrix.size() > 0 && gltfNode.matrix.size() == 16)
			{
				glm::mat4x4 modelMatrix = glm::mat4x4(1.0f);
				std::memcpy(&modelMatrix[0], gltfNode.matrix.data(), sizeof(float) * 16);
				//Extract translation, rotation and scale from the model matrix
				translation = glm::vec3(modelMatrix[3][0], modelMatrix[3][1], modelMatrix[3][2]);
				rotation = glm::quat_cast(modelMatrix);
				scale = glm::vec3(glm::length(glm::vec3(modelMatrix[0][0], modelMatrix[0][1], modelMatrix[0][2])),
					glm::length(glm::vec3(modelMatrix[1][0], modelMatrix[1][1], modelMatrix[1][2])),
					glm::length(glm::vec3(modelMatrix[2][0], modelMatrix[2][1], modelMatrix[2][2])));

			}
			else
			{
				//Extract translation, rotation and scale	
				if (gltfNode.translation.size() == 3)
				{
					translation = glm::vec3(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
				}
				if (gltfNode.rotation.size() == 4)
				{
					rotation = glm::quat(gltfNode.rotation[3], gltfNode.rotation[0], gltfNode.rotation[1], gltfNode.rotation[2]);
				}
				if (gltfNode.scale.size() == 3)
				{
					scale = glm::vec3(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
				}

			}

			mNodes.push_back(Node{
				gltfNode.name,
				translation, rotation, scale,
				gltfNode.mesh,
				gltfNode.children
				});
		}

	}

	void AnimatedModel::extractSkins(const tinygltf::Model& model)
	{
		//Extract skin
		mSkins.reserve(model.skins.size());
		for (const auto& gltfSkin : model.skins)
		{
			Skin skin;
			skin.name = gltfSkin.name;
			skin.rootNode = gltfSkin.skeleton;

			if (gltfSkin.joints.size() > MAX_BONE_COUNT)
			{
				throw std::runtime_error("Skin has more than " + std::to_string(MAX_BONE_COUNT) + " joints, which is not supported.");
			}
			//Extract joints
			skin.joints.reserve(gltfSkin.joints.size());
			for (const auto& joint : gltfSkin.joints)
			{
				skin.joints.push_back(joint);
			}
			//Extract inverse bind matrices
			const auto& accessor = model.accessors.at(gltfSkin.inverseBindMatrices);
			const auto& bufferView = model.bufferViews.at(accessor.bufferView);
			const auto& buffer = model.buffers.at(bufferView.buffer);
			skin.inverseBindMatrices.reserve(accessor.count);
			for (size_t i = 0; i < accessor.count; i++)
			{
				glm::mat4x4 matrix;
				std::memcpy(glm::value_ptr(matrix), buffer.data.data() + bufferView.byteOffset + i * sizeof(glm::mat4x4), sizeof(glm::mat4x4));
				skin.inverseBindMatrices.push_back(matrix);
			}
			mSkins.push_back(std::move(skin));
		}
	}

	void AnimatedModel::extractedAnimatedRawMeshes(const tinygltf::Model& model)
	{
		std::vector<uint32_t> indices;
		std::vector<glm::vec3> positions, normals;
		std::vector<glm::vec2> uvs;
		std::vector<glm::ivec4> boneIds;
		std::vector<glm::vec4> boneWeights;

		mAnimatedRawMeshes.reserve(model.meshes.size());
		for (const auto& mesh : model.meshes)
		{
			for (const auto& primitive : mesh.primitives)
			{
				if (primitive.material < 0)
					throw std::runtime_error("Mesh must have a material !");
				indices.clear();
				positions.clear();
				normals.clear();
				uvs.clear();
				boneIds.clear();
				boneWeights.clear();

				//Index buffer
				{
					uint32_t accessorIndex = primitive.indices;
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);


					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						std::vector<uint16_t> tempIndices;
						tempIndices.resize(accessor.count);
						std::memcpy(tempIndices.data(), (void*)(buffer.data.data() + bufferView.byteOffset), accessor.count * sizeof(uint16_t));
						indices.reserve(indices.size() + tempIndices.size());
						for (const auto& index : tempIndices)
						{
							indices.push_back(index);
						}
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					{
						std::vector<uint32_t> tempIndices;
						tempIndices.resize(accessor.count);
						std::memcpy(tempIndices.data(), (void*)(buffer.data.data() + bufferView.byteOffset), accessor.count * sizeof(uint32_t));
						indices.reserve(indices.size() + tempIndices.size());
						for (const auto& index : tempIndices)
						{
							indices.push_back(index);
						}
						break;
					}
					}

				}

				//Positions
				{
					uint32_t positionAccessorIndex = primitive.attributes.at("POSITION");
					const auto& positionAccessor = model.accessors.at(positionAccessorIndex);
					const auto& positionBufferView = model.bufferViews.at(positionAccessor.bufferView);
					const auto& positionBuffer = model.buffers.at(positionBufferView.buffer);

					positions.resize(positionAccessor.count);
					std::memcpy(positions.data(),
						positionBuffer.data.data() + positionBufferView.byteOffset,
						sizeof(glm::vec3) * positionAccessor.count);
				}

				//Normals
				{
					uint32_t accessorIndex = primitive.attributes.at("NORMAL");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					normals.resize(accessor.count);
					std::memcpy(normals.data(),
						buffer.data.data() + bufferView.byteOffset,
						sizeof(glm::vec3) * accessor.count);
				}

				//texture coords
				{
					uint32_t accessorIndex = primitive.attributes.at("TEXCOORD_0");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					uvs.resize(accessor.count);
					std::memcpy(uvs.data(), buffer.data.data() + bufferView.byteOffset, sizeof(glm::vec2) * accessor.count);
				}

				//Bone ids
				{
					uint32_t accessorIndex = primitive.attributes.at("JOINTS_0");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					boneIds.resize(accessor.count);
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
					{
						std::vector<uint8_t> tempBoneIds;
						tempBoneIds.resize(accessor.count * 4); // 4 bone ids per vertex
						std::memcpy(tempBoneIds.data(), buffer.data.data() + bufferView.byteOffset, accessor.count * sizeof(uint8_t) * 4);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							boneIds[i] = glm::ivec4(
								tempBoneIds[i * 4 + 0],
								tempBoneIds[i * 4 + 1],
								tempBoneIds[i * 4 + 2],
								tempBoneIds[i * 4 + 3]);
						}
						break;
					}
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					{
						std::vector<uint16_t> tempBoneIds;
						tempBoneIds.resize(accessor.count * 4); // 4 bone ids per vertex
						std::memcpy(tempBoneIds.data(), buffer.data.data() + bufferView.byteOffset, accessor.count * sizeof(uint16_t) * 4);
						for (size_t i = 0; i < accessor.count; ++i)
						{
							boneIds[i] = glm::ivec4(
								tempBoneIds[i * 4 + 0],
								tempBoneIds[i * 4 + 1],
								tempBoneIds[i * 4 + 2],
								tempBoneIds[i * 4 + 3]);
						}
						break;
					}
					}
				}

				//Bone weights
				{
					uint32_t accessorIndex = primitive.attributes.at("WEIGHTS_0");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					boneWeights.resize(accessor.count);
					std::memcpy(boneWeights.data(), buffer.data.data() + bufferView.byteOffset, sizeof(glm::vec4) * accessor.count);
				}



				AnimatedRawMesh rawMesh{
					Renderer::GPUBuffer(positions.data(),
					positions.size() * sizeof(glm::vec3),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(normals.data(),
					normals.size() * sizeof(glm::vec3),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(uvs.data(),
					uvs.size() * sizeof(glm::vec2),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(boneIds.data(),
					boneIds.size() * sizeof(glm::ivec4), //(uint32_t, uint32_t, uint32_t, uint32_t)
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(boneWeights.data(),
					boneWeights.size() * sizeof(glm::vec4), //(uint32_t, uint32_t, uint32_t, uint32_t)
					vk::BufferUsageFlagBits::eVertexBuffer),


					Renderer::GPUBuffer(indices.data(), indices.size() * sizeof(uint32_t),
					vk::BufferUsageFlagBits::eIndexBuffer),

					static_cast<uint32_t>(primitive.material),
					static_cast<uint32_t>(indices.size()),
				};

				mAnimatedRawMeshes.push_back(std::move(rawMesh));
			}
		}
	}
}
