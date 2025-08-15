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

		void drawWorldDebuggerDialog(eg::World::GameObjectManager& gameObjManager, eg::World::JsonToIGameObjectDispatcher dispatcherFn);
		void drawPhysicsDebuggerDialog();
		void drawRendererSettingsDialog();
	private:
		bool mEnabled = false;

	};
}