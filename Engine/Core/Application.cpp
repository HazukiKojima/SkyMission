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
			m_context->WaitForGpu(); // 終了前にGPU処理を完了させ、使用中のリソースを安全に解放する
		}
	}

	struct ConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		float time;
		float padding[3];
		DirectX::XMFLOAT3 cameraPos;
		float pad2;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float pad3;
	};

	// 初期化処理：ウィンドウ・デバイス・コマンドコンテキスト・パイプライン等をセットアップ
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		// リサイズ時にバックバッファとカメラの投影を更新
		m_window->SetOnResize([this](UINT w, UINT h) {
			if (w == 0 || h == 0) return; // 最小化中はサイズが0になるため無視
			if (m_context) m_context->WaitForGpu(); // リサイズ前にGPU処理を完了させる
			m_device->Resize(w, h);
			if (m_camera) m_camera->OnResize(w, h);
			// MVPはUpdateで毎フレーム再計算する
			});

		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight());

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());

		// 基本描画用パイプラインを初期化
		m_pipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_pipeline->Initialize(m_device->GetDevice());

		// 空描画用パイプラインを初期化
		m_skyPipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_skyPipeline->InitializeWithShaders(m_device->GetDevice(), L"SkyVS.cso", L"SkyPS.cso");

		// 空を描画する球体メッシュを初期化
		m_skySphere = std::make_unique<Engine::SkySphere>();
		m_skySphere->Initialize(m_device->GetDevice(), 100.0f, 64, 32);

		// 海面グリッドの頂点形式
		struct Vertex {
			float pos[3];
			float uv[2];
		};
		// 海面グリッドの頂点とインデックスを生成
		const int gridSize = 256;
		const float gridExtent = 4000.0f;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		for (int z = 0; z < gridSize; ++z) {
			for (int x = 0; x < gridSize; ++x) {
				float px = (float)x / (gridSize - 1) * gridExtent - gridExtent * 0.5f;
				float pz = (float)z / (gridSize - 1) * gridExtent - gridExtent * 0.5f;
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

		// 頂点バッファを初期化
		m_vertexBuffer = std::make_unique<Engine::VertexBuffer>();
		m_vertexBuffer->Initialize(m_device->GetDevice(), vertices.data(), sizeof(Vertex) * vertices.size(), sizeof(Vertex));

		// インデックスバッファを作成
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

		// テクスチャを読み込み、SRVを作成
		m_texture = std::make_unique<Engine::Texture>();
		// テクスチャのアップロードコマンドを記録
		m_context->BeginFrame();
		UINT srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSrvDescriptor(&srvIndex);
		// 実行ファイルの場所を取得
		wchar_t buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring exePath = buffer;
		std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));

		// 実行ファイルの場所を基準にアセットのパスを組み立てる
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
			// フォールバック: 単色テクスチャを作成
			m_texture->CreateFromSolidColor(m_device->GetDevice(), m_context->GetCommandList(), 200, 200, 200, 255);
		}
		m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);

		m_oceanNormalTexture = std::make_unique<Engine::Texture>();
		UINT normalSrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE normalCpuHandle = m_device->AllocateSrvDescriptor(&normalSrvIndex);
		std::vector<std::wstring> normalTexturePaths = {
			exeDir + L"\\..\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\..\\..\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\Assets\\Images\\T_Ocean_Normal.jpg"
		};
		bool normalTextureLoaded = false;
		for (const auto& normalPath : normalTexturePaths) {
			WIN32_FILE_ATTRIBUTE_DATA normalFileInfo;
			if (GetFileAttributesExW(normalPath.c_str(), GetFileExInfoStandard, &normalFileInfo) != 0 &&
				m_oceanNormalTexture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), normalPath, false)) {
				normalTextureLoaded = true;
				break;
			}
		}
		if (!normalTextureLoaded) {
			OutputDebugStringA("Application::Initialize - failed to load ocean normal map\n");
		}
		// 法線マップがロードできなかった場合はフラットな法線マップ(0.5,0.5,1.0)を生成
		if (!normalTextureLoaded || !m_oceanNormalTexture->GetResource()) {
			// flat normal in R8G8B8A8: (128,128,255)
			m_oceanNormalTexture->CreateFromSolidColor(m_device->GetDevice(), m_context->GetCommandList(), 128, 128, 255, 255);
		}
		m_oceanNormalTexture->CreateShaderResourceView(m_device->GetDevice(), normalCpuHandle);
		m_context->EndFrame();
		// テクスチャのアップロード完了を待つ
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;
		m_oceanNormalTextureSrvIndex = normalSrvIndex;

		// HDR/EXRの空テクスチャを読み込む
		m_skyTexture = std::make_unique<Engine::Texture>();
		m_context->BeginFrame();
		UINT skySrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE skyCpuHandle = m_device->AllocateSrvDescriptor(&skySrvIndex);

		// 実行ファイル相対の候補も試し、特定環境の絶対パスに依存しない
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
			// 候補ファイルが存在する場合のみ読み込む
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

		// フォールバック: ロードに失敗している場合は単色テクスチャを作成してSRVを再作成
		if (!skyTextureLoaded || !m_skyTexture->GetResource()) {
			OutputDebugStringA("Sky texture missing, creating fallback solid texture\n");
			m_context->BeginFrame();
			m_skyTexture->CreateFromSolidColor(m_device->GetDevice(), m_context->GetCommandList(), 128, 180, 230, 255); // 空色
			m_skyTexture->CreateShaderResourceView(m_device->GetDevice(), skyCpuHandle);
			m_context->EndFrame();
			m_context->WaitForGpu();
		}

		// デバッグ出力: テクスチャのロード状態とSRVインデックス
		{
			char buf[256];
			sprintf_s(buf, "DEBUG: skyTextureLoaded=%d, m_skyTexture resource=%p, skySrvIndex=%u\n",
				skyTextureLoaded ? 1 : 0,
				reinterpret_cast<void*>(m_skyTexture ? m_skyTexture->GetResource() : nullptr),
				(unsigned)m_skyTextureSrvIndex);
			OutputDebugStringA(buf);
		}

		// MVP用の定数バッファを作成
		{
			using namespace DirectX;
			UINT64 cbSize = (sizeof(ConstantBufferData) + 255) & ~255; // 定数バッファのサイズを256バイト境界に合わせる

			CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)));

			// CPUから書き込めるようにマップ
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataPtr)));

			XMMATRIX world = XMMatrixIdentity();
			// 初期カメラ位置と上方向を設定
			XMVECTOR eye = XMVectorSet(10.0f, 15.0f, -10.0f, 0.0f);
			XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
			float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
			XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
			XMMATRIX mvp = world * view * proj;
			XMMATRIX mvpT = XMMatrixTranspose(mvp); // シェーダー側の行列レイアウトに合わせて転置

			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, mvpT);
			// 初期カメラ位置を定数バッファに設定
			ConstantBufferData* cbInit = reinterpret_cast<ConstantBufferData*>(m_cbvDataPtr);
			cbInit->mvp = m;
			cbInit->time = 0.0f;
			cbInit->cameraPos = DirectX::XMFLOAT3(10.0f, 15.0f, -10.0f);
			cbInit->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
			cbInit->sunIntensity = 1.8f;
			cbInit->sunColor = DirectX::XMFLOAT3(1.0f, 0.91f, 0.76f);
			cbInit->ambientIntensity = 0.32f;
			cbInit->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);
		}

		m_camera = std::make_unique<Engine::Camera>();
		m_camera->Initialize(m_window->GetHandle(), DirectX::XM_PIDIV4, static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight()), 0.1f, 5000.0f);
		m_lastTime = std::chrono::steady_clock::now();
	}

	// メッセージ処理と更新・描画を実行
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
		// 前回更新からの実経過時間を取得
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		float deltaSeconds = dt.count();

		time += deltaSeconds; // 実時間差分でアニメーションを進め、フレームレート依存を避ける

		// カメラを更新してからビュー行列を計算
		if (m_camera) m_camera->Update(deltaSeconds);

		// MVP行列を計算
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
		// シェーダーでの視線計算に使うカメラ位置を設定
		if (m_camera) {
			auto camPos = m_camera->GetPosition();
			data->cameraPos = camPos;
		}
		data->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
		data->sunIntensity = 1.8f;
		data->sunColor = DirectX::XMFLOAT3(1.0f, 0.91f, 0.76f);
		data->ambientIntensity = 0.32f;
		data->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);

		m_constantBuffer->Unmap(0, nullptr);
	}

	// フレームの描画コマンドを記録して実行
	void Application::Render() {
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto resource = m_device->GetCurrentRenderTarget();

		// 描画先をレンダーターゲット状態に遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_window->GetWidth()), static_cast<float>(m_window->GetHeight()), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(m_window->GetWidth()), static_cast<LONG>(m_window->GetHeight()) };

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissorRect);

		// レンダーターゲットをクリア
		auto rtv = m_device->GetCurrentRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		// 空を背景として先に描画
		if (m_skySphere && m_skyTexture && m_skyTexture->GetResource()) {
			cmd->SetGraphicsRootSignature(m_skyPipeline->GetRootSignature());
			cmd->SetPipelineState(m_skyPipeline->GetPSO());

			// 空のテクスチャをバインド
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_skyTextureSrvIndex));

			// 定数バッファをバインド
			if (m_constantBuffer) {
				cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
			}

			// 球体の頂点・インデックスバッファをバインド
			auto skyView = m_skySphere->GetVertexBufferView();
			auto& skyIndexView = m_skySphere->GetIndexBufferView();
			cmd->IASetVertexBuffers(0, 1, &skyView);
			cmd->IASetIndexBuffer(&skyIndexView);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 空を描画
			cmd->DrawIndexedInstanced(m_skySphere->GetIndexCount(), 1, 0, 0, 0);
		}

		// 海面を空の手前に描画
		cmd->SetGraphicsRootSignature(m_pipeline->GetRootSignature());
		cmd->SetPipelineState(m_pipeline->GetPSO());

		// テクスチャがある場合のみSRVをバインド
		if (m_texture && m_oceanNormalTexture) {
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_textureSrvIndex));
		}

		// 頂点シェーダー用の定数バッファをルートにバインド
		if (m_constantBuffer) {
			cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
		}

		// 入力アセンブラーに頂点・インデックスバッファを設定
		auto view = m_vertexBuffer->GetView();
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->IASetIndexBuffer(&m_indexBufferView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 海面メッシュを描画
		cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

		// バックバッファをPresent状態に戻す
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

}