#include <Window.h>

#define GLFW_INCLUDE_NONE
#include <glfw/glfw3.h>
#include <string>
#include <stdexcept>

 
namespace eg::Window
{
	static GLFWwindow* gWindow = nullptr;
	static std::string gTitle;

	void create(uint32_t width, uint32_t height, const char* title)
	{

		if (!glfwInit())
			throw std::runtime_error("Failed to init glfw !");
		
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
		gWindow = glfwCreateWindow(width, height, title, nullptr, nullptr);
		if (!gWindow)
		{
			glfwTerminate();
			throw std::runtime_error("Failed to create glfw window !");
		}

		if (!glfwVulkanSupported())
		{
			throw std::runtime_error("Vulkan is not avalidable !"); 
		}
		glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		

	}
	void destroy()
	{
		glfwDestroyWindow(gWindow);
		glfwTerminate();
	}

	bool shouldClose()
	{
		return glfwWindowShouldClose(gWindow);
	}

	void poll()
	{
		glfwPollEvents();
	}

	GLFWwindow* getHandle()
	{
		return gWindow;
	}

}
