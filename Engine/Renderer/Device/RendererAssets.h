#pragma once
#include "../../Resources/Mesh/Model.h"
#include "../../Resources/Mesh/SkySphere.h"
#include "../../Resources/Texture/Texture.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include "../RendererDefinition.h"
#include <unordered_map>
#include <string_view>

namespace Engine {
	class RendererAssets {
	  public:
		void InitializeSkySphere(ID3D12Device* device, ID3D12GraphicsCommandList* commands, float radius);
		// 転送を記録し、実行とGPU完了待機はRendererに任せる
		void Load(RenderDevice* device, CommandContext* context, const RendererDefinition& assets);
		void ReleaseUploadResources();

		SkySphere* GetSkySphere() const {
			return m_skySphere.get();
		}
		const std::shared_ptr<Texture>& GetOceanTexture() const {
			return m_oceanTexture;
		}
		const std::shared_ptr<Texture>& GetOceanNormalTexture() const {
			return m_oceanNormalTexture;
		}
		const std::shared_ptr<Texture>& GetOceanNormalTexture2() const {
			return m_oceanNormalTexture2;
		}
		const std::shared_ptr<Texture>& GetSkyTexture() const {
			return m_skyTexture;
		}
		UINT GetOceanTextureSrvIndex() const {
			return m_oceanTextureSrvIndex;
		}
		UINT GetSkyTextureSrvIndex() const {
			return m_skyTextureSrvIndex;
		}
		std::shared_ptr<Model> GetModel(std::string_view id) const {
			auto found = m_models.find(std::string(id));
			return found == m_models.end() ? nullptr : found->second;
		}

	  private:
		std::unique_ptr<SkySphere> m_skySphere;
		std::shared_ptr<Texture> m_oceanTexture;
		UINT m_oceanTextureSrvIndex = 0;
		std::shared_ptr<Texture> m_oceanNormalTexture;
		UINT m_oceanNormalTextureSrvIndex = 0;
		std::shared_ptr<Texture> m_oceanNormalTexture2;
		UINT m_oceanNormalTexture2SrvIndex = 0;
		std::shared_ptr<Texture> m_skyTexture;
		UINT m_skyTextureSrvIndex = 0;
		std::unordered_map<std::string, std::shared_ptr<Model>> m_models;
	};
} // namespace Engine
