#include "Application.h"
#include "Engine/Window/Window.h"
#include "Engine/Renderer/Device/Renderer.h"
#include "Game/Scene/FlightSceneLoader.h"
#include <DirectXMath.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <future>
#include <algorithm>
#include "Engine/Log.h"
#include "Engine/StartupTiming.h"
#include "Engine/ExecutablePath.h"
#include "Game/Weapons/WeaponController.h"
#include "Game/Combat/CombatController.h"

namespace Game {
	using namespace Engine;

	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {}

	// アプリケーション初期化処理
	void Application::Initialize() {
		StartupTiming timing("application.window_and_json");
		const auto startupTime = std::chrono::steady_clock::now();
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		m_window->SetLoading(true);
		ShowWindow(m_window->GetHandle(), SW_SHOW);
		UpdateWindow(m_window->GetHandle());

		const auto assetsDir = AssetsDirectory();
		const FlightSceneLoader sceneLoader;
		const SceneSourcePaths scenePaths = {.environment = assetsDir / L"Environment/environment.json",
			.playerAircraft = assetsDir / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
			.enemyAircraft = assetsDir / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
			.retroFilm = assetsDir / L"Environment/retroFilm.json",
			.ships = assetsDir / L"Ships/ships.json",
			.mission = assetsDir / L"Combat/mission.json",
			.bomb = assetsDir / L"Ammunition/Bomb/ammunition.json"};
		const FlightSceneDefinition assets = sceneLoader.Load(scenePaths);
		timing.Mark("application.renderer");

		m_renderer = std::make_unique<Renderer>();
		const HWND hwnd = m_window->GetHandle();
		const UINT width = m_window->GetWidth(), height = m_window->GetHeight();
		auto loading = std::async(std::launch::async, [&, hwnd, width, height]() {
			ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
			struct ComScope {
				~ComScope() {
					CoUninitialize();
				}
			} comScope;
			m_renderer->Initialize(hwnd, width, height, assets.BuildRendererDefinition());
		});
		// DXGI may send window messages during swap-chain creation, so keep the UI thread pumping.
		while (loading.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
			MSG message{};
			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}
			MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}
		loading.get(); // Propagate initialization errors after the worker has stopped.
		m_window->SetLoading(false);
		if (!m_window->GetHandle()) return;
		if (width != m_window->GetWidth() || height != m_window->GetHeight()) {
			if (m_window->GetWidth() && m_window->GetHeight())
				m_renderer->Resize(m_window->GetWidth(), m_window->GetHeight());
		}
		m_windowTitle = L"SkyMission | GPU: " + m_renderer->GetAdapterName();
		SetWindowTextW(m_window->GetHandle(), m_windowTitle.c_str());

		InitializeFlightScene(assets);
		timing.Mark("application.audio");
		m_combatAudio.Initialize(assetsDir / L"Sound", assets.cinematics);
		ConfigureResizeHandling();

