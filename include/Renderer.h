#pragma once

#include <MyVulkan.h>
#include <vulkan-memory-allocator-hpp/vk_mem_alloc.hpp>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <functional>

#include <Logger.h>

namespace eg::Data
{
	class StaticModel;
	class Camera;
	class PointLight;
}

namespace eg::Renderer
{
	//Global functions
	std::vector<uint32_t> compileShaderFromFile(const std::string& filePath, uint32_t kind);
	void immediateSubmit(std::function<void(vk::CommandBuffer cmd)>&& function);


	void create(uint32_t width, uint32_t height);
	void waitIdle();
	void destory();

	vk::CommandBuffer begin(const Data::Camera& camera);
	void renderStaticModel(vk::CommandBuffer cmd, const Data::StaticModel& staticMesh);
	void end();


	vk::Instance getInstance();
	vk::Device getDevice();
	vma::Allocator getAllocator();
	vk::DescriptorPool getDescriptorPool();

	vk::DescriptorSet getCurrentFrameGUBODescSet();
	vk::DescriptorSetLayout getGlobalDescriptorSet();

	const class DefaultRenderPass& getDefaultRenderPass();
	const class CombinedImageSampler2D& getDefaultWhiteImage();
	const class CombinedImageSampler2D& getDefaultCheckerboardImage();


	

	//Datas
	class GPUBuffer
	{
	private:
		vk::Buffer mBuffer;
		vma::Allocation mAllocation;
	public:
		GPUBuffer(void* data, size_t dataSize, vk::BufferUsageFlags usage);
		~GPUBuffer();

		//Can only move, cant copy
		GPUBuffer(const GPUBuffer&) = delete;
		GPUBuffer operator=(const GPUBuffer&) = delete;

		GPUBuffer(GPUBuffer&& other) noexcept
		{
			Logger::gTrace("GPUBuffer moved !");
			this->mBuffer = other.mBuffer;
			this->mAllocation = other.mAllocation;
			other.mBuffer = nullptr;
			other.mAllocation = nullptr;
		}

		GPUBuffer& operator=(GPUBuffer&& other) noexcept
		{
			Logger::gTrace("GPUBuffer moved !");
			this->mBuffer = other.mBuffer;
			this->mAllocation = other.mAllocation;
			other.mBuffer = nullptr;
			other.mAllocation = nullptr;
		};


		vk::Buffer getBuffer() const { return mBuffer; }
	};

	class CPUBuffer
	{
	private:
		vk::Buffer mBuffer;
		vma::Allocation mAllocation;
		vma::AllocationInfo mInfo;
	public:
		CPUBuffer(void* data, size_t dataSize, vk::BufferUsageFlags usage);
		~CPUBuffer();

		void write(const void* data, size_t size);

		const vma::AllocationInfo& getInfo() const { return mInfo;  }

		CPUBuffer(const CPUBuffer&) = delete;
		CPUBuffer operator=(const CPUBuffer&) = delete;

		CPUBuffer(CPUBuffer&& other) noexcept
		{
			Logger::gTrace("CPUBuffer moved !");
			this->mBuffer = other.mBuffer;
			this->mAllocation = other.mAllocation;
			other.mBuffer = nullptr;
			other.mAllocation = nullptr;
		}

		CPUBuffer& operator=(CPUBuffer&& other) noexcept
		{
			Logger::gTrace("CPUBuffer moved !");
			this->mBuffer = other.mBuffer;
			this->mAllocation = other.mAllocation;
			other.mBuffer = nullptr;
			other.mAllocation = nullptr;

			return *this;
		};

		vk::Buffer getBuffer() const { return mBuffer; }
	};


	class Image2D
	{
	private:
		vk::Image mImage;
		uint32_t mMipLevels;
		vma::Allocation mAllocation;
		vk::ImageView mImageView;
	public:
		Image2D(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags,
			void* data = nullptr, size_t sizeInBytes = 0);
		~Image2D();

		Image2D(const Image2D&) = delete;
		Image2D operator=(const Image2D&) = delete;
		Image2D(Image2D&& other) noexcept
		{
			Logger::gTrace("Image2D moved !");
			this->mImage = other.mImage;
			this->mAllocation = other.mAllocation;
			this->mImageView = other.mImageView;

			other.mImage = nullptr;
			other.mAllocation = nullptr;
			other.mImageView = nullptr;
		};
		Image2D& operator=(Image2D&& other) noexcept
		{
			Logger::gTrace("Image2D moved !");
			this->mImage = other.mImage;
			this->mAllocation = other.mAllocation;
			this->mImageView = other.mImageView;

			other.mImage = nullptr;
			other.mAllocation = nullptr;
			other.mImageView = nullptr;
			return *this;
		};



