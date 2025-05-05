#pragma once

namespace eg::Renderer
{
	enum class RenderStage
	{
		SHADOW,
		SUBPASS0_GBUFFER,
		SUBPASS1_POINTLIGHT,
	};
}