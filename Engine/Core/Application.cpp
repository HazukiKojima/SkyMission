#include "Application.h"
#include "Window.h"
#include "../Renderer/Device/RenderDevice.h"
#include "../Renderer/Device/CommandContext.h"
#include "../Resources/Texture/Texture.h"
#include <DirectXMath.h>
#include <chrono>

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
			if (m_camera) m_camera->OnResize(w, h);
			// 投影は Update() で毎フレーム再計算しているためここでは何もしない
		});

		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight());

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());

		// パイプラインの初期化
		m_pipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_pipeline->Initialize(m_device->GetDevice());

		// Sky Sphere パイプラインの初期化
		m_skyPipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_skyPipeline->InitializeWithShaders(m_device->GetDevice(), L"SkyVS.cso", L"SkyPS.cso");

		// Sky Sphere メッシュの初期化
		m_skySphere = std::make_unique<Engine::SkySphere>();
		m_skySphere->Initialize(m_device->GetDevice(), 100.0f, 64, 32);

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

		// Assets へのパスを動的に解く
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);
		m_context->EndFrame();
		// アップロード終了まで待機
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;

		// Sky Sphere テクスチャ（HDR/EXR）の読み込み
		m_skyTexture = std::make_unique<Engine::Texture>();
		m_context->BeginFrame();
		UINT skySrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE skyCpuHandle = m_device->AllocateSrvDescriptor(&skySrvIndex);
		
		// 複数のパスを試す
		std::vector<std::wstring> skyTexturePaths = {
			L"C:\\Users\\hazu0\\DX12\\SkyMission\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\Assets\\Images\\water-bg-pattern-04.jpg",
			exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg",
		};
		
		bool skyTextureLoaded = false;
		for (const auto& path : skyTexturePaths) {
			// ファイルが存在するか確認
			WIN32_FILE_ATTRIBUTE_DATA fileInfo;
			if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo) != 0) {
				char pathBuffer[512];
				size_t converted = 0;
				wcstombs_s(&converted, pathBuffer, sizeof(pathBuffer), path.c_str(), _TRUNCATE);
				OutputDebugStringA("Trying to load sky texture from: ");
				OutputDebugStringA(pathBuffer);
				OutputDebugStringA("\n");
				
				if (m_skyTexture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
					OutputDebugStringA("Sky texture loaded successfully!\n");
					skyTextureLoaded = true;
					break;
				}
				else {
					OutputDebugStringA("Failed to load this file.\n");
				}
			}
			else {
				char pathBuffer[512];
				size_t converted = 0;
				wcstombs_s(&converted, pathBuffer, sizeof(pathBuffer), path.c_str(), _TRUNCATE);
				OutputDebugStringA("File not found: ");
				OutputDebugStringA(pathBuffer);
				OutputDebugStringA("\n");
			}
		}
		
		if (!skyTextureLoaded) {
			OutputDebugStringA("Warning: No sky texture could be loaded from any path.\n");
		}
		
		m_skyTexture->CreateShaderResourceView(m_device->GetDevice(), skyCpuHandle);
		m_context->EndFrame();
		m_context->WaitForGpu();
		m_skyTextureSrvIndex = skySrvIndex;

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

		m_camera = std::make_unique<Engine::Camera>();
		m_camera->Initialize(m_window->GetHandle(), DirectX::XM_PIDIV4, static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight()), 0.1f, 1000.0f);
		m_lastTime = std::chrono::steady_clock::now();
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
		// compute delta
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		float deltaSeconds = dt.count();

		time += deltaSeconds; // use real delta time for animation speed

		// Update camera first
		if (m_camera) m_camera->Update(deltaSeconds);

		// MVP行列を再計算
		using namespace DirectX;
		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX view = m_camera->GetView();

		float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
		XMMATRIX proj = m_camera->GetProjection();
		XMMATRIX mvp = world * view * proj;
		XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 m;
		XMStoreFloat4x4(&m, mvpT);

		// 定数バッファを更新
		ConstantBufferData* data;
		m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&data));

		data->mvp = m;
		data->time = time;
		// カメラ位置を現在のカメラから取得（VS/PS のフレネル計算用）
		if (m_camera) {
			auto camPos = m_camera->GetPosition();
			data->cameraPos = camPos;
		}

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

		// クリアと設定
		auto rtv = m_device->GetCurrentRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		// Sky Sphere を最初に描画（背景として）
		if (m_skySphere && m_skyTexture && m_skyTexture->GetResource()) {
			cmd->SetGraphicsRootSignature(m_skyPipeline->GetRootSignature());
			cmd->SetPipelineState(m_skyPipeline->GetPSO());

			// Sky Sphere 用のテクスチャをバインド
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_skyTextureSrvIndex));

			// 定数バッファをバインド
			if (m_constantBuffer) {
				cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
			}

			// Sky Sphere の頂点・インデックスバッファをバインド
			auto skyView = m_skySphere->GetVertexBufferView();
			auto& skyIndexView = m_skySphere->GetIndexBufferView();
			cmd->IASetVertexBuffers(0, 1, &skyView);
			cmd->IASetIndexBuffer(&skyIndexView);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Sky Sphere を描画
			cmd->DrawIndexedInstanced(m_skySphere->GetIndexCount(), 1, 0, 0, 0);
		}

		// 次に水面メッシュを描画（前景として）
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

		// 水面メッシュを描画
		cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

		// Present へ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

}