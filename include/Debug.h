#pragma once

#include <MyVulkan.h>
#include <Renderer.h>

namespace eg::Debug
{
	void create();
	void destroy();

	void checkForKeyboardInput();
	void render(vk::CommandBuffer cmd);
	
	Renderer::Image2D& getImage();

	bool enabled();
}