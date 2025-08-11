#include "Renderer.h"

namespace eg::Renderer
{
	PostprocessingRenderPass::PostprocessingRenderPass(uint32_t width, uint32_t height, vk::Format format) :
		mDrawImage(width, height, format,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
			vk::ImageAspectFlagBits::eColor)
	{
		vk::AttachmentDescription attachments[] =
		{
			//Draw image, id = 0
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				format,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eLoad,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eTransferSrcOptimal // For copying to swapchain image
			)
		};

		vk::AttachmentReference pass0OutputAttachmentRef[] =
		{
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
		};

		vk::SubpassDescription subpasses[] = {
			//Subpass 0
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				sizeof(pass0OutputAttachmentRef) / sizeof(pass0OutputAttachmentRef[0]), pass0OutputAttachmentRef, //Output
				nullptr, //Resolve
				nullptr, //Depth
				0, nullptr//Preserve
			),
		};

		vk::SubpassDependency dependencies[] = {
			// External -> Subpass 0
			vk::SubpassDependency(
				VK_SUBPASS_EXTERNAL,            // from outside render pass
				0,                               // to subpass 0
				vk::PipelineStageFlagBits::eFragmentShader,         // reading resources before draw
				vk::PipelineStageFlagBits::eColorAttachmentOutput,  // going to write colors
				vk::AccessFlagBits::eShaderRead,                     // possibly reading textures
				vk::AccessFlagBits::eColorAttachmentWrite,           // writing color output
				vk::DependencyFlagBits::eByRegion
			),

				// Subpass 0 -> External
				vk::SubpassDependency(
					0,                               // from subpass 0
					VK_SUBPASS_EXTERNAL,             // to outside render pass (present or next pass)
					vk::PipelineStageFlagBits::eColorAttachmentOutput,  // wait for color writes
					vk::PipelineStageFlagBits::eBottomOfPipe,            // before GPU finishes
					vk::AccessFlagBits::eColorAttachmentWrite,           // color writes
					vk::AccessFlagBits::eMemoryRead,                     // make visible to presentation engine
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
			mDrawImage.getImageView(),
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(width)
			.setHeight(height)
			.setLayers(1);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);
	}
	PostprocessingRenderPass::~PostprocessingRenderPass()
	{
		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}
	void PostprocessingRenderPass::begin(const vk::CommandBuffer& cmd) const
	{
		vk::ClearValue clearValues[] =
		{
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
		};

		vk::RenderPassBeginInfo renderPassBI{};
		renderPassBI.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffer)
			.setRenderArea(Renderer::getDrawExtent())
			.setClearValues(clearValues);


		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eInline);
	}
}