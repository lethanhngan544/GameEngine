#include <Input.h>

#include <GLFW/glfw3.h>

namespace eg::Input::Mouse
{

	static float xPos = 0.0f, yPos = 0.0f, prevXPos = 0.0f, prevYPos = 0.0f;
	static float deltaX = 0.0f;
	static float deltaY = 0.0f;

	void create(GLFWwindow* window)
	{
		glfwSetCursorPosCallback(window, [](GLFWwindow * window, double xpos, double ypos)
			{
				xPos = xpos;
				yPos = ypos;
			});
	}

	void update()
	{
		deltaX = xPos - prevXPos;
		deltaY = yPos - prevYPos;
		prevXPos = xPos;
		prevYPos = yPos;
	}

	float getDeltaX()
	{
		return deltaX;
	}
	float getDeltaY()
	{
		return deltaY;
	}
}