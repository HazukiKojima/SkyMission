#include "Application.h"
#include "Engine/Window/Window.h"
#include "Engine/Renderer/Device/Renderer.h"
#include "Game/Scene/FlightSceneLoader.h"
#include <DirectXMath.h>
#include <chrono>
#include <filesystem>
#include "Engine/ExecutablePath.h"
#include "Game/Weapons/WeaponController.h"

namespace Game {
	using namespace Engine;

	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {
	}

	Application::~Application() {
	}

	// アプリケーション初期化処理
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		const auto exeDir = ExecutableDirectory();
		const std::filesystem::path assetsDir = exeDir / L"..\\..\\Assets";
		const FlightSceneLoader sceneLoader;
		const SceneSourcePaths scenePaths = {.environment = assetsDir / L"Environment/environment.json",
			.playerAircraft = assetsDir / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
			.enemyAircraft = assetsDir / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
			.retroFilm = assetsDir / L"Environment/retroFilm.json"};
		const FlightSceneDefinition assets = sceneLoader.Load(scenePaths);

		m_renderer = std::make_unique<Renderer>();
		m_renderer->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight(),
			assets.BuildRendererDefinition());
		m_windowTitle = L"SkyMission | GPU: " + m_renderer->GetAdapterName();
		SetWindowTextW(m_window->GetHandle(), m_windowTitle.c_str());

		InitializeFlightScene(assets);
		ConfigureResizeHandling();

		m_lastTime = std::chrono::steady_clock::now();
	}

	void Application::InitializeFlightScene(const FlightSceneDefinition& assets) {
		m_scene = std::make_unique<FlightScene>();
		m_scene->Initialize(
			static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight()), 1.0f,
			m_renderer->GetOceanSize());
		SceneEnvironment environment;
		environment.sunDirection = assets.environment.sky.sunDirection;
		environment.sunIntensity = assets.environment.sky.sunIntensity;
		environment.sunColor = assets.environment.sky.sunColor;
		environment.ambientIntensity = assets.environment.sky.ambientIntensity;
		environment.ambientColor = assets.environment.sky.ambientColor;
		m_scene->SetEnvironment(environment);
		m_scene->SetPlayerAircraftVisualScale(assets.playerAircraft.visualScale);
		m_scene->SetPlayerAircraftFlightParameters(assets.playerAircraft.flightParameters);
		m_scene->AddPlayerObject(m_renderer->GetModel(PlayerModelId));
		m_scene->AddEnemyAircraft(m_renderer->GetModel(EnemyModelId), {30.0f, 0.0f, -100.0f},
			assets.enemyAircraft.visualScale, assets.enemyAircraft.flightParameters,
			assets.enemyAIParameters);

		m_scene->ConfigurePlayerWeapon(assets.playerWeapon, m_renderer->GetModel(AmmunitionModelId));
		m_renderer->InitializeConstantBuffers(m_scene->GetRenderScene());
		m_renderer->ReserveModelCapacity(m_scene->GetObjects().size() + assets.playerWeapon.gun.maxProjectiles);
	}

	void Application::ConfigureResizeHandling() {
		// ウィンドウリサイズ時のコールバックを設定
		m_window->SetOnResize([this](UINT w, UINT h) {
			// 最小化などで幅/高さが0のときは処理しない
			if (w == 0 || h == 0)
				return;

			m_renderer->Resize(w, h);

			if (m_scene) {
				m_scene->OnResize(w, h);
			}
		});
	}

	// メインループ（メッセージ処理と更新/描画）
	int Application::Run() {
		MSG msg = {};
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else {
				Update();
				Render();
			}
		}

		return static_cast<int>(msg.wParam);
	}

	void Application::Update() {
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		float deltaSeconds = dt.count();
		m_fpsTimer += deltaSeconds;
		++m_fpsFrameCount;
		if (m_fpsTimer >= 0.25f) {
			const float fps = static_cast<float>(m_fpsFrameCount) / m_fpsTimer;
			std::wstring title = m_windowTitle + L" | FPS: " + std::to_wstring(static_cast<int>(fps + 0.5f));
			SetWindowTextW(m_window->GetHandle(), title.c_str());
			m_fpsTimer = 0.0f;
			m_fpsFrameCount = 0;
		}

		m_elapsedTime += deltaSeconds;

		if (m_scene) {
			const InputSnapshot input = m_window->ConsumeInput();
			m_scene->Update(m_aircraftController.ReadInput(input), deltaSeconds, input,
				WeaponController{}.ReadInput(input));
			m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
		}
	}

	// 描画処理（レンダリングコマンド発行）
	void Application::Render() {
		m_renderer->Render(m_window->GetWidth(), m_window->GetHeight(), m_scene->GetRenderScene());
	}

} // namespace Game
