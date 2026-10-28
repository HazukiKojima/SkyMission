#include "Texture.h"
#include "../../Log.h"
#include <DirectXTex.h>
#include "d3dx12.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace Engine {
	namespace {
		bool IsBlockCompressedFormat(DXGI_FORMAT format) {
			return DirectX::IsCompressed(format);
		}

		std::filesystem::path GetDiskCachePath(const std::wstring& filePath, bool useSrgb, bool generateMipmaps,
											   bool invertNormalGreen, DXGI_FORMAT targetFormat) {
			std::error_code error;
			const auto fileSize = std::filesystem::file_size(filePath, error);
			if (error) {
				return {};
			}
			const auto lastWriteTime = std::filesystem::last_write_time(filePath, error);
			if (error) {
				return {};
			}

			std::filesystem::path normalizedPath = std::filesystem::weakly_canonical(filePath, error);
			if (error) {
				error.clear();
				normalizedPath = std::filesystem::absolute(filePath, error);
				if (error) {
					normalizedPath = filePath;
				}
			}

			uint64_t hash = 14695981039346656037ULL;
			auto mix = [&hash](uint64_t value) {
				for (int byteIndex = 0; byteIndex < 8; ++byteIndex) {
					hash ^= value & 0xff;
					hash *= 1099511628211ULL;
					value >>= 8;
				}
			};
			for (wchar_t character : normalizedPath.wstring()) {
				mix(static_cast<uint64_t>(towlower(character)));
			}
			mix(2); // Processing version: mip generation precedes compression.
			mix(fileSize);
			mix(static_cast<uint64_t>(lastWriteTime.time_since_epoch().count()));
			mix(static_cast<uint64_t>(targetFormat));
			mix(useSrgb ? 1 : 0);
			mix(generateMipmaps ? 1 : 0);
			mix(invertNormalGreen ? 1 : 0);

			error.clear();
			const std::filesystem::path tempDirectory = std::filesystem::temp_directory_path(error);
			if (error) {
				return {};
			}
			return tempDirectory / L"SkyMissionTextureCache" / (std::to_wstring(hash) + L".dds");
		}
	} // namespace

	std::shared_ptr<Texture> Texture::LoadCached(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
												 const std::wstring& filePath, const TextureLoadOptions& options) {
		const auto& [useSrgb, generateMipmaps, invertNormalGreen, targetFormat] = options;
		std::error_code error;
		std::filesystem::path normalizedPath = std::filesystem::absolute(filePath, error);
		if (error) {
			normalizedPath = filePath;
		}
		std::wstring cacheKey = normalizedPath.lexically_normal().wstring();
		std::transform(cacheKey.begin(), cacheKey.end(), cacheKey.begin(), towlower);
		cacheKey += L"|device" + std::to_wstring(reinterpret_cast<uintptr_t>(device));
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
		if (!texture->LoadFromFile(device, cmdList, filePath, options)) {
			return {};
		}
		cache[std::move(cacheKey)] = texture;
		return texture;
	}

	bool Texture::LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath,
							   const TextureLoadOptions& options) {
		if (!DecodeAndProcessImage(filePath, options))
			return false;
		UploadImage(device, cmdList);
		m_image.Release();
		return true;
	}

	bool Texture::DecodeAndProcessImage(const std::wstring& filePath, const TextureLoadOptions& options) {
		const auto& [useSrgb, generateMipmaps, invertNormalGreen, targetFormat] = options;
		std::wstring ext = std::filesystem::path(filePath).extension().wstring();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

		HRESULT hr = E_FAIL;
		std::filesystem::path diskCachePath;
		bool diskCacheHit = false;
		if (IsBlockCompressedFormat(targetFormat)) {
			diskCachePath = GetDiskCachePath(filePath, useSrgb, generateMipmaps, invertNormalGreen, targetFormat);
			if (!diskCachePath.empty()) {
				std::error_code error;
				if (std::filesystem::exists(diskCachePath, error) && !error) {
					hr = DirectX::LoadFromDDSFile(diskCachePath.c_str(), DirectX::DDS_FLAGS_NONE, &m_meta, m_image);
					diskCacheHit = SUCCEEDED(hr) && m_meta.format == targetFormat;
					if (diskCacheHit) {
						Log(L"Texture cache hit: ");
						Log(L"{}", filePath);
						Log(L"\n");
					} else {
						m_image.Release();
					}
				}
			}
		}

		if (!diskCacheHit) {
			if (ext == L".dds") {
				hr = DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS_NONE, &m_meta, m_image);
			} else if (ext == L".hdr") {
				hr = DirectX::LoadFromHDRFile(filePath.c_str(), &m_meta, m_image);
			} else {
				hr = DirectX::LoadFromWICFile(
					filePath.c_str(), useSrgb ? DirectX::WIC_FLAGS_DEFAULT_SRGB : DirectX::WIC_FLAGS_IGNORE_SRGB,
					&m_meta, m_image);
			}

			if (FAILED(hr)) {
				Log(L"{}", filePath);
				Log("Texture image loader failed (HR: 0x{:08X})\n", static_cast<unsigned long>(hr));
				return false;
			}
		}

		if (!diskCacheHit && DirectX::IsCompressed(m_meta.format) &&
			(invertNormalGreen || (generateMipmaps && m_meta.mipLevels == 1) || m_meta.format != targetFormat)) {
			DirectX::ScratchImage decompressed;
			hr = DirectX::Decompress(m_image.GetImages(), m_image.GetImageCount(), m_meta, DXGI_FORMAT_R8G8B8A8_UNORM,
									 decompressed);
			if (FAILED(hr)) {
				Log("Texture decompression failed (HR: 0x{:08X})\n", static_cast<unsigned long>(hr));
				return false;
			}
			m_image = std::move(decompressed);
			m_meta = m_image.GetMetadata();
		}

		if (!diskCacheHit && invertNormalGreen &&
			(m_meta.format == DXGI_FORMAT_R8G8B8A8_UNORM || m_meta.format == DXGI_FORMAT_B8G8R8A8_UNORM)) {
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

		if (!diskCacheHit && generateMipmaps && m_meta.mipLevels == 1 &&
			m_meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) {
			DirectX::ScratchImage mipmappedImage;
			hr = DirectX::GenerateMipMaps(m_image.GetImages(), m_image.GetImageCount(), m_meta,
										  DirectX::TEX_FILTER_DEFAULT, 0, mipmappedImage);
			if (FAILED(hr)) {
				Log("Texture mip generation failed (HR: 0x{:08X})\n", static_cast<unsigned long>(hr));
				return false;
			}
			m_image = std::move(mipmappedImage);
			m_meta = m_image.GetMetadata();
		}

		if (targetFormat != DXGI_FORMAT_UNKNOWN && m_meta.format != targetFormat) {
			const DXGI_FORMAT sourceFormat = m_meta.format;
			DirectX::ScratchImage convertedImage;
			if (IsBlockCompressedFormat(targetFormat)) {
				DirectX::TEX_COMPRESS_FLAGS compressionFlags = DirectX::TEX_COMPRESS_PARALLEL;
				if (targetFormat == DXGI_FORMAT_BC7_UNORM || targetFormat == DXGI_FORMAT_BC7_UNORM_SRGB) {
					compressionFlags =
						static_cast<DirectX::TEX_COMPRESS_FLAGS>(compressionFlags | DirectX::TEX_COMPRESS_BC7_QUICK);
				}
				const auto compressionStart = std::chrono::steady_clock::now();
				hr = DirectX::Compress(m_image.GetImages(), m_image.GetImageCount(), m_meta, targetFormat,
									   compressionFlags, 0.5f, convertedImage);
				const auto compressionMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
														 std::chrono::steady_clock::now() - compressionStart)
														 .count();
				Log("Texture compression: {} ms\n", compressionMilliseconds);
			} else {
				hr = DirectX::Convert(m_image.GetImages(), m_image.GetImageCount(), m_meta, targetFormat,
									  DirectX::TEX_FILTER_DEFAULT, 0.5f, convertedImage);
			}
			if (FAILED(hr)) {
				Log(L"Texture::LoadFromFile - format conversion/compression failed for: ");
				Log(L"{}", filePath);
				Log(" (target DXGI_FORMAT {}, HR: 0x{:08X})\n", static_cast<unsigned int>(targetFormat),
					static_cast<unsigned long>(hr));
				return false;
			}
			m_image = std::move(convertedImage);
			m_meta = m_image.GetMetadata();
			const bool reconstructNormalZ =
				targetFormat == DXGI_FORMAT_R8G8B8A8_UNORM &&
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
							const float normalZ =
								std::sqrt(std::max(0.0f, 1.0f - normalX * normalX - normalY * normalY));
							pixel[2] = static_cast<uint8_t>(std::lround((normalZ * 0.5f + 0.5f) * 255.0f));
						}
					}
				}
			}
		}

		if (!diskCacheHit && !diskCachePath.empty()) {
			std::error_code error;
			std::filesystem::create_directories(diskCachePath.parent_path(), error);
			if (!error) {
				const HRESULT saveResult = DirectX::SaveToDDSFile(m_image.GetImages(), m_image.GetImageCount(), m_meta,
																  DirectX::DDS_FLAGS_NONE, diskCachePath.c_str());
				if (SUCCEEDED(saveResult)) {
					Log(L"Texture cache saved: ");
					Log(L"{}", diskCachePath.wstring());
					Log(L"\n");
				}
			}
		}

		return true;
	}

	void Texture::UploadImage(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList) {
		// GPU用テクスチャリソースを作成
		auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
			m_meta.format, static_cast<UINT>(m_meta.width), static_cast<UINT>(m_meta.height),
			static_cast<UINT16>(m_meta.arraySize), static_cast<UINT16>(m_meta.mipLevels));

		// デフォルトヒープにテクスチャを確保
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
													  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
													  IID_PPV_ARGS(&m_texture)));

		// 転送用アップロードバッファを作成
		const UINT64 uploadBufferSize =
			GetRequiredIntermediateSize(m_texture.Get(), 0, static_cast<UINT>(m_meta.mipLevels * m_meta.arraySize));

		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
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
		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, static_cast<UINT>(subresources.size()),
						   subresources.data());

		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);

		const D3D12_RESOURCE_ALLOCATION_INFO allocationInfo = device->GetResourceAllocationInfo(0, 1, &resDesc);
		Log("Texture uploaded: {} x {}, format {}, {} mips, GPU allocation {:.2f} MiB\n", m_meta.width, m_meta.height,
			static_cast<unsigned int>(m_meta.format), m_meta.mipLevels,
			static_cast<double>(allocationInfo.SizeInBytes) / (1024.0 * 1024.0));
	}

	bool Texture::CreateFromSolidColor(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint8_t r, uint8_t g,
									   uint8_t b, uint8_t a) {
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
													  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
													  IID_PPV_ARGS(&m_texture)));

		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
		CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
		ThrowIfFailed(device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
													  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
													  IID_PPV_ARGS(&m_uploadHeap)));

		D3D12_SUBRESOURCE_DATA subresource = {};
		subresource.pData = image->pixels;
		subresource.RowPitch = image->rowPitch;
		subresource.SlicePitch = image->slicePitch;
		UpdateSubresources(cmdList, m_texture.Get(), m_uploadHeap.Get(), 0, 0, 1, &subresource);
		D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			m_texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		cmdList->ResourceBarrier(1, &barrier);
		m_image.Release();
		return true;
	}

	void Texture::CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const {
		if (!m_texture) {
			Log("Texture::CreateShaderResourceView - texture resource is null\n");
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
} // namespace Engine
