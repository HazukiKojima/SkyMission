#include "Texture.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"
#include <algorithm>

namespace Engine {

	bool Texture::LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb) {
		// 拡張子でHDR/WICを切り替える
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
			// ロード失敗をデバッグ出力
			std::wstring ext = filePath.substr(filePath.find_last_of(L"."));
			char extBuffer[32];
			size_t converted = 0;
			wcstombs_s(&converted, extBuffer, sizeof(extBuffer), ext.c_str(), _TRUNCATE);

			char message[256];
			sprintf_s(message, sizeof(message), "Texture::LoadFromFile - loader failed for: %s (HR: 0x%08X)\n", extBuffer, hr);
			OutputDebugStringA(message);

			return false;
		}

		// GPU用テクスチャリソースを作成
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_meta.format, static_cast<UINT>(m_meta.width), static_cast<UINT>(m_meta.height), static_cast<UINT16>(m_meta.arraySize), static_cast<UINT16>(m_meta.mipLevels));

		// デフォルトヒープにテクスチャを確保
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture)));

		// アップロード用バッファを作成
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

		// サブリソースデータを準備
		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		subresources.resize(m_meta.mipLevels * m_meta.arraySize);

		const DirectX::Image* img = m_image.GetImages();
		for (size_t i = 0; i < subresources.size(); ++i) {
			subresources[i].pData = img[i].pixels;
			subresources[i].RowPitch = img[i].rowPitch;
			subresources[i].SlicePitch = img[i].slicePitch;
		}

		// アップロードコマンドを発行
		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

		// シェーダリソースとして使用できる状態へ遷移
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);

		return true;
	}

	void Texture::CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const {
		if (!m_texture) {
			OutputDebugStringA("Texture::CreateShaderResourceView - texture resource is null\n");
			return;
		}

		char buf[256];
		sprintf_s(buf, "Texture::CreateShaderResourceView - creating SRV. resource=%p, srvHandle.ptr=0x%016llX, format=%u, mips=%u\n",
			reinterpret_cast<const void*>(m_texture.Get()), (unsigned long long)srvHandle.ptr,
			(unsigned)m_meta.format, (unsigned)m_meta.mipLevels);
		OutputDebugStringA(buf);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = m_meta.format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(m_meta.mipLevels);

		device->CreateShaderResourceView(m_texture.Get(), &srvDesc, srvHandle);

		sprintf_s(buf, "Texture::CreateShaderResourceView - SRV created. srvHandle.ptr=0x%016llX\n", (unsigned long long)srvHandle.ptr);
		OutputDebugStringA(buf);
	}

	bool Texture::CreateFromSolidColor(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		// 1x1 RGBA8 テクスチャを生成して GPU にアップロードする
		DirectX::ScratchImage img;
		DirectX::Image image = {};
		image.width = 1;
		image.height = 1;
		image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint8_t* pixels = new uint8_t[4];
		pixels[0] = r; pixels[1] = g; pixels[2] = b; pixels[3] = a;
		image.pixels = pixels;
		image.rowPitch = 4;
		image.slicePitch = 4;

		m_meta.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		m_meta.width = 1;
		m_meta.height = 1;
		m_meta.mipLevels = 1;
		m_meta.arraySize = 1;

		// テクスチャリソースを作成
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_meta.format, 1, 1, 1, 1);
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture)));

		char buf[256];
		sprintf_s(buf, "Texture::CreateFromSolidColor - created default resource=%p\n", reinterpret_cast<void*>(m_texture.Get()));
		OutputDebugStringA(buf);

		// アップロードバッファ
		const UINT64 uploadSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_uploadHeap)));

		D3D12_SUBRESOURCE_DATA sub = {};
		sub.pData = pixels;
		sub.RowPitch = 4;
		sub.SlicePitch = 4;

		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, 1, &sub);
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);

		sprintf_s(buf, "Texture::CreateFromSolidColor - UpdateSubresources recorded, resource=%p\n", reinterpret_cast<void*>(m_texture.Get()));
		OutputDebugStringA(buf);

		delete[] pixels;
		return true;
	}

}
