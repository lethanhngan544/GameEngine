#include <Data.h>
#include <Logger.h>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <vector>


namespace eg::Data
{
	StaticModel::StaticModel(const std::string& filePath, vk::DescriptorSetLayout materialSetLayout)
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
						std::memcpy(tempIndices.data(), (void*)(buffer.data.data() + bufferView.byteOffset), bufferView.byteLength);
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
						std::memcpy(tempIndices.data(), (void*)(buffer.data.data() + bufferView.byteOffset), bufferView.byteLength);
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
					std::memcpy(positions.data(), positionBuffer.data.data() + positionBufferView.byteOffset, positionBufferView.byteLength);
				}

				//Normals
				{
					uint32_t accessorIndex = primitive.attributes.at("NORMAL");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					normals.resize(accessor.count);
					std::memcpy(normals.data(), buffer.data.data() + bufferView.byteOffset, bufferView.byteLength);
				}

				//texture coords
				{
					uint32_t accessorIndex = primitive.attributes.at("TEXCOORD_0");
					const auto& accessor = model.accessors.at(accessorIndex);
					const auto& bufferView = model.bufferViews.at(accessor.bufferView);
					const auto& buffer = model.buffers.at(bufferView.buffer);

					uvs.resize(accessor.count);
					std::memcpy(uvs.data(), buffer.data.data() + bufferView.byteOffset, bufferView.byteLength);
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

		//Extract images
		for (const auto& texture : model.textures)
		{
			//Default sampler
			//TODO: implement a huge switch statement
			const auto& sampler = model.samplers.at(texture.sampler);
			
			auto& image = model.images.at(texture.source);
			if (image.mimeType.find("image/png") == std::string::npos &&
				image.mimeType.find("image/jpeg") == std::string::npos)
			{
				throw std::runtime_error("Unsupported image format: should be png or jpeg");
			}

			mImages.emplace_back(image.width, image.height, vk::Format::eR8G8B8A8Srgb, vk::ImageUsageFlagBits::eSampled,
				vk::ImageAspectFlagBits::eColor, image.image.data(), image.width* image.height* image.component);

		}

		//Extract materials
		for (const auto& material : model.materials)
		{
			Material newMaterial{};
			newMaterial.albedo = material.pbrMetallicRoughness.baseColorTexture.index;
			Renderer::CombinedImageSampler2D& baseColor =  mImages.at(newMaterial.albedo);
			vk::DescriptorImageInfo baseColorInfo{};
			baseColorInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(baseColor.getImage().getImageView())
				.setSampler(baseColor.getSampler());

			//Allocate descriptor set
			vk::DescriptorSetLayout setLayouts[] =
			{
				materialSetLayout
			};
			vk::DescriptorSetAllocateInfo ai{};
			ai.setDescriptorPool(Renderer::getDescriptorPool())
				.setDescriptorSetCount(1)
				.setSetLayouts(setLayouts);
			newMaterial.mSet = Renderer::getDevice().allocateDescriptorSets(ai).at(0);

			Renderer::getDevice().updateDescriptorSets({
				vk::WriteDescriptorSet(newMaterial.mSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &baseColorInfo, nullptr, nullptr, nullptr)
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