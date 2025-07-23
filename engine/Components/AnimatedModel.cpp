#include <Components.h>
#include <Data.h>
#include <string>
#include <glm/gtc/type_ptr.hpp>

namespace eg::Components
{
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
