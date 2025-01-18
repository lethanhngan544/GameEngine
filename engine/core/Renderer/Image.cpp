#include <Renderer.h>


namespace eg::Renderer
{
	Image2D::Image2D(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags,
		void* data, size_t sizeInBytes)
	{


		vk::ImageCreateInfo imageCI{};
		imageCI.setImageType(vk::ImageType::e2D)
			.setExtent({ width, height, 1 })
			.setMipLevels(1)
			.setArrayLayers(1)
			.setFormat(format)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(usage | (data ? vk::ImageUsageFlagBits::eTransferDst : (vk::ImageUsageFlags)0))
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setFlags(vk::ImageCreateFlags{});
		vma::AllocationCreateInfo allocCI{};
		allocCI.setUsage(vma::MemoryUsage::eGpuOnly);
		auto [image, allocation] = getAllocator().createImage(imageCI, allocCI);
		this->mImage = image;
		this->mAllocation = allocation;

		vk::ImageViewCreateInfo imageViewCI{};
		imageViewCI.setImage(this->mImage)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(format)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(vk::ImageSubresourceRange{ aspectFlags, 0, 1, 0, 1 });
		this->mImageView = getDevice().createImageView(imageViewCI);


		//Create staging
		if (data)
		{
			vma::AllocationInfo info;
			auto [stagingBuffer, stagingAllocation] = getAllocator().createBuffer(
				vk::BufferCreateInfo{
					{},
					sizeInBytes ,
					vk::BufferUsageFlagBits::eTransferSrc,
				},
				vma::AllocationCreateInfo{
					vma::AllocationCreateFlagBits::eMapped | vma::AllocationCreateFlagBits::eHostAccessSequentialWrite,
					vma::MemoryUsage::eAutoPreferHost,
				},
				info
				);


			std::memcpy(info.pMappedData, data, sizeInBytes);

			immediateSubmit([&](vk::CommandBuffer cmd)
				{
					cmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eAllCommands,
						vk::PipelineStageFlagBits::eAllCommands,
						vk::DependencyFlagBits::eByRegion
						, {}, {},
						{
							vk::ImageMemoryBarrier
							(
								{},
								{},
								vk::ImageLayout::eUndefined,
								vk::ImageLayout::eTransferDstOptimal,
								{},
								{},
								mImage,
								vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)

							)
						});

					cmd.copyBufferToImage(stagingBuffer, this->mImage, vk::ImageLayout::eTransferDstOptimal, 
						{ vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), {0, 0, 0}, {width, height, 1}) });
				
					cmd.pipelineBarrier(
						vk::PipelineStageFlagBits::eAllCommands,
						vk::PipelineStageFlagBits::eAllCommands,
						vk::DependencyFlagBits::eByRegion
						, {}, {},
						{
							vk::ImageMemoryBarrier
							(
								{},
								{},
								vk::ImageLayout::eTransferDstOptimal,
								vk::ImageLayout::eShaderReadOnlyOptimal,
								{},
								{},
								mImage,
								vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)

							)
						});
			});
			getAllocator().destroyBuffer(stagingBuffer, stagingAllocation);
		}

	}

	Image2D::~Image2D()
	{
		getDevice().destroyImageView(this->mImageView);
		getAllocator().destroyImage(this->mImage, this->mAllocation);

	}


	CombinedImageSampler2D::CombinedImageSampler2D(uint32_t width, uint32_t height,
		vk::Format format,
		vk::ImageUsageFlags usage,
		vk::ImageAspectFlags aspectFlags,
		void* data, size_t sizeInBytes) :
		mImage(width, height, format, usage, aspectFlags, data, sizeInBytes)
	{
		vk::SamplerCreateInfo ci{};
		ci.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eNearest)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat)
			.setMipLodBias(-1)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways);

		mSampler = getDevice().createSampler(ci);
	}

	CombinedImageSampler2D::~CombinedImageSampler2D()
	{
		getDevice().destroySampler(mSampler);
	}


}