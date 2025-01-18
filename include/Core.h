#pragma once


#include "Logger.h"
#include "Renderer.h"
#include "Data.h"

struct GLFWwindow;
namespace eg::Window
{
	void create(uint32_t width, uint32_t height, const char* title);
	void destroy();

	void poll();
	bool shouldClose();



	GLFWwindow* getHandle();

}