		m_lastTime = std::chrono::steady_clock::now();
		Log("Startup completed in {:.0f} ms\n",
			std::chrono::duration<double, std::milli>(m_lastTime - startupTime).count());
	}

	void Application::InitializeFlightScene(const FlightSceneDefinition& assets) {
		m_captureShipModels.clear();
		for (const auto& ship : assets.ships)
			m_captureShipModels.push_back(m_renderer->GetModel(ship.asset.id));
		m_hudPresenter.Configure(assets.hud);
		m_hudPresenter.ConfigureTuning(assets.ui);
		m_hudPresenter.ConfigureText(assets.text);
		m_effectPresenter.Configure(assets.effects);
		m_scene = std::make_unique<FlightScene>(FlightScene::CreateMission(
			assets,
			[this](const std::string& id) {
				return m_renderer->GetModel(id);
			},
			m_window->GetWidth(), m_window->GetHeight(), m_renderer->GetOceanSize()));
		m_scene->ConfigureCinematics(
			assets.cinematics,
			[this, enemyScale = assets.enemyAircraft.visualScale,
				playerScale = assets.playerAircraft.visualScale](const std::string& id) {
				const float scale = id == EnemyModelId ? enemyScale
					: id == PlayerModelId			   ? playerScale
													   : 1.0f;
				return FlightScene::CinematicModel{m_renderer->GetModel(id), scale};
			},
			m_renderer->GetOceanSize());
		m_renderer->InitializeConstantBuffers(m_scene->GetRenderScene());
		m_renderer->ReserveModelCapacity(m_scene->GetObjects().size() +
			assets.playerWeapon.gun.maxProjectiles + assets.mission.maxHostileProjectiles +
			assets.mission.finalFighterCount + 1 +
			CarrierMissileSystem::RequiredModelCapacity(
				assets.mission.bombReloadSeconds, assets.bomb.missile));
	}

	void Application::ConfigureResizeHandling() {
		// ウィンドウリサイズ時のコールバックを設定
		m_window->SetOnResize([this](UINT w, UINT h) {
			// 最小化などで幅/高さが0のときは処理しない
			if (w == 0 || h == 0) return;

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
				suspended = true;
				m_combatAudio.SetSuspended(true);
				WaitMessage();
			} else {
				if (suspended) {
					// 復帰時に最小化中の時間を持ち込まず、機体や演出の急な進行を防ぐ
					m_lastTime = std::chrono::steady_clock::now();
					m_fpsTimer = 0;
					m_fpsFrameCount = 0;
					suspended = false;
					m_combatAudio.SetSuspended(false);
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
			const auto& gpu = m_renderer->GetGpuTimings();
			if (gpu.valid)
				title +=
					std::format(L" | GPU {:.2f} ms (Sea {:.2f}, Models {:.2f}, Clouds {:.2f}, Post {:.2f})",
						gpu.TotalMs(), gpu.oceanMs, gpu.modelsMs, gpu.cloudsMs, gpu.postProcessMs);
			SetWindowTextW(m_window->GetHandle(), title.c_str());
			m_fpsTimer = 0.0f;
			m_fpsFrameCount = 0;
		}

		m_elapsedTime += deltaSeconds;

		if (m_scene) {
			const InputSnapshot input = m_window->ConsumeInput();
			m_combatAudio.SetSuspended(!input.active);
			if (m_screen == Screen::Title || m_screen == Screen::Clear) {
				if (input.active && input.WasKeyPressed(VK_RETURN)) {
					m_combatAudio.ResetPlayback();
					if (m_screen == Screen::Clear) {
						m_screen = Screen::Title;
					} else {
						m_scene->StartNewMission();
						m_screen = Screen::Playing;
						m_captureMode = 0;
					}
				}
				if (m_screen != Screen::Playing) {
					m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
					BuildMenuHud();
					return;
				}
				// 開始のEnterをオープニングのスキップ操作として再利用しない。
				m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
				m_hud.Reset(m_window->GetWidth(), m_window->GetHeight());
				m_hudPresenter.Build(m_scene->GetCombatSnapshot(),
					{.width = m_window->GetWidth(), .height = m_window->GetHeight()}, m_hud);
				return;
			}

			if (input.WasKeyPressed(VK_F6)) {
				m_captureMode = (m_captureMode + 1) % 3;
				Log("Capture view: {} (0=normal, 1=environment+ships, 2=environment)\n", m_captureMode);
			}
			auto combat = CombatController{}.ReadInput(input);
			if (m_scene->GetCombatSnapshot().result == CombatResult::Victory) combat.retry = false;
			const float simulationSeconds =
				m_scene->GetCombatSnapshot().cinematic.active && !input.active ? 0 : deltaSeconds;
			const bool endingWasActive = m_scene->GetCombatSnapshot().cinematic.active &&
				m_scene->GetCombatSnapshot().cinematic.cinematicId == "ending";
			m_scene->Update(m_aircraftController.ReadInput(input), simulationSeconds, input, combat);
			if (endingWasActive && !m_scene->GetCombatSnapshot().cinematic.active &&
				m_scene->GetCombatSnapshot().result == CombatResult::Victory) {
				m_screen = Screen::Clear;
				m_captureMode = 0;
				m_combatAudio.ResetPlayback();
				m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
				BuildMenuHud();
				return;
			}
			const auto& combatState = m_scene->GetCombatSnapshot();
			if (combatState.audio.restarted) m_combatAudio.ResetPlayback();
			const auto voice = m_combatAudio.UpdateCinematic(combatState.cinematic);
			m_combatAudio.UpdateEnemyRadio(combatState.enemyRadio);
			m_scene->UpdateCinematicBriefing(voice, input.active ? deltaSeconds : 0);
			m_combatAudio.Update(combatState.audio, combatState.hitCount, false, false, deltaSeconds);
			const auto view = m_renderer->UpdateFrameConstants(PrepareCaptureScene(), m_elapsedTime);
			// 描画リストはフレームごとに一度だけ初期化し、エフェクト、照準の順に重ねる
			m_hud.Reset(view.width, view.height);
			if (m_captureMode == 0) {
				if (!m_scene->GetCombatSnapshot().cinematic.active)
					m_effectPresenter.Build(m_scene->GetEffectSnapshot(), view, m_hud);
				m_hudPresenter.Build(m_scene->GetCombatSnapshot(), view, m_hud);
			}
		}
	}

	void Application::BuildMenuHud() {
		const float width = float(m_window->GetWidth()), height = float(m_window->GetHeight());
		m_hud.Reset(m_window->GetWidth(), m_window->GetHeight());
		m_hud.FilledRect({0, 0}, {width, height}, {0, 0, 0, 1});
		const float scale = std::min(width / 800.0f, height / 600.0f);
		const auto centered = [&](std::string_view text, float y, float size) {
			m_hud.Text({(width - HudDrawList::MeasureTextWidth(text, size)) * 0.5f, y},
				text, size, {1, 1, 1, 1});
		};
		centered(m_screen == Screen::Title ? "SkyMission" : "ミッションクリア",
			height * 0.4f, 48 * scale);
		centered(m_screen == Screen::Title ? "エンターキーでスタート" : "エンターでタイトルに戻る",
			height * 0.56f, 26 * scale);
	}

	const Engine::Scene& Application::PrepareCaptureScene() {
		const auto& source = m_scene->GetRenderScene();
		if (m_captureMode == 0) return source;
		// Build a render-only scene; gameplay objects, damage and mission state stay intact.
		m_captureScene = Engine::Scene{};
		m_captureScene.SetCamera(source.GetCamera());
		m_captureScene.SetEnvironment(source.GetEnvironment());
		if (m_captureMode == 1) {
			for (const auto& object : m_scene->GetObjects()) {
				if (std::find(m_captureShipModels.begin(), m_captureShipModels.end(), object.model) ==
					m_captureShipModels.end()) continue;
				const auto id = m_captureScene.AddObject(object.model, object.worldTransform);
				if (auto* copy = m_captureScene.FindObject(id)) copy->nodeRotationRadians = object.nodeRotationRadians;
			}
		}
		return m_captureScene;
	}

	// 描画処理（レンダリングコマンド発行）
	void Application::Render() {
		const auto& scene = m_captureMode == 0 ? m_scene->GetRenderScene() : m_captureScene;
		m_renderer->Render(m_window->GetWidth(), m_window->GetHeight(), scene,
			m_screen != Screen::Playing || m_captureMode == 0 ? &m_hud : nullptr);
	}

} // namespace Game
