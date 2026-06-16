#include "Application.h"
#include "Window.h"
#include "../Renderer/RenderDevice.h"
#include "../Renderer/CommandContext.h"

namespace Engine {
	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {
		if (m_context) {
			m_context->WaitForGpu(); // 未消化コマンドによるメモリリークや強制終了を抑止
		}
	}

	// アプリケーション基盤および各グラフィックスコンポーネントの構築
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"DirectX 12 Engine (Engine/Core/Renderer Style)", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight());

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());
	}

	// メッセージループの駆動およびメイン更新・描画パスの制御
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

	void Application::Update() {}

	// 1フレームのレンダリングコマンド生成・実行パス
	void Application::Render() {
		m_context->BeginFrame();

		auto rtvHandle = m_device->GetCurrentRtvHandle();
		auto resource = m_device->GetCurrentRenderTarget();

		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f }; // Cornflower Blue
		m_context->GetCommandList()->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// TODO: パイプラインバインドおよび描画コマンドの追加スロット

		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();

		m_context->MoveToNextFrame(m_device.get());
	}
}