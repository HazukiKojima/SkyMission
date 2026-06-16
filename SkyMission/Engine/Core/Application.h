#pragma once
#include "../EngineCommon.h"

namespace Engine {
	class Window;
	class RenderDevice;
	class CommandContext;

	class Application {
	public:
		Application(HINSTANCE hInstance);
		virtual ~Application();

		void Initialize();
		int Run();

	protected:
		virtual void Update();
		virtual void Render();

	private:
		HINSTANCE m_hInstance;
		std::unique_ptr<Window> m_window;

		std::unique_ptr<RenderDevice> m_device;
		std::unique_ptr<CommandContext> m_context;
	};
}