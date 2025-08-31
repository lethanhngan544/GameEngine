#include <Renderer.h>
#include <Core.h>

namespace eg::Renderer::DefaultRenderPass
{
	vk::RenderPass mRenderPass;
	vk::Framebuffer mFramebuffer;
	std::optional<Image2D> mNormal, mAlbedo, mMr, mDrawImage, mDepth;
	vk::Format mDrawImageFormat;


	void create(uint32_t width, uint32_t height, vk::Format format)
	{
		mDrawImageFormat = format; 
		mNormal.emplace(width, height, vk::Format::eR32G32B32A32Sfloat,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);

		mAlbedo.emplace(width, height, vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);

		mMr.emplace(width, height, vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);

		mDrawImage.emplace(width, height, format,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
			vk::ImageAspectFlagBits::eColor);

		mDepth.emplace(width, height, vk::Format::eD24UnormS8Uint,
			vk::ImageUsageFlagBits::eSampled |
			vk::ImageUsageFlagBits::eDepthStencilAttachment |
			vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth);


		vk::AttachmentDescription attachments[] =
		{
			//Normal, id = 0
			vk::AttachmentDescription(
				(vk::AttachmentDescriptionFlags)0,
				vk::Format::eR32G32B32A32Sfloat,
				vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear,
				vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare,
				vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eColorAttachmentOptimal
			),

				//Albedo, id = 1
				vk::AttachmentDescription(
					(vk::AttachmentDescriptionFlags)0,
					vk::Format::eR8G8B8A8Unorm,
					vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare,
					vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eColorAttachmentOptimal
				),

				//Mr, id = 2
				vk::AttachmentDescription(
					(vk::AttachmentDescriptionFlags)0,
					vk::Format::eR8G8B8A8Unorm,
					vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare,
					vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eColorAttachmentOptimal
				),

				//Depth, id = 3
				vk::AttachmentDescription(
					(vk::AttachmentDescriptionFlags)0,
					vk::Format::eD24UnormS8Uint,
					vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eDepthStencilAttachmentOptimal
				),

				//Draw image, id = 4
				vk::AttachmentDescription(
					(vk::AttachmentDescriptionFlags)0,
					format,
					vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear,
					vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare,
					vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined,
					vk::ImageLayout::eShaderReadOnlyOptimal
				)

		};

		vk::AttachmentReference pass0OutputAttachmentRef[] =
		{
			vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentReference(1, vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentReference(2, vk::ImageLayout::eColorAttachmentOptimal),
		};

		vk::AttachmentReference pass0OutputDepthStencilAttachmentRef = vk::AttachmentReference(3, vk::ImageLayout::eDepthStencilAttachmentOptimal); //Depth;

		vk::AttachmentReference pass1InputAttachmentRef[] =
		{
			vk::AttachmentReference(0, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentReference(1, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentReference(2, vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentReference(3, vk::ImageLayout::eShaderReadOnlyOptimal)
		};


		vk::AttachmentReference pass1OutputAttachmentRef[] =
		{
			vk::AttachmentReference(4, vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::AttachmentReference pass1OutputDepthStencilAttachmentRef = vk::AttachmentReference(3, vk::ImageLayout::eDepthStencilAttachmentOptimal); //Depth;


		vk::SubpassDescription subpasses[] = {
			//Subpass 0
			vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
				vk::PipelineBindPoint::eGraphics,
				0, nullptr, // Input
				sizeof(pass0OutputAttachmentRef) / sizeof(pass0OutputAttachmentRef[0]), pass0OutputAttachmentRef, //Output
				nullptr, //Resolve
				&pass0OutputDepthStencilAttachmentRef,
				0, nullptr//Preserve
			),
				//Subpass 1
				vk::SubpassDescription((vk::SubpassDescriptionFlags)0,
					vk::PipelineBindPoint::eGraphics,
					sizeof(pass1InputAttachmentRef) / sizeof(pass1InputAttachmentRef[0]), pass1InputAttachmentRef, // Input
					sizeof(pass1OutputAttachmentRef) / sizeof(pass1OutputAttachmentRef[0]), pass1OutputAttachmentRef, //Output
					nullptr, //Resolve
					&pass1OutputDepthStencilAttachmentRef, //Depth
					0, nullptr//Preserve
				),


		};

		vk::SubpassDependency dependencies[] = {
			vk::SubpassDependency(0, 1,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite,
				vk::AccessFlagBits::eInputAttachmentRead,
				vk::DependencyFlagBits::eByRegion),
		};

		vk::RenderPassCreateInfo renderPassCI{};
		renderPassCI
			.setAttachments(attachments)
			.setSubpasses(subpasses)
			.setDependencies(dependencies);

		mRenderPass = getDevice().createRenderPass(renderPassCI);


		vk::ImageView frameBufferAttachments[] =
		{
			mNormal->getImageView(),
			mAlbedo->getImageView(),
			mMr->getImageView(),
			mDepth->getImageView(),
			mDrawImage->getImageView(),
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(width)
			.setHeight(height)
			.setLayers(1);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);

	}

	void destroy()
	{
		mNormal.reset();
		mAlbedo.reset();
		mMr.reset();
		mDrawImage.reset();
		mDepth.reset();
		getDevice().destroyFramebuffer(mFramebuffer);
		getDevice().destroyRenderPass(mRenderPass);
	}

	void resize(uint32_t width, uint32_t height)
	{
		//Destroy framebuffer, images
		getDevice().destroyFramebuffer(mFramebuffer);
		mNormal.reset();
		mAlbedo.reset(); 
		mMr.reset();
		mDrawImage.reset();
		mDepth.reset();

		mNormal.emplace(width, height, vk::Format::eR32G32B32A32Sfloat,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mAlbedo.emplace(width, height, vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mMr.emplace(width, height, vk::Format::eR8G8B8A8Unorm,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eColor);
		mDrawImage.emplace(width, height, mDrawImageFormat,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
			vk::ImageAspectFlagBits::eColor);

		mDepth.emplace(width, height, vk::Format::eD24UnormS8Uint,
			vk::ImageUsageFlagBits::eSampled |
			vk::ImageUsageFlagBits::eDepthStencilAttachment |
			vk::ImageUsageFlagBits::eInputAttachment,
			vk::ImageAspectFlagBits::eDepth);

		vk::ImageView frameBufferAttachments[] =
		{
			mNormal->getImageView(),
			mAlbedo->getImageView(),
			mMr->getImageView(),
			mDepth->getImageView(),
			mDrawImage->getImageView(),
		};
		vk::FramebufferCreateInfo framebufferCI{};
		framebufferCI.setRenderPass(mRenderPass)
			.setAttachments(frameBufferAttachments)
			.setWidth(width)
			.setHeight(height)
			.setLayers(1);


		mFramebuffer = getDevice().createFramebuffer(framebufferCI);
	}

	void begin(const vk::CommandBuffer& cmd)
	{
		vk::ClearValue clearValues[] =
		{
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 0.0f}),
			vk::ClearDepthStencilValue(1.0f, 0),
			vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
		};

		Command::Var* renderScaleCVar = Command::findVar("eg::Renderer::ScreenRenderScale");
		Command::Var* widthCVar = Command::findVar("eg::Renderer::ScreenWidth");
		Command::Var* heightCVar = Command::findVar("eg::Renderer::ScreenHeight");


		uint32_t scaledWidth = static_cast<uint32_t>(widthCVar->value * renderScaleCVar->value);
		uint32_t scaledHeight = static_cast<uint32_t>(heightCVar->value * renderScaleCVar->value);

		vk::RenderPassBeginInfo renderPassBI{};
		renderPassBI.setRenderPass(mRenderPass)
			.setFramebuffer(mFramebuffer)
			.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(scaledWidth, scaledHeight)))
			.setClearValues(clearValues);


		cmd.beginRenderPass(renderPassBI, vk::SubpassContents::eSecondaryCommandBuffers);
	}


	vk::RenderPass getRenderPass() { return mRenderPass; }
	vk::Framebuffer getFramebuffer() { return mFramebuffer; }
	Image2D& getDrawImage() { return *mDrawImage; }
	Image2D& getNormal() { return *mNormal; }
	Image2D& getAlbedo() { return *mAlbedo; }
	Image2D& getMr() { return *mMr; }
	Image2D& getDepth() { return *mDepth; }
}