#pragma once

#include <World.h>
#include <Components.h>

namespace sndbx
{
	class Debugger
	{
	public:
		Debugger() = default;
		~Debugger() = default;

		void checkKeyboardInput();

		void drawLightDebuggerDialog(eg::Components::DirectionalLight& directionalLight);
		void drawWorldDebuggerDialog(eg::World::GameObjectManager& gameObjManager, eg::World::JsonToIGameObjectDispatcher dispatcherFn);
		void drawPhysicsDebuggerDialog();
	private:
		bool mEnabled = false;

	};
}