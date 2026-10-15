#include "Texture.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace Engine {
	namespace {
		const char* GetFormatName(DXGI_FORMAT format) {
		switch (format) {
		case DXGI_FORMAT_R8_UNORM: return "DXGI_FORMAT_R8_UNORM";
		case DXGI_FORMAT_R8G8_UNORM: return "DXGI_FORMAT_R8G8_UNORM";
		case DXGI_FORMAT_R8G8_SNORM: return "DXGI_FORMAT_R8G8_SNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM: return "DXGI_FORMAT_R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R16G16_UNORM: return "DXGI_FORMAT_R16G16_UNORM";
		case DXGI_FORMAT_R16G16_SNORM: return "DXGI_FORMAT_R16G16_SNORM";
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return "DXGI_FORMAT_R16G16B16A16_FLOAT";
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return "DXGI_FORMAT_R32G32B32A32_FLOAT";
		case DXGI_FORMAT_B8G8R8A8_UNORM: return "DXGI_FORMAT_B8G8R8A8_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB";
		case DXGI_FORMAT_BC1_UNORM: return "DXGI_FORMAT_BC1_UNORM";
		case DXGI_FORMAT_BC1_UNORM_SRGB: return "DXGI_FORMAT_BC1_UNORM_SRGB";
		case DXGI_FORMAT_BC2_UNORM: return "DXGI_FORMAT_BC2_UNORM";
		case DXGI_FORMAT_BC2_UNORM_SRGB: return "DXGI_FORMAT_BC2_UNORM_SRGB";
		case DXGI_FORMAT_BC3_UNORM: return "DXGI_FORMAT_BC3_UNORM";
		case DXGI_FORMAT_BC3_UNORM_SRGB: return "DXGI_FORMAT_BC3_UNORM_SRGB";
		case DXGI_FORMAT_BC4_UNORM: return "DXGI_FORMAT_BC4_UNORM";
		case DXGI_FORMAT_BC5_UNORM: return "DXGI_FORMAT_BC5_UNORM";
		case DXGI_FORMAT_BC5_SNORM: return "DXGI_FORMAT_BC5_SNORM";
		case DXGI_FORMAT_BC6H_UF16: return "DXGI_FORMAT_BC6H_UF16";
		case DXGI_FORMAT_BC7_UNORM: return "DXGI_FORMAT_BC7_UNORM";
		case DXGI_FORMAT_BC7_UNORM_SRGB: return "DXGI_FORMAT_BC7_UNORM_SRGB";
		default: return nullptr;
		}
		}

		bool IsBlockCompressedFormat(DXGI_FORMAT format) {
			return format >= DXGI_FORMAT_BC1_TYPELESS && format <= DXGI_FORMAT_BC7_UNORM_SRGB;
		}
	}

	std::shared_ptr<Texture> Texture::LoadCached(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb, bool generateMipmaps, bool invertNormalGreen, DXGI_FORMAT targetFormat) {
		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::absolute(filePath, error);
		if (error) {
			normalizedPath = filePath;
		}
		std::wstring cacheKey = normalizedPath.lexically_normal().wstring();
		std::transform(cacheKey.begin(), cacheKey.end(), cacheKey.begin(), towlower);
		cacheKey += useSrgb ? L"|s1" : L"|s0";
		cacheKey += generateMipmaps ? L"|m1" : L"|m0";
		cacheKey += invertNormalGreen ? L"|n1" : L"|n0";
		cacheKey += L"|f" + std::to_wstring(static_cast<unsigned int>(targetFormat));

		static std::mutex cacheMutex;
		static std::unordered_map<std::wstring, std::weak_ptr<Texture>> cache;
		std::lock_guard<std::mutex> lock(cacheMutex);
		const auto cached = cache.find(cacheKey);
		if (cached != cache.end()) {
			if (std::shared_ptr<Texture> texture = cached->second.lock()) {
				return texture;
			}
		}

		auto texture = std::make_shared<Texture>();
		if (!texture->LoadFromFile(device, cmdList, filePath, useSrgb, generateMipmaps, invertNormalGreen, targetFormat)) {
			return {};
		}
		cache[std::move(cacheKey)] = texture;
		return texture;
	}

	bool Texture::LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb, bool generateMipmaps, bool invertNormalGreen, DXGI_FORMAT targetFormat) {
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

		if (invertNormalGreen && (m_meta.format == DXGI_FORMAT_R8G8B8A8_UNORM || m_meta.format == DXGI_FORMAT_B8G8R8A8_UNORM)) {
			for (size_t imageIndex = 0; imageIndex < m_image.GetImageCount(); ++imageIndex) {
				const DirectX::Image* image = &m_image.GetImages()[imageIndex];
				for (size_t y = 0; y < image->height; ++y) {
					uint8_t* row = image->pixels + y * image->rowPitch;
					for (size_t x = 0; x < image->width; ++x) {
						row[x * 4 + 1] = 255 - row[x * 4 + 1];
					}
				}
			}
		}

		if (targetFormat != DXGI_FORMAT_UNKNOWN && m_meta.format != targetFormat) {
			const DXGI_FORMAT sourceFormat = m_meta.format;
			DirectX::ScratchImage convertedImage;
			if (IsBlockCompressedFormat(targetFormat)) {
				DirectX::TEX_COMPRESS_FLAGS compressionFlags = DirectX::TEX_COMPRESS_PARALLEL;
				if (targetFormat == DXGI_FORMAT_BC7_UNORM || targetFormat == DXGI_FORMAT_BC7_UNORM_SRGB) {
					compressionFlags = static_cast<DirectX::TEX_COMPRESS_FLAGS>(compressionFlags | DirectX::TEX_COMPRESS_BC7_QUICK);
				}
				hr = DirectX::Compress(m_image.GetImages(), m_image.GetImageCount(), m_meta, targetFormat,
					compressionFlags, 0.5f, convertedImage);
			}
			else {
				hr = DirectX::Convert(m_image.GetImages(), m_image.GetImageCount(), m_meta, targetFormat,
					DirectX::TEX_FILTER_DEFAULT, 0.5f, convertedImage);
			}
			if (FAILED(hr)) {
				OutputDebugStringW(L"Texture::LoadFromFile - format conversion/compression failed for: ");
				OutputDebugStringW(filePath.c_str());
				char message[64];
				sprintf_s(message, sizeof(message), " (target DXGI_FORMAT %u, HR: 0x%08X)\n", static_cast<unsigned int>(targetFormat), hr);
				OutputDebugStringA(message);
				return false;
			}
			m_image = std::move(convertedImage);
			m_meta = m_image.GetMetadata();
			const bool reconstructNormalZ = targetFormat == DXGI_FORMAT_R8G8B8A8_UNORM &&
				(sourceFormat == DXGI_FORMAT_R8G8_UNORM || sourceFormat == DXGI_FORMAT_R8G8_SNORM ||
					sourceFormat == DXGI_FORMAT_R16G16_UNORM || sourceFormat == DXGI_FORMAT_R16G16_SNORM ||
					sourceFormat == DXGI_FORMAT_BC5_UNORM || sourceFormat == DXGI_FORMAT_BC5_SNORM);
			if (reconstructNormalZ) {
				for (size_t imageIndex = 0; imageIndex < m_image.GetImageCount(); ++imageIndex) {
					const DirectX::Image* image = &m_image.GetImages()[imageIndex];
					for (size_t y = 0; y < image->height; ++y) {
						uint8_t* row = image->pixels + y * image->rowPitch;
						for (size_t x = 0; x < image->width; ++x) {
							uint8_t* pixel = row + x * 4;
							const float normalX = pixel[0] / 127.5f - 1.0f;
							const float normalY = pixel[1] / 127.5f - 1.0f;
							const float normalZ = std::sqrt((std::max)(0.0f, 1.0f - normalX * normalX - normalY * normalY));
							pixel[2] = static_cast<uint8_t>(std::lround((normalZ * 0.5f + 0.5f) * 255.0f));
						}
					}
				}
			}
		}

		if (generateMipmaps && m_meta.mipLevels == 1 && m_meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
			DirectX::ScratchImage mipmappedImage;
			hr = DirectX::GenerateMipMaps(
				m_image.GetImages(),
				m_image.GetImageCount(),
				m_meta,
				DirectX::TEX_FILTER_DEFAULT,
				0,
				mipmappedImage);
			if (SUCCEEDED(hr)) {
				m_image = std::move(mipmappedImage);
				m_meta = m_image.GetMetadata();
			}
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

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);

		const D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = device->GetResourceAllocationInfo(0, 1, &resDesc);
		OutputDebugStringW(L"Texture loaded: ");
		OutputDebugStringW(filePath.c_str());
		char message[160];
		const char* formatName = GetFormatName(m_meta.format);
		if (formatName) {
			sprintf_s(message, sizeof(message), " (%llu x %llu, %s, GPU allocation: %.2f MiB)\n",
				static_cast<unsigned long long>(m_meta.width),
				static_cast<unsigned long long>(m_meta.height),
				formatName,
				static_cast<double>(allocationInfo.SizeInBytes) / (1024.0 * 1024.0));
		}
		else {
			sprintf_s(message, sizeof(message), " (%llu x %llu, DXGI_FORMAT %u, GPU allocation: %.2f MiB)\n",
			static_cast<unsigned long long>(m_meta.width),
			static_cast<unsigned long long>(m_meta.height),
			static_cast<unsigned int>(m_meta.format),
			static_cast<double>(allocationInfo.SizeInBytes) / (1024.0 * 1024.0));
		}
		OutputDebugStringA(message);

		m_image.Release();
		return true;
	}

	bool Texture::CreateFromSolidColor(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		HRESULT hr = m_image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1);
		if (FAILED(hr)) {
			return false;
		}

		m_meta = m_image.GetMetadata();
		const DirectX::Image* image = m_image.GetImage(0, 0, 0);
		image->pixels[0] = r;
		image->pixels[1] = g;
		image->pixels[2] = b;
		image->pixels[3] = a;

		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(m_meta.format, 1, 1, 1, 1);
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_uploadHeap)));

		D3D12_SUBRESOURCE_DATA subresource = {};
		subresource.pData = image->pixels;
		subresource.RowPitch = image->rowPitch;
		subresource.SlicePitch = image->slicePitch;
		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, 1, &subresource);
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_texture.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);
		m_image.Release();
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

	void Texture::ReleaseUploadResources() {
		m_uploadHeap.Reset();
		m_image.Release();
	}
}
