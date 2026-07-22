#pragma once
#include "../../EngineCommon.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"

namespace Engine {
	// Texture: DirectXTex を使用してファイルから読み込み、D3D12 リソースと SRV を作成する
	class Texture {
	public:
		Texture() = default;
		~Texture() = default;

		// ファイルから読み込み。device とコマンドリストはアップロードに使用する。
		bool LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath);

		// SRV を作成するためのヘルパー。RenderDevice 側で割当てた CPU ハンドルを渡す。
		void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const;

		ID3D12Resource* GetResource() const { return m_texture.Get(); }

	private:
		ComPtr<ID3D12Resource> m_texture;        // GPU 上のテクスチャリソース
		ComPtr<ID3D12Resource> m_uploadHeap;     // アップロード用ヒープ（ライフタイムを保持するため保存）
		DirectX::TexMetadata m_meta;
		DirectX::ScratchImage m_image;
	};
}
