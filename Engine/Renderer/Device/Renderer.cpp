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

	void Renderer::Initialize(HWND hwnd, UINT width, UINT height, const SceneDefinition& assets) {
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
		m_constantBuffers.Initialize(m_device->GetDevice());
		m_frameUpdater.Update(scene, 0.0f, m_retroFilmParameters, m_constantBuffers, m_device->GetDevice(),
							  m_context.get());
	}

	void Renderer::UpdateFrameConstants(const Scene& scene, float elapsedTime) {
		m_frameUpdater.Update(scene, elapsedTime, m_retroFilmParameters, m_constantBuffers, m_device->GetDevice(),
							  m_context.get());
	}

	void Renderer::Render(UINT width, UINT height, const Scene& runtimeScene) {
		if (width == 0 || height == 0)
			return;
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto backBuffer = m_device->GetCurrentRenderTarget();
		auto sceneTarget = m_device->GetSceneRenderTarget();
		auto postProcessTarget = m_device->GetPostProcessRenderTarget();
		auto sceneDepth = m_device->GetSceneDepth();

		// バックバッファをレンダーターゲットへ遷移
		m_context->TransitionResource(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(sceneTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(postProcessTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_context->TransitionResource(sceneDepth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
									  D3D12_RESOURCE_STATE_DEPTH_WRITE);

		D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
		D3D12_RECT scissorRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissorRect);

		RecordScenePass(runtimeScene);
		RecordOceanFogPass();
		RecordRetroFilmPass();

		// Present 処理
		m_context->TransitionResource(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

	void Renderer::RecordScenePass(const Scene& scene) {
		auto cmd = m_context->GetCommandList();
		// 画面クリア
		auto rtv = m_device->GetSceneRtvHandle();
		const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		auto dsv = m_device->GetDsvHandle();
		cmd->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		const FrameContext frame{cmd, m_device.get(), m_pipelines, m_constantBuffers, m_context->GetFrameIndex()};
		m_skyPass.Record(frame, m_assets);
		m_oceanPass.Record(frame, m_assets, m_oceanGeometry);
		m_modelPass.Record(frame, scene);
	}

	void Renderer::RecordOceanFogPass() {
		auto cmd = m_context->GetCommandList();
		m_context->TransitionResource(m_device->GetSceneRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
									  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_context->TransitionResource(m_device->GetSceneDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
									  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		auto rtv = m_device->GetPostProcessRtvHandle();
		const float postProcessClear[] = {0.1f, 0.1f, 0.1f, 1.0f};
		cmd->ClearRenderTargetView(rtv, postProcessClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		m_oceanFogPass.Record({cmd, m_device.get(), m_pipelines, m_constantBuffers, m_context->GetFrameIndex()});
		m_context->TransitionResource(m_device->GetPostProcessRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
									  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	void Renderer::RecordRetroFilmPass() {
		auto cmd = m_context->GetCommandList();
		auto rtv = m_device->GetCurrentRtvHandle();
		const float backBufferClear[] = {0.0f, 0.0f, 0.0f, 1.0f};
		cmd->ClearRenderTargetView(rtv, backBufferClear, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		m_retroFilmPass.Record({cmd, m_device.get(), m_pipelines, m_constantBuffers, m_context->GetFrameIndex()});
	}

	void Renderer::Resize(UINT width, UINT height) {
		if (width == 0 || height == 0 || !m_context || !m_device) {
			return;
		}

		m_context->WaitForGpu();
		m_device->Resize(width, height);
		m_context->SynchronizeFrameIndex(*m_device);
	}
} // namespace Engine