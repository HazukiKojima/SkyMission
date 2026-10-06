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
			UINT textureSrvIndex = 0;
			UINT indexCount = 0;
		};

		std::unique_ptr<Texture> m_fallbackTexture;
		UINT m_fallbackTextureSrvIndex = 0;
		std::vector<Mesh> m_meshes;
	};
}
