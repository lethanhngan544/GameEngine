#include <Renderer.h>


namespace eg::Renderer
{
	Image2D::Image2D(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags,
		uint32_t miplevels)
	{
		vk::ImageCreateInfo imageCI{};
		imageCI.setImageType(vk::ImageType::e2D)
			.setExtent({ width, height, 1 })
			.setMipLevels(miplevels)
			.setArrayLayers(1)
			.setFormat(format)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(usage)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setFlags(vk::ImageCreateFlags{});
		vma::AllocationCreateInfo allocCI{};
		allocCI.setUsage(vma::MemoryUsage::eAutoPreferDevice);
		auto [image, allocation] = getAllocator().createImage(imageCI, allocCI);
		this->mImage = image;
		this->mAllocation = allocation;

		vk::ImageSubresourceRange subresourceRange{};
		subresourceRange.setAspectMask(aspectFlags)
			.setBaseMipLevel(0)
			.setLevelCount(miplevels)
			.setBaseArrayLayer(0)
			.setLayerCount(1);

		vk::ImageViewCreateInfo imageViewCI{};
		imageViewCI.setImage(this->mImage)
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(format)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(subresourceRange);
		this->mImageView = getDevice().createImageView(imageViewCI);

	}
	Image2D::Image2D(uint32_t width, uint32_t height, vk::Format format, vk::ImageUsageFlags usage, vk::ImageAspectFlags aspectFlags,
		void* data, size_t sizeInBytes)
	{
		//Calculate mip levels
		mMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))) + 1);
		if (!data)
			mMipLevels = 1;

		vk::ImageCreateInfo imageCI{};
		imageCI.setImageType(vk::ImageType::e2D)
			.setExtent({ width, height, 1 })
			.setMipLevels(1)
			.setArrayLayers(1)
			.setFormat(format)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(usage | vk::ImageUsageFlagBits::eTransferSrc  | (data ? vk::ImageUsageFlagBits::eTransferDst : (vk::ImageUsageFlags)0))
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setFlags(vk::ImageCreateFlags{});
		vma::AllocationCreateInfo allocCI{};
		allocCI.setUsage(vma::MemoryUsage::eAutoPreferDevice);
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
	
					//Copy data to image

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
						{ vk::BufferImageCopy(0, 0, 0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
							{0, 0, 0}, {width, height, 1}) });

					//int32_t mipWidth = width;
					//int32_t mipHeight = height;
					//for (uint32_t i = 1; i < mMipLevels; i++)
					//{

					//	//Transition previous mip level to transfer src
					//	cmd.pipelineBarrier(
					//		vk::PipelineStageFlagBits::eAllCommands,
					//		vk::PipelineStageFlagBits::eAllCommands,
					//		vk::DependencyFlagBits::eByRegion
					//		, {}, {},
					//	{
					//		vk::ImageMemoryBarrier
					//		(
					//			vk::AccessFlagBits::eTransferWrite,
					//			vk::AccessFlagBits::eTransferRead,
					//			vk::ImageLayout::eTransferDstOptimal,
					//			vk::ImageLayout::eTransferSrcOptimal,
					//			{},
					//			{},
					//			mImage,
					//			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i - 1, 1, 0, 1)

					//		)
					//	});

					//	//Transition current mip level to transfer dst
					//	cmd.pipelineBarrier(
					//		vk::PipelineStageFlagBits::eAllCommands,
					//		vk::PipelineStageFlagBits::eAllCommands,
					//		vk::DependencyFlagBits::eByRegion
					//		, {}, {},
					//	{
					//		vk::ImageMemoryBarrier
					//		(
					//			{},
					//			vk::AccessFlagBits::eTransferRead,
					//			vk::ImageLayout::eUndefined,
					//			vk::ImageLayout::eTransferDstOptimal,
					//			{},
					//			{},
					//			mImage,
					//			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, i, 1, 0, 1)

					//		)
					//	});

					//	//Blitz mip level i-1 to i
					//	cmd.blitImage(mImage, vk::ImageLayout::eTransferSrcOptimal, mImage, vk::ImageLayout::eTransferDstOptimal,
					//		{
					//			vk::ImageBlit
					//			(
					//				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1),
					//				{ vk::Offset3D{0, 0, 0}, vk::Offset3D{mipWidth, mipHeight, 1} },
					//				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1),
					//				{ vk::Offset3D{0, 0, 0}, vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1} }
					//			)
					//		},
					//		vk::Filter::eLinear
					//	);

	

					//	if (mipWidth > 1) mipWidth /= 2;
					//	if (mipHeight > 1) mipHeight /= 2;
					//}
					

				
					//Transition shader read only except the last mip level
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

					/*cmd.pipelineBarrier(
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
								vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, mMipLevels - 1, 1, 0, 1)

							)
						});*/
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

		//I am having a stroke

		vk::SamplerCreateInfo ci{};
		ci.setMagFilter(vk::Filter::eLinear)
			.setMinFilter(vk::Filter::eLinear)
			.setMipmapMode(vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(static_cast<uint32_t>(mImage.getMipLevels()))
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