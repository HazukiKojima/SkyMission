#include "../../Renderer/RootBindings.h"
#include "Model.h"
#include "ModelGeometry.h"
#include "ModelGeometryCache.h"
#include "MaterialTexturePaths.h"
#include "../Buffer/BufferUtilities.h"
#include "../../Log.h"
#include "../../Renderer/Device/RenderDevice.h"
#include <assimp/material.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include "Engine/StartupTiming.h"
namespace Engine {
	using namespace ModelProcessing;
	namespace {
		std::filesystem::path ResolveMaterialTexture(const aiMaterial* material, aiTextureType type,
			const std::filesystem::path& modelDirectory, const ModelLoadOptions& options) {
			aiString texturePath;
			if (material->GetTexture(type, 0, &texturePath) != AI_SUCCESS || texturePath.length == 0 ||
				texturePath.C_Str()[0] == '*') {
				return {};
			}

			const char8_t* pathUtf8 = reinterpret_cast<const char8_t*>(texturePath.C_Str());
			const std::filesystem::path relativePath(std::u8string(pathUtf8, pathUtf8 + texturePath.length));
			const auto resolved =
				MaterialTexturePaths::FindTexturePath(modelDirectory, relativePath, options);
			if (resolved.empty())
				Log(L"Model::Load - missing texture '{}' in '{}'\n", relativePath.wstring(),
					modelDirectory.wstring());
			return resolved;
		}

	} // namespace
	bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
		const std::wstring& filePath, const ModelLoadOptions& options) {
		for (auto& meshes : m_lodMeshes)
			meshes.clear();
		m_lodCount = options.generateLods ? 3u : 1u;
		m_cullingRadius = 0;
		m_nodePositions.clear();
		m_localBounds.reset();
		m_vertexBuffer.Reset();
		m_indexBuffer.Reset();
		m_vertexUpload.Reset();
		m_indexUpload.Reset();
		m_fallbackTexture.reset();
		m_fallbackMetallicTexture.reset();
		m_fallbackRoughnessTexture.reset();
		m_fallbackNormalTexture.reset();

		const std::filesystem::path modelPath(filePath);
		auto prepared = PrepareModelGeometry(modelPath, options);
		m_sourceMeshCount = prepared.levels[0].size();
		if (prepared.levels[0].empty()) return false;
		m_nodePositions = std::move(prepared.nodePositions);
		m_localBounds = prepared.bounds;
		m_cullingRadius = prepared.cullingRadius;
		StartupTiming timing("model.materials_and_upload");
		InitializeFallbackTextures(device, commandList);
		const auto modelDirectory = modelPath.parent_path();
		return CreateMeshes(prepared, options, modelDirectory, device, commandList, renderDevice);
	}

	bool Model::CreateMeshes(PreparedModelGeometry& prepared, const ModelLoadOptions& options,
		const std::filesystem::path& modelDirectory, ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice) {
		struct Batch {
			Mesh mesh;
			std::vector<ModelVertex> vertices;
			std::vector<uint32_t> indices;
		};
		std::array<std::vector<Batch>, 3> batches;
		std::vector<TextureDescriptorSet> textureDescriptorSets;
		std::unordered_map<unsigned, Mesh> materials;
		for (size_t geometryIndex = 0; geometryIndex < prepared.levels[0].size(); ++geometryIndex) {
			auto& geometry = prepared.levels[0][geometryIndex];
			const auto materialIndex = geometry.materialIndex;
			auto [material, inserted] = materials.try_emplace(materialIndex);
			if (inserted) {
				if (materialIndex < prepared.materials.size())
					LoadMaterialTextures(material->second, prepared.materials[materialIndex], modelDirectory,
						options, device, commandList);
				AssignMaterialDescriptors(material->second, device, renderDevice, textureDescriptorSets);
			}
			for (unsigned lod = 0; lod < m_lodCount; ++lod) {
				const unsigned level = m_lodCount - 1 - lod;
				auto& data = prepared.levels[level][geometryIndex];
				if (data.indices.empty()) continue;
				auto& list = batches[level];
				auto batch = list.end();
				if (options.mergeStaticMeshes && !geometry.isRotatingNode)
					batch = std::find_if(list.begin(), list.end(), [&](const Batch& item) {
						return !item.mesh.isRotatingNode &&
							item.mesh.textureSrvIndex == material->second.textureSrvIndex;
					});
				if (batch == list.end()) {
					Mesh mesh = material->second;
					mesh.isRotatingNode = geometry.isRotatingNode;
					mesh.nodeRotationPivot = geometry.nodeRotationPivot;
					mesh.nodeRotationAxis = geometry.nodeRotationAxis;
					list.push_back({std::move(mesh), {}, {}});
					batch = std::prev(list.end());
				}
				const auto offset = static_cast<uint32_t>(batch->vertices.size());
				for (auto index : data.indices)
					batch->indices.push_back(index + offset);
				batch->vertices.insert(batch->vertices.end(), data.vertices.begin(), data.vertices.end());
			}
			std::vector<ModelVertex>().swap(geometry.vertices);
			std::vector<uint32_t>().swap(geometry.indices);
			for (unsigned level = 1; level < m_lodCount; ++level) {
				std::vector<ModelVertex>().swap(prepared.levels[level][geometryIndex].vertices);
				std::vector<uint32_t>().swap(prepared.levels[level][geometryIndex].indices);
			}
		}
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		size_t vertexCount = 0, indexCount = 0;
		for (const auto& level : batches)
			for (const auto& batch : level) {
				vertexCount += batch.vertices.size();
				indexCount += batch.indices.size();
			}
		if (vertexCount > UINT_MAX / sizeof(ModelVertex) || indexCount > UINT_MAX / sizeof(uint32_t))
			throw std::runtime_error("Combined model buffers exceed D3D12 view limits");
		vertices.reserve(vertexCount);
		indices.reserve(indexCount);
		for (unsigned level = 0; level < m_lodCount; ++level) {
			for (auto& batch : batches[level]) {
				auto& mesh = batch.mesh;
				mesh.vertexBufferView = {vertices.size() * sizeof(ModelVertex),
					static_cast<UINT>(batch.vertices.size() * sizeof(ModelVertex)), sizeof(ModelVertex)};
				mesh.indexBufferView = {indices.size() * sizeof(uint32_t),
					static_cast<UINT>(batch.indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT};
				mesh.indexCount = static_cast<UINT>(batch.indices.size());
				vertices.insert(vertices.end(), batch.vertices.begin(), batch.vertices.end());
				indices.insert(indices.end(), batch.indices.begin(), batch.indices.end());
				std::vector<ModelVertex>().swap(batch.vertices);
				std::vector<uint32_t>().swap(batch.indices);
				m_lodMeshes[level].push_back(std::move(mesh));
			}
			if (m_lodMeshes[level].empty() && level > 0) m_lodMeshes[level] = m_lodMeshes[level - 1];
		}
		if (!IsLoaded()) return false;
		UploadGeometry(device, commandList, vertices.data(), vertices.size() * sizeof(ModelVertex),
			indices.data(), indices.size() * sizeof(uint32_t));
		Log("Model batches {}, triangles {}/{}/{}\n", GetMeshCount(), GetTriangleCount(0),
			GetTriangleCount(1), GetTriangleCount(2));
		return true;
	}

	size_t Model::CountNodes(std::string_view name) const {
		const auto found = m_nodePositions.find(std::string(name));
		return found == m_nodePositions.end() ? 0 : found->second.size();
	}

	std::optional<DirectX::XMFLOAT3> Model::FindNodePosition(std::string_view name) const {
		const auto found = m_nodePositions.find(std::string(name));
		if (found == m_nodePositions.end() || found->second.size() != 1) return std::nullopt;
		return found->second.front();
	}

	void Model::AssignMaterialDescriptors(Mesh& mesh, ID3D12Device* device, RenderDevice* renderDevice,
		std::vector<TextureDescriptorSet>& sets) {
		const Texture* diffuseTexture =
			mesh.diffuseTexture ? mesh.diffuseTexture.get() : m_fallbackTexture.get();
		const Texture* metallicTexture =
			mesh.metallicTexture ? mesh.metallicTexture.get() : m_fallbackMetallicTexture.get();
		const Texture* roughnessTexture =
			mesh.roughnessTexture ? mesh.roughnessTexture.get() : m_fallbackRoughnessTexture.get();
		const Texture* normalTexture =
			mesh.normalTexture ? mesh.normalTexture.get() : m_fallbackNormalTexture.get();
		const auto descriptorSet =
			std::find_if(sets.begin(), sets.end(), [&](const TextureDescriptorSet& set) {
				return set.diffuse == diffuseTexture && set.metallic == metallicTexture &&
					set.roughness == roughnessTexture && set.normal == normalTexture;
			});
		if (descriptorSet != sets.end()) {
			mesh.textureSrvIndex = descriptorSet->srvIndex;
		} else {
			auto descriptorHandle = renderDevice->AllocateSrvDescriptor(&mesh.textureSrvIndex);
			diffuseTexture->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			metallicTexture->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			roughnessTexture->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			normalTexture->CreateShaderResourceView(device, descriptorHandle);
			sets.push_back(
				{diffuseTexture, metallicTexture, roughnessTexture, normalTexture, mesh.textureSrvIndex});
		}
	}

	void Model::UploadGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
		const void* vertices, size_t vertexBytes, const void* indices, size_t indexBytes) {
		m_vertexBuffer = CreateStaticBuffer(device, commandList, vertices, vertexBytes,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_vertexUpload);
		m_indexBuffer = CreateStaticBuffer(
			device, commandList, indices, indexBytes, D3D12_RESOURCE_STATE_INDEX_BUFFER, m_indexUpload);
		for (auto& meshes : m_lodMeshes)
			for (auto& mesh : meshes) {
				mesh.vertexBufferView.BufferLocation += m_vertexBuffer->GetGPUVirtualAddress();
				mesh.indexBufferView.BufferLocation += m_indexBuffer->GetGPUVirtualAddress();
			}
	}

	void Model::InitializeFallbackTextures(ID3D12Device* device, ID3D12GraphicsCommandList* commands) {
		m_fallbackTexture = std::make_unique<Texture>();
		if (!m_fallbackTexture->CreateFromSolidColor(device, commands, 255, 255, 255)) {
			throw std::runtime_error("Failed to initialize fallback material texture.");
		}

		m_fallbackMetallicTexture = std::make_unique<Texture>();
		if (!m_fallbackMetallicTexture->CreateFromSolidColor(device, commands, 0, 0, 0)) {
			throw std::runtime_error("Failed to initialize fallback material texture.");
		}

		m_fallbackRoughnessTexture = std::make_unique<Texture>();
		if (!m_fallbackRoughnessTexture->CreateFromSolidColor(device, commands, 255, 255, 255)) {
			throw std::runtime_error("Failed to initialize fallback material texture.");
		}

		m_fallbackNormalTexture = std::make_unique<Texture>();
		if (!m_fallbackNormalTexture->CreateFromSolidColor(device, commands, 128, 128, 255)) {
			throw std::runtime_error("Failed to initialize fallback material texture.");
		}
	}

	void Model::LoadMaterialTextures(Mesh& mesh, const PreparedMaterial& source,
		const std::filesystem::path& modelDirectory, const ModelLoadOptions& options, ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList) {
		aiMaterial materialData;
		const aiString sourceName(source.name);
		materialData.AddProperty(&sourceName, AI_MATKEY_NAME);
		constexpr aiTextureType types[] = {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
			aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_NORMALS};
		for (size_t type = 0; type < std::size(types); ++type) {
			if (source.textures[type].empty()) continue;
			const aiString path(source.textures[type]);
			materialData.AddProperty(&path, AI_MATKEY_TEXTURE(types[type], 0));
		}
		const aiMaterial* material = &materialData;
		aiString materialName;
		material->Get(AI_MATKEY_NAME, materialName);
		std::filesystem::path resolvedTexturePath =
			ResolveMaterialTexture(material, aiTextureType_BASE_COLOR, modelDirectory, options);
		if (resolvedTexturePath.empty()) {
			resolvedTexturePath =
				ResolveMaterialTexture(material, aiTextureType_DIFFUSE, modelDirectory, options);
		}

		if (resolvedTexturePath.empty()) {
			resolvedTexturePath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), {}, options.baseColorSuffix, options);
		}

		if (resolvedTexturePath.empty()) {
			resolvedTexturePath =
				MaterialTexturePaths::FindFallbackTexturePath(modelDirectory, materialName.C_Str(), options);
		}

		if (!resolvedTexturePath.empty()) {
			mesh.diffuseTexture = Texture::LoadCached(device, commandList, resolvedTexturePath.wstring(),
				{.useSrgb = true,
					.generateMipmaps = true,
					.invertNormalGreen = false,
					.targetFormat = DXGI_FORMAT_BC3_UNORM_SRGB,
					.maxDimension = options.maxTextureDimension});
		}

		const auto loadScalarMap = [&](aiTextureType type, const std::wstring& suffix, DXGI_FORMAT format) {
			auto path = ResolveMaterialTexture(material, type, modelDirectory, options);
			if (path.empty())
				path = MaterialTexturePaths::FindNamedMaterialTexturePath(
					modelDirectory, materialName.C_Str(), resolvedTexturePath, suffix, options);
			if (path.empty()) return std::shared_ptr<Texture>{};
			return Texture::LoadCached(device, commandList, path.wstring(),
				{.useSrgb = false,
					.generateMipmaps = true,
					.targetFormat = format,
					.maxDimension = options.maxTextureDimension});
		};
		mesh.metallicTexture =
			loadScalarMap(aiTextureType_METALNESS, options.metallicSuffix, DXGI_FORMAT_BC4_UNORM);
		mesh.roughnessTexture =
			loadScalarMap(aiTextureType_DIFFUSE_ROUGHNESS, options.roughnessSuffix, DXGI_FORMAT_BC4_UNORM);

		std::filesystem::path normalPath =
			ResolveMaterialTexture(material, aiTextureType_NORMALS, modelDirectory, options);
		if (normalPath.empty()) {
			normalPath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), resolvedTexturePath, options.normalSuffix, options);
		}

		if (normalPath.empty()) {
			normalPath = MaterialTexturePaths::FindNamedMaterialTexturePath(modelDirectory,
				materialName.C_Str(), resolvedTexturePath, options.openGlNormalSuffix, options);
		}

		if (!normalPath.empty()) {
			std::wstring normalFilename = normalPath.filename().wstring();
			std::transform(normalFilename.begin(), normalFilename.end(), normalFilename.begin(), towlower);
			const bool invertNormalGreen = !options.openGlMarker.empty() &&
				normalFilename.find(options.openGlMarker) != std::wstring::npos;
			mesh.normalTexture = Texture::LoadCached(device, commandList, normalPath.wstring(),
				{.useSrgb = false,
					.generateMipmaps = true,
					.invertNormalGreen = invertNormalGreen,
					.targetFormat = DXGI_FORMAT_BC5_UNORM,
					.maxDimension = options.maxTextureDimension});
		}
	}

	void Model::Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
		float nodeRotationAngle, unsigned lod) const {
		struct NodeRotationConstants {
			float angle;
			DirectX::XMFLOAT3 pivot;
			DirectX::XMFLOAT3 axis;
			UINT enabled;
		};
		static_assert(sizeof(NodeRotationConstants) == RootBinding::NodeRotationDwords * sizeof(UINT));

		for (const Mesh& mesh : m_lodMeshes[std::min(lod, m_lodCount - 1)]) {
			const NodeRotationConstants nodeRotationConstants = {nodeRotationAngle, mesh.nodeRotationPivot,
				mesh.nodeRotationAxis, mesh.isRotatingNode ? 1u : 0u};
			commandList->SetGraphicsRoot32BitConstants(
				RootBinding::NodeRotation, RootBinding::NodeRotationDwords, &nodeRotationConstants, 0);
			commandList->SetGraphicsRootDescriptorTable(
				RootBinding::TextureTable, renderDevice->GetSrvGpuHandle(mesh.textureSrvIndex));
			commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
			commandList->IASetIndexBuffer(&mesh.indexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
		}
	}

	void Model::ReleaseUploadResources() {
		m_vertexUpload.Reset();
		m_indexUpload.Reset();
		if (m_fallbackTexture) {
			m_fallbackTexture->ReleaseUploadResources();
		}

		if (m_fallbackMetallicTexture) {
			m_fallbackMetallicTexture->ReleaseUploadResources();
		}

		if (m_fallbackRoughnessTexture) {
			m_fallbackRoughnessTexture->ReleaseUploadResources();
		}

		if (m_fallbackNormalTexture) {
			m_fallbackNormalTexture->ReleaseUploadResources();
		}

		for (auto& meshes : m_lodMeshes)
			for (Mesh& mesh : meshes) {
				if (mesh.diffuseTexture) {
					mesh.diffuseTexture->ReleaseUploadResources();
				}

				if (mesh.metallicTexture) {
					mesh.metallicTexture->ReleaseUploadResources();
				}

				if (mesh.roughnessTexture) {
					mesh.roughnessTexture->ReleaseUploadResources();
				}

				if (mesh.normalTexture) {
					mesh.normalTexture->ReleaseUploadResources();
				}
			}
	}
} // namespace Engine
