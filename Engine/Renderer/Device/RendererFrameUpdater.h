#pragma once
#include "RendererConstantBuffers.h"
#include "../../Scene/Scene.h"
#include "../../Scene/SceneLoader.h"

namespace Engine {
	class RendererFrameUpdater {
	public:
		void Configure(const OceanSettings& settings);
		void Update(
			const Scene& scene,
			float elapsedTime,
			float deltaSeconds,
			RendererConstantBuffers& constantBuffers,
			ID3D12Device* device,
			CommandContext* context);

		float GetOceanSize() const { return m_oceanSettings.size; }
		const OceanSettings& GetOceanSettings() const { return m_oceanSettings; }
		float GetElapsedTime() const { return m_elapsedTime; }
		float GetPropellerAngle() const { return m_propellerAngle; }

	private:
		OceanSettings m_oceanSettings{};
		float m_elapsedTime = 0.0f;
		float m_propellerAngle = 0.0f;
	};
}
