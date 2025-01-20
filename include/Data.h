#pragma once

#include <Renderer.h>
#include <string>
#include <optional>
#include <tuple>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

namespace eg::Data
{
	class Camera
	{
	public:
		glm::vec3 mPosition = { 0, 0, 0 };
		float mPitch = 0.0f, mYaw = 0.0f, mRoll = 0.0f;
		float mFov = 90.0f;
		float mNear = 0.1f;
		float mFar = 1000.0f;

	public:
		Camera() = default;


		glm::mat4x4 buildProjection(vk::Extent2D extent) const;
		glm::mat4x4 buildView() const;
	};


	class PointLight
	{
	public:
		struct UniformBuffer
		{
			glm::vec4 position = { 0, 0, 0, 1 };
			glm::vec4 color = { 1, 1, 1, 1 };
			float intensity = 1.0f;
			float constant = 1.0f;
			float linear = 0.01f;
			float exponent = 0.001f;
		} mUniformBuffer{};
	private:
		vk::DescriptorSet mSet;
		Renderer::CPUBuffer	mBuffer;
	public:
		PointLight();
		~PointLight();

		const Renderer::CPUBuffer& getBuffer() const { return mBuffer; }
		vk::DescriptorSet getDescriptorSet() const { return mSet; }
	};

	//Entity component system

	class Component
	{
	public:
		bool valid = false;
	};


	class StaticModel : public Component
	{
	private:
		struct RawMesh
		{
			Renderer::GPUBuffer vertexBuffer;
			Renderer::GPUBuffer indexBuffer;
			uint32_t materialIndex = 0;
			uint32_t vertexCount = 0;
		};

		struct Material
		{
			int32_t albedo = -1;
			vk::DescriptorSet mSet;
		};

		std::vector<RawMesh> mRawMeshes;
		std::vector<Renderer::CombinedImageSampler2D> mImages;
		std::vector<Material> mMaterials;
	public:
		StaticModel() = default;
		StaticModel(const std::string& filePath, vk::DescriptorSetLayout materialSetLayout);
		~StaticModel();


		StaticModel(const StaticModel&) = delete;
		StaticModel operator=(const StaticModel&) = delete;

		StaticModel(StaticModel&& other) noexcept
		{
			this->mRawMeshes = std::move(other.mRawMeshes);
			this->mImages = std::move(other.mImages);
			this->mMaterials = std::move(other.mMaterials);
			this->valid = true;

			other.mRawMeshes.clear();
			other.mImages.clear();
			other.mMaterials.clear();
			other.valid = false;
		}

		StaticModel& operator=(StaticModel&& other) noexcept
		{
			this->mRawMeshes = std::move(other.mRawMeshes);
			this->mImages = std::move(other.mImages);
			this->mMaterials = std::move(other.mMaterials);
			this->valid = true;

			other.mRawMeshes.clear();
			other.mImages.clear();
			other.mMaterials.clear();
			other.valid = false;

			return *this;
		}


		const std::vector<RawMesh>& getRawMeshes() const { return mRawMeshes; }
		const std::vector<Material>& getMaterials() const { return mMaterials; }

	};

	//Entity

	class Entity
	{
	public:
		glm::vec3 mPosition = { 0, 0, 0 };
		glm::quat mRotation = { 1, 0, 0, 0 }; //x y z w, glm::quat(w, x, y, z)
		glm::vec3 mScale = { 1, 1, 1 };
	private:
		std::tuple<StaticModel> mComponents;
	public:
		Entity() = default;
		~Entity() = default;

		Entity(const Entity&) = delete;
		Entity operator=(const Entity&) = delete;

		Entity(Entity&&) = default;
		Entity& operator=(Entity&&) = default;



		template<typename T, typename... TArgs>
		T& add(TArgs&&... args)
		{
			auto& component = std::get<T>(mComponents);
			component = std::move(T(std::forward<TArgs>(args)...));
			component.valid = true;
			return component;
		}

		template<typename T>
		T& get()
		{
			return std::get<T>(mComponents);
		}

		template<typename T>
		const T& get() const
		{
			return std::get<T>(mComponents);
		}

		template<typename T>
		bool has() const
		{
			return get<T>().valid;
		}
	};

	//Systems

	class StaticModelRenderer
	{
	public:
		struct VertexFormat
		{
			glm::vec3 pos;
			glm::vec3 normal;
			glm::vec2 uv;
		};

		struct VertexPushConstant
		{
			glm::mat4x4 model;
		};
	private:
		vk::Device mDevice;
		vk::Pipeline mPipeline;
		vk::PipelineLayout mPipelineLayout;
		vk::DescriptorSetLayout mDescriptorLayout;
	public:
		StaticModelRenderer(vk::Device device,
			vk::RenderPass pass,
			uint32_t subpassIndex,
			vk::DescriptorSetLayout globalDescriptorSetLayout);
		~StaticModelRenderer();

		StaticModelRenderer(const StaticModelRenderer&) = delete;
		StaticModelRenderer(StaticModelRenderer&&) noexcept = delete;

		StaticModelRenderer operator=(const StaticModelRenderer&) = delete;
		StaticModelRenderer& operator=(StaticModelRenderer&&) noexcept = delete;

		void render(vk::CommandBuffer cmd,
			vk::Rect2D drawExtent,
			vk::DescriptorSet globalSet,
			const Entity* filteredEntities, size_t entityCount) const;

		vk::DescriptorSetLayout getMaterialSetLayout() const { return mDescriptorLayout; }

	};
}