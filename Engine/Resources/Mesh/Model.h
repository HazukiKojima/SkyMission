#pragma once
#include "../../EngineCommon.h"
#include "../Texture/Texture.h"
#include "ModelLoadOptions.h"
#include <memory>
#include <filesystem>

struct aiMaterial;
#include <string>
#include <vector>

namespace Engine {
	class RenderDevice;

	class Model {
	  public:
		bool Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
			const std::wstring& filePath, const ModelLoadOptions& options = {});
		void Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
			float nodeRotationAngle = 0.0f) const;
		void ReleaseUploadResources();
		bool IsLoaded() const {
			return !m_meshes.empty();
		}

	  private:
		struct Mesh {
			D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
			D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
			std::shared_ptr<Texture> diffuseTexture;
			std::shared_ptr<Texture> metallicTexture;
			std::shared_ptr<Texture> roughnessTexture;
			std::shared_ptr<Texture> normalTexture;
			UINT textureSrvIndex = 0;
			UINT indexCount = 0;
			bool isRotatingNode = false;
			DirectX::XMFLOAT3 nodeRotationPivot = {};
			DirectX::XMFLOAT3 nodeRotationAxis = {0.0f, 0.0f, 1.0f};
		};

		struct TextureDescriptorSet {
			const Texture* diffuse;
			const Texture* metallic;
			const Texture* roughness;
			const Texture* normal;
			UINT srvIndex;
		};
		void AssignMaterialDescriptors(Mesh& mesh, ID3D12Device* device, RenderDevice* renderDevice,
			std::vector<TextureDescriptorSet>& sets);
		void UploadGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* commands, const void* vertices,
			size_t vertexBytes, const void* indices, size_t indexBytes);

		void InitializeFallbackTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commands);
		void LoadMaterialTextures(Mesh& mesh, const aiMaterial* material,
			const std::filesystem::path& directory, const ModelLoadOptions& options, ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList);
		ComPtr<ID3D12Resource> m_vertexBuffer, m_indexBuffer;
		ComPtr<ID3D12Resource> m_vertexUpload, m_indexUpload;
		std::unique_ptr<Texture> m_fallbackTexture;
		std::unique_ptr<Texture> m_fallbackMetallicTexture;
		std::unique_ptr<Texture> m_fallbackRoughnessTexture;
		std::unique_ptr<Texture> m_fallbackNormalTexture;
		std::vector<Mesh> m_meshes;
	};
} // namespace Engine
