#pragma once

#include <Renderer.h>
#include <RenderStages.h>
#include <string>
#include <optional>
#include <tuple>
#include <memory>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>



namespace eg::Data
{
	class IGameObject
	{	
	public:
		IGameObject() = default;
		virtual ~IGameObject() = default;

		virtual void update(float delta) = 0;
		virtual void render(vk::CommandBuffer cmd, Renderer::RenderStage stage) = 0;

	};

	class GameObjectManager
	{
	private:
		std::vector<std::unique_ptr<IGameObject>> mGameObjects;
	public:

		void addGameObject(std::unique_ptr<IGameObject> gameobject);
		void removeGameObject(const IGameObject* gameObject);

		void update(float delta);
		void render(vk::CommandBuffer cmd, Renderer::RenderStage stage);
	};
	
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

	class Transform
	{
	public:
		glm::vec3 mPosition = { 0, 0, 0 };
		glm::quat mRotation = { 1, 0, 0, 0 };
		glm::vec3 mScale = { 1, 1, 1 };
	public:
		Transform() = default;
		Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale) :
			mPosition(position), mRotation(rotation), mScale(scale) {
		}

		void identity()
		{
			mPosition = { 0, 0, 0 };
			mRotation = { 1, 0, 0, 0 };
			mScale = { 1, 1, 1 };
		}

		glm::mat4x4 build() const
		{
			glm::mat4x4 matrix(1.0f);
			matrix = glm::translate(matrix, mPosition);
			matrix *= glm::mat4_cast(mRotation);
			matrix = glm::scale(matrix, mScale);
			return matrix;
		}


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
			float linear = 0.1f;
			float exponent = 0.01f;
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


	class StaticModel
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
			struct UniformBuffer
			{
				uint32_t has_albedo = 0;
				uint32_t has_normal = 0;
				uint32_t has_mr = 0;
			};
			std::shared_ptr<Renderer::CPUBuffer> mUniformBuffer;
			std::shared_ptr<Renderer::CombinedImageSampler2D> mAlbedo;
			std::shared_ptr<Renderer::CombinedImageSampler2D> mNormal;
			std::shared_ptr<Renderer::CombinedImageSampler2D> mMr;
			vk::DescriptorSet mSet;
		};

		std::vector<RawMesh> mRawMeshes;
		std::vector<std::shared_ptr<Renderer::CombinedImageSampler2D>> mImages;
		std::vector<Material> mMaterials;
	public:
		StaticModel() = delete;
		StaticModel(const std::string& filePath);
		~StaticModel();

		const std::vector<RawMesh>& getRawMeshes() const { return mRawMeshes; }
		const std::vector<Material>& getMaterials() const { return mMaterials; }

	};

	namespace StaticModelCache
	{
		void clear();
		std::shared_ptr<StaticModel> load(const std::string& filePath);

	};

	//Systems

	namespace StaticModelRenderer
	{
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

		void create();
		void destroy();


		void begin(vk::CommandBuffer cmd);

		void render(vk::CommandBuffer cmd,
			const StaticModel& model, 
			glm::mat4x4 worldTransform);


		vk::DescriptorSetLayout getMaterialSetLayout();

	};

	namespace LightRenderer
	{
		void create();
		void destroy();

		void renderAmbient(vk::CommandBuffer cmd);

		void beginPointLight(vk::CommandBuffer cmd);

		void renderPointLight(vk::CommandBuffer cmd,
			const PointLight& pointLight);

		vk::DescriptorSetLayout getPointLightPerDescLayout();
	};
	
}