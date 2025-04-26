#pragma once


#include <Data.h>

namespace sndbx
{
	class ParticleEmitter
	{
	private:
		eg::Data::CombinedImageSampler2D mParticleTexture;
		eg::Data::GPUBuffer mParticleVertexBuffer;
		
		JPH::BodyID mBody;
		glm::vec3 mPosition = { 0, 0, 0 };
		glm::vec3 mVelocity = { 0, 0, 0 };
		float mLifeTime = 1.0f;
		float mTimeAlive = 0.0f;
	public:
		ParticleEmitter(const std::string& modelPath);
		void update(float delta);
		void render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage);
	};
}