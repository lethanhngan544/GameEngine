#include "Components.h"

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
	void extractRotationData(Animation::Channel::Data& data,
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


	Animation::Animation(const std::string& animPath)
	{
		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		if (!loader.LoadBinaryFromFile(&model, &err, &warn, animPath))
		{
			throw std::runtime_error("Failed to load animation file !");
		}
		try
		{
			const auto& gltfAnimations = model.animations.at(0);
			mName = animPath;


			mChannels.reserve(gltfAnimations.channels.size());
			for (const auto& gltfChannel : gltfAnimations.channels)
			{
				const auto& gltfSampler = gltfAnimations.samplers.at(gltfChannel.sampler);
				Channel channel;
				channel.targetJoint = gltfChannel.target_node;

				if (gltfChannel.target_path == "translation")
				{
					channel.path = Channel::Path::Translation;
				}
				else if (gltfChannel.target_path == "rotation")
				{
					channel.path = Channel::Path::Rotation;
				}
				else if (gltfChannel.target_path == "scale")
				{
					channel.path = Channel::Path::Scale;
				}
				else
				{
					throw std::runtime_error("Unknown animation channel path: " + gltfChannel.target_path);
				}

				if (gltfSampler.interpolation == "LINEAR")
				{
					channel.interpolation = Channel::Interpolation::Linear;
				}
				else if (gltfSampler.interpolation == "STEP")
				{
					channel.interpolation = Channel::Interpolation::Step;
				}
				else if (gltfSampler.interpolation == "CUBICSPLINE")
				{
					channel.interpolation = Channel::Interpolation::CubicSpline;
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

				mDuration = inputAccessor.maxValues.at(0);

				//Convert it all to floats
				channel.data.reserve(inputAccessor.count);
				for (size_t i = 0; i < outputAccessor.count; i++)
				{
					//Vec3 of floats
					switch (channel.path)
					{
					case Channel::Path::Translation: // Vec3 of floats
					{
						Channel::Data data;
						std::memcpy(&data.translation,
							outputBuffer.data.data() + outputBufferView.byteOffset + i * sizeof(glm::vec3),
							sizeof(glm::vec3));
						channel.data.push_back(std::move(data));
						break;
					}

					case Channel::Path::Rotation: // vec4 of float/byte/ubyte/short/ushort
					{
						Channel::Data data;
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

					case Channel::Path::Scale:
					{
						Channel::Data data;
						std::memcpy(&data.scale,
							outputBuffer.data.data() + outputBufferView.byteOffset + i * sizeof(glm::vec3),
							sizeof(glm::vec3));
						channel.data.push_back(std::move(data));
						break;
					}
					}
				}
				mChannels.push_back(std::move(channel));
			}

		}
		catch (const std::exception& e)
		{
			throw std::runtime_error("Animation error: " + animPath + "\n" + e.what());
		}


		Logger::gInfo("Animation loaded: " + animPath);
	}
}