#pragma once


struct GLFWwindow;

namespace eg::Input
{
	namespace Keyboard
	{
		void create(GLFWwindow* window);

		void update();

		bool isKeyDown(int glfwKeyCode);
		bool isKeyDownOnce(int glfwKeyCode);
	}

	namespace Mouse
	{
		void create(GLFWwindow* window);
	}
}