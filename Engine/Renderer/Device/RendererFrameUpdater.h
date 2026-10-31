#pragma once
#include "RendererConstantBuffers.h"
#include "../../Scene/Scene.h"
#include "../RendererDefinition.h"
#include "Engine/Renderer/Hud/HudDrawList.h"

namespace Engine {
	class RendererFrameUpdater {
	  public:
		void Configure(const OceanSettings& settings);
		RenderView GetRenderView() const {
			return m_renderView;
		}
		void Update(const Scene& scene, float elapsedTime, const RetroFilmParameters& filmParameters,
			RendererConstantBuffers& constantBuffers, ID3D12Device* device, CommandContext* context);

		float GetOceanSize() const {
			return m_oceanSettings.size;
		}
		const OceanSettings& GetOceanSettings() const {
			return m_oceanSettings;
		}

	  private:
		struct FrameViewData;
		FrameViewData BuildFrameView(const Scene& scene, float elapsedTime) const;
		void UpdateOceanConstants(
			const FrameViewData&, const SceneEnvironment&, OceanConstantBufferData*) const;
		void UpdateFogConstants(
			const FrameViewData&, const SceneEnvironment&, OceanFogConstantBufferData*) const;
		void UpdateFilmConstants(
			const FrameViewData&, const RetroFilmParameters&, FilmConstantBufferData*) const;
		void UpdateModelConstants(const FrameViewData&, const SceneEnvironment&, const Scene&,
			RendererConstantBuffers&, UINT) const;

		OceanSettings m_oceanSettings{};
		RenderView m_renderView;
	};
} // namespace Engine
