#include <Components.h>
#include <Data.h>
#include <Logger.h>

#include <tiny_gltf.h>
#include <shaderc/shaderc.hpp>
#include <vector>

namespace eg::Components
{
	//Static field
	vk::Pipeline StaticModel::sPipeline;
	vk::PipelineLayout StaticModel::sPipelineLayout;
	vk::DescriptorSetLayout StaticModel::sDescriptorLayout;

	vk::Pipeline StaticModel::sShadowPipeline;
	vk::PipelineLayout StaticModel::sShadowPipelineLayout;

	Command::Var* StaticModel::sRenderScaleCVar;
	Command::Var* StaticModel::sWidthCVar;
	Command::Var* StaticModel::sHeightCVar;

	void StaticModel::create()
	{
		sRenderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");
		sWidthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		sHeightCVar = Command::findVar("eg::Renderer::ScreenHeight");
		Command::registerFn("eg::Renderer::ReloadAllPipelines", [](size_t, char* []) {
			destroy();
			createStaticModelPipeline();
			createStaticModelShadowPipeline();
			});
		createStaticModelPipeline();
		createStaticModelShadowPipeline();
	}
	void StaticModel::destroy()
	{
		vk::Device dv = Renderer::getDevice();
		dv.destroyPipeline(sPipeline);
		dv.destroyPipelineLayout(sPipelineLayout);
		dv.destroyDescriptorSetLayout(sDescriptorLayout);
		dv.destroyPipeline(sShadowPipeline);
		dv.destroyPipelineLayout(sShadowPipelineLayout);
	}


	void StaticModel::createStaticModelPipeline()
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

