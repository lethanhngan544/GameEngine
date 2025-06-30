#include <Components.h>
#include <Data.h>
#include <string>
#include <glm/gtc/type_ptr.hpp>

namespace eg::Components
{
	template<typename T>
	float convertFn(T value)
	{
		static_assert(false);
	}

	template<>
	float convertFn(float value)
	{
		return value;
	}


	template<>
	float convertFn<int8_t>(int8_t value)
	{
		return std::max(value / 127.0f, -1.0f);
	}

	template<>
	float convertFn<uint8_t>(uint8_t value)
	{
		return value / 255.0f;
	}

	template<>
	float convertFn<int16_t>(int16_t value)
	{
		return std::max(value / 32767.0f, -1.0f);
	}

	template<>
	float convertFn<uint16_t>(uint16_t value)
	{
		return value / 65535.0f;
	}

	template<typename T>
	void extractRotationData(AnimatedModel::AnimationChannel::Data& data,
		const tinygltf::Buffer& buffer,
		const tinygltf::BufferView& bufferView,
		size_t i)
	{
		T quat[4];
		std::memcpy(&quat,
			buffer.data.data() + bufferView.byteOffset + i * sizeof(quat[0]) * 4,
			sizeof(quat[0]) * 4);

		data.rotation = glm::quat(convertFn(quat[3]),
			convertFn(quat[0]), convertFn(quat[1]), convertFn(quat[2]));
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
			this->loadTinygltfModel(model);
		}
		catch (...)
		{
			throw std::runtime_error("StaticModel, Error loading model !");
		}

		Logger::gInfo("Animated model: " + filePath + " loaded !");
	}

	AnimatedModel::~AnimatedModel()
	{
		for (const auto& material : mMaterials)
		{
			Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), material.mSet);
		}
	}

	void AnimatedModel::loadTinygltfModel(const tinygltf::Model& model)
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

		//Extract animations
		mAnimations.reserve(model.animations.size());
		for (const auto& gltfAnimations : model.animations)
		{
			std::shared_ptr<Animation> animation = std::make_shared<Animation>();
			animation->name = gltfAnimations.name;
			

			animation->channels.reserve(gltfAnimations.channels.size());
			for (const auto& gltfChannel : gltfAnimations.channels)
			{
				const auto& gltfSampler = gltfAnimations.samplers.at(gltfChannel.sampler);
				AnimationChannel channel;
				channel.targetJoint = gltfChannel.target_node;

				if (gltfChannel.target_path == "translation")
				{
					channel.path = AnimationChannel::Path::Translation;
				}
				else if (gltfChannel.target_path == "rotation")
				{
					channel.path = AnimationChannel::Path::Rotation;
				}
				else if (gltfChannel.target_path == "scale")
				{
					channel.path = AnimationChannel::Path::Scale;
				}
				else
				{
					throw std::runtime_error("Unknown animation channel path: " + gltfChannel.target_path);
				}

				if (gltfSampler.interpolation == "LINEAR")
				{
					channel.interpolation = AnimationChannel::Interpolation::Linear;
				}
				else if (gltfSampler.interpolation == "STEP")
				{
					channel.interpolation = AnimationChannel::Interpolation::Step;
				}
				else if (gltfSampler.interpolation == "CUBICSPLINE")
				{
					channel.interpolation = AnimationChannel::Interpolation::CubicSpline;
				}
				else
				{
					throw std::runtime_error("Unknown animation channel interpolation: " + gltfSampler.interpolation);
				}
				//Extract channels data
				const auto& inputAccessor = model.accessors.at(gltfSampler.input);
				const auto& inputBufferView = model.bufferViews.at(inputAccessor.bufferView);
				const auto& inputBuffer = model.buffers.at(inputBufferView.buffer);

				const auto& outputAccessor = model.accessors.at(gltfSampler.output);
				const auto& outputBufferView = model.bufferViews.at(outputAccessor.bufferView);
				const auto& outputBuffer = model.buffers.at(outputBufferView.buffer);

				channel.keyTimes.resize(inputAccessor.count);
				std::memcpy(channel.keyTimes.data(),
					inputBuffer.data.data() + inputBufferView.byteOffset,
					sizeof(float) * inputAccessor.count);

				animation->duration = inputAccessor.maxValues.at(0);

				//Convert it all to floats
				channel.data.reserve(inputAccessor.count);
				for (size_t i = 0; i < outputAccessor.count; i++)
				{
					//Vec3 of floats
					switch (channel.path)
					{
					case AnimationChannel::Path::Translation: // Vec3 of floats
					{
						AnimationChannel::Data data;
						std::memcpy(&data.translation,
							outputBuffer.data.data() + outputBufferView.byteOffset + i * sizeof(glm::vec3),
							sizeof(glm::vec3));
						channel.data.push_back(std::move(data));
						break;
					}

					case AnimationChannel::Path::Rotation: // vec4 of float/byte/ubyte/short/ushort
					{
						AnimationChannel::Data data;
						switch (outputAccessor.componentType)
						{
						case TINYGLTF_COMPONENT_TYPE_FLOAT:
						{
							extractRotationData<float>(data,
								outputBuffer, outputBufferView, i);
							break;
						}
						case TINYGLTF_COMPONENT_TYPE_BYTE:
						{
							extractRotationData<int8_t>(data,
								outputBuffer, outputBufferView, i);
							break;
						}

						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
						{
							extractRotationData<uint8_t>(data,
								outputBuffer, outputBufferView, i);
							break;
						}

						case TINYGLTF_COMPONENT_TYPE_SHORT:
						{
							extractRotationData<int16_t>(data,
								outputBuffer, outputBufferView, i);
							break;
						}

						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						{
							extractRotationData<uint16_t>(data,
								outputBuffer, outputBufferView, i);
							break;
						}
						}
						channel.data.push_back(std::move(data));
						break;
					}

					case AnimationChannel::Path::Scale:
					{
						AnimationChannel::Data data;
						std::memcpy(&data.scale,
							outputBuffer.data.data() + outputBufferView.byteOffset + i * sizeof(glm::vec3),
							sizeof(glm::vec3));
						channel.data.push_back(std::move(data));
						break;
					}
					}
				}
				animation->channels.push_back(std::move(channel));
			}
			mAnimations.push_back(std::move(animation));
		}
		//Extract vertex datas
		//mRawMeshes.reserve(model.meshes.size());
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
					Data::StaticModelRenderer::getMaterialSetLayout()
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
	}
}
