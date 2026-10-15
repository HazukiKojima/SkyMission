#pragma once
#include "../../EngineCommon.h"
#include "../Texture/Texture.h"
#include <memory>
#include <string>
#include <vector>

namespace Engine {
	class RenderDevice;

	class Model {
	public:
		bool Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice, const std::wstring& filePath);
		void Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice) const;
		bool IsLoaded() const { return !m_meshes.empty(); }

	private:
		struct Mesh {
			ComPtr<ID3D12Resource> vertexBuffer;
			ComPtr<ID3D12Resource> indexBuffer;
			D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
			D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
			std::unique_ptr<Texture> diffuseTexture;
			std::unique_ptr<Texture> metallicTexture;
			std::unique_ptr<Texture> roughnessTexture;
			std::unique_ptr<Texture> normalTexture;
			UINT textureSrvIndex = 0;
			UINT indexCount = 0;
		};

		std::unique_ptr<Texture> m_fallbackTexture;
		std::unique_ptr<Texture> m_fallbackMetallicTexture;
		std::unique_ptr<Texture> m_fallbackRoughnessTexture;
		std::unique_ptr<Texture> m_fallbackNormalTexture;
		std::vector<Mesh> m_meshes;
	};
}
