#pragma once
#include "RendererConstantBuffers.h"
#include "../../Scene/Scene.h"
#include "../../Scene/SceneLoader.h"

namespace Engine {
	class RendererFrameUpdater {
	  public:
		void Configure(const OceanSettings& settings);
		void Update(const Scene& scene, float elapsedTime, const RetroFilmParameters& filmParameters,
					RendererConstantBuffers& constantBuffers, ID3D12Device* device, CommandContext* context);

		float GetOceanSize() const {
			return m_oceanSettings.size;
		}
		const OceanSettings& GetOceanSettings() const {
			return m_oceanSettings;
		}

	  private:
		OceanSettings m_oceanSettings{};
	};
} // namespace Engine
