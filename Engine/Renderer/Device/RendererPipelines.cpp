#include "RendererPipelines.h"
#include "RenderDevice.h"

namespace Engine {
	void RendererPipelines::Initialize(ID3D12Device* device) {
		m_ocean = std::make_unique<GraphicsPipeline>();
		m_ocean->Initialize(
			device, {.vs = L"OceanVS.cso", .ps = L"OceanPS.cso", .srvCount = 3, .depth = true});

		m_retroFilm = std::make_unique<GraphicsPipeline>();
		m_retroFilm->Initialize(device,
			{.vs = L"RetroFilmVS.cso",
				.ps = L"RetroFilmPS.cso",
				.srvCount = 3,
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.rootLayout = RootSignatureLayout::PostProcess});

		m_sky = std::make_unique<GraphicsPipeline>();
		m_clouds = std::make_unique<GraphicsPipeline>();
		m_clouds->Initialize(device,
			{.vs = L"RetroFilmVS.cso",
				.ps = L"CloudPS.cso",
				.srvCount = 2,
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.renderTargetFormat = RenderDevice::CloudFormat});
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
