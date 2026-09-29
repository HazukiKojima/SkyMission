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
		// 作業用の単色テクスチャを作成する（ロード失敗時のフォールバック）
		bool CreateFromSolidColor(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
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
