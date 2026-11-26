#pragma once
#include "Engine/EngineCommon.h"
#include "Engine/Input/RawInputState.h"
#include "Game/Aircraft/AircraftController.h"
#include "Game/Scene/FlightScene.h"
#include <chrono>
#include "Game/Audio/CombatAudio.h"
#include "Game/UI/CombatHudPresenter.h"
#include "Game/Effects/Presentation/CombatEffectPresenter.h"
#include "Game/Combat/CombatRumble.h"

namespace Engine {
	class Window;
	class Renderer;
	struct TimedInputSample;
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
		double UpdateFrameTiming();
		void UpdateInputDevice(const Engine::InputSnapshot& input);
		void UpdateMenu(const Engine::InputSnapshot& input);
		void UpdateGameplay(const Engine::InputSnapshot& input, double deltaSeconds);
		void StepGameplay(const Engine::InputSnapshot& input, float stepSeconds);
		void UpdateGameplayFrame();
		void UpdateMenuFrame();
		void BuildMenuHud();
		enum class Screen { Title, Playing, Clear, GameOver };
		Screen m_screen = Screen::Title;
		void PrepareCaptureScene();
		HINSTANCE m_hInstance;
		std::unique_ptr<Window> m_window;

		std::unique_ptr<Renderer> m_renderer;

		std::unique_ptr<FlightScene> m_scene;
		AircraftController m_aircraftController;
		CombatHudPresenter m_hudPresenter;
		CombatEffectPresenter m_effectPresenter;
		CombatAudio m_combatAudio;
		CombatRumble m_combatRumble;
		double m_pendingSimulationSeconds = 0;
		Engine::InputSnapshot m_gameplayInput;
		std::vector<Engine::TimedInputSample> m_inputSamples;
		Engine::Scene m_previousRenderScene;
		Engine::Scene m_renderScene;
		CombatSnapshot m_previousCombatState;
		bool m_hasRenderHistory = false;
		double m_previousSceneSeconds = 0;
		bool m_usingGamepad = false;
		Engine::HudDrawList m_hud;

		// F6: normal -> environment with ships -> environment only.
		unsigned m_captureMode = 0;
		Engine::Scene m_captureScene;
		std::vector<std::shared_ptr<Engine::Model>> m_captureShipModels;
		std::wstring m_windowTitle = L"SkyMission";

		// フレーム時間とFPSの計測
		double m_sceneSeconds = 0;
		std::chrono::steady_clock::time_point m_lastTime;
		float m_fpsTimer = 0.0f;
		UINT m_fpsFrameCount = 0;
	};
} // namespace Game
