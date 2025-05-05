#include "Renderer.h"


namespace eg::Renderer
{
	ShadowRenderPass::ShadowRenderPass(uint32_t size) :
		mDepth(size, size, vk::Format::eD32Sfloat, vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
			vk::ImageAspectFlagBits::eDepth)
	{
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
			mDepth.getImageView(),
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(size)
			.setHeight(size)
			.setLayers(1);


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


		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eInline);
	}


	ShadowRenderPass::~ShadowRenderPass()
	{
		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}
}