#include "RendererPipelines.h"
#include "Engine/Renderer/Device/RenderDevice.h"

namespace Engine {
	void RendererPipelines::Initialize(ID3D12Device* device) {
		constexpr UINT kOceanTextureCount = 3; // 色と2枚の法線マップ
		constexpr UINT kModelTextureCount = 4; // 色、金属度、粗さ、法線
		constexpr UINT kPostProcessTextureCount = RenderDevice::ReservedSrvCount / RenderDevice::FrameCount;
		constexpr UINT kCloudInputTextureCount = RenderDevice::SceneDepthSrvIndex(0) + 1;
		m_ocean = std::make_unique<GraphicsPipeline>();
		m_ocean->Initialize(
			device, {.vs = L"OceanVS.cso", .ps = L"OceanPS.cso", .srvCount = kOceanTextureCount, .depth = true});

		// 画面全体の合成では深度テストを使わず、参照先の端をクランプする
		m_retroFilm = std::make_unique<GraphicsPipeline>();
		m_retroFilm->Initialize(device,
			{.vs = L"RetroFilmVS.cso",
				.ps = L"RetroFilmPS.cso",
				.srvCount = kPostProcessTextureCount,
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.rootLayout = RootSignatureLayout::PostProcess});

		m_sky = std::make_unique<GraphicsPipeline>();
		m_clouds = std::make_unique<GraphicsPipeline>();
		m_clouds->Initialize(device,
			{.vs = L"RetroFilmVS.cso",
				.ps = L"CloudPS.cso",
				.srvCount = kCloudInputTextureCount,
				.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
				.renderTargetFormat = RenderDevice::CloudFormat});
		m_sky->Initialize(device, {.vs = L"SkyVS.cso", .ps = L"SkyPS.cso"});

		// モデル用には法線・接線とノード回転のバインドを追加する
		m_model = std::make_unique<GraphicsPipeline>();
		m_model->Initialize(device,
			{.vs = L"ModelVS.cso",
				.ps = L"ModelPS.cso",
				.srvCount = kModelTextureCount,
				.depth = true,
				.vertexNormalTangent = true,
				.cullMode = D3D12_CULL_MODE_BACK,
				.rootLayout = RootSignatureLayout::NodeRotation});
	}
} // namespace Engine
