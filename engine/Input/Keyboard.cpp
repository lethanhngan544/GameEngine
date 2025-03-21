#include <Input.h>
#include <bitset>

#include <GLFW/glfw3.h>

namespace eg::Input::Keyboard
{
	static std::bitset<512> gKeys, gPrevKeys;

	void create(GLFWwindow* window)
	{
		gKeys.reset();

		glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
			{
				if (key <= 0 || key >= gKeys.size())
				{
					return;
				}

				if (action == GLFW_PRESS)
				{
					gKeys[key] = true;
				}
				else if (action == GLFW_RELEASE)
				{
					gKeys[key] = false;
				}
			});
	}

	void update()
	{
		gPrevKeys = gKeys;
	}

	bool isKeyDown(int glfwKeyCode)
	{
		return gKeys[glfwKeyCode];
	}
	bool isKeyDownOnce(int glfwKeyCode)
	{
		return isKeyDown(glfwKeyCode) && !gPrevKeys[glfwKeyCode];
	}
}