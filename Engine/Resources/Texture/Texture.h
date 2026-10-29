#pragma once
#include "../../EngineCommon.h"
#include <DirectXTex.h>
#include "d3dx12.h"
#include <memory>
#include <filesystem>
#include <span>

namespace Engine {
	struct TextureLoadOptions {
		bool useSrgb = true;
		bool generateMipmaps = true;
		bool invertNormalGreen = false;
		DXGI_FORMAT targetFormat = DXGI_FORMAT_UNKNOWN;
	};

	class Texture {
	  public:
		Texture() = default;
		~Texture() = default;

		static std::shared_ptr<Texture> LoadCached(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
			const std::wstring& filePath, const TextureLoadOptions& options = {});
		bool LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,
			const std::wstring& filePath, const TextureLoadOptions& options = {});
		// 作業用の単色テクスチャを作成する（ロード失敗時のフォールバック）
		bool CreateFromSolidColor(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, uint8_t r,
			uint8_t g, uint8_t b, uint8_t a = 255);
		void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const;
		void ReleaseUploadResources();
		void CreateFromRgba(ID3D12Device* device, ID3D12GraphicsCommandList* commands,
			UINT width, UINT height, std::span<const uint8_t> pixels);

		ID3D12Resource* GetResource() const {
			return m_texture.Get();
		}
		bool IsLoaded() const {
			return m_texture != nullptr;
		}

	  private:
		bool DecodeAndProcessImage(const std::wstring& filePath, const TextureLoadOptions& options);
		bool TryLoadDiskCache(
			const std::filesystem::path& cachePath, DXGI_FORMAT targetFormat, const std::wstring& source);
		bool DecodeSource(const std::wstring& filePath, bool useSrgb);
		bool DecompressForProcessing(const TextureLoadOptions& options);
		void CorrectNormalGreen(bool invertNormalGreen);
		bool GenerateMipChain(bool generateMipmaps);
		bool ConvertFormat(DXGI_FORMAT targetFormat, const std::wstring& filePath);
		void TrySaveDiskCache(const std::filesystem::path& path) const;

		void UploadImage(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);

		ComPtr<ID3D12Resource> m_texture;
		ComPtr<ID3D12Resource> m_uploadHeap;
		DirectX::TexMetadata m_meta;
		DirectX::ScratchImage m_image;
	};
} // namespace Engine
