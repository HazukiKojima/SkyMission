#include "Model.h"
#include "MaterialTexturePaths.h"
#include "../Buffer/BufferUtilities.h"
#include "../../Log.h"
#include "../../Renderer/Device/RenderDevice.h"
#include <assimp/Importer.hpp>
#include <assimp/matrix3x3.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <cstring>
#include <DirectXMath.h>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace Engine {
	namespace {
		struct ModelVertex {
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT2 texcoord;
			DirectX::XMFLOAT3 normal;
			DirectX::XMFLOAT4 tangent;
		};

		struct MeshInstance {
			const aiMesh* mesh;
			aiMatrix4x4 transform;
			bool isPropeller;
		};

		struct ModelNormalization {
			aiVector3D minimum, maximum;
			float scale, groundOffset;
			DirectX::XMFLOAT3 Transform(const aiVector3D& position) const {
				return {(position.x - (minimum.x + maximum.x) * 0.5f) * scale,
						(position.y - minimum.y) * scale + groundOffset,
						(position.z - (minimum.z + maximum.z) * 0.5f) * scale};
			}
		};
		struct MeshGeometry {
			const aiMesh* source = nullptr;
			std::vector<ModelVertex> vertices;
			std::vector<uint32_t> indices;
			bool isPropeller = false;
			DirectX::XMFLOAT3 propellerPivot{};
			DirectX::XMFLOAT3 propellerAxis{0.0f, 0.0f, 1.0f};
		};
		MeshGeometry BuildMeshGeometry(const MeshInstance& instance, const ModelNormalization& normalization) {
			MeshGeometry result;
			const aiMesh* sourceMesh = instance.mesh;
			if (!sourceMesh->HasPositions() || !sourceMesh->HasFaces())
				return result;
			result.source = sourceMesh;
			auto& vertices = result.vertices;
			vertices.reserve(sourceMesh->mNumVertices);
			const bool isPropeller = instance.isPropeller;
			const aiMatrix3x3 linearTransform(instance.transform);
			aiMatrix3x3 normalTransform(linearTransform);
			normalTransform.Inverse().Transpose();
			aiVector3D propellerBoundsMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
										  std::numeric_limits<float>::max());
			aiVector3D propellerBoundsMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
										  std::numeric_limits<float>::lowest());
			for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex) {
				const aiVector3D position = instance.transform * sourceMesh->mVertices[vertexIndex];
				if (isPropeller) {
					propellerBoundsMin.x = std::min(propellerBoundsMin.x, position.x);
					propellerBoundsMin.y = std::min(propellerBoundsMin.y, position.y);
					propellerBoundsMin.z = std::min(propellerBoundsMin.z, position.z);
					propellerBoundsMax.x = std::max(propellerBoundsMax.x, position.x);
					propellerBoundsMax.y = std::max(propellerBoundsMax.y, position.y);
					propellerBoundsMax.z = std::max(propellerBoundsMax.z, position.z);
				}
				const aiVector3D texcoord =
					sourceMesh->HasTextureCoords(0) ? sourceMesh->mTextureCoords[0][vertexIndex] : aiVector3D();
				const aiVector3D normal =
					normalTransform *
					(sourceMesh->HasNormals() ? sourceMesh->mNormals[vertexIndex] : aiVector3D(0.0f, 1.0f, 0.0f));
				const aiVector3D tangent =
					linearTransform * (sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mTangents[vertexIndex]
																			  : aiVector3D(1.0f, 0.0f, 0.0f));
				const aiVector3D bitangent =
					linearTransform * (sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mBitangents[vertexIndex]
																			  : aiVector3D(0.0f, 0.0f, 1.0f));
				const float tangentSign = normal.x * (tangent.y * bitangent.z - tangent.z * bitangent.y) +
													  normal.y * (tangent.z * bitangent.x - tangent.x * bitangent.z) +
													  normal.z * (tangent.x * bitangent.y - tangent.y * bitangent.x) <
												  0.0f
											  ? -1.0f
											  : 1.0f;
				vertices.push_back({normalization.Transform(position),
									{texcoord.x, texcoord.y},
									{normal.x, normal.y, normal.z},
									{tangent.x, tangent.y, tangent.z, tangentSign}});
			}

			auto& indices = result.indices;
			indices.reserve(static_cast<size_t>(sourceMesh->mNumFaces) * 3);
			for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex) {
				const aiFace& face = sourceMesh->mFaces[faceIndex];
				if (face.mNumIndices == 3) {
					indices.insert(indices.end(), face.mIndices, face.mIndices + 3);
				}
			}
			if (indices.empty() || vertices.size() > UINT_MAX / sizeof(ModelVertex) ||
				indices.size() > UINT_MAX / sizeof(uint32_t)) {
				return {};
			}

			result.isPropeller = isPropeller;
			if (isPropeller) {
				const float extentX = propellerBoundsMax.x - propellerBoundsMin.x;
				const float extentY = propellerBoundsMax.y - propellerBoundsMin.y;
				const float extentZ = propellerBoundsMax.z - propellerBoundsMin.z;
				result.propellerPivot = normalization.Transform((propellerBoundsMin + propellerBoundsMax) * 0.5f);
				if (extentX <= extentY && extentX <= extentZ) {
					result.propellerAxis = {1.0f, 0.0f, 0.0f};
				} else if (extentY <= extentZ) {
					result.propellerAxis = {0.0f, 1.0f, 0.0f};
				}
			}
			return result;
		}

		void CollectMeshInstances(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parentTransform,
								  bool parentIsPropeller, const std::string& propellerName,
								  std::vector<MeshInstance>& instances) {
			const aiMatrix4x4 transform = parentTransform * node->mTransformation;
			const bool isPropeller =
				parentIsPropeller || (!propellerName.empty() && node->mName.C_Str() == propellerName);
			for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
				instances.push_back(
					{mesh, transform, isPropeller || (!propellerName.empty() && mesh->mName.C_Str() == propellerName)});
			}

			for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
				CollectMeshInstances(scene, node->mChildren[childIndex], transform, isPropeller, propellerName,
									 instances);
			}
		}

		std::filesystem::path ResolveMaterialTexture(const aiMaterial* material, aiTextureType type,
													 const std::filesystem::path& modelDirectory,
													 const ModelLoadOptions& options) {
			aiString texturePath;
			if (material->GetTexture(type, 0, &texturePath) != AI_SUCCESS || texturePath.length == 0 ||
				texturePath.C_Str()[0] == '*') {
				return {};
			}

			const char8_t* pathUtf8 = reinterpret_cast<const char8_t*>(texturePath.C_Str());
			const std::filesystem::path relativePath(std::u8string(pathUtf8, pathUtf8 + texturePath.length));
			return MaterialTexturePaths::FindTexturePath(modelDirectory, relativePath, options);
		}

	} // namespace

	bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice,
					 const std::wstring& filePath, const ModelLoadOptions& options) {
		m_meshes.clear();
		m_vertexBuffer.Reset();
		m_indexBuffer.Reset();
		m_vertexUpload.Reset();
		m_indexUpload.Reset();
		m_fallbackTexture.reset();
		m_fallbackMetallicTexture.reset();
		m_fallbackRoughnessTexture.reset();
		m_fallbackNormalTexture.reset();

		const std::filesystem::path modelPath(filePath);
		const auto utf8Path = modelPath.u8string();
		const std::string importerPath(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(
			importerPath, aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality |
							  aiProcess_ConvertToLeftHanded | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);
		if (!scene || !scene->HasMeshes() || !scene->mRootNode) {
			Log("{}", importer.GetErrorString());
			Log("\n");
			return false;
		}

		std::vector<MeshInstance> meshInstances;
		CollectMeshInstances(scene, scene->mRootNode, aiMatrix4x4(), false, options.propellerName, meshInstances);
		if (meshInstances.empty()) {
			Log("Model::Load - model has no node mesh instances\n");
			return false;
		}

		aiVector3D boundsMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
							 std::numeric_limits<float>::max());
		aiVector3D boundsMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
							 std::numeric_limits<float>::lowest());
		for (const MeshInstance& instance : meshInstances) {
			const aiMesh* mesh = instance.mesh;
			for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
				const aiVector3D position = instance.transform * mesh->mVertices[vertexIndex];
				boundsMin.x = std::min(boundsMin.x, position.x);
				boundsMin.y = std::min(boundsMin.y, position.y);
				boundsMin.z = std::min(boundsMin.z, position.z);
				boundsMax.x = std::max(boundsMax.x, position.x);
				boundsMax.y = std::max(boundsMax.y, position.y);
				boundsMax.z = std::max(boundsMax.z, position.z);
			}
		}

		const float extent =
			std::max({boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z});
		if (!std::isfinite(extent) || extent <= 0.0f) {
			Log("Model::Load - model has no valid dimensions\n");
			return false;
		}
		const ModelNormalization normalization{boundsMin, boundsMax, options.normalizedExtent / extent,
											   options.groundOffset};
		std::vector<MeshGeometry> geometries;
		for (const auto& instance : meshInstances) {
			auto geometry = BuildMeshGeometry(instance, normalization);
			if (!geometry.indices.empty())
				geometries.push_back(std::move(geometry));
		}
		if (geometries.empty())
			return false;

		InitializeFallbackTextures(device, commandList);

		const std::filesystem::path modelDirectory = modelPath.parent_path();
		m_meshes.reserve(meshInstances.size());
		struct TextureDescriptorSet {
			const Texture* diffuse;
			const Texture* metallic;
			const Texture* roughness;
			const Texture* normal;
			UINT srvIndex;
		};
		std::vector<TextureDescriptorSet> textureDescriptorSets;
		std::vector<ModelVertex> allVertices;
		std::vector<uint32_t> allIndices;
		uint64_t totalVertices = 0;
		uint64_t totalIndices = 0;
		uint64_t totalVertexBufferBytes = 0;
		uint64_t totalIndexBufferBytes = 0;
		for (const auto& geometry : geometries) {
			const aiMesh* sourceMesh = geometry.source;
			const auto& vertices = geometry.vertices;
			const auto& indices = geometry.indices;
			Mesh mesh;
			mesh.isPropeller = geometry.isPropeller;
			mesh.propellerPivot = geometry.propellerPivot;
			mesh.propellerAxis = geometry.propellerAxis;
			mesh.vertexBufferView.BufferLocation = allVertices.size() * sizeof(ModelVertex);
			mesh.indexBufferView.BufferLocation = allIndices.size() * sizeof(uint32_t);
			allVertices.insert(allVertices.end(), vertices.begin(), vertices.end());
			allIndices.insert(allIndices.end(), indices.begin(), indices.end());
			mesh.vertexBufferView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(ModelVertex));
			mesh.vertexBufferView.StrideInBytes = sizeof(ModelVertex);
			mesh.indexBufferView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(uint32_t));
			mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			mesh.indexCount = static_cast<UINT>(indices.size());
			totalVertices += vertices.size();
			totalIndices += indices.size();
			totalVertexBufferBytes += vertices.size() * sizeof(ModelVertex);
			totalIndexBufferBytes += indices.size() * sizeof(uint32_t);

			if (sourceMesh->mMaterialIndex < scene->mNumMaterials)
				LoadMaterialTextures(mesh, scene->mMaterials[sourceMesh->mMaterialIndex], modelDirectory, options,
									 device, commandList);

			const Texture* diffuseTexture = mesh.diffuseTexture ? mesh.diffuseTexture.get() : m_fallbackTexture.get();
			const Texture* metallicTexture =
				mesh.metallicTexture ? mesh.metallicTexture.get() : m_fallbackMetallicTexture.get();
			const Texture* roughnessTexture =
				mesh.roughnessTexture ? mesh.roughnessTexture.get() : m_fallbackRoughnessTexture.get();
			const Texture* normalTexture =
				mesh.normalTexture ? mesh.normalTexture.get() : m_fallbackNormalTexture.get();
			const auto descriptorSet = std::find_if(
				textureDescriptorSets.begin(), textureDescriptorSets.end(), [&](const TextureDescriptorSet& set) {
					return set.diffuse == diffuseTexture && set.metallic == metallicTexture &&
						   set.roughness == roughnessTexture && set.normal == normalTexture;
				});
			if (descriptorSet != textureDescriptorSets.end()) {
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
				textureDescriptorSets.push_back(
					{diffuseTexture, metallicTexture, roughnessTexture, normalTexture, mesh.textureSrvIndex});
			}
			m_meshes.push_back(std::move(mesh));
		}

		if (m_meshes.empty())
			return false;
		m_vertexBuffer =
			CreateStaticBuffer(device, commandList, allVertices.data(), allVertices.size() * sizeof(ModelVertex),
							   D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_vertexUpload);
		m_indexBuffer = CreateStaticBuffer(device, commandList, allIndices.data(), allIndices.size() * sizeof(uint32_t),
										   D3D12_RESOURCE_STATE_INDEX_BUFFER, m_indexUpload);
		for (auto& mesh : m_meshes) {
			mesh.vertexBufferView.BufferLocation += m_vertexBuffer->GetGPUVirtualAddress();
			mesh.indexBufferView.BufferLocation += m_indexBuffer->GetGPUVirtualAddress();
		}
		Log("Model loaded: {} meshes, {} vertices, {} indices, {:.2f} MiB static geometry\n", m_meshes.size(),
			totalVertices, totalIndices,
			static_cast<double>(totalVertexBufferBytes + totalIndexBufferBytes) / (1024.0 * 1024.0));

		return !m_meshes.empty();
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

	void Model::LoadMaterialTextures(Mesh& mesh, const aiMaterial* material,
									 const std::filesystem::path& modelDirectory, const ModelLoadOptions& options,
									 ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
		aiString materialName;
		material->Get(AI_MATKEY_NAME, materialName);
		std::filesystem::path resolvedTexturePath =
			ResolveMaterialTexture(material, aiTextureType_BASE_COLOR, modelDirectory, options);
		if (resolvedTexturePath.empty()) {
			resolvedTexturePath = ResolveMaterialTexture(material, aiTextureType_DIFFUSE, modelDirectory, options);
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
													   .targetFormat = DXGI_FORMAT_BC3_UNORM_SRGB});
		}

		std::filesystem::path metallicPath =
			ResolveMaterialTexture(material, aiTextureType_METALNESS, modelDirectory, options);
		if (metallicPath.empty()) {
			metallicPath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), resolvedTexturePath, options.metallicSuffix, options);
		}
		if (!metallicPath.empty()) {
			mesh.metallicTexture = Texture::LoadCached(device, commandList, metallicPath.wstring(),
													   {.useSrgb = false,
														.generateMipmaps = true,
														.invertNormalGreen = false,
														.targetFormat = DXGI_FORMAT_BC4_UNORM});
		}

		std::filesystem::path roughnessPath =
			ResolveMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS, modelDirectory, options);
		if (roughnessPath.empty()) {
			roughnessPath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), resolvedTexturePath, options.roughnessSuffix, options);
		}
		if (!roughnessPath.empty()) {
			mesh.roughnessTexture = Texture::LoadCached(device, commandList, roughnessPath.wstring(),
														{.useSrgb = false,
														 .generateMipmaps = true,
														 .invertNormalGreen = false,
														 .targetFormat = DXGI_FORMAT_BC4_UNORM});
		}

		std::filesystem::path normalPath =
			ResolveMaterialTexture(material, aiTextureType_NORMALS, modelDirectory, options);
		if (normalPath.empty()) {
			normalPath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), resolvedTexturePath, options.normalSuffix, options);
		}
		if (normalPath.empty()) {
			normalPath = MaterialTexturePaths::FindNamedMaterialTexturePath(
				modelDirectory, materialName.C_Str(), resolvedTexturePath, options.openGlNormalSuffix, options);
		}
		if (!normalPath.empty()) {
			std::wstring normalFilename = normalPath.filename().wstring();
			std::transform(normalFilename.begin(), normalFilename.end(), normalFilename.begin(), towlower);
			const bool invertNormalGreen =
				!options.openGlMarker.empty() && normalFilename.find(options.openGlMarker) != std::wstring::npos;
			mesh.normalTexture = Texture::LoadCached(device, commandList, normalPath.wstring(),
													 {.useSrgb = false,
													  .generateMipmaps = true,
													  .invertNormalGreen = invertNormalGreen,
													  .targetFormat = DXGI_FORMAT_BC5_UNORM});
		}
	}

	void Model::Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice, float propellerAngle) const {
		struct PropellerConstants {
			float angle;
			DirectX::XMFLOAT3 pivot;
			DirectX::XMFLOAT3 axis;
			UINT enabled;
		};
		static_assert(sizeof(PropellerConstants) == 8 * sizeof(UINT));

		for (const Mesh& mesh : m_meshes) {
			const PropellerConstants propellerConstants = {propellerAngle, mesh.propellerPivot, mesh.propellerAxis,
														   mesh.isPropeller ? 1u : 0u};
			commandList->SetGraphicsRoot32BitConstants(2, 8, &propellerConstants, 0);
			commandList->SetGraphicsRootDescriptorTable(0, renderDevice->GetSrvGpuHandle(mesh.textureSrvIndex));
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

		for (Mesh& mesh : m_meshes) {
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
