#include <SandBox_MapObject.h>

namespace sndbx
{
	MapObject::MapObject(const std::string& modelPath)
	{
		mModel = eg::Data::StaticModelCache::load(modelPath);

	}

	void MapObject::update(float delta)
	{
		
	}

	void MapObject::render(vk::CommandBuffer cmd, eg::Renderer::RenderStage stage)
	{
		switch (stage)
		{
		case eg::Renderer::RenderStage::SUBPASS0_GBUFFER:
			eg::Data::StaticModelRenderer::render(cmd, *mModel, mTransform.build());
			break;
		default:
			break;
		}
	}
}