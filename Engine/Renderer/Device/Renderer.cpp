#include "Renderer.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include "../../Core/Camera.h"
#include <DirectXMath.h>

namespace Engine {

	struct ConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		float time;
		float oceanOffset[3];
		DirectX::XMFLOAT3 cameraPos;
		float pad2;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float pad3;
	};

	struct CloudConstants {
		DirectX::XMFLOAT4X4 inverseViewProjection;
		DirectX::XMFLOAT3 cameraPosition;
		float time;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float cloudDensity;
		float cloudBottom;
		float cloudTop;
		float shapeScale;
		float detailScale;
		float detailStrength;
		float absorption;
		float stepSize;
		int stepCount;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float oceanFogBottom;
		float oceanFogTop;
		float oceanFogDensity;
		float oceanFogDistance;
	};

	Renderer::~Renderer() {
		if (m_context) {
			// GPU処理の完了を待機してリソース解放を安全に行う
			m_context->WaitForGpu();
		}
	}

	void Renderer::Initialize(HWND hwnd, UINT width, UINT height) {
		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(hwnd, width, height);

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());

		// 基本描画用パイプライン
		m_pipeline =
			std::make_unique<Engine::GraphicsPipeline>();

		m_pipeline->Initialize(
			m_device->GetDevice()
		);


		// 海面描画用パイプライン
		m_oceanPipeline =
			std::make_unique<Engine::GraphicsPipeline>();

		m_oceanPipeline->InitializeWithShaders(
			m_device->GetDevice(),
			L"OceanVS.cso",
			L"OceanPS.cso",
			2,
			false,
			true
		);

		m_cloudPipeline =
			std::make_unique<Engine::GraphicsPipeline>();

		m_cloudPipeline->InitializeWithShaders(
			m_device->GetDevice(),
			L"CloudVS.cso",
			L"CloudPS.cso",
			2,
			false
		);


		// 空描画用パイプライン
		m_skyPipeline =
			std::make_unique<Engine::GraphicsPipeline>();

		m_skyPipeline->InitializeWithShaders(
			m_device->GetDevice(),
			L"SkyVS.cso",
			L"SkyPS.cso",
			1
		);

		m_modelPipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_modelPipeline->InitializeWithShaders(
			m_device->GetDevice(),
			L"ModelVS.cso",
			L"ModelPS.cso",
			1,
			false,
			true
		);

		// Sky Sphere の生成
		m_skySphere = std::make_unique<Engine::SkySphere>();
		m_skySphere->Initialize(m_device->GetDevice(), 100.0f, 64, 32);

		// 頂点フォーマット定義
		struct Vertex {
			float pos[3];
			float uv[2];
		};
		// --- グリッドメッシュ作成 ---
		const int gridSize = 1000;
		const float oceanSize = 5000.0f;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		vertices.reserve(static_cast<size_t>(gridSize) * gridSize);
		indices.reserve(static_cast<size_t>(gridSize - 1) * (gridSize - 1) * 6);

		for (int z = 0; z < gridSize; ++z) {
			for (int x = 0; x < gridSize; ++x) {
				float px =
					(float)x / (gridSize - 1) * oceanSize
					- oceanSize * 0.5f;

				float pz =
					(float)z / (gridSize - 1) * oceanSize
					- oceanSize * 0.5f;

				float u = (float)x / (gridSize - 1);
				float v = (float)z / (gridSize - 1);

				vertices.push_back({
					{ px, 0.0f, pz },
					{ u, v }
					});
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

		// --- 頂点バッファの作成 ---
		m_vertexBuffer = std::make_unique<Engine::VertexBuffer>();
		m_vertexBuffer->Initialize(m_device->GetDevice(), vertices.data(), sizeof(Vertex) * vertices.size(), sizeof(Vertex));

		// --- インデックス用GPUバッファ作成（アップロード） ---
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

		// テクスチャ読み込みとSRV作成の準備
		m_texture = std::make_unique<Engine::Texture>();
		// 実行ファイルパスからアセット候補パスを構築
		m_context->BeginFrame();
		UINT srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSrvDescriptor(&srvIndex);
		// 実行ファイルのパスを取得
		wchar_t buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring exePath = buffer;
		std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));

		// Assets 配下の候補パスを作成
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
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
		m_oceanNormalTexture->CreateShaderResourceView(m_device->GetDevice(), normalCpuHandle);
		m_context->EndFrame();
		// フレーム終了後にGPU完了待ち
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;
		m_oceanNormalTextureSrvIndex = normalSrvIndex;

		// Sky Sphere 用の環境テクスチャ(HDR/EXR)読み込み
		m_skyTexture = std::make_unique<Engine::Texture>();
		m_context->BeginFrame();
		UINT skySrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE skyCpuHandle = m_device->AllocateSrvDescriptor(&skySrvIndex);

		// 読み込み候補パス一覧
		std::vector<std::wstring> skyTexturePaths = {
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg",
		};

		bool skyTextureLoaded = false;
		for (const auto& path : skyTexturePaths) {
			// ファイル存在チェックと読み込み
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

		const std::vector<std::wstring> modelPaths = {
			exeDir + L"\\..\\..\\Assets\\Models\\model.fbx",
			exeDir + L"\\..\\..\\..\\Assets\\Models\\model.fbx",
			exeDir + L"\\..\\Assets\\Models\\model.fbx",
			exeDir + L"\\Assets\\Models\\model.fbx"
		};
		for (const auto& modelPath : modelPaths) {
			WIN32_FILE_ATTRIBUTE_DATA fileInfo;
			if (GetFileAttributesExW(modelPath.c_str(), GetFileExInfoStandard, &fileInfo) != 0) {
				m_model = std::make_unique<Engine::Model>();
				if (m_model->Load(m_device->GetDevice(), m_context->GetCommandList(), m_device.get(), modelPath)) {
					OutputDebugStringA("Application::Initialize - FBX model loaded\n");
				}
				else {
					m_model.reset();
				}
				break;
			}
		}
		if (!m_model) {
			OutputDebugStringA("Application::Initialize - place an FBX at Assets\\Models\\model.fbx to render it\n");
		}

		m_context->EndFrame();
		m_context->WaitForGpu();
		m_skyTextureSrvIndex = skySrvIndex;
	}

	void Renderer::InitializeConstantBuffers(Camera* camera) {
		// 定数バッファ (MVP) を作成して初期値をセット
		{
			using namespace DirectX;
			UINT64 cbSize = (sizeof(ConstantBufferData) + 255) & ~255; // 256バイト境界に揃える

			CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)));

			// マッピングして初期値を書き込む
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataPtr)));

			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX view = camera->GetView();
			XMMATRIX proj = camera->GetProjection();
			XMMATRIX mvp = world * view * proj;
			XMMATRIX mvpT = XMMatrixTranspose(mvp);

			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, mvpT);
			// 定数バッファへ初期値を書き込む（カメラ位置など）
			ConstantBufferData* cbInit = reinterpret_cast<ConstantBufferData*>(m_cbvDataPtr);
			cbInit->mvp = m;
			cbInit->time = 0.0f;
			cbInit->oceanOffset[0] = 10.0f;
			cbInit->oceanOffset[1] = 0.0f;
			cbInit->oceanOffset[2] = -10.0f;
			cbInit->cameraPos = DirectX::XMFLOAT3(10.0f, 15.0f, -10.0f);
			cbInit->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
			cbInit->sunIntensity = 1.8f;
			cbInit->sunColor = DirectX::XMFLOAT3(1.0f, 0.98f, 0.96f);
			cbInit->ambientIntensity = 0.32f;
			cbInit->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);
		}

		UINT64 cloudCbSize = (sizeof(CloudConstants) + 255) & ~255;
		CD3DX12_RESOURCE_DESC cloudCbDesc = CD3DX12_RESOURCE_DESC::Buffer(cloudCbSize);
		CD3DX12_HEAP_PROPERTIES cloudHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
			&cloudHeapProps, D3D12_HEAP_FLAG_NONE, &cloudCbDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&m_cloudConstantBuffer)));
		CD3DX12_RANGE cloudReadRange(0, 0);
		ThrowIfFailed(m_cloudConstantBuffer->Map(0, &cloudReadRange, reinterpret_cast<void**>(&m_cloudCbvDataPtr)));
	}

	void Renderer::Update(Camera* camera, float elapsedTime) {
		// MVPを計算して定数バッファにセット
		using namespace DirectX;
		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX view = camera->GetView();

		XMMATRIX proj = camera->GetProjection();
		XMMATRIX mvp = world * view * proj;
		XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, view * proj);
		XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 m;
		XMStoreFloat4x4(&m, mvpT);

		// 定数バッファへ書き込み
		ConstantBufferData* data = reinterpret_cast<ConstantBufferData*>(m_cbvDataPtr);

		data->mvp = m;
		data->time = elapsedTime;
		// カメラ位置などの情報をセット（VS/PSで参照）
		if (camera) {
			auto camPos = camera->GetPosition();
			data->cameraPos = camPos;

			data->oceanOffset[0] = camPos.x;
			data->oceanOffset[1] = 0.0f;
			data->oceanOffset[2] = camPos.z;
		}
		data->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
		data->sunIntensity = 1.8f;
		data->sunColor = DirectX::XMFLOAT3(1.0f, 0.98f, 0.96f);
		data->ambientIntensity = 0.32f;
		data->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);

		CloudConstants* cloud = reinterpret_cast<CloudConstants*>(m_cloudCbvDataPtr);
		XMStoreFloat4x4(&cloud->inverseViewProjection, XMMatrixTranspose(inverseViewProjection));
		cloud->cameraPosition = camera->GetPosition();
		cloud->time = elapsedTime;
		cloud->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
		cloud->sunIntensity = 1.8f;
		cloud->sunColor = DirectX::XMFLOAT3(1.0f, 0.98f, 0.96f);
		cloud->cloudDensity = 0.62f;
		cloud->cloudBottom = 800.0f;
		cloud->cloudTop = 2100.0f;
		cloud->shapeScale = 0.0018f;
		cloud->detailScale = 0.0045f;
		cloud->detailStrength = 0.10f;
		cloud->absorption = 0.010f;
		cloud->stepSize = 120.0f;
		cloud->stepCount = 32;
		cloud->ambientIntensity = 0.32f;
		cloud->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);
		cloud->oceanFogBottom = 0.0f;
		cloud->oceanFogTop = 350.0f;
		cloud->oceanFogDensity = 0.00035f;
		cloud->oceanFogDistance = 12000.0f;
	}

	void Renderer::Render(UINT width, UINT height) {
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto resource = m_device->GetCurrentRenderTarget();
		auto scene = m_device->GetSceneRenderTarget();
		auto depth = m_device->GetSceneDepth();

		// バックバッファをレンダーターゲットへ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(scene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissorRect);

		// 画面クリア
		auto rtv = m_device->GetSceneRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		auto dsv = m_device->GetDsvHandle();
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		// Sky Sphere を描画
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

			// Sky Sphere の頂点/インデックスバッファをセット
			auto skyView = m_skySphere->GetVertexBufferView();
			auto& skyIndexView = m_skySphere->GetIndexBufferView();
			cmd->IASetVertexBuffers(0, 1, &skyView);
			cmd->IASetIndexBuffer(&skyIndexView);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 描画
			cmd->DrawIndexedInstanced(m_skySphere->GetIndexCount(), 1, 0, 0, 0);
		}

		// 海面を空の手前に描画
		cmd->SetGraphicsRootSignature(m_oceanPipeline->GetRootSignature());
		cmd->SetPipelineState(m_oceanPipeline->GetPSO());

		// テクスチャ用と法線マップ用のSRVをセット
		if (m_texture && m_oceanNormalTexture) {
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_textureSrvIndex));
		}

		// 定数バッファ（MVP）のGPUバインド
		if (m_constantBuffer) {
			cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
		}

		// 頂点/インデックスバッファをセット
		auto view = m_vertexBuffer->GetView();
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->IASetIndexBuffer(&m_indexBufferView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 描画コマンド発行
		cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

		if (m_model && m_model->IsLoaded()) {
			cmd->SetGraphicsRootSignature(m_modelPipeline->GetRootSignature());
			cmd->SetPipelineState(m_modelPipeline->GetPSO());
			ID3D12DescriptorHeap* modelHeaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
			cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
			m_model->Draw(cmd, m_device.get());
		}

		m_context->TransitionResource(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_context->TransitionResource(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		rtv = m_device->GetCurrentRtvHandle();
		const float backBufferClear[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd->ClearRenderTargetView(rtv, backBufferClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		// Scene Colorを含む完成色をCloud Passからバックバッファへ書き込む
		cmd->SetGraphicsRootSignature(m_cloudPipeline->GetRootSignature());
		cmd->SetPipelineState(m_cloudPipeline->GetPSO());
		// Scene Color SRV は常にバインドしておく（Cloud PSで参照するため）
		ID3D12DescriptorHeap* cloudHeaps[] = { m_device->GetSrvDescriptorHeap() };
		cmd->SetDescriptorHeaps(_countof(cloudHeaps), cloudHeaps);
		cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_device->GetSceneColorSrvIndex()));
		cmd->SetGraphicsRootConstantBufferView(1, m_cloudConstantBuffer->GetGPUVirtualAddress());
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		cmd->DrawInstanced(3, 1, 0, 0);

		// Present 処理
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

	void Renderer::Resize(UINT width, UINT height) {
		if (!m_context || !m_device) {
			return;
		}

		m_context->WaitForGpu();
		m_device->Resize(width, height);
	}
}