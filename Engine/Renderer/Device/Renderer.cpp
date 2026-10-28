#include "Renderer.h"
#include "../../Scene/Scene.h"
#include "../../Scene/SceneLoader.h"
#include <DirectXMath.h>

namespace Engine {

	Renderer::~Renderer() {
		if (m_context) {
			// GPU処理の完了を待機してリソース解放を安全に行う
			m_context->WaitForGpu();
		}
	}

	void Renderer::Initialize(HWND hwnd, UINT width, UINT height, const SceneAssetPaths& assets) {
		m_frameUpdater.Configure(assets.ocean);
		m_retroFilmParameters = assets.retroFilm;
		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(hwnd, width, height);

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());

		m_pipelines.Initialize(m_device->GetDevice());

		m_assets.InitializeSkySphere(m_device->GetDevice(), assets.sky.radius);

		m_oceanGeometry.Initialize(m_device->GetDevice(), m_frameUpdater.GetOceanSize());
		m_assets.Load(m_device.get(), m_context.get(), assets);
	}

	void Renderer::InitializeConstantBuffers(const Scene& scene) {
		const Camera& camera = scene.GetCamera();
		const SceneEnvironment& environment = scene.GetEnvironment();
		m_constantBuffers.Initialize(m_device->GetDevice());

		{
			using namespace DirectX;
			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX view = camera.GetView();
			XMMATRIX proj = camera.GetProjection();
			XMMATRIX mvp = world * view * proj;
			XMMATRIX mvpT = XMMatrixTranspose(mvp);

			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, mvpT);
			OceanConstantBufferData* cbInit = m_constantBuffers.GetOceanData();
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
			const OceanSettings& ocean = m_frameUpdater.GetOceanSettings();
			cbInit->oceanSize = ocean.size;
			cbInit->oceanWaveLength = ocean.waveLength;
			cbInit->oceanWaveSteepness = ocean.waveSteepness;
			cbInit->oceanNormalUvScale = ocean.normalUvScale;
			cbInit->oceanWavePatternUvScale = ocean.wavePatternUvScale;
			cbInit->oceanFogStartDistance = ocean.fogStartDistance;
			cbInit->oceanFogEndDistance = ocean.fogEndDistance;
			cbInit->oceanEdgeFadeStart = ocean.edgeFadeStart;
			cbInit->oceanEdgeFadeWidth = ocean.edgeFadeWidth;
			cbInit->oceanWaveHeightOffset = 0.0f;
		}
	}

	void Renderer::Update(const Scene& scene, float elapsedTime, float deltaSeconds) {
		m_frameUpdater.Update(scene, elapsedTime, deltaSeconds, m_constantBuffers, m_device->GetDevice(), m_context.get());
	}

	void Renderer::Render(UINT width, UINT height, const Scene& runtimeScene) {
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto resource = m_device->GetCurrentRenderTarget();
		auto scene = m_device->GetSceneRenderTarget();
		auto postProcess = m_device->GetPostProcessRenderTarget();
		auto depth = m_device->GetSceneDepth();

		// バックバッファをレンダーターゲットへ遷移
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(scene, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(postProcess, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
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

		m_skyPass.Record(cmd, m_device.get(), m_assets, m_pipelines, m_constantBuffers);
		m_oceanPass.Record(cmd, m_device.get(), m_assets, m_pipelines, m_constantBuffers, m_oceanGeometry);
		m_modelPass.Record(cmd, m_device.get(), m_context.get(), m_pipelines, m_constantBuffers, runtimeScene, m_frameUpdater.GetPropellerAngle());

		m_context->TransitionResource(scene, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_context->TransitionResource(depth, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		rtv = m_device->GetPostProcessRtvHandle();
		const float postProcessClear[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd->ClearRenderTargetView(rtv, postProcessClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		m_oceanFogPass.Record(cmd, m_device.get(), m_pipelines, m_constantBuffers);
		m_context->TransitionResource(postProcess, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		rtv = m_device->GetCurrentRtvHandle();
		const float backBufferClear[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		cmd->ClearRenderTargetView(rtv, backBufferClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		m_retroFilmPass.Record(cmd, m_device.get(), m_context.get(), m_pipelines, m_constantBuffers, m_retroFilmParameters, m_frameUpdater.GetElapsedTime());

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