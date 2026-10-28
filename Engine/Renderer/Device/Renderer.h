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

namespace Engine {
	class Scene;
	struct SceneDefinition;

	class Renderer {
	public:
		Renderer() = default;
		~Renderer();

		void Initialize(HWND hwnd, UINT width, UINT height, const SceneDefinition& assets);
		void InitializeConstantBuffers(const Scene& scene);
		void Update(const Scene& scene, float elapsedTime, float deltaSeconds);
		void Render(UINT width, UINT height, const Scene& scene);
		void Resize(UINT width, UINT height);
		float GetOceanSize() const { return m_frameUpdater.GetOceanSize(); }
		const std::wstring& GetAdapterName() const { return m_device->GetAdapterName(); }
		std::shared_ptr<Engine::Model> GetModel() const { return m_assets.GetModel(); }
		std::shared_ptr<Engine::Model> GetEnemyAircraftModel() const { return m_assets.GetEnemyAircraftModel(); }

	private:
		void RecordScenePass(const Scene& scene);
		void RecordOceanFogPass();
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
		OceanFogRenderPass m_oceanFogPass;
		RetroFilmRenderPass m_retroFilmPass;

		RetroFilmParameters m_retroFilmParameters{};

	};
}