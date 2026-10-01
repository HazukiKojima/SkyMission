#include "Texture.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"
#include <algorithm>

namespace Engine {

	bool Texture::LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb) {
		// DirectXTex を使って画像ファイルを読み込む
		std::wstring ext = filePath.substr(filePath.find_last_of(L'.'));
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

		HRESULT hr;

		if (ext == L".hdr")
		{
			hr = DirectX::LoadFromHDRFile(
				filePath.c_str(),
				&m_meta,
				m_image);
		}
		else
		{
			hr = DirectX::LoadFromWICFile(
				filePath.c_str(),
				useSrgb ? DirectX::WIC_FLAGS_DEFAULT_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB,
				&m_meta,
				m_image);
		}
		
		if (FAILED(hr)) {
			// 画像の読み込みに失敗した
			std::wstring ext = filePath.substr(filePath.find_last_of(L"."));
			char extBuffer[32];
			size_t converted = 0;
			wcstombs_s(&converted, extBuffer, sizeof(extBuffer), ext.c_str(), _TRUNCATE);
			
			char message[256];
			sprintf_s(message, sizeof(message), "Texture::LoadFromFile - WIC loader failed for: %s (HR: 0x%08X)\n", extBuffer, hr);
			OutputDebugStringA(message);
			
			return false;
		}

		// GPU用テクスチャリソースを作成
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_meta.format,
			static_cast<UINT>(m_meta.width),
			static_cast<UINT>(m_meta.height),
			static_cast<UINT16>(m_meta.arraySize),
			static_cast<UINT16>(m_meta.mipLevels));

		// デフォルトヒープにテクスチャを確保
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture)));

		// 転送用アップロードバッファを作成
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, static_cast<UINT>(m_meta.mipLevels * m_meta.arraySize));

		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(
			&uploadHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_uploadHeap)));

		// サブリソース配列を作成してアップロード
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		subresources.resize(m_meta.mipLevels * m_meta.arraySize);

		const DirectX::Image* img = m_image.GetImages();
		for (size_t i = 0; i < subresources.size(); ++i) {
			subresources[i].pData = img[i].pixels;
			subresources[i].RowPitch = img[i].rowPitch;
			subresources[i].SlicePitch = img[i].slicePitch;
		}

		// GPUへデータを転送してピクセルシェーダ用に遷移
		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);

		return true;
	}

	void Texture::CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const {
		if (!m_texture) {
			OutputDebugStringA("Texture::CreateShaderResourceView - texture resource is null\n");
			return;
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_meta.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(m_meta.mipLevels);

		device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);
	}

}
