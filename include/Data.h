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


namespace tinygltf
{
	class Model;
	class Image;
}

namespace eg::Data
{
	bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
		std::string* warn, int req_width, int req_height,
		const unsigned char* bytes, int size, void* user_data);
	
	class IGameObject
	{	
	public:
		IGameObject() = default;
		virtual ~IGameObject() = default;

		virtual void update(float delta) = 0;
		virtual void fixedUpdate(float delta) = 0;
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
		void fixedUpdate(float delta);
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


	class DirectionalLight //This will act like uniform buffer
	{
	public:
		struct UniformBuffer
		{
			glm::vec3 direction = { 1, -1, 0 };
			float intensity = 5.0f;
			glm::vec4 color = { 1, 1, 1, 1 };
		} mUniformBuffer;

		Renderer::CPUBuffer mBuffer;
	private:
		vk::DescriptorSet mSet;
	public:
		DirectionalLight();
		~DirectionalLight();

		void update();

		glm::mat4x4 getDirectionalLightViewProj(const Camera& camera) const;

		inline vk::DescriptorSet getSet() const { return mSet; }
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

		void update();

		const Renderer::CPUBuffer& getBuffer() const { return mBuffer; }
		vk::DescriptorSet getDescriptorSet() const { return mSet; }
	};


	class StaticModel
	{
	private:
		struct RawMesh
		{
			Renderer::GPUBuffer positionBuffer;
			Renderer::GPUBuffer normalBuffer;
			Renderer::GPUBuffer uvBuffer;
			Renderer::GPUBuffer indexBuffer;
			uint32_t materialIndex = 0;
			uint32_t vertexCount = 0;
		};

		struct Material
		{
			struct UniformBuffer
			{
				float albedoColor[3] = {};
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
		StaticModel(const tinygltf::Model& model);
		~StaticModel();

		const std::vector<RawMesh>& getRawMeshes() const { return mRawMeshes; }
		const std::vector<Material>& getMaterials() const { return mMaterials; }
	private:
		void loadTinygltfModel(const tinygltf::Model& model);
	};

	namespace StaticModelCache
	{
		void clear();
		std::shared_ptr<StaticModel> load(const std::string& filePath);

	};

	//Systems

	namespace StaticModelRenderer
	{
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


		void beginShadow(vk::CommandBuffer cmd);

		void renderShadow(vk::CommandBuffer cmd,
			const StaticModel& model,
			glm::mat4x4 worldTransform);


		vk::DescriptorSetLayout getMaterialSetLayout();

	}

	namespace LightRenderer
	{
	

		void create();
		void destroy();

		void renderAmbient(vk::CommandBuffer cmd);
		void renderDirectionalLight(vk::CommandBuffer cmd, const DirectionalLight& light);

		void beginPointLight(vk::CommandBuffer cmd);

		void renderPointLight(vk::CommandBuffer cmd,
			const PointLight& pointLight);

		vk::DescriptorSetLayout getPointLightPerDescLayout();
		vk::DescriptorSetLayout getDirectionalPerDescLayout();
	}


	

	namespace ParticleRenderer
	{
		struct VertexPushConstant
		{
			glm::ivec2 atlasSize; // vi du 4x4
		};

		struct ParticleInstance {
			glm::vec4 positionSize; // xyz = position, w = size
			glm::ivec2 frameIndex; // offset into the texture grid
		};

		void create();
		void destroy();
		void recordParticle(const ParticleInstance instance, vk::DescriptorSet textureAtlasSet, glm::uvec2 atlasSize);
		void updateBuffers(vk::CommandBuffer cmd);
		void render(vk::CommandBuffer cmd);

		vk::DescriptorSetLayout getTextureAtlasDescLayout();
	}
	
	class ParticleEmitter
	{
	//Static data, for caching atlas textures
	private:
		struct AtlasTexture
		{
			std::optional<Renderer::CombinedImageSampler2D> texture;
			vk::DescriptorSet set;
			glm::uvec2 size;
		};

		static std::unordered_map<std::string, std::shared_ptr<AtlasTexture>> mAtlasTextures;
		static std::shared_ptr<AtlasTexture> getAtlasTexture(const std::string& textureAtlas, glm::uvec2 atlasSize);
	public:
		static void clearAtlasTextures();
	private:
		struct ParticleEntity
		{
			ParticleRenderer::ParticleInstance particle;
			glm::vec3 velocity;
			float age;
		};

		glm::vec3 mPosition = { 0, 0, 0 };
		glm::vec3 mDirection = { 0, 0, 1 };
		float mLifeSpan = 2.0f;
		float mGenerateRate = 0.005f;
		float mGenerateRateCounter = 0.0f;

		std::shared_ptr<AtlasTexture> mAtlasTexture;
		std::vector<ParticleEntity> mParticles;

	public:
		ParticleEmitter(const std::string& textureAtlas, glm::uvec2 atlasSize);
		~ParticleEmitter() = default;
		void update(float delta);
		void record();

		inline void setPosition(const glm::vec3& position) { mPosition = position; }
		inline void setDirection(const glm::vec3& direction) { mDirection = direction; }
	};

	namespace DebugRenderer
	{
		struct VertexFormat
		{
			glm::vec3 pos;
			glm::vec3 color;
		};

		void create();
		void destroy();

		void recordLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);

		void updateVertexBuffers(vk::CommandBuffer cmd);
		void render(vk::CommandBuffer cmd);
	}


	
}