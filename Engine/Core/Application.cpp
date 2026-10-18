#include "Application.h"
#include "Window.h"
#include "../Renderer/Device/Renderer.h"
#include <DirectXMath.h>
#include <chrono>

namespace Engine {

	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {}

	// アプリケーション初期化処理
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		m_renderer = std::make_unique<Renderer>();
		m_renderer->Initialize(
			m_window->GetHandle(),
			m_window->GetWidth(),
			m_window->GetHeight()
		);

		m_scene = std::make_unique<Engine::Scene>();
		m_scene->Initialize(
			m_window->GetHandle(),
			static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight()),
			1.0f,
			m_renderer->GetOceanSize()
		);
		m_scene->AddObject(m_renderer->GetModel());

		m_renderer->InitializeConstantBuffers(*m_scene);

		// ウィンドウリサイズ時のコールバックを設定
		m_window->SetOnResize([this](UINT w, UINT h) {
			// 最小化などで幅/高さが0のときは処理しない
			if (w == 0 || h == 0) return;

			m_renderer->Resize(w, h);

			if (m_scene) {
				m_scene->OnResize(w, h);
			}
			});

		m_lastTime = std::chrono::steady_clock::now();
	}

	// メインループ（メッセージ処理と更新/描画）
	int Application::Run() {
		MSG msg = {};
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				Update();
				Render();
			}
		}
		return static_cast<int>(msg.wParam);
	}

	void Application::Update() {
		//static float time = 0.0f;
		// compute delta
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		float deltaSeconds = dt.count();
		m_fpsTimer += deltaSeconds;
		++m_fpsFrameCount;
		if (m_fpsTimer >= 0.25f) {
			const float fps = static_cast<float>(m_fpsFrameCount) / m_fpsTimer;
			std::wstring title = L"SkyMission | FPS: " + std::to_wstring(static_cast<int>(fps + 0.5f));
			SetWindowTextW(m_window->GetHandle(), title.c_str());
			m_fpsTimer = 0.0f;
			m_fpsFrameCount = 0;
		}

		m_elapsedTime += deltaSeconds;

		if (m_scene) {
			m_scene->Update(deltaSeconds);
			m_renderer->Update(*m_scene, m_elapsedTime);
		}
	}

	// 描画処理（レンダリングコマンド発行）
	void Application::Render() {
		m_renderer->Render(m_window->GetWidth(), m_window->GetHeight(), *m_scene);
	}

}