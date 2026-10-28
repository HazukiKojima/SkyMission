#pragma once
#include "../../Resources/Mesh/Model.h"
#include "../../Resources/Mesh/SkySphere.h"
#include "../../Resources/Texture/Texture.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include "../../Scene/SceneLoader.h"

namespace Engine {
	class RendererAssets {
	public:
		void InitializeSkySphere(ID3D12Device* device, float radius);
		void Load(RenderDevice* device, CommandContext* context, const SceneDefinition& assets);

		SkySphere* GetSkySphere() const { return m_skySphere.get(); }
		const std::shared_ptr<Texture>& GetOceanTexture() const { return m_oceanTexture; }
		const std::shared_ptr<Texture>& GetOceanNormalTexture() const { return m_oceanNormalTexture; }
		const std::shared_ptr<Texture>& GetOceanNormalTexture2() const { return m_oceanNormalTexture2; }
		const std::shared_ptr<Texture>& GetSkyTexture() const { return m_skyTexture; }
		UINT GetOceanTextureSrvIndex() const { return m_oceanTextureSrvIndex; }
		UINT GetSkyTextureSrvIndex() const { return m_skyTextureSrvIndex; }
		std::shared_ptr<Model> GetModel() const { return m_model; }
		std::shared_ptr<Model> GetEnemyAircraftModel() const { return m_enemyAircraftModel; }

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
		std::shared_ptr<Model> m_model;
		std::shared_ptr<Model> m_enemyAircraftModel;
	};
}
