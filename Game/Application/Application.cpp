#include "Application.h"
#include "Engine/Window/Window.h"
#include "Engine/Renderer/Renderer.h"
#include "Game/Scene/FlightSceneLoader.h"
#include <DirectXMath.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <future>
#include <mutex>
#include <optional>
#include <algorithm>
#include "Engine/Diagnostics/Log.h"
#include "Engine/Diagnostics/StartupTiming.h"
#include "Engine/ExecutablePath.h"
#include "Engine/Interop/Redox/RedoxBridge.h"
#include "Game/Weapons/WeaponController.h"
#include "Game/Combat/CombatController.h"

namespace Game {
	using namespace Engine;

	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {}

	void Application::Initialize() {
		// 読み込み用のウィンドウを表示する
		StartupTiming timing("application.window_and_json");
		const auto startupTime = std::chrono::steady_clock::now();
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		m_window->SetLoading(true);
		m_window->SetLoadingProgress(0, L"読み込みエンジンの準備");
		ShowWindow(m_window->GetHandle(), SW_SHOW);
		UpdateWindow(m_window->GetHandle());

		timing.Mark("application.redox_warmup");
		RedoxModule::WarmUp();
		timing.Mark("application.scene_settings");
		m_window->SetLoadingProgress(5, L"マップとミッションの設定");

		// 起動に必要な設定を読み込む
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

		// ワーカーで描画リソースを準備する
		m_renderer = std::make_unique<Renderer>();
		const HWND hwnd = m_window->GetHandle();
		const UINT width = m_window->GetWidth(), height = m_window->GetHeight();
		// ワーカーは最新の進捗だけを記録し、ロード画面の更新はUIスレッドで行う
		using LoadingProgress = std::pair<int, std::wstring>;
		std::mutex progressMutex;
		std::optional<LoadingProgress> pendingProgress;
		auto loading = std::async(std::launch::async, [&, hwnd, width, height]() {
			ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
			struct ComScope {
				~ComScope() {
					CoUninitialize();
				}
			} comScope;
			m_renderer->Initialize(hwnd, width, height, assets.BuildRendererDefinition(),
				[&](size_t completed, size_t total, std::wstring_view item) {
					const std::lock_guard lock(progressMutex);
					pendingProgress.emplace(10 + static_cast<int>(75 * completed / total), item);
				});
		});

		// スワップチェーン作成中にDXGIがウィンドウメッセージを送るため、UIスレッドで処理し続ける
		while (loading.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
			MSG message{};

			while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&message);
				DispatchMessageW(&message);
			}

			std::optional<LoadingProgress> progress;
			{
				const std::lock_guard lock(progressMutex);
				progress.swap(pendingProgress);
			}
			if (progress) m_window->SetLoadingProgress(progress->first, progress->second);
			MsgWaitForMultipleObjectsEx(0, nullptr, 16, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
		}

		// 読み込み後の画面寸法を反映する
		loading.get(); // ワーカー停止後に、初期化エラーを呼び出し元へ伝播する
		if (m_window->IsCloseRequested()) {
			m_window->SetLoading(false);
			return;
		}
		m_window->SetLoadingProgress(85, L"ミッションと機体の準備");
		if (width != m_window->GetWidth() || height != m_window->GetHeight()) {
			if (m_window->GetWidth() && m_window->GetHeight())
				m_renderer->Resize(m_window->GetWidth(), m_window->GetHeight());
		}

		m_windowTitle = L"SkyMission | GPU: " + m_renderer->GetAdapterName();
		SetWindowTextW(m_window->GetHandle(), m_windowTitle.c_str());

		// シーンと音声を初期化する
		InitializeFlightScene(assets);
		timing.Mark("application.audio");
		m_window->SetLoadingProgress(95, L"効果音と音声");
		m_combatAudio.Initialize(assetsDir / L"Sound", assets.cinematics);
		ConfigureResizeHandling();
		// 読み込み中の決定入力でタイトル画面を飛ばさない
		m_window->ConsumeInput();
		m_window->SetLoadingProgress(100, L"読み込み完了");
		m_window->SetLoading(false);

		m_lastTime = std::chrono::steady_clock::now();
		Log("Startup completed in {:.0f} ms\n",
			std::chrono::duration<double, std::milli>(m_lastTime - startupTime).count());
	}

	void Application::InitializeFlightScene(const FlightSceneDefinition& assets) {
		m_captureShipModels.clear();

		for (const auto& ship : assets.ships)
			m_captureShipModels.push_back(m_renderer->GetModel(ship.asset.id));

		// 読み込んだ設定を表示と演出へ反映して任務シーンを作る
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
		m_combatRumble.Configure(assets.rumble, assets.playerAircraft.flightParameters.maximumSpeed);
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

		// 弾や増援を見込んだ描画領域を確保する
		m_renderer->InitializeConstantBuffers(m_scene->GetRenderScene());
		m_renderer->ReserveModelCapacity(m_scene->GetObjects().size() +
			assets.playerWeapon.gun.maxProjectiles + assets.mission.maxHostileProjectiles +
			assets.mission.finalFighterCount + 1 +
			CarrierMissileSystem::RequiredModelCapacity(
				assets.mission.bombReloadSeconds, assets.bomb.missile));
	}

	void Application::ConfigureResizeHandling() {
		m_window->SetOnResize([this](UINT w, UINT h) {
			if (w == 0 || h == 0) return;

			m_renderer->Resize(w, h);

			if (m_scene) {
				m_scene->OnResize(w, h);
			}
		});
	}

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
		const float deltaSeconds = UpdateFrameTiming();
		if (!m_scene) return;

		const InputSnapshot input = m_window->ConsumeInput();
		m_combatAudio.SetSuspended(!input.active);
		UpdateInputDevice(input);

		if (m_screen == Screen::Playing) {
			UpdateGameplay(input, deltaSeconds);
		} else {
			UpdateMenu(input);
		}
	}

	float Application::UpdateFrameTiming() {
		// 経過時間を測り、一定間隔でFPSとGPU時間の表示を更新する
		const auto now = std::chrono::steady_clock::now();
		const std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		const float deltaSeconds = dt.count();
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
		return deltaSeconds;
	}

	void Application::UpdateInputDevice(const InputSnapshot& input) {
		const bool keyboardOrMouse = std::ranges::any_of(input.keys, std::identity{}) ||
			std::ranges::any_of(input.pressedKeys, std::identity{}) || input.leftMouseDown ||
			input.middleMouseDown || input.rightMouseDown || input.rightMousePressCount != 0;
		const auto& pad = input.gamepad;
		const bool gamepadInput = pad.buttons != 0 || pad.northTapped ||
			std::ranges::any_of(pad.presses, std::identity{}) || pad.leftX != 0 || pad.leftY != 0 ||
			pad.rightX != 0 || pad.rightY != 0 || pad.leftTrigger != 0 || pad.rightTrigger != 0;
		// 無入力時やフォーカス喪失時も表示方式を維持し、同時入力時はキーボード・マウスを優先する
		if (input.active && keyboardOrMouse)
			m_usingGamepad = false;
		else if (input.active && gamepadInput)
			m_usingGamepad = true;
	}

	void Application::UpdateMenu(const InputSnapshot& input) {
		if (!input.WasConfirmPressed()) {
			UpdateMenuFrame();
			return;
		}

		m_combatAudio.ResetPlayback();
		if (m_screen == Screen::Clear) {
			m_screen = Screen::Title;
			UpdateMenuFrame();
			return;
		}

		m_scene->StartNewMission();
		m_screen = Screen::Playing;
		m_captureMode = 0;

		// 開始に使った入力を同じフレームのスキップ判定に渡さない
		m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
		m_hud.Reset(m_window->GetWidth(), m_window->GetHeight());
		m_hudPresenter.Build(m_scene->GetCombatSnapshot(),
			{.width = m_window->GetWidth(), .height = m_window->GetHeight()}, m_hud, m_usingGamepad);
	}

	void Application::UpdateGameplay(const InputSnapshot& input, float deltaSeconds) {
		if (input.WasKeyPressed(VK_F6)) {
			m_captureMode = (m_captureMode + 1) % 3;
			Log("Capture view: {} (0=normal, 1=environment+ships, 2=environment)\n", m_captureMode);
		}

		// 入力から戦闘を進め、演出終了時に結果画面へ切り替える
		auto combat = CombatController{}.ReadInput(input);

		const auto& previousState = m_scene->GetCombatSnapshot();
		const bool defeated = previousState.result == CombatResult::Defeat;
		if (defeated) combat.skipOpening = false;
		const float simulationSeconds =
			(previousState.cinematic.active || defeated) && !input.active ? 0 : deltaSeconds;
		const bool endingWasActive = previousState.cinematic.active &&
			previousState.cinematic.cinematicId == "ending";
		const float previousHealth = previousState.playerHealth;
		const auto flight = m_aircraftController.ReadInput(input);
		m_scene->Update(flight, simulationSeconds, input, combat);
		const auto& combatState = m_scene->GetCombatSnapshot();
		if (combatState.result == CombatResult::Defeat) m_captureMode = 0;
		const bool defeatRadioActive =
			combatState.cinematic.cinematicId == "defeat" && combatState.cinematic.radioLine >= 0;
		const bool showClearScreen = endingWasActive && !combatState.cinematic.active &&
			combatState.result == CombatResult::Victory;
		const bool showGameOver = combatState.result == CombatResult::Defeat &&
			!combatState.cinematic.active && !defeatRadioActive;
		const bool showResultScreen = showClearScreen || showGameOver;
		// フォーカス喪失時や結果画面への切り替え時は振動を止める
		const auto rumble = m_combatRumble.Update(deltaSeconds, input.active && m_usingGamepad && !showResultScreen,
			flight, combatState, combatState.playerHealth < previousHealth);
		m_window->SetGamepadRumble(rumble.lowFrequency, rumble.highFrequency);

		if (showResultScreen) {
			m_screen = showGameOver ? Screen::GameOver : Screen::Clear;
			m_captureMode = 0;
			m_combatAudio.ResetPlayback();
			UpdateMenuFrame();
			return;
		}

		// シーンの状態で音声を更新し、再生状況を台詞の進行へ返す
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
			m_hudPresenter.Build(m_scene->GetCombatSnapshot(), view, m_hud, m_usingGamepad);
		}
	}

	void Application::UpdateMenuFrame() {
		m_renderer->UpdateFrameConstants(m_scene->GetRenderScene(), m_elapsedTime);
		BuildMenuHud();
	}

	void Application::BuildMenuHud() {
		const float width = float(m_window->GetWidth()), height = float(m_window->GetHeight());
		m_hud.Reset(m_window->GetWidth(), m_window->GetHeight());
		m_hud.FilledRect({0, 0}, {width, height}, {0, 0, 0, 1});
		const float scale = std::min(width / 800.0f, height / 600.0f);
		const auto centered = [&](std::string_view text, float y, float size) {
			m_hud.Text(
				{(width - HudDrawList::MeasureTextWidth(text, size)) * 0.5f, y}, text, size, {1, 1, 1, 1});
		};

		const char* title = "SkyMission";
		const char* action = " でスタート";
		if (m_screen == Screen::GameOver) {
			title = "ゲームオーバー";
			action = " で再挑戦";
		} else if (m_screen == Screen::Clear) {
			title = "ミッションクリア";
			action = " でタイトルに戻る";
		}
		centered(title, height * 0.4f, 48 * scale);
		// コントローラーの機種にかかわらず、操作ガイドはPS4表記に統一する
		const auto confirm = m_usingGamepad ? "×" : "Enter";
		centered(std::string(confirm) + action, height * 0.56f, 26 * scale);
	}

	const Engine::Scene& Application::PrepareCaptureScene() {
		const auto& source = m_scene->GetRenderScene();

		if (m_captureMode == 0) return source;

		// 描画専用のシーンを作る。ゲーム内オブジェクト、被害、ミッション状態は変更しない
		m_captureScene = Engine::Scene{};
		m_captureScene.SetCamera(source.GetCamera());
		m_captureScene.SetEnvironment(source.GetEnvironment());

		if (m_captureMode == 1) {
			for (const auto& object : m_scene->GetObjects()) {
				if (std::find(m_captureShipModels.begin(), m_captureShipModels.end(), object.model) ==
					m_captureShipModels.end())
					continue;

				const auto id = m_captureScene.AddObject(object.model, object.worldTransform);

				if (auto* copy = m_captureScene.FindObject(id))
					copy->nodeRotationRadians = object.nodeRotationRadians;
			}
		}
		return m_captureScene;
	}

	void Application::Render() {
		const auto& scene = m_captureMode == 0 ? m_scene->GetRenderScene() : m_captureScene;
		m_renderer->Render(m_window->GetWidth(), m_window->GetHeight(), scene,
			m_screen != Screen::Playing || m_captureMode == 0 ? &m_hud : nullptr);
	}

} // namespace Game