		sDescriptorLayout = Renderer::getDevice().createDescriptorSetLayout(descLayoutCI);

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
			sDescriptorLayout, //Slot 1
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
			throw std::runtime_error("Failed to create StaticModel pipeline !");
		}
		sPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(geometryShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}
	void StaticModel::createStaticModelShadowPipeline()
	{
		Logger::gTrace("Creating depth only static model renderer !");

		//Load shaders
		auto vertexBinary = Renderer::compileShaderFromFile("shaders/static_model_shadow_vs.glsl", shaderc_glsl_vertex_shader);
		auto geometryBinary = Renderer::compileShaderFromFile("shaders/static_model_shadow_gs.glsl", shaderc_glsl_geometry_shader);
		auto fragmentBinary = Renderer::compileShaderFromFile("shaders/static_model_shadow_fs.glsl", shaderc_glsl_fragment_shader);

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
			Renderer::getGlobalDescriptorSet(), // Slot0,
			Renderer::Atmosphere::getDirectionalDescLayout() // Slot 1
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
			throw std::runtime_error("Failed to create depth only StaticModel pipeline !");
		}
		sShadowPipeline = pipeLineResult.value;



		//Destroy shader modules
		Renderer::getDevice().destroyShaderModule(vertexShaderModule);
		Renderer::getDevice().destroyShaderModule(geometryShaderModule);
		Renderer::getDevice().destroyShaderModule(fragmentShaderModule);
	}


	//Per instance field
	StaticModel::StaticModel(const std::string& filePath) :
		mFilePath(filePath)
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
			this->extractRawMeshes(model);
			this->extractMaterials(model);
		}
		catch (...)
		{
			throw std::runtime_error("StaticModel, Error loading model !");
		}

		Logger::gInfo("Model: " + filePath + " loaded !");
	}

	StaticModel::StaticModel(const std::string& filePath, const tinygltf::Model& model) :
		mFilePath(filePath)
	{
		try
		{
			this->extractRawMeshes(model);
			this->extractMaterials(model);
		}
		catch (...)
		{
			throw std::runtime_error("StaticModel, Error loading model !");
		}
	}

	void StaticModel::render(vk::CommandBuffer cmd,
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

		//Build model matrix
		VertexPushConstant ps{};
		ps.model = worldTransform;
		cmd.pushConstants(sPipelineLayout,
			vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry, 0, sizeof(ps), &ps);
		for (const auto& rawMesh : mRawMeshes)
		{
			cmd.bindVertexBuffers(0, { rawMesh.positionBuffer.getBuffer(), rawMesh.normalBuffer.getBuffer(), rawMesh.uvBuffer.getBuffer() }, { 0, 0, 0 });
			cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
				sPipelineLayout,
				1, { mMaterials.at(rawMesh.materialIndex).mSet }, {});
			cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
		}
	}
	void StaticModel::renderShadow(vk::CommandBuffer cmd,
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

		//Build model matrix
		VertexPushConstant ps{};
		ps.model = worldTransform;
		cmd.pushConstants(sShadowPipelineLayout,
			vk::ShaderStageFlagBits::eVertex, 0, sizeof(ps), &ps);
		for (const auto& rawMesh : mRawMeshes)
		{
			cmd.bindVertexBuffers(0, { rawMesh.positionBuffer.getBuffer() }, { 0, });
			cmd.bindIndexBuffer(rawMesh.indexBuffer.getBuffer(), 0, vk::IndexType::eUint32);
			cmd.drawIndexed(rawMesh.vertexCount, 1, 0, 0, 0);
		}
	}

	void StaticModel::extractRawMeshes(const tinygltf::Model& model)
	{
		std::vector<uint32_t> indices;
		std::vector<glm::vec3> positions, normals;
		std::vector<glm::vec2> uvs;

		mRawMeshes.reserve(model.meshes.size());
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



				RawMesh rawMesh{
					Renderer::GPUBuffer(positions.data(),
					positions.size() * sizeof(glm::vec3),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(normals.data(),
					normals.size() * sizeof(glm::vec3),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(uvs.data(),
					uvs.size() * sizeof(glm::vec2),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(indices.data(), indices.size() * sizeof(uint32_t),
					vk::BufferUsageFlagBits::eIndexBuffer),

					static_cast<uint32_t>(primitive.material),
					static_cast<uint32_t>(indices.size()),
				};

				mRawMeshes.push_back(std::move(rawMesh));
			}


		}
	}

	void StaticModel::extractMaterials(const tinygltf::Model& model)
	{
		//Extract materials
		for (const auto& material : model.materials)
		{
			Material newMaterial{};

			//Load albedo image
			if (material.pbrMetallicRoughness.baseColorTexture.index > -1) {
				if (material.pbrMetallicRoughness.baseColorTexture.index >= this->mImages.size())
				{
					//Load new texture
					auto& texture = model.textures.at(material.pbrMetallicRoughness.baseColorTexture.index);
					auto& image = model.images.at(texture.source);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eBc3UnormBlock,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						(void*)image.image.data(), image.image.size());

					this->mImages.push_back(newImage);
					newMaterial.mAlbedo = newImage;
				}
				else //Texture already in the cache
				{
					newMaterial.mAlbedo = mImages.at(material.pbrMetallicRoughness.baseColorTexture.index);
				}

			}

			//Load normal image
			if (material.normalTexture.index > -1) {
				if (material.normalTexture.index >= this->mImages.size())
				{
					//Load new texture
					auto& texture = model.textures.at(material.normalTexture.index);
					auto& image = model.images.at(texture.source);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eBc3UnormBlock,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						(void*)image.image.data(), image.image.size());

					this->mImages.push_back(newImage);
					newMaterial.mNormal = newImage;
				}
				else //Texture already in the cache
				{
					newMaterial.mNormal = mImages.at(material.normalTexture.index);
				}

			}

			//Load Mr image
			if (material.pbrMetallicRoughness.metallicRoughnessTexture.index > -1) {
				if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= this->mImages.size())
				{
					//Load new texture
					auto& texture = model.textures.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
					auto& image = model.images.at(texture.source);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eBc3UnormBlock,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						(void*)image.image.data(), image.image.size());

					this->mImages.push_back(newImage);
					newMaterial.mMr = newImage;
				}
				else //Texture already in the cache
				{
					newMaterial.mMr = mImages.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
				}

			}


			vk::DescriptorImageInfo baseColorInfo{};
			baseColorInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(newMaterial.mAlbedo ? newMaterial.mAlbedo->getImage().getImageView() : Renderer::getDefaultCheckerboardImage().getImage().getImageView())
				.setSampler(newMaterial.mAlbedo ? newMaterial.mAlbedo->getSampler() : Renderer::getDefaultCheckerboardImage().getSampler());

			vk::DescriptorImageInfo normalInfo{};
			normalInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(newMaterial.mNormal ? newMaterial.mNormal->getImage().getImageView() : Renderer::getDefaultCheckerboardImage().getImage().getImageView())
				.setSampler(newMaterial.mNormal ? newMaterial.mNormal->getSampler() : Renderer::getDefaultCheckerboardImage().getSampler());

			vk::DescriptorImageInfo metallicRoughnessInfo{};
			metallicRoughnessInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(newMaterial.mMr ? newMaterial.mMr->getImage().getImageView() : Renderer::getDefaultCheckerboardImage().getImage().getImageView())
				.setSampler(newMaterial.mMr ? newMaterial.mMr->getSampler() : Renderer::getDefaultCheckerboardImage().getSampler());

			//Load buffer
			Material::UniformBuffer uniformBuffer;
			uniformBuffer.has_albedo = newMaterial.mAlbedo ? 1 : 0;
			uniformBuffer.has_normal = newMaterial.mNormal ? 1 : 0;
			uniformBuffer.has_mr = newMaterial.mMr ? 1 : 0;
			uniformBuffer.albedoColor[0] = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]);
			uniformBuffer.albedoColor[1] = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]);
			uniformBuffer.albedoColor[2] = static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]);

			newMaterial.mUniformBuffer
				= std::make_shared<Renderer::CPUBuffer>(&uniformBuffer, sizeof(Material::UniformBuffer), vk::BufferUsageFlagBits::eUniformBuffer);
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo
				.setOffset(0)
				.setRange(sizeof(Material::UniformBuffer))
				.setBuffer(newMaterial.mUniformBuffer->getBuffer());

			//Allocate descriptor set
			vk::DescriptorSetLayout setLayouts[] =
			{
				sDescriptorLayout
			};
			vk::DescriptorSetAllocateInfo ai{};
			ai.setDescriptorPool(Renderer::getDescriptorPool())
				.setDescriptorSetCount(1)
				.setSetLayouts(setLayouts);
			newMaterial.mSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

			Renderer::getDevice().updateDescriptorSets({
				vk::WriteDescriptorSet(newMaterial.mSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfo, nullptr, nullptr),
				vk::WriteDescriptorSet(newMaterial.mSet, 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &baseColorInfo, nullptr, nullptr, nullptr),
				vk::WriteDescriptorSet(newMaterial.mSet, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &normalInfo, nullptr, nullptr, nullptr),
				vk::WriteDescriptorSet(newMaterial.mSet, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &metallicRoughnessInfo, nullptr, nullptr, nullptr)
				}, {});

			this->mMaterials.push_back(newMaterial);
		}
	}


	StaticModel::~StaticModel()
	{
		for (const auto& material : mMaterials)
		{
			Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), material.mSet);
		}
	}
}