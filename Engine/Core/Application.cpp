#include "Application.h"
#include "Window.h"
#include "../Renderer/Device/RenderDevice.h"
#include "../Renderer/Device/CommandContext.h"
#include "../Resources/Texture/Texture.h"
#include <DirectXMath.h>

namespace Engine {
	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {
		if (m_context) {
			m_context->WaitForGpu(); // 未消化コマンドによるメモリリークや強制終了を抑止
		}
	}

	struct ConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		float time;
		float padding[3];
		DirectX::XMFLOAT3 cameraPos;
		float pad2;
	};

	// アプリケーション基盤および各グラフィックスコンポーネントの構築
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		// リサイズイベントを購読してデバイスや投影行列を更新
		m_window->SetOnResize([this](UINT w, UINT h) {
			if (w == 0 || h == 0) return; // 最小化時など無効な値を無視
			if (m_context) m_context->WaitForGpu(); // 未処理コマンドを完了させてからリサイズ
			m_device->Resize(w, h);
			// 投影は Update() で毎フレーム再計算しているためここでは何もしない
		});

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
		// --- 10x10 グリッドの頂点・インデックス生成 ---
		const int gridSize = 1000;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		for (int z = 0; z < gridSize; ++z) {
			for (int x = 0; x < gridSize; ++x) {
				float px = (float)x / (gridSize - 1) * 500.0f - 250.0f;
				float pz = (float)z / (gridSize - 1) * 500.0f - 250.0f;
				float u = (float)x / (gridSize - 1);
				float v = (float)z / (gridSize - 1);
				vertices.push_back({ {px, 0.0f, pz}, {u, v} });
			}
		}

		for (int z = 0; z < gridSize - 1; ++z) {
			for (int x = 0; x < gridSize - 1; ++x) {
				uint32_t i0 = z * gridSize + x;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = (z + 1) * gridSize + x;
				uint32_t i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i1); indices.push_back(i3); indices.push_back(i2);
			}
		}
		m_indexCount = (UINT)indices.size();

		// --- 頂点バッファの初期化 ---
		m_vertexBuffer = std::make_unique<Engine::VertexBuffer>();
		m_vertexBuffer->Initialize(m_device->GetDevice(), vertices.data(), sizeof(Vertex) * vertices.size(), sizeof(Vertex));

		// --- インデックスバッファの作成 ---
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * indices.size());
		ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&m_indexBuffer)));

		void* pData;
		m_indexBuffer->Map(0, nullptr, &pData);
		memcpy(pData, indices.data(), sizeof(uint32_t) * indices.size());
		m_indexBuffer->Unmap(0, nullptr);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = sizeof(uint32_t) * indices.size();

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
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);
		m_context->EndFrame();
		// アップロードが完了するまで待機
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;

		// 定数バッファ (MVP) を作成してトップダウン視点の行列を設定
		{
			using namespace DirectX;
			UINT64 cbSize = (sizeof(ConstantBufferData) + 255) & ~255; // 256 バイト境界にアライン

			CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)));

			// マップして行列を書き込む
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataPtr)));

			XMMATRIX world = XMMatrixIdentity();
			// カメラを上方に置き、原点を見る (Y軸が上方向)
			XMVECTOR eye = XMVectorSet(10.0f, 15.0f, -10.0f, 0.0f);
			XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
			float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
			XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
			XMMATRIX mvp = world * view * proj;
			XMMATRIX mvpT = XMMatrixTranspose(mvp); // シェーダとの行列オーダ互換のため転置

			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, mvpT);
			// 初期値を書き込む（カメラは初期の eye と合わせる）
			ConstantBufferData* cbInit = reinterpret_cast<ConstantBufferData*>(m_cbvDataPtr);
			cbInit->mvp = m;
			cbInit->time = 0.0f;
			cbInit->cameraPos = DirectX::XMFLOAT3(10.0f, 15.0f, -10.0f);
		}
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

	void Application::Update() {
		static float time = 0.0f;
		time += 0.01f;

		// MVP行列を再計算
		using namespace DirectX;
		XMMATRIX world = XMMatrixIdentity();
		XMVECTOR eye = XMVectorSet(15.0f, 40.0f, -0.0f, 0.0f); // 真上近くから少しずらして見下ろす
		XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);   // ターゲットは原点
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);   // 上方向
		XMMATRIX view = XMMatrixLookAtLH(eye, at, up);

		float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
		XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
		XMMATRIX mvp = world * view * proj;
		XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 m;
		XMStoreFloat4x4(&m, mvpT);

		// 定数バッファを更新
		ConstantBufferData* data;
		m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&data));

		data->mvp = m;
		data->time = time;
		// カメラ位置を更新（VS/PS のフレネル計算用）
		data->cameraPos = DirectX::XMFLOAT3(0.0f, 20.0f, -0.1f);

		m_constantBuffer->Unmap(0, nullptr);
	}

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

		// 頂点シェーダ用の定数バッファをルートにバインド
		if (m_constantBuffer) {
			cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
		}

		// 描画設定（頂点バッファをバインド）
		auto view = m_vertexBuffer->GetView();
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->IASetIndexBuffer(&m_indexBufferView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// クリアと描画実行
		auto rtv = m_device->GetCurrentRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

		// Presentへ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

}