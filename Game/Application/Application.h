#pragma once
#include "Engine/EngineCommon.h"
#include "Game/Aircraft/AircraftController.h"
#include "Game/Scene/FlightScene.h"
#include <chrono>
#include "Game/Audio/CombatAudio.h"
#include "Game/UI/CombatHudPresenter.h"
#include "Game/Effects/CombatEffectPresenter.h"

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
		void BuildMenuHud();
		enum class Screen { Title, Playing, Clear };
		Screen m_screen = Screen::Title;
		const Engine::Scene& PrepareCaptureScene();
		HINSTANCE m_hInstance;
		std::unique_ptr<Window> m_window;

		std::unique_ptr<Renderer> m_renderer;

		std::unique_ptr<FlightScene> m_scene;
		AircraftController m_aircraftController;
		CombatHudPresenter m_hudPresenter;
		CombatEffectPresenter m_effectPresenter;
		CombatAudio m_combatAudio;
		Engine::HudDrawList m_hud;
		// F6: normal -> environment with ships -> environment only.
		unsigned m_captureMode = 0;
		Engine::Scene m_captureScene;
		std::vector<std::shared_ptr<Engine::Model>> m_captureShipModels;
		std::wstring m_windowTitle = L"SkyMission";
		// フレーム時間とFPSの計測
		float m_elapsedTime = 0.0f;
		std::chrono::steady_clock::time_point m_lastTime;
		float m_fpsTimer = 0.0f;
		UINT m_fpsFrameCount = 0;
	};
} // namespace Game
