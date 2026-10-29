#pragma once
#include "../Pipeline/GraphicsPipeline.h"

namespace Engine {
	class RendererPipelines {
	  public:
		void Initialize(ID3D12Device* device);

		GraphicsPipeline* GetOcean() const {
			return m_ocean.get();
		}
		GraphicsPipeline* GetSky() const {
			return m_sky.get();
		}
		GraphicsPipeline* GetOceanFog() const {
			return m_oceanFog.get();
		}
		GraphicsPipeline* GetRetroFilm() const {
			return m_retroFilm.get();
		}
		GraphicsPipeline* GetModel() const {
			return m_model.get();
		}

	  private:
		std::unique_ptr<GraphicsPipeline> m_ocean;
		std::unique_ptr<GraphicsPipeline> m_sky;
		std::unique_ptr<GraphicsPipeline> m_oceanFog;
		std::unique_ptr<GraphicsPipeline> m_retroFilm;
		std::unique_ptr<GraphicsPipeline> m_model;
	};
} // namespace Engine
