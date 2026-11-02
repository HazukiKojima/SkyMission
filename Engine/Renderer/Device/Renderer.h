#pragma once
#include "../../EngineCommon.h"
#include "../RetroFilmParameters.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include "RendererConstantBuffers.h"
#include "OceanGeometry.h"
#include "RendererPipelines.h"
#include "RendererAssets.h"
#include "RendererFrameUpdater.h"
#include "RendererPasses.h"
#include "GpuFrameProfiler.h"
#include "Engine/Renderer/Hud/HudRenderer.h"

namespace Engine {
	class Scene;
	struct RendererDefinition;

	class Renderer {
	  public:
		void SetVerticalSync(bool enabled) {
			m_device->SetVerticalSync(enabled);
		}
		bool IsVerticalSyncEnabled() const {
			return m_device->IsVerticalSyncEnabled();
		}
		const GpuFrameTimings& GetGpuTimings() const {
			return m_gpuProfiler.Latest();
		}
		const RenderStatistics& GetRenderStatistics() const {
			return m_statistics;
		}
		Renderer() = default;
		~Renderer() noexcept;

		void Initialize(HWND hwnd, UINT width, UINT height, const RendererDefinition& assets);
		void InitializeConstantBuffers(const Scene& scene);
		void ReserveModelCapacity(size_t objectCount);
		size_t GetModelCapacity() const {
			return m_constantBuffers.GetModelCapacity();
		}
		RenderView UpdateFrameConstants(const Scene& scene, float elapsedTime);
		void Render(UINT width, UINT height, const Scene& scene, const HudDrawList* hud = nullptr);
		void Resize(UINT width, UINT height);
		float GetOceanSize() const {
			return m_frameUpdater.GetOceanSize();
		}
		const std::wstring& GetAdapterName() const {
			return m_device->GetAdapterName();
		}
		std::shared_ptr<Model> GetModel(std::string_view id) const {
			return m_assets.GetModel(id);
		}

	  private:
		void RecordScenePass(const Scene& scene);
		void RecordRetroFilmPass();

		std::unique_ptr<RenderDevice> m_device;
		std::unique_ptr<CommandContext> m_context;

		OceanGeometry m_oceanGeometry;
		RendererPipelines m_pipelines;
		RendererAssets m_assets;
		RendererFrameUpdater m_frameUpdater;
		RendererConstantBuffers m_constantBuffers;
		SkyRenderPass m_skyPass;
		OceanRenderPass m_oceanPass;
		ModelRenderPass m_modelPass;
		GpuFrameProfiler m_gpuProfiler;
		RenderStatistics m_statistics;
		RetroFilmRenderPass m_retroFilmPass;
		HudRenderer m_hudRenderer;

		RetroFilmParameters m_retroFilmParameters{};
	};
} // namespace Engine
