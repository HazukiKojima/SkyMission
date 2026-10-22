#include "Renderer.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include "../../Core/Scene.h"
#include "../../Core/SceneLoader.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace Engine {

	namespace {
		constexpr float kOceanFogTop = 350.0f;
		constexpr float kOceanFogDensity = 0.00000112f;
		constexpr float kPi = 3.14159265359f;

		float CalculateOceanWaveHeight(float x, float z, float time, float wavelength, float steepness) {
			const float baseWindX = 1.0f / std::sqrt(1.0f + 0.6f * 0.6f);
			const float baseWindZ = 0.6f / std::sqrt(1.0f + 0.6f * 0.6f);
			float height = 0.0f;

			for (int i = 0; i < 6; ++i) {
				const float phaseOffset = static_cast<float>(i) * 21.53f;
				const float randomAngle = std::sin(static_cast<float>(i) * 7.3f) * 1.1f;
				const float sinAngle = std::sin(randomAngle);
				const float cosAngle = std::cos(randomAngle);
				const float directionX = cosAngle * baseWindX - sinAngle * baseWindZ;
				const float directionZ = sinAngle * baseWindX + cosAngle * baseWindZ;
				const float k = 2.0f * kPi / wavelength;
				const float waveSpeed = std::sqrt(9.8f / k);
				const float phase = k * (directionX * x + directionZ * z - waveSpeed * time * 3.0f) + phaseOffset;

				height += (steepness / k) * std::sin(phase);
				wavelength *= 0.82f;
				steepness *= 0.72f;
			}

			return height;
		}

		struct ModelConstantBufferData {
			DirectX::XMFLOAT4X4 mvp;
			DirectX::XMFLOAT4X4 world;
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
	}

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
		float oceanSize;
		float oceanWaveLength;
		float oceanWaveSteepness;
		float oceanNormalUvScale;
		float oceanWavePatternUvScale;
		float oceanFogStartDistance;
		float oceanFogEndDistance;
		float oceanEdgeFadeStart;
		float oceanEdgeFadeWidth;
		float oceanWaveHeightOffset;
	};

	struct OceanFogConstants {
		DirectX::XMFLOAT4X4 inverseViewProjection;
		DirectX::XMFLOAT3 cameraPosition;
		float padding0;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float oceanFogBottom;
		float oceanFogTop;
		float oceanFogDensity;
		float oceanFogStartDistance;
		float oceanFogEndDistance;
		float oceanEdgeFadeStart;
		float oceanEdgeFadeWidth;
	};

	Renderer::~Renderer() {
		if (m_context) {
			// GPU処理の完了を待機してリソース解放を安全に行う
			m_context->WaitForGpu();
		}
	}

	void Renderer::Initialize(HWND hwnd, UINT width, UINT height, const SceneAssetPaths& assets) {
		m_oceanSize = assets.ocean.size;
		m_oceanWaveLength = assets.ocean.waveLength;
		m_oceanWaveSteepness = assets.ocean.waveSteepness;
		m_oceanNormalUvScale = assets.ocean.normalUvScale;
		m_oceanWavePatternUvScale = assets.ocean.wavePatternUvScale;
		m_oceanFogStartDistance = assets.ocean.fogStartDistance;
		m_oceanFogEndDistance = assets.ocean.fogEndDistance;
		m_oceanEdgeFadeStart = assets.ocean.edgeFadeStart;
		m_oceanEdgeFadeWidth = assets.ocean.edgeFadeWidth;
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
			3,
			false,
			true
		);

		m_oceanFogPipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_oceanFogPipeline->InitializeWithShaders(
			m_device->GetDevice(),
			L"OceanFogVS.cso",
			L"OceanFogPS.cso",
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
			4,
			false,
			true,
			true
		);

		// Sky Sphere の生成
		m_skySphere = std::make_unique<Engine::SkySphere>();
		m_skySphere->Initialize(m_device->GetDevice(), assets.sky.radius, 64, 32);

		// 頂点フォーマット定義
		struct Vertex {
			float pos[3];
			float uv[2];
		};
		// --- グリッドメッシュ作成 ---
		const int gridSize = 1000;
		const float oceanSize = m_oceanSize;

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
		m_context->BeginFrame();
		UINT srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSrvDescriptor(&srvIndex);
		m_texture = Engine::Texture::LoadCached(m_device->GetDevice(), m_context->GetCommandList(), assets.oceanTexture.wstring(), true, true);
		if (!m_texture) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		else {
			m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);
		}

		UINT normalSrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE normalCpuHandle = m_device->AllocateSrvDescriptor(&normalSrvIndex);
		m_oceanNormalTexture = Engine::Texture::LoadCached(m_device->GetDevice(), m_context->GetCommandList(), assets.oceanNormalTexture.wstring(), false, true);
		if (!m_oceanNormalTexture) {
			OutputDebugStringA("Application::Initialize - failed to load ocean normal map\n");
		}
		else {
			m_oceanNormalTexture->CreateShaderResourceView(m_device->GetDevice(), normalCpuHandle);
		}

		UINT normal2SrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE normal2CpuHandle = m_device->AllocateSrvDescriptor(&normal2SrvIndex);
		m_oceanNormalTexture2 = Engine::Texture::LoadCached(m_device->GetDevice(), m_context->GetCommandList(), assets.oceanNormalTexture2.wstring(), false, true);
		if (!m_oceanNormalTexture2) {
			OutputDebugStringA("Application::Initialize - failed to load second ocean normal map\n");
		}
		else {
			m_oceanNormalTexture2->CreateShaderResourceView(m_device->GetDevice(), normal2CpuHandle);
		}
		m_context->EndFrame();
		// フレーム終了後にGPU完了待ち
		m_context->WaitForGpu();
		if (m_texture) {
			m_texture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture) {
			m_oceanNormalTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture2) {
			m_oceanNormalTexture2->ReleaseUploadResources();
		}
		m_textureSrvIndex = srvIndex;
		m_oceanNormalTextureSrvIndex = normalSrvIndex;
		m_oceanNormalTexture2SrvIndex = normal2SrvIndex;

		// Sky Sphere 用の環境テクスチャ(HDR/EXR)読み込み
		m_context->BeginFrame();
		UINT skySrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE skyCpuHandle = m_device->AllocateSrvDescriptor(&skySrvIndex);

		m_skyTexture = Engine::Texture::LoadCached(
			m_device->GetDevice(),
			m_context->GetCommandList(),
			assets.skyTexture.wstring());
		if (!m_skyTexture) {
			OutputDebugStringA("Failed to load sky texture from JSON path\n");
		}

		if (m_skyTexture) {
			m_skyTexture->CreateShaderResourceView(m_device->GetDevice(), skyCpuHandle);
		}

		m_model = std::make_shared<Model>();
		if (!m_model->Load(m_device->GetDevice(), m_context->GetCommandList(), m_device.get(), assets.aircraftModel.wstring())) {
			OutputDebugStringA("Failed to load F16 model\n");
			m_model.reset();
		}

		m_context->EndFrame();
		m_context->WaitForGpu();
		if (m_texture) {
			m_texture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture) {
			m_oceanNormalTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture2) {
			m_oceanNormalTexture2->ReleaseUploadResources();
		}
		if (m_skyTexture) {
			m_skyTexture->ReleaseUploadResources();
		}
		if (m_model) {
			m_model->ReleaseUploadResources();
		}
		m_skyTextureSrvIndex = skySrvIndex;
	}

	void Renderer::InitializeConstantBuffers(const Scene& scene) {
		const Camera& camera = scene.GetCamera();
		const SceneEnvironment& environment = scene.GetEnvironment();
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
			XMMATRIX view = camera.GetView();
			XMMATRIX proj = camera.GetProjection();
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
			cbInit->cameraPos = camera.GetPosition();
			cbInit->sunDirection = environment.sunDirection;
			cbInit->sunIntensity = environment.sunIntensity;
			cbInit->sunColor = environment.sunColor;
			cbInit->ambientIntensity = environment.ambientIntensity;
			cbInit->ambientColor = environment.ambientColor;
			cbInit->oceanSize = m_oceanSize;
			cbInit->oceanWaveLength = m_oceanWaveLength;
			cbInit->oceanWaveSteepness = m_oceanWaveSteepness;
			cbInit->oceanNormalUvScale = m_oceanNormalUvScale;
			cbInit->oceanWavePatternUvScale = m_oceanWavePatternUvScale;
			cbInit->oceanFogStartDistance = m_oceanFogStartDistance;
			cbInit->oceanFogEndDistance = m_oceanFogEndDistance;
			cbInit->oceanEdgeFadeStart = m_oceanEdgeFadeStart;
			cbInit->oceanEdgeFadeWidth = m_oceanEdgeFadeWidth;
			cbInit->oceanWaveHeightOffset = 0.0f;
		}

		UINT64 fogCbSize = (sizeof(OceanFogConstants) + 255) & ~255;
		CD3DX12_RESOURCE_DESC fogCbDesc = CD3DX12_RESOURCE_DESC::Buffer(fogCbSize);
		CD3DX12_HEAP_PROPERTIES fogHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
			&fogHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&fogCbDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_oceanFogConstantBuffer)));
		CD3DX12_RANGE fogReadRange(0, 0);
		ThrowIfFailed(m_oceanFogConstantBuffer->Map(0, &fogReadRange, reinterpret_cast<void**>(&m_oceanFogCbvDataPtr)));

	}

	void Renderer::EnsureModelConstantBufferCapacity(size_t objectCount) {
		if (objectCount <= m_modelConstantBuffers[0].size() && objectCount <= m_modelConstantBuffers[1].size()) {
			return;
		}

		m_context->WaitForGpu();
		const UINT64 bufferSize = (sizeof(ModelConstantBufferData) + 255) & ~255;
		for (UINT frameIndex = 0; frameIndex < RenderDevice::FrameCount; ++frameIndex) {
			auto& buffers = m_modelConstantBuffers[frameIndex];
			while (buffers.size() < objectCount) {
				ModelConstantBuffer buffer;
				CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
				auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
				ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&desc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&buffer.resource)));
				CD3DX12_RANGE readRange(0, 0);
				ThrowIfFailed(buffer.resource->Map(0, &readRange, reinterpret_cast<void**>(&buffer.mappedData)));
				buffers.push_back(std::move(buffer));
			}
		}
	}

	void Renderer::Update(const Scene& scene, float elapsedTime) {
		// MVPを計算して定数バッファにセット
		using namespace DirectX;
		const Camera& camera = scene.GetCamera();
		const SceneEnvironment& environment = scene.GetEnvironment();
		const auto camPos = camera.GetPosition();
		const float cameraHeight = (camPos.y < 0.0f) ? -camPos.y : camPos.y;
		const float oceanHalfDiagonal = m_oceanSize * 0.70710678f;
		const float oceanCornerDistance = std::sqrt(
			oceanHalfDiagonal * oceanHalfDiagonal + cameraHeight * cameraHeight);

		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX view = camera.GetView();

		XMMATRIX proj = camera.GetProjection(oceanCornerDistance * 1.1f);
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
		data->cameraPos = camPos;
		data->oceanOffset[0] = camPos.x;
		data->oceanOffset[1] = 0.0f;
		data->oceanOffset[2] = camPos.z;
		data->sunDirection = environment.sunDirection;
		data->sunIntensity = environment.sunIntensity;
		data->sunColor = environment.sunColor;
		data->ambientIntensity = environment.ambientIntensity;
		data->ambientColor = environment.ambientColor;
		data->oceanSize = m_oceanSize;
		data->oceanWaveLength = m_oceanWaveLength;
		data->oceanWaveSteepness = m_oceanWaveSteepness;
		data->oceanNormalUvScale = m_oceanNormalUvScale;
		data->oceanWavePatternUvScale = m_oceanWavePatternUvScale;
		data->oceanFogStartDistance = m_oceanFogStartDistance;
		data->oceanFogEndDistance = m_oceanFogEndDistance;
		data->oceanEdgeFadeStart = m_oceanEdgeFadeStart;
		data->oceanEdgeFadeWidth = m_oceanEdgeFadeWidth;
		data->oceanWaveHeightOffset = CalculateOceanWaveHeight(
			camPos.x,
			camPos.z,
			elapsedTime,
			m_oceanWaveLength,
			m_oceanWaveSteepness);

		OceanFogConstants* oceanFog = reinterpret_cast<OceanFogConstants*>(m_oceanFogCbvDataPtr);
		XMStoreFloat4x4(&oceanFog->inverseViewProjection, XMMatrixTranspose(inverseViewProjection));
		oceanFog->cameraPosition = camPos;
		oceanFog->padding0 = 0.0f;
		oceanFog->sunDirection = environment.sunDirection;
		oceanFog->sunIntensity = environment.sunIntensity;
		oceanFog->sunColor = environment.sunColor;
		oceanFog->ambientIntensity = environment.ambientIntensity;
		oceanFog->ambientColor = environment.ambientColor;
		oceanFog->oceanFogBottom = 0.0f;
		oceanFog->oceanFogTop = kOceanFogTop;
		oceanFog->oceanFogDensity = kOceanFogDensity;
		oceanFog->oceanFogStartDistance = m_oceanFogStartDistance;
		oceanFog->oceanFogEndDistance = m_oceanFogEndDistance;
		oceanFog->oceanEdgeFadeStart = m_oceanEdgeFadeStart;
		oceanFog->oceanEdgeFadeWidth = m_oceanEdgeFadeWidth;

		const auto& objects = scene.GetObjects();
		EnsureModelConstantBufferCapacity(objects.size());
		const UINT frameIndex = m_context->GetFrameIndex();
		for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
			const SceneObject& object = objects[objectIndex];
			ModelConstantBufferData* modelData = reinterpret_cast<ModelConstantBufferData*>(m_modelConstantBuffers[frameIndex][objectIndex].mappedData);
			const XMMATRIX objectWorld = XMLoadFloat4x4(&object.worldTransform);
			XMStoreFloat4x4(&modelData->mvp, XMMatrixTranspose(objectWorld * view * proj));
			XMStoreFloat4x4(&modelData->world, XMMatrixTranspose(objectWorld));
			modelData->time = elapsedTime;
			modelData->padding[0] = modelData->padding[1] = modelData->padding[2] = 0.0f;
			modelData->cameraPos = camPos;
			modelData->pad2 = 0.0f;
			modelData->sunDirection = environment.sunDirection;
			modelData->sunIntensity = environment.sunIntensity;
			modelData->sunColor = environment.sunColor;
			modelData->ambientIntensity = environment.ambientIntensity;
			modelData->ambientColor = environment.ambientColor;
			modelData->pad3 = 0.0f;
		}
	}

	void Renderer::Render(UINT width, UINT height, const Scene& runtimeScene) {
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
		if (m_texture && m_oceanNormalTexture && m_oceanNormalTexture2) {
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

		const auto& objects = runtimeScene.GetObjects();
		const UINT frameIndex = m_context->GetFrameIndex();
		if (m_modelPipeline) {
			cmd->SetGraphicsRootSignature(m_modelPipeline->GetRootSignature());
			cmd->SetPipelineState(m_modelPipeline->GetPSO());
			ID3D12DescriptorHeap* modelHeaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
			for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
				const SceneObject& object = objects[objectIndex];
				if (!object.model || !object.model->IsLoaded()) {
					continue;
				}
				cmd->SetGraphicsRootConstantBufferView(1, m_modelConstantBuffers[frameIndex][objectIndex].resource->GetGPUVirtualAddress());
				object.model->Draw(cmd, m_device.get());
			}
		}

		m_context->TransitionResource(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_context->TransitionResource(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		rtv = m_device->GetCurrentRtvHandle();
		const float backBufferClear[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd->ClearRenderTargetView(rtv, backBufferClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		cmd->SetGraphicsRootSignature(m_oceanFogPipeline->GetRootSignature());
		cmd->SetPipelineState(m_oceanFogPipeline->GetPSO());
		ID3D12DescriptorHeap* fogHeaps[] = { m_device->GetSrvDescriptorHeap() };
		cmd->SetDescriptorHeaps(_countof(fogHeaps), fogHeaps);
		cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_device->GetSceneColorSrvIndex()));
		cmd->SetGraphicsRootConstantBufferView(1, m_oceanFogConstantBuffer->GetGPUVirtualAddress());
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