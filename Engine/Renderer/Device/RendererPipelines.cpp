#include "RendererPipelines.h"

namespace Engine {
	void RendererPipelines::Initialize(ID3D12Device* device) {
		m_basic = std::make_unique<GraphicsPipeline>();
		m_basic->Initialize(device);

		m_ocean = std::make_unique<GraphicsPipeline>();
		m_ocean->InitializeWithShaders(device, L"OceanVS.cso", L"OceanPS.cso", 3, false, true);

		m_oceanFog = std::make_unique<GraphicsPipeline>();
		m_oceanFog->InitializeWithShaders(device, L"OceanFogVS.cso", L"OceanFogPS.cso", 2, false);

		m_retroFilm = std::make_unique<GraphicsPipeline>();
		m_retroFilm->InitializeWithShaders(device, L"RetroFilmVS.cso", L"RetroFilmPS.cso", 1, false);

		m_sky = std::make_unique<GraphicsPipeline>();
		m_sky->InitializeWithShaders(device, L"SkyVS.cso", L"SkyPS.cso", 1);

		m_model = std::make_unique<GraphicsPipeline>();
		m_model->InitializeWithShaders(device, L"ModelVS.cso", L"ModelPS.cso", 4, false, true, true);
	}
}
