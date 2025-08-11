#include "Renderer.h"


namespace eg::Renderer
{
	ShadowRenderPass::ShadowRenderPass(uint32_t size)
	{
		vk::ImageCreateInfo imageCI{};
		imageCI.setImageType(vk::ImageType::e2D)
			.setExtent({ size, size, 1 })
			.setMipLevels(1)
			.setArrayLayers(static_cast<uint32_t>(mCsmCount))
			.setFormat(mDepthFormat)
			.setTiling(vk::ImageTiling::eOptimal)
			.setInitialLayout(vk::ImageLayout::eUndefined)
			.setUsage(mUsageFlags)
			.setSharingMode(vk::SharingMode::eExclusive)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setFlags(vk::ImageCreateFlags{});
		vma::AllocationCreateInfo allocCI{};
		allocCI.setUsage(vma::MemoryUsage::eGpuOnly);
		auto [image, allocation] = getAllocator().createImage(imageCI, allocCI);
		mDepthImage = image;
		mDepthAllocation = allocation;

		vk::ImageViewCreateInfo imageViewCI{};
		imageViewCI.setImage(mDepthImage)
			.setViewType(vk::ImageViewType::e2DArray)
			.setFormat(mDepthFormat)
			.setComponents(vk::ComponentMapping{})
			.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, static_cast<uint32_t>(mCsmCount) });
		mDepthImageView = getDevice().createImageView(imageViewCI);

		//Sampler
		vk::SamplerCreateInfo ci{};
		ci.setMagFilter(vk::Filter::eLinear)
			.setMinFilter(vk::Filter::eLinear)
			.setMipmapMode(vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eRepeat)
			.setAddressModeV(vk::SamplerAddressMode::eRepeat)
			.setAddressModeW(vk::SamplerAddressMode::eRepeat)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(1)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways);

		mDepthSampler = getDevice().createSampler(ci);


		//Create sampler for depth
		vk::SamplerCreateInfo samplerCI{};
		samplerCI.setMagFilter(vk::Filter::eNearest)
			.setMinFilter(vk::Filter::eNearest)
			.setMipmapMode(vk::SamplerMipmapMode::eLinear)
			.setAddressModeU(vk::SamplerAddressMode::eClampToBorder)
			.setAddressModeV(vk::SamplerAddressMode::eClampToBorder)
			.setAddressModeW(vk::SamplerAddressMode::eClampToBorder)
			.setMipLodBias(0)
			.setMinLod(-1)
			.setMaxLod(0)
			.setAnisotropyEnable(false)
			.setMaxAnisotropy(0.0f)
			.setCompareEnable(false)
			.setCompareOp(vk::CompareOp::eAlways)
			.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);

		mDepthSampler = getDevice().createSampler(samplerCI);

		vk::AttachmentDescription attachments[] =
		{
				//Depth, id = 0
				vk::AttachmentDescription(
					(vk::AttachmentDescriptionFlags)0,
					vk::Format::eD32Sfloat,
					vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare,
					vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eShaderReadOnlyOptimal
				),

		};


		vk::AttachmentReference pass0OutputDepthStencilAttachmentRef = vk::AttachmentReference(0, vk::ImageLayout::eDepthStencilAttachmentOptimal); //Depth;

		vk::SubpassDescription subpasses[] = {
			//Subpass 0
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				0, nullptr, //Output
				nullptr, //Resolve
				&pass0OutputDepthStencilAttachmentRef,
				0, nullptr//Preserve
			),


		};

		vk::SubpassDependency dependencies[] = {
			// External -> Shadow map pass: ensure previous depth reads/writes are done
			vk::SubpassDependency(
				VK_SUBPASS_EXTERNAL, 0,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eEarlyFragmentTests,
				vk::AccessFlagBits::eShaderRead,
				vk::AccessFlagBits::eDepthStencilAttachmentWrite,
				vk::DependencyFlagBits::eByRegion
			),

			// Shadow map pass -> External: ensure depth write is visible to future shader reads
			vk::SubpassDependency(
				0, VK_SUBPASS_EXTERNAL,
				vk::PipelineStageFlagBits::eLateFragmentTests,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eDepthStencilAttachmentWrite,
				vk::AccessFlagBits::eShaderRead,
				vk::DependencyFlagBits::eByRegion
			)
		};
		vk::RenderPassCreateInfo renderPassCI{};
		renderPassCI
			.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		mRenderPass = getDevice().createRenderPass(renderPassCI);


		vk::ImageView frameBufferAttachments[] =
		{
			mDepthImageView
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(size)
			.setHeight(size)
			.setLayers(mCsmCount);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);

	}


	void ShadowRenderPass::begin(const vk::CommandBuffer& cmd) const
	{
		vk::ClearValue clearValues[] =
		{
			vk::ClearDepthStencilValue(1.0f, 0),
		};

		vk::RenderPassBeginInfo renderPassBI{};
		renderPassBI.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0),
				vk::Extent2D(Renderer::getShadowMapResolution(), Renderer::getShadowMapResolution())))
			.setClearValues(clearValues);


		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eSecondaryCommandBuffers);
	}


	ShadowRenderPass::~ShadowRenderPass()
	{
		getDevice().destroySampler(mDepthSampler);
		getDevice().destroyImageView(mDepthImageView);
		getAllocator().destroyImage(mDepthImage, mDepthAllocation);

		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}
}