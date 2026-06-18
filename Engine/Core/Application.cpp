#include "Application.h"
#include "Window.h"
#include "../Renderer/Device/RenderDevice.h"
#include "../Renderer/Device/CommandContext.h"
#include "../Resources/Texture/Texture.h"

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

		// パイプラインの初期化
		m_pipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_pipeline->Initialize(m_device->GetDevice());

		// 四角形の頂点データ作成
		struct Vertex {
			float pos[3];
			float uv[2];
		};
		Vertex quad[] = {
			// 位置(x,y,z)             // UV(u,v)
			{-0.25f,  0.25f, 0.0f,    0.0f, 0.0f}, // 左上
			{ 0.25f,  0.25f, 0.0f,    1.0f, 0.0f}, // 右上
			{-0.25f, -0.25f, 0.0f,    0.0f, 1.0f}, // 左下
			{ 0.25f, -0.25f, 0.0f,    1.0f, 1.0f}  // 右下
		};

		// 頂点バッファの生成
		m_vertexBuffer = std::make_unique<Engine::VertexBuffer>();
		m_vertexBuffer->Initialize(m_device->GetDevice(), quad, sizeof(quad), sizeof(Vertex));

		// テクスチャを読み込み、SRV を作成してディスクリプタヒープへ配置
		m_texture = std::make_unique<Engine::Texture>();
		// コマンドリストをリセットしてアップロード処理を行う
		m_context->BeginFrame();
		UINT srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSrvDescriptor(&srvIndex);
		// 実行ファイルのパスを取得する簡易的な手法
		wchar_t buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring exePath = buffer;
		std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));

		// Assetsへの絶対パスを動的に作る
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\SampleImage.png";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);
		m_context->EndFrame();
		// アップロードが完了するまで待機
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;
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

	// フレームのレンダリングコマンド生成・実行パス
	void Application::Render() {
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto resource = m_device->GetCurrentRenderTarget();

		// レンダーターゲットへ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_window->GetWidth()), static_cast<float>(m_window->GetHeight()), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(m_window->GetWidth()), static_cast<LONG>(m_window->GetHeight()) };

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissorRect);

		// パイプラインとルートシグネチャをセット
		cmd->SetGraphicsRootSignature(m_pipeline->GetRootSignature());
		cmd->SetPipelineState(m_pipeline->GetPSO());

		// テクスチャがあればディスクリプタヒープをセットしてルートに SRV をバインド
		if (m_texture) {
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_textureSrvIndex));
		}

		// 描画設定（頂点バッファをバインド）
		auto view = m_vertexBuffer->GetView();
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		// クリアと描画実行
		auto rtv = m_device->GetCurrentRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		cmd->DrawInstanced(4, 1, 0, 0); // 4頂点で四角形

		// Presentへ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

}