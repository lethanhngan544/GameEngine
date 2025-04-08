#include <Data.h>
#include <Logger.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <vector>


namespace eg::Data
{
	StaticModel::StaticModel(const std::string& filePath)
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
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

		//Extract 

		//Extract vertex datas
		//mRawMeshes.reserve(model.meshes.size());
		std::vector<StaticModelRenderer::VertexFormat> vertices;
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
				vertices.clear();
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

				for (size_t i = 0; i < positions.size(); i++)
				{
					vertices.push_back({ positions.at(i), normals.at(i), uvs.at(i) });
				}

				RawMesh rawMesh{
					Renderer::GPUBuffer(vertices.data(),
					vertices.size() * sizeof(StaticModelRenderer::VertexFormat),
					vk::BufferUsageFlagBits::eVertexBuffer),

					Renderer::GPUBuffer(indices.data(), indices.size() * sizeof(uint32_t),
					vk::BufferUsageFlagBits::eIndexBuffer),

					static_cast<uint32_t>(primitive.material),
					static_cast<uint32_t>(indices.size()),
				};

				mRawMeshes.push_back(std::move(rawMesh));
			}


		}

		//Extract materials
		for (const auto& material : model.materials)
		{
			Material newMaterial{};

			
			//Load albedo image
			if (material.pbrMetallicRoughness.baseColorTexture.index > -1) {
				if (material.pbrMetallicRoughness.baseColorTexture.index >= this->mImages.size())
				{
					//Load new texture
					auto& image = model.images.at(material.pbrMetallicRoughness.baseColorTexture.index);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eR8G8B8A8Srgb,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						image.image.data(), image.image.size());

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
					auto& image = model.images.at(material.normalTexture.index);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eR8G8B8A8Unorm,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						image.image.data(), image.image.size());

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
					auto& image = model.images.at(material.pbrMetallicRoughness.metallicRoughnessTexture.index);
					if (image.mimeType.find("image/png") == std::string::npos &&
						image.mimeType.find("image/jpeg") == std::string::npos)
					{
						throw std::runtime_error("Unsupported image format: should be png or jpeg");
					}

					auto newImage = std::make_shared<Renderer::CombinedImageSampler2D>(
						image.width, image.height,
						vk::Format::eR8G8B8A8Unorm,
						vk::ImageUsageFlagBits::eSampled,
						vk::ImageAspectFlagBits::eColor,
						image.image.data(), image.image.size());

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
				StaticModelRenderer::getMaterialSetLayout()
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


		Logger::gInfo("Model: " + filePath + " loaded !");
	}

	StaticModel::~StaticModel()
	{
		for (const auto& material : mMaterials)
		{
			Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), material.mSet);
		}
	}
}