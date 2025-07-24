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
#include <Components.h>




namespace eg::Data
{
	bool LoadImageData(tinygltf::Image* image, const int image_idx, std::string* err,
		std::string* warn, int req_width, int req_height,
		const unsigned char* bytes, int size, void* user_data);
	

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
			const Components::StaticModel& model, 
			glm::mat4x4 worldTransform);


		void beginShadow(vk::CommandBuffer cmd);

		void renderShadow(vk::CommandBuffer cmd,
			const Components::StaticModel& model,
			glm::mat4x4 worldTransform);


		vk::DescriptorSetLayout getMaterialSetLayout();

	}

	namespace AnimatedModelRenderer
	{
		struct VertexPushConstant
		{
			glm::mat4x4 model;
		};

		void create();
		void destroy();


		void render(vk::CommandBuffer cmd,
			const Components::AnimatedModel& model, const Components::Animator& animator,
			glm::mat4x4 worldTransform);


		void renderShadow(vk::CommandBuffer cmd,
			const Components::AnimatedModel& model, const Components::Animator& animator,
			glm::mat4x4 worldTransform);


		vk::DescriptorSetLayout getMaterialSetLayout();
		vk::DescriptorSetLayout getBoneSetLayout();

	}

	namespace LightRenderer
	{
		void create();
		void destroy();

		void renderAmbient(vk::CommandBuffer cmd);
		void renderDirectionalLight(vk::CommandBuffer cmd, const Components::DirectionalLight& light);

		void beginPointLight(vk::CommandBuffer cmd);

		void renderPointLight(vk::CommandBuffer cmd,
			const Components::PointLight& pointLight);

		vk::DescriptorSetLayout getPointLightPerDescLayout();
		vk::DescriptorSetLayout getDirectionalPerDescLayout();
	}


	

	namespace ParticleRenderer
	{
		struct VertexPushConstant
		{
			glm::ivec2 atlasSize; // vi du 4x4
		};

		

		void create();
		void destroy();
		void recordParticle(const Components::ParticleInstance instance, vk::DescriptorSet textureAtlasSet, glm::uvec2 atlasSize);
		void updateBuffers(vk::CommandBuffer cmd);
		void render(vk::CommandBuffer cmd);

		vk::DescriptorSetLayout getTextureAtlasDescLayout();
	}

	//Theres only a single skydome, so we can use a singleton pattern for it.
	namespace SkydomeRenderer
	{
		struct SkydomeSettings
		{
			glm::vec3 upperColor = glm::vec3(0.5f, 0.7f, 1.0f); // Default color
			glm::vec3 lowerColor = glm::vec3(0.1f, 0.1f, 0.2f); // Default color
		};

		void create();
		void destroy();
		void render(vk::CommandBuffer cmd, const SkydomeSettings& skydome);
		vk::DescriptorSetLayout getDescLayout();
		vk::PipelineLayout getPipelineLayout();
		vk::Pipeline getPipeline();
	}
	
	

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