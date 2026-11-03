#include "Application.h"
#include "Engine/Window/Window.h"
#include "Engine/Renderer/Device/Renderer.h"
#include "Game/Scene/FlightSceneLoader.h"
#include <DirectXMath.h>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include "Engine/ExecutablePath.h"
#include "Game/Weapons/WeaponController.h"
#include "Game/Combat/CombatController.h"

namespace Game {
	using namespace Engine;

	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {}

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
			.retroFilm = assetsDir / L"Environment/retroFilm.json",
			.ships = assetsDir / L"Ships/ships.json",
			.mission = assetsDir / L"Combat/mission.json"};
		const FlightSceneDefinition assets = sceneLoader.Load(scenePaths);

		m_renderer = std::make_unique<Renderer>();
		m_renderer->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight(),
			assets.BuildRendererDefinition());
		m_windowTitle = L"SkyMission | GPU: " + m_renderer->GetAdapterName();
		SetWindowTextW(m_window->GetHandle(), m_windowTitle.c_str());

		InitializeFlightScene(assets);
		m_combatAudio.Initialize(assetsDir / L"Sound");
		ConfigureResizeHandling();

		m_lastTime = std::chrono::steady_clock::now();
	}

	void Application::InitializeFlightScene(const FlightSceneDefinition& assets) {
		m_scene = std::make_unique<FlightScene>();
		m_scene->ConfigureCombat(assets.combat);
		m_hudPresenter.Configure(assets.hud);
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
		m_scene->SetPlayerAircraftPosition({0, 1500, 0});
		m_scene->SetPlayerAircraftVisualScale(assets.playerAircraft.visualScale);
		m_scene->SetPlayerAircraftFlightParameters(assets.playerAircraft.flightParameters);
		m_scene->AddPlayerObject(m_renderer->GetModel(PlayerModelId));
		m_scene->ConfigureMission(assets.mission, m_renderer->GetModel(EnemyModelId),
			assets.enemyAircraft.visualScale, assets.enemyAircraft.flightParameters, assets.enemyAIParameters,
			m_renderer->GetModel(RocketModelId));
		m_scene->ConfigurePlayerWeapon(assets.playerWeapon, m_renderer->GetModel(AmmunitionModelId));
		if (assets.ships.size() < 3)
			throw std::runtime_error("Mission requires a carrier and two escorts");
		for (size_t i = 0; i < assets.ships.size(); ++i) {
			const auto& ship = assets.ships[i];
			const auto id = m_scene->AddObject(m_renderer->GetModel(ship.asset.id), ship.BuildTransform());
			m_scene->RegisterMissionShip(id, i == 0);
		}
		m_renderer->InitializeConstantBuffers(m_scene->GetRenderScene());
		m_renderer->ReserveModelCapacity(m_scene->GetObjects().size() +
			assets.playerWeapon.gun.maxProjectiles + assets.mission.maxHostileProjectiles +
			assets.mission.finalFighterCount + 1 +
			static_cast<size_t>(std::ceil(120 / assets.mission.bombReloadSeconds)) + 2);
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
		bool suspended = false;
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			} else if (IsIconic(m_window->GetHandle()) || m_window->GetWidth() == 0 ||
				m_window->GetHeight() == 0) {
				// 最小化中は描画とシミュレーションを止め、メッセージ到着までCPUを休ませる
				m_combatAudio.SetSuspended(true);
				suspended = true;
				WaitMessage();
			} else {
				if (suspended) {
					// 復帰時に最小化中の時間を持ち込まず、機体や演出の急な進行を防ぐ
					m_lastTime = std::chrono::steady_clock::now();
					m_fpsTimer = 0;
					m_fpsFrameCount = 0;
					m_combatAudio.SetSuspended(false);
					suspended = false;
				}
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
			title += m_renderer->IsVerticalSyncEnabled() ? L" | VSync ON [F10]" : L" | VSync OFF [F10]";
			const auto& gpu = m_renderer->GetGpuTimings();
			if (gpu.valid)
				title += std::format(L" | GPU {:.2f} ms (Sea {:.2f}, Models {:.2f}, Post {:.2f})",
					gpu.TotalMs(), gpu.oceanMs, gpu.modelsMs, gpu.postProcessMs);
			SetWindowTextW(m_window->GetHandle(), title.c_str());
			m_fpsTimer = 0.0f;
			m_fpsFrameCount = 0;
		}

		m_elapsedTime += deltaSeconds;

		if (m_scene) {
			const InputSnapshot input = m_window->ConsumeInput();
			if (input.WasKeyPressed(VK_F10))
				m_renderer->SetVerticalSync(!m_renderer->IsVerticalSyncEnabled());
			const auto combat = CombatController{}.ReadInput(input);
			m_scene->Update(m_aircraftController.ReadInput(input), deltaSeconds, input, combat);
			const auto& combatState = m_scene->GetCombatSnapshot();
			m_combatAudio.Update(combatState.audio, combatState.hitCount,
				combatState.threatWarning == ThreatWarning::LockOn,
				combatState.threatWarning == ThreatWarning::Missile, deltaSeconds);
			const auto view = m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
			// 描画リストはフレームごとに一度だけ初期化し、エフェクト、照準の順に重ねる
			m_hud.Reset(view.width, view.height);
			m_effectPresenter.Build(m_scene->GetEffectSnapshot(), view, m_hud);
			m_hudPresenter.Build(m_scene->GetCombatSnapshot(), view, m_hud);
		}
	}

	// 描画処理（レンダリングコマンド発行）
	void Application::Render() {
		m_renderer->Render(m_window->GetWidth(), m_window->GetHeight(), m_scene->GetRenderScene(), &m_hud);
	}

} // namespace Game
