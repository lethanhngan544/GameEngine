#include <Components.h>
#include <Data.h>


#include <stb_image.h>

namespace eg::Components
{
	std::unordered_map<std::string, std::shared_ptr<ParticleEmitter::AtlasTexture>> ParticleEmitter::mAtlasTextures;

	std::shared_ptr<ParticleEmitter::AtlasTexture> ParticleEmitter::getAtlasTexture(const std::string& textureAtlas, glm::uvec2 atlasSize)
	{
		//Check if texture is already loaded
		if (mAtlasTextures.find(textureAtlas) != mAtlasTextures.end())
		{
			//Return existing texture
			return mAtlasTextures.at(textureAtlas);
		}
		else //Load new texture
		{
			eg::Logger::gInfo("Loading new particle atlas texture: " + textureAtlas);
			//Create new texture
			auto& atlas = mAtlasTextures[textureAtlas];
			atlas = std::make_unique<AtlasTexture>();
			atlas->size = atlasSize;

			//Load image using stbimage
			int width, height, channels;
			stbi_set_flip_vertically_on_load(true);
			auto imageData = stbi_load(textureAtlas.c_str(), &width, &height, &channels, STBI_rgb_alpha);
			if (!imageData)
			{
				throw std::runtime_error("Failed to load texture atlas");
			}
			//Create texture

			uint32_t size = width * height * 4;
			atlas->texture.emplace(width, height, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eSampled, vk::ImageAspectFlagBits::eColor, imageData,
				size);


			//Create descriptor set
			auto layouts = Data::ParticleRenderer::getTextureAtlasDescLayout();
			vk::DescriptorSetAllocateInfo descSetAllocInfo{};
			descSetAllocInfo.setDescriptorPool(eg::Renderer::getDescriptorPool())
				.setSetLayouts(layouts);

			//Allocate descriptor set
			atlas->set = Renderer::getDevice().allocateDescriptorSets(descSetAllocInfo)[0];
			//Update descriptor set
			vk::DescriptorImageInfo imageInfo{};
			imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(atlas->texture->getImage().getImageView())
				.setSampler(atlas->texture->getSampler());
			vk::WriteDescriptorSet writeDescSet{};
			writeDescSet.setDstSet(atlas->set)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setPImageInfo(&imageInfo)
				.setDstBinding(0);
			Renderer::getDevice().updateDescriptorSets(writeDescSet, {});

			//Free stbi image
			stbi_image_free(imageData);

			return atlas;
		}
	}

	void ParticleEmitter::clearAtlasTextures()
	{
		for (const auto& [atlasName, texture] : mAtlasTextures)
		{
			//Free descriptor set
			Renderer::getDevice().freeDescriptorSets(Renderer::getDescriptorPool(), texture->set);
		}
		mAtlasTextures.clear();
	}

	ParticleEmitter::ParticleEmitter(const std::string& textureAtlas, glm::uvec2 atlasSize)
	{
		mAtlasTexture = getAtlasTexture(textureAtlas, atlasSize);
	}

	void ParticleEmitter::record()
	{
		for (auto it = mParticles.rbegin(); it != mParticles.rend(); it++)
		{
			//Record particles
			Data::ParticleRenderer::recordParticle(it->particle, mAtlasTexture->set, mAtlasTexture->size);
		}
	}
	void ParticleEmitter::update(float delta)
	{

		//Spawn particles
		mGenerateRateCounter += delta;
		if (mGenerateRateCounter > mGenerateRate)
		{
			mGenerateRateCounter = 0.0f;
			ParticleEntity particle;

			//Random scale
			float randomScale = (float)(rand() % 100) / 100.0f + 0.5f;
			particle.particle.positionSize = { mPosition.x, mPosition.y, mPosition.z, randomScale };
			particle.particle.frameIndex = { 0, 0 };

			//Give velocity ramdom spread
			//Generate random spread
			glm::vec3 randomSpread = { (float)(rand() % 100) / 100.0f - 0.5f, (float)(rand() % 100) / 100.0f - 0.5f, (float)(rand() % 100) / 100.0f - 0.5f };
			float randomSpeed = (float)(rand() % 10) / 10.0f;
			particle.velocity = mDirection * randomSpeed + randomSpread;
			particle.age = 0.0f;
			mParticles.push_back(particle);
		}

		//Update particles
		for (auto& particle : mParticles)
		{
			particle.particle.positionSize += glm::vec4(particle.velocity * delta, 0.0f);
			particle.particle.positionSize.w = 1.0f;
			particle.age += delta;

			//Advance frame index based on age, go from left to right, top to bottom

			uint32_t index = static_cast<uint32_t>((particle.age / mLifeSpan) * mAtlasTexture->size.x * mAtlasTexture->size.y);
			particle.particle.frameIndex.x = index % mAtlasTexture->size.x;
			particle.particle.frameIndex.y = mAtlasTexture->size.y - 1 - index / mAtlasTexture->size.x;



		}

		//Remove dead particles
		mParticles.erase(std::remove_if(mParticles.begin(), mParticles.end(),
			[this](const ParticleEntity& particle) { return particle.age > mLifeSpan; }),
			mParticles.end());
	}
}