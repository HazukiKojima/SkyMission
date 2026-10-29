#include "RendererPipelines.h"

namespace Engine {
	void RendererPipelines::Initialize(ID3D12Device* device) {
		m_ocean = std::make_unique<GraphicsPipeline>();
		m_ocean->Initialize(
			device, {.vs = L"OceanVS.cso", .ps = L"OceanPS.cso", .srvCount = 3, .depth = true});

		m_oceanFog = std::make_unique<GraphicsPipeline>();
		m_oceanFog->Initialize(device,
			{.vs = L"OceanFogVS.cso",
				.ps = L"OceanFogPS.cso",
				.srvCount = 2,
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP});

		m_retroFilm = std::make_unique<GraphicsPipeline>();
		m_retroFilm->Initialize(device,
			{.vs = L"RetroFilmVS.cso",
				.ps = L"RetroFilmPS.cso",
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP});

		m_sky = std::make_unique<GraphicsPipeline>();
		m_sky->Initialize(device, {.vs = L"SkyVS.cso", .ps = L"SkyPS.cso"});

		m_model = std::make_unique<GraphicsPipeline>();
		m_model->Initialize(device,
			{.vs = L"ModelVS.cso",
				.ps = L"ModelPS.cso",
				.srvCount = 4,
				.depth = true,
				.vertexNormalTangent = true,
				.cullMode = D3D12_CULL_MODE_BACK,
				.rootLayout = RootSignatureLayout::NodeRotation});
	}
} // namespace Engine
