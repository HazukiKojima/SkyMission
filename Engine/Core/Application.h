#pragma once
#include "../EngineCommon.h"
#include "Camera.h"
#include <chrono>

namespace Engine {
	class Window;
	class Renderer;

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

		std::unique_ptr<Renderer> m_renderer;

		// Camera
		std::unique_ptr<Engine::Camera> m_camera;
		// timing
		float m_elapsedTime = 0.0f;
		std::chrono::steady_clock::time_point m_lastTime;
		float m_fpsTimer = 0.0f;
		UINT m_fpsFrameCount = 0;
	};
}