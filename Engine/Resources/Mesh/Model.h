#pragma once
#include "../../EngineCommon.h"
#include "../Texture/Texture.h"
#include "ModelLoadOptions.h"
#include <memory>
#include <filesystem>

#include <string>
#include <vector>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <array>
#include <algorithm>
#include "Engine/Scene/Bounds3.h"

struct aiMaterial;
struct aiScene;
namespace Engine::ModelProcessing {
	struct MeshGeometry;
}

namespace Engine {
	class RenderDevice;

	class Model {
	  public:
		bool Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
			const std::wstring& filePath, const ModelLoadOptions& options = {});
		void Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
			float nodeRotationAngle = 0.0f, unsigned lod = 0) const;
		void ReleaseUploadResources();

		unsigned GetLodCount() const {
			return m_lodCount;
		}
		size_t GetMeshCount(unsigned lod = 0) const {
			return m_lodMeshes[std::min(lod, m_lodCount - 1)].size();
		}
		size_t GetTriangleCount(unsigned lod = 0) const {
			size_t count = 0;
			for (const auto& mesh : m_lodMeshes[std::min(lod, m_lodCount - 1)])
				count += mesh.indexCount / 3;
			return count;
		}
		float GetCullingRadius() const {
			return m_cullingRadius;
		}
		size_t GetSourceMeshCount() const {
			return m_sourceMeshCount;
		}
		// 同名ノードが複数ある場合も曖昧な位置を返さない
		std::optional<DirectX::XMFLOAT3> FindNodePosition(std::string_view name) const;
		size_t CountNodes(std::string_view name) const;
		const std::optional<Bounds3>& GetLocalBounds() const {
			return m_localBounds;
		}
		bool IsLoaded() const {
			return !m_lodMeshes[0].empty();
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
		bool CreateMeshes(const aiScene& scene, std::vector<ModelProcessing::MeshGeometry>& geometries,
			const ModelLoadOptions& options, const std::filesystem::path& directory, ID3D12Device* device,
			ID3D12GraphicsCommandList* commands, RenderDevice* renderDevice);
		void AssignMaterialDescriptors(Mesh& mesh, ID3D12Device* device, RenderDevice* renderDevice,
			std::vector<TextureDescriptorSet>& sets);
		void UploadGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* commands, const void* vertices,
			size_t vertexBytes, const void* indices, size_t indexBytes);

		void InitializeFallbackTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commands);
		void LoadMaterialTextures(Mesh& mesh, const aiMaterial* material,
			const std::filesystem::path& directory, const ModelLoadOptions& options, ID3D12Device* device,
			ID3D12GraphicsCommandList* commandList);
		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_indexBuffer;
		ComPtr<ID3D12Resource> m_vertexUpload;
		ComPtr<ID3D12Resource> m_indexUpload;
		std::unique_ptr<Texture> m_fallbackTexture;
		std::unique_ptr<Texture> m_fallbackMetallicTexture;
		std::unique_ptr<Texture> m_fallbackRoughnessTexture;
		std::unique_ptr<Texture> m_fallbackNormalTexture;
		std::array<std::vector<Mesh>, 3> m_lodMeshes;
		unsigned m_lodCount = 1;
		float m_cullingRadius = 0;
		size_t m_sourceMeshCount = 0;
		std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>> m_nodePositions;
		std::optional<Bounds3> m_localBounds;
	};
} // namespace Engine
