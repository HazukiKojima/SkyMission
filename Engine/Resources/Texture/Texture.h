#pragma once
#include "../../EngineCommon.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"

namespace Engine {
	class Texture {
	public:
		Texture() = default;
		~Texture() = default;

		bool LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb = true);
		void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const;

		ID3D12Resource* GetResource() const { return m_texture.Get(); }
		bool IsLoaded() const { return m_texture != nullptr; }

	private:
		ComPtr<ID3D12Resource> m_texture;
		ComPtr<ID3D12Resource> m_uploadHeap;
		DirectX::TexMetadata m_meta;
		DirectX::ScratchImage m_image;
	};
}
