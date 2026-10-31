#include "RendererAssets.h"
#include "../../Log.h"
#include <cassert>
#include <stdexcept>

namespace Engine {
	namespace {
		std::shared_ptr<Texture> LoadTexture(RenderDevice& device, CommandContext& context,
			const std::filesystem::path& path, const TextureLoadOptions& options, UINT& srvIndex) {
			const auto handle = device.AllocateSrvDescriptor(&srvIndex);
			auto texture =
				Texture::LoadCached(device.GetDevice(), context.GetCommandList(), path.wstring(), options);
			if (texture)
				texture->CreateShaderResourceView(device.GetDevice(), handle);
			else {
				D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
				nullSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				nullSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				nullSrv.Texture2D.MipLevels = 1;
				device.GetDevice()->CreateShaderResourceView(nullptr, &nullSrv, handle);
				const auto utf8 = path.u8string();
				Log("RendererAssets: skipping missing texture {}\n",
					std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size()));
			}
			return texture;
		}

		std::shared_ptr<Model> LoadModel(
			RenderDevice& device, CommandContext& context, const ModelAssetDefinition& definition) {
			auto model = std::make_shared<Model>();
			if (!model->Load(device.GetDevice(), context.GetCommandList(), &device, definition.file.wstring(),
					definition.options)) {
				const auto path = definition.file.u8string();
				throw std::runtime_error("Failed to load required model '" + definition.id +
					"': " + std::string(reinterpret_cast<const char*>(path.data()), path.size()));
			}
			return model;
		}
	} // namespace

	void RendererAssets::InitializeSkySphere(
		ID3D12Device* device, ID3D12GraphicsCommandList* commands, float radius) {
		m_skySphere = std::make_unique<SkySphere>();
		m_skySphere->Initialize(device, commands, radius, 64, 32);
	}

	void RendererAssets::Load(
		RenderDevice* device, CommandContext* context, const RendererDefinition& assets) {
		// Oceanのt0〜t2を連続させるため、途中で別のディスクリプタを割り当てない
		m_oceanTexture = LoadTexture(
			*device, *context, assets.environment.oceanTexture, {.useSrgb = true}, m_oceanTextureSrvIndex);
		m_oceanNormalTexture = LoadTexture(*device, *context, assets.environment.oceanNormalTexture,
			{.useSrgb = false}, m_oceanNormalTextureSrvIndex);
		m_oceanNormalTexture2 = LoadTexture(*device, *context, assets.environment.oceanNormalTexture2,
			{.useSrgb = false}, m_oceanNormalTexture2SrvIndex);
		assert(m_oceanNormalTextureSrvIndex == m_oceanTextureSrvIndex + 1);
		assert(m_oceanNormalTexture2SrvIndex == m_oceanTextureSrvIndex + 2);
		m_skyTexture =
			LoadTexture(*device, *context, assets.environment.skyTexture, {}, m_skyTextureSrvIndex);
		for (const auto& asset : assets.models) {
			if (asset.id.empty() || m_models.contains(asset.id))
				throw std::invalid_argument("Model asset IDs must be unique and nonempty.");
			m_models.emplace(asset.id, LoadModel(*device, *context, asset));
		}
	}

	void RendererAssets::ReleaseUploadResources() {
		if (m_skySphere)
			m_skySphere->ReleaseUploadResources();
		for (const auto& texture :
			{m_oceanTexture, m_oceanNormalTexture, m_oceanNormalTexture2, m_skyTexture})
			if (texture)
				texture->ReleaseUploadResources();
		for (const auto& [id, model] : m_models)
			if (model)
				model->ReleaseUploadResources();
	}
} // namespace Engine
