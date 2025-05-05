#include <SandBox_MapObject.h>

#include <tiny_gltf.h>
#include <Physics.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>

namespace sndbx
{
	MapObject::MapObject(const std::string& modelPath)
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		loader.SetImageLoader(eg::Data::LoadImageData, nullptr);
		std::string err;
		std::string warn;
		bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, modelPath);
		if (!warn.empty()) {
			eg::Logger::gWarn(warn);
		}

		if (!err.empty()) {
			eg::Logger::gError(err);
		}

		if (!ret) {
			throw std::runtime_error("Failed to load map object: " + modelPath);
		}

		mModel = std::make_shared<eg::Data::StaticModel>(model);

		//Create rigid body
		{
			//Extract from model
			std::vector<uint32_t> indices;
			std::vector<glm::vec3> positions;
			uint32_t vertexOffset = 0;
			for (const auto& mesh : model.meshes)
			{
				for (const auto& primitive : mesh.primitives)
				{
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
								indices.push_back(index + vertexOffset);
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
								indices.push_back(index + vertexOffset);
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

						positions.resize(positions.size() + positionAccessor.count);
						std::memcpy(positions.data(),
							positionBuffer.data.data() + positionBufferView.byteOffset,
							sizeof(glm::vec3) * positionAccessor.count);

						vertexOffset += positionAccessor.count;
					}
				}
			}

			JPH::VertexList vertices;
			JPH::IndexedTriangleList indexList;

			for (const auto& p : positions)
			{
				vertices.push_back(JPH::Float3(p.x, p.y, p.z));
			}

			for (uint32_t i = 0; i < (indices.size() - 3); i += 3)
			{
				indexList.push_back(JPH::IndexedTriangle(indices.at(i), indices.at(i + 1), indices.at(i + 2)));
			}


			JPH::MeshShapeSettings shapeSettings(vertices, indexList);
			shapeSettings.SetEmbedded();
			JPH::BodyCreationSettings bodySetting(&shapeSettings,
				JPH::RVec3(0.0f, 10.0f, 0.0f),
				JPH::Quat::sIdentity(),
				JPH::EMotionType::Static,
				eg::Physics::Layers::NON_MOVING);
			bodySetting.mFriction = 0.8f;
			bodySetting.mMassPropertiesOverride.mMass = 0.0f;
			bodySetting.mLinearDamping = 0.9f;
			bodySetting.mAngularDamping = 0.0f;
			bodySetting.mAllowSleeping = false;
			bodySetting.mMotionQuality = JPH::EMotionQuality::Discrete;

			mBody = eg::Physics::getBodyInterface()->CreateAndAddBody(bodySetting, JPH::EActivation::DontActivate);
		}

	}


	void MapObject::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{

		JPH::BodyInterface* bodyInterface = eg::Physics::getBodyInterface();
		JPH::Mat44 matrix = bodyInterface->GetCenterOfMassTransform(mBody);
		glm::mat4x4 glmMatrix;
		std::memcpy(&glmMatrix[0][0], &matrix, sizeof(glmMatrix));

		switch (stage)
		{
		case eg::Renderer::RenderStage::SHADOW:
			if (mModel)
			{
				eg::Data::StaticModelRenderer::renderShadow(cmd, *mModel, glmMatrix);
			}
			break;
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
			if (mModel)
			{
				eg::Data::StaticModelRenderer::render(cmd, *mModel, glmMatrix);
			}

			break;
		default:
			break;
		}
	}
}