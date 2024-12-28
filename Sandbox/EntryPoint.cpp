
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "Core.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
	using namespace eg;
	try
	{
		Logger::create(std::make_unique<VisualStudioLogger>());
		Window::create(1600, 900, "Sandbox");
		Renderer::create(1600, 900);


		while (!Window::shouldClose())
		{
			Window::poll();
		}

		Renderer::destory();
		Window::destroy();
	}
	catch (std::exception& e)
	{
		MessageBox(nullptr, e.what(), "Standard exception", MB_ICONEXCLAMATION);
	}

	catch (...)
	{
		MessageBox(nullptr, "TODO", "Unknown exception", MB_ICONEXCLAMATION);
	}


	return 0;
}