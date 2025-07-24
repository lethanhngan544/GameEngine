#pragma once

#include <optional>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <MyVulkan.h>
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include <Renderer.h>


namespace eg::Components
{

	class RigidBody
	{
	public:
		JPH::BodyID mBodyID;
		float mMass;
		float mFriction;
		float mRestitution;

	public:
		RigidBody() = default;
		~RigidBody() = default;

		glm::mat4x4 getBodyMatrix() const;

		nlohmann::json toJson() const;
	};
	

	/*class Transform
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


	};*/


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

		nlohmann::json toJson() const
		{
			nlohmann::json json;
			json["position"] = { mPosition.x, mPosition.y, mPosition.z };
			json["pitch"] = mPitch;
			json["yaw"] = mYaw;
			json["roll"] = mRoll;
			json["fov"] = mFov;
			json["near"] = mNear;
			json["far"] = mFar;
			return json;
		}
		void fromJson(const nlohmann::json& json)
		{
			if (json.contains("position")) {
				auto pos = json["position"];
				mPosition = { pos[0], pos[1], pos[2] };
			}
			if (json.contains("pitch")) mPitch = json["pitch"];
			if (json.contains("yaw")) mYaw = json["yaw"];
			if (json.contains("roll")) mRoll = json["roll"];
			if (json.contains("fov")) mFov = json["fov"];
			if (json.contains("near")) mNear = json["near"];
			if (json.contains("far")) mFar = json["far"];
		}
	};

	class CameraFrustumCuller
	{
	private:
		struct FrustumPlane {
			glm::vec3 normal;
			float d;
		};

	public:
		CameraFrustumCuller(const Camera& camera) : mCamera(camera) {}
		// Check if a point is inside the camera frustum
		bool isPointInFrustum(const glm::vec3& point) const;
		// Check if a bounding box is inside the camera frustum
		bool isBoundingBoxInFrustum(const glm::vec3& min, const glm::vec3& max, const glm::vec3* offset = nullptr) const;
		// Check if a sphere is inside the camera frustum
		bool isSphereInFrustum( const glm::vec3& center, float radius) const;
		// Update the frustum planes based on the camera's view and projection matrices
		void updateFrustumPlanes(vk::Extent2D extent);
	private:
		FrustumPlane extractPlane(float a, float b, float c, float d);
	private:
		const Camera& mCamera;
		std::array<FrustumPlane, 6> mFrustumPlanes;
	};

	class DirectionalLight
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

		nlohmann::json toJson() const
		{
			return
			{
				{ "position", { mUniformBuffer.position.x, mUniformBuffer.position.y, mUniformBuffer.position.z } },
				{ "color", { mUniformBuffer.color.r, mUniformBuffer.color.g, mUniformBuffer.color.b } },
				{ "intensity", mUniformBuffer.intensity },
				{ "constant", mUniformBuffer.constant },
				{ "linear", mUniformBuffer.linear },
				{ "exponent", mUniformBuffer.exponent },
			};
		}

		void fromJson(const nlohmann::json& json)
		{
			if (json.contains("position")) {
				auto pos = json["position"];
				mUniformBuffer.position = { pos[0], pos[1], pos[2], 1.0f };
			}
			if (json.contains("color")) {
				auto col = json["color"];
				mUniformBuffer.color = { col[0], col[1], col[2], 1.0f };
			}
			if (json.contains("intensity")) mUniformBuffer.intensity = json["intensity"];
			if (json.contains("constant")) 
mUniformBuffer.constant = json["constant"];
			if (json.contains("linear")) 
				mUniformBuffer.linear = json["linear"];
			if (json.contains("exponent")) 
				mUniformBuffer.exponent = json["exponent"];
		}

		const Renderer::CPUBuffer& getBuffer() const { return mBuffer; }
		vk::DescriptorSet getDescriptorSet() const { return mSet; }
	};

	

	class StaticModel
	{
	protected:
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

		std::string mFilePath;
	protected:
		StaticModel() = default;

		void extractRawMeshes(const tinygltf::Model& model);
		void extractMaterials(const tinygltf::Model& model);
	public:
		StaticModel(const std::string& filePath);
		StaticModel(const std::string& filePath, const tinygltf::Model& model);
		virtual ~StaticModel();

		const std::vector<RawMesh>& getRawMeshes() const { return mRawMeshes; }
		const std::vector<Material>& getMaterials() const { return mMaterials; }

		nlohmann::json toJson() const
		{
			return { {"model_path", mFilePath} };
		}


		inline void setFilePath(const std::string& filePath) { mFilePath = filePath; }
	};


	class Animation
	{
	private:
		friend class Animator;
		struct Channel
		{
			int32_t targetJoint;
			enum class Path
			{
				Translation,
				Rotation,
				Scale
			} path;
			enum class Interpolation
			{
				Linear,
				Step,
				CubicSpline
			} interpolation;

			union Data
			{
				glm::vec3 translation;
				glm::quat rotation;
				glm::vec3 scale;
			};
			std::vector<float> keyTimes;
			std::vector<Data> data;
		};

	private:
		std::string mName;
		std::vector<Channel> mChannels;
		float mDuration;
	public:
		Animation(const std::string& animPath);
		~Animation() = default;

		inline const std::string& getName() const { return mName; }
		inline const std::vector<Channel>& getChannels() const { return mChannels; }
		inline float getDuration() const { return mDuration; }
	};

	class AnimatedModel : public StaticModel
	{
	private:
		friend class Animator;
		struct AnimatedRawMesh
		{
			Renderer::GPUBuffer positionBuffer;
			Renderer::GPUBuffer normalBuffer;
			Renderer::GPUBuffer uvBuffer;
			Renderer::GPUBuffer boneIdsBuffer;
			Renderer::GPUBuffer boneWeightsBuffer;
			Renderer::GPUBuffer indexBuffer;
			uint32_t materialIndex = 0;
			uint32_t vertexCount = 0;
		};

		struct Node
		{
			std::string name;
			glm::vec3 position;
			glm::quat rotation;
			glm::vec3 scale;
			int meshIndex;
			std::vector<int32_t> children;
		};

		struct Skin
		{
			std::string name;
			int32_t rootNode;
			std::vector<int32_t> joints;
			std::vector<glm::mat4x4> inverseBindMatrices;
		};
		
		int mRootNodeIndex = -1;
		std::vector<AnimatedRawMesh> mAnimatedRawMeshes;
		std::vector<Node> mNodes;
		std::vector<Skin> mSkins;
		
	public:

		static constexpr size_t MAX_BONE_COUNT = 100;

		AnimatedModel() = delete;
		AnimatedModel(const std::string& filePath);
		virtual ~AnimatedModel();


		inline const std::vector<AnimatedRawMesh>& getAnimatedRawMehses() const { return mAnimatedRawMeshes; }
		inline const std::vector<Skin>& getSkins() const { return mSkins; }
		inline const int getRootNodeIndex() const { return mRootNodeIndex; }
	private:
		void extractedAnimatedRawMeshes(const tinygltf::Model& model);
		void extractNodes(const tinygltf::Model& model);
		void extractSkins(const tinygltf::Model& model);
	};

	class Animator
	{
	public:
		struct AnimationNode
		{
			std::string name;
			glm::vec3 position;
			glm::quat rotation;
			glm::vec3 scale;
			int meshIndex;
			std::vector<int32_t> children;
			glm::mat4x4 modelLocalTransform;
		};
	private:
		using AnimationNodeVec = std::vector<std::shared_ptr<AnimationNode>>;
		using AnimationMap = std::unordered_map<std::string, std::shared_ptr<Animation>>;
		using BoneMatrices = std::array<glm::mat4x4, AnimatedModel::MAX_BONE_COUNT>;

		const AnimatedModel& mModel;
		std::vector<std::shared_ptr<Animation>> mAnimations;
		std::shared_ptr<Animation> mCurrentAnimation;
		BoneMatrices mBoneMatrices;
		AnimationNodeVec mAnimationNodes;
		vk::DescriptorSet mDescriptorSet;
		Renderer::CPUBuffer mUniformBuffer;
		AnimationMap mAnimationMap;
		float mCurrentTime = 0.0f;
		float mTimeScale = 1.0f;
	public:
		Animator(const std::vector<std::shared_ptr<Animation>>& animations, const AnimatedModel& model);
		~Animator();

		void update(float deltaTime);


		void setAnimation(const std::string& animationName);
		inline vk::DescriptorSet getDescriptorSet() const { return mDescriptorSet; }
		inline const AnimationNodeVec& getAnimationNodes() const { return mAnimationNodes; }

		std::shared_ptr<AnimationNode> getAnimationNodeByName(const std::string& name);
		void setTimeScale(float scale) { mTimeScale = scale; }
	private:
		void processCurrentAnimation(float delta);
	};

	namespace ModelCache
	{
		std::shared_ptr<StaticModel> loadStaticModel(const std::string& filePath);
		std::shared_ptr<StaticModel> loadStaticModelFromJson(const nlohmann::json& json);
		std::shared_ptr<AnimatedModel> loadAnimatedModel(const std::string& filePath);
		std::shared_ptr<AnimatedModel> loadAnimatedModelFromJson(const nlohmann::json& json);

		std::shared_ptr<Animation> loadAnimation(const std::string& animPath);
		void clearCache();
	}


	struct ParticleInstance {
		glm::vec4 positionSize; // xyz = position, w = size
		glm::ivec2 frameIndex; // offset into the texture grid
	};

	class ParticleEmitter
	{
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
			ParticleInstance particle;
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


}