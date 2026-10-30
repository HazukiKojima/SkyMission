#pragma once
#include "Engine/EngineCommon.h"
#include "Game/Aircraft/AircraftController.h"
#include "Game/Scene/FlightScene.h"
#include <chrono>
#include "Game/Audio/CombatAudio.h"
#include "Game/UI/CombatHudPresenter.h"

namespace Engine {
	class Window;
	class Renderer;
} // namespace Engine

namespace Game {
	struct FlightSceneDefinition;
	using Engine::Renderer;
	using Engine::Window;

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
		void InitializeFlightScene(const FlightSceneDefinition& assets);
		void ConfigureResizeHandling();
		HINSTANCE m_hInstance;
		std::unique_ptr<Window> m_window;

		std::unique_ptr<Renderer> m_renderer;

		std::unique_ptr<FlightScene> m_scene;
		AircraftController m_aircraftController;
		CombatHudPresenter m_hudPresenter;
		CombatAudio m_combatAudio;
		Engine::HudDrawList m_hud;
		std::wstring m_windowTitle = L"SkyMission";
		// timing
		float m_elapsedTime = 0.0f;
		std::chrono::steady_clock::time_point m_lastTime;
		float m_fpsTimer = 0.0f;
		UINT m_fpsFrameCount = 0;
	};
} // namespace Game