		uint32_t getMipLevels() const { return mMipLevels; }
		vk::Image getImage() const { return mImage; }
		vk::ImageView getImageView() const { return mImageView; }
	};


	class CombinedImageSampler2D
	{
	private:
		Image2D mImage;
		vk::Sampler mSampler;
	public:
		CombinedImageSampler2D(uint32_t width, uint32_t height, 
			vk::Format format, 
			vk::ImageUsageFlags usage,
			vk::ImageAspectFlags aspectFlags,
			void* data = nullptr, size_t sizeInBytes = 0);
		~CombinedImageSampler2D();

		CombinedImageSampler2D(const CombinedImageSampler2D&) = delete;
		CombinedImageSampler2D operator=(const CombinedImageSampler2D&) = delete;
		CombinedImageSampler2D(CombinedImageSampler2D&& other) noexcept :
			mImage(std::move(other.mImage))
		{
			Logger::gTrace("CombinedImageSampler2D moved !");
			this->mSampler = other.mSampler;

			other.mSampler = nullptr;
		};
		CombinedImageSampler2D& operator=(CombinedImageSampler2D&& other) noexcept
		{
			Logger::gTrace("Image2D moved !");
			this->mImage = std::move(other.mImage);
			this->mSampler = other.mSampler;

			other.mSampler = nullptr;
			return *this;
		};


		const Image2D& getImage() const { return mImage;  }
		vk::Sampler getSampler() const { return mSampler;  }
	};

	class GlobalUniformBuffer
	{
	public:
		struct Data
		{
			glm::mat4x4 mProjection = { 1.0f };
			glm::mat4x4 mView = { 1.0f };
			glm::vec3 mCameraPosition = { 0, 0, 0 };
		};

		class DescriptorSet
		{
		private:
			CPUBuffer mBuffer;
			vk::DescriptorSet mSet;
		public:
			DescriptorSet(vk::DescriptorSetLayout layout) :
				mBuffer(nullptr, sizeof(Data), vk::BufferUsageFlagBits::eUniformBuffer),
				mSet(nullptr)
			{
				vk::DescriptorSetAllocateInfo ai{};
				ai.setDescriptorPool(getDescriptorPool())
					.setDescriptorSetCount(1)
					.setSetLayouts(layout);
				this->mSet = getDevice().allocateDescriptorSets(ai).at(0);

				vk::DescriptorBufferInfo bufferInfo{};
				bufferInfo.setBuffer(this->mBuffer.getBuffer())
					.setOffset(0)
					.setRange(sizeof(Data));

				vk::WriteDescriptorSet write{};
				write.setDstSet(mSet)
					.setDstBinding(0)
					.setDstArrayElement(0)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBufferInfo(bufferInfo);
				getDevice().updateDescriptorSets(write, {});
			}
			~DescriptorSet()
			{
				getDevice().freeDescriptorSets(getDescriptorPool(), mSet);
			}

			CPUBuffer& getCPUBuffer() { return mBuffer;  }
			vk::DescriptorSet getSet() const { return mSet; }
		};

	private:
		vk::DescriptorSetLayout mLayout;
		std::vector<std::unique_ptr<DescriptorSet>> mDesc;

	public:
		GlobalUniformBuffer(size_t frameCount);
		~GlobalUniformBuffer();

		void update(const Data& data, size_t frameIndex);

		const DescriptorSet& getDescriptorSet(size_t currentFrame);
		vk::DescriptorSetLayout getLayout() const { return mLayout;  }
	};


	//Renderpasses
	class DefaultRenderPass
	{
	private:
		vk::RenderPass mRenderPass;
		vk::Framebuffer mFramebuffer;
		Image2D mPosition, mNormal, mAlbedo, mMr, mDrawImage, mDepth;
	public:
		DefaultRenderPass(uint32_t width, uint32_t height, vk::Format format);
		~DefaultRenderPass();

		void begin(const vk::CommandBuffer& cmd, vk::Rect2D drawExtent) const;
		

		vk::RenderPass getRenderPass() const { return mRenderPass; }
		vk::Framebuffer getFramebuffer() const { return mFramebuffer; }
		Image2D& getDrawImage() { return mDrawImage; }
		Image2D& getPosition() { return mPosition; }
		Image2D& getNormal() { return mNormal; }
		Image2D& getAlbedo() { return mAlbedo; }
		Image2D& getMr() { return mMr; }

		const Image2D& getDrawImage() const  { return mDrawImage; }
		const Image2D& getPosition() const { return mPosition; }
		const Image2D& getNormal() const { return mNormal; }
		const Image2D& getAlbedo() const { return mAlbedo; }
		const Image2D& getMr() const { return mMr; }
	};
}