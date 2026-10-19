#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Model.h"
#include "../../Renderer/Device/RenderDevice.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cctype>
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

		bool CreateUploadBuffer(ID3D12Device* device, const void* data, UINT size, ComPtr<ID3D12Resource>& buffer) {
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
			ThrowIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
				D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer)));

			void* mappedData = nullptr;
			HRESULT hr = buffer->Map(0, nullptr, &mappedData);
			if (FAILED(hr)) {
				return false;
			}
			memcpy(mappedData, data, size);
			buffer->Unmap(0, nullptr);
			return true;
		}

		std::filesystem::path FindTexturePath(const std::filesystem::path& modelDirectory, const std::filesystem::path& texturePath) {
			const std::filesystem::path texturesDirectory = modelDirectory / L"Textures";
			const std::filesystem::path candidates[] = {
				texturePath.is_absolute() ? texturePath : modelDirectory / texturePath,
				texturesDirectory / texturePath,
				texturesDirectory / texturePath.filename()
			};

			for (const auto& candidate : candidates) {
				if (std::filesystem::exists(candidate)) {
					return candidate;
				}
			}

			return {};
		}

		std::filesystem::path FindFallbackTexturePath(const std::filesystem::path& modelDirectory, const std::string& materialName) {
			const std::filesystem::path texturesDirectory = modelDirectory / L"Textures";
			const std::filesystem::path fallbackPath = materialName.find("boomb") != std::string::npos
				? texturesDirectory / L"T_boomb_AO.png"
				: texturesDirectory / L"T_Jet_ID.png";
			return std::filesystem::exists(fallbackPath) ? fallbackPath : std::filesystem::path();
		}

		std::filesystem::path ResolveMaterialTexture(const aiMaterial* material, aiTextureType type, const std::filesystem::path& modelDirectory) {
			aiString texturePath;
			if (material->GetTexture(type, 0, &texturePath) != AI_SUCCESS || texturePath.length == 0 || texturePath.C_Str()[0] == '*') {
				return {};
			}

			const char8_t* pathUtf8 = reinterpret_cast<const char8_t*>(texturePath.C_Str());
			const std::filesystem::path relativePath(std::u8string(pathUtf8, pathUtf8 + texturePath.length));
			return FindTexturePath(modelDirectory, relativePath);
		}

		std::filesystem::path FindNamedMaterialTexturePath(
			const std::filesystem::path& modelDirectory,
			const std::string& materialName,
			const std::filesystem::path& baseColorPath,
			const std::wstring& suffix) {
			const std::filesystem::path texturesDirectory = modelDirectory / L"Textures";
			const std::wstring baseColorSuffix = L"_Base_color_1001";
			std::wstring baseName = baseColorPath.stem().wstring();
			std::wstring baseNameLower = baseName;
			std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), towlower);
			std::wstring baseSuffixLower = baseColorSuffix;
			std::transform(baseSuffixLower.begin(), baseSuffixLower.end(), baseSuffixLower.begin(), towlower);
			const size_t baseColorPosition = baseNameLower.find(baseSuffixLower);
			if (baseColorPosition != std::wstring::npos) {
				const auto candidate = texturesDirectory / (baseName.substr(0, baseColorPosition) + suffix);
				if (std::filesystem::exists(candidate)) {
					return candidate;
				}
			}

			std::string materialNameLower = materialName;
			std::transform(materialNameLower.begin(), materialNameLower.end(), materialNameLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			std::error_code error;
			for (const auto& entry : std::filesystem::directory_iterator(texturesDirectory, error)) {
				if (error || !entry.is_regular_file()) {
					continue;
				}
				std::wstring filename = entry.path().filename().wstring();
				std::wstring filenameLower = filename;
				std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), towlower);
				std::wstring suffixLower = suffix;
				std::transform(suffixLower.begin(), suffixLower.end(), suffixLower.begin(), towlower);
				if (filenameLower.size() < suffixLower.size() || filenameLower.substr(filenameLower.size() - suffixLower.size()) != suffixLower) {
					continue;
				}

				const std::wstring prefix = filename.substr(0, filename.size() - suffix.size());
				const std::string prefixUtf8(prefix.begin(), prefix.end());
				std::string prefixLower = prefixUtf8;
				std::transform(prefixLower.begin(), prefixLower.end(), prefixLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (prefixLower == materialNameLower) {
					return entry.path();
				}
			}

			return {};
		}
	}

	bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice, const std::wstring& filePath) {
		m_meshes.clear();
		m_fallbackTexture.reset();
		m_fallbackMetallicTexture.reset();
		m_fallbackRoughnessTexture.reset();
		m_fallbackNormalTexture.reset();

		const std::filesystem::path modelPath(filePath);
		const auto utf8Path = modelPath.u8string();
		const std::string importerPath(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(importerPath,
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_ConvertToLeftHanded |
			aiProcess_GenSmoothNormals |
			aiProcess_CalcTangentSpace |
			aiProcess_PreTransformVertices);
		if (!scene || !scene->HasMeshes()) {
			OutputDebugStringA(importer.GetErrorString());
			OutputDebugStringA("\n");
			return false;
		}

		aiVector3D boundsMin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
		aiVector3D boundsMax(std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
		for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			for (unsigned int vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
				const aiVector3D& position = mesh->mVertices[vertexIndex];
				boundsMin.x = std::min(boundsMin.x, position.x);
				boundsMin.y = std::min(boundsMin.y, position.y);
				boundsMin.z = std::min(boundsMin.z, position.z);
				boundsMax.x = std::max(boundsMax.x, position.x);
				boundsMax.y = std::max(boundsMax.y, position.y);
				boundsMax.z = std::max(boundsMax.z, position.z);
			}
		}

		const float extent = std::max({ boundsMax.x - boundsMin.x, boundsMax.y - boundsMin.y, boundsMax.z - boundsMin.z });
		if (extent <= 0.0f) {
			OutputDebugStringA("Model::Load - model has no valid dimensions\n");
			return false;
		}
		const float scale = 10.0f / extent;

		m_fallbackTexture = std::make_unique<Texture>();
		if (!m_fallbackTexture->CreateFromSolidColor(device, commandList, 255, 255, 255)) {
			return false;
		}
		m_fallbackMetallicTexture = std::make_unique<Texture>();
		if (!m_fallbackMetallicTexture->CreateFromSolidColor(device, commandList, 0, 0, 0)) {
			return false;
		}
		m_fallbackRoughnessTexture = std::make_unique<Texture>();
		if (!m_fallbackRoughnessTexture->CreateFromSolidColor(device, commandList, 255, 255, 255)) {
			return false;
		}
		m_fallbackNormalTexture = std::make_unique<Texture>();
		if (!m_fallbackNormalTexture->CreateFromSolidColor(device, commandList, 128, 128, 255)) {
			return false;
		}

		const std::filesystem::path modelDirectory = modelPath.parent_path();
		m_meshes.reserve(scene->mNumMeshes);
		uint64_t totalVertices = 0;
		uint64_t totalIndices = 0;
		uint64_t totalVertexBufferBytes = 0;
		uint64_t totalIndexBufferBytes = 0;
		for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
			const aiMesh* sourceMesh = scene->mMeshes[meshIndex];
			if (!sourceMesh->HasPositions() || !sourceMesh->HasFaces()) {
				continue;
			}

			std::vector<ModelVertex> vertices;
			vertices.reserve(sourceMesh->mNumVertices);
			for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex) {
				const aiVector3D& position = sourceMesh->mVertices[vertexIndex];
				const aiVector3D texcoord = sourceMesh->HasTextureCoords(0) ? sourceMesh->mTextureCoords[0][vertexIndex] : aiVector3D();
				const aiVector3D normal = sourceMesh->HasNormals() ? sourceMesh->mNormals[vertexIndex] : aiVector3D(0.0f, 1.0f, 0.0f);
				const aiVector3D tangent = sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mTangents[vertexIndex] : aiVector3D(1.0f, 0.0f, 0.0f);
				const aiVector3D bitangent = sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mBitangents[vertexIndex] : aiVector3D(0.0f, 0.0f, 1.0f);
				const float tangentSign = normal.x * (tangent.y * bitangent.z - tangent.z * bitangent.y)
					+ normal.y * (tangent.z * bitangent.x - tangent.x * bitangent.z)
					+ normal.z * (tangent.x * bitangent.y - tangent.y * bitangent.x) < 0.0f ? -1.0f : 1.0f;
				vertices.push_back({
					{ (position.x - (boundsMin.x + boundsMax.x) * 0.5f) * scale,
					  (position.y - boundsMin.y) * scale + 0.1f,
					  (position.z - (boundsMin.z + boundsMax.z) * 0.5f) * scale },
					{ texcoord.x, texcoord.y },
					{ normal.x, normal.y, normal.z },
					{ tangent.x, tangent.y, tangent.z, tangentSign }
					});
			}

			std::vector<uint32_t> indices;
			indices.reserve(static_cast<size_t>(sourceMesh->mNumFaces) * 3);
			for (unsigned int faceIndex = 0; faceIndex < sourceMesh->mNumFaces; ++faceIndex) {
				const aiFace& face = sourceMesh->mFaces[faceIndex];
				if (face.mNumIndices == 3) {
					indices.insert(indices.end(), face.mIndices, face.mIndices + 3);
				}
			}
			if (indices.empty() || vertices.size() > UINT_MAX / sizeof(ModelVertex) || indices.size() > UINT_MAX / sizeof(uint32_t)) {
				continue;
			}

			Mesh mesh;
			if (!CreateUploadBuffer(device, vertices.data(), static_cast<UINT>(vertices.size() * sizeof(ModelVertex)), mesh.vertexBuffer) ||
				!CreateUploadBuffer(device, indices.data(), static_cast<UINT>(indices.size() * sizeof(uint32_t)), mesh.indexBuffer)) {
				return false;
			}
			mesh.vertexBufferView.BufferLocation = mesh.vertexBuffer->GetGPUVirtualAddress();
			mesh.vertexBufferView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(ModelVertex));
			mesh.vertexBufferView.StrideInBytes = sizeof(ModelVertex);
			mesh.indexBufferView.BufferLocation = mesh.indexBuffer->GetGPUVirtualAddress();
			mesh.indexBufferView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(uint32_t));
			mesh.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			mesh.indexCount = static_cast<UINT>(indices.size());
			totalVertices += vertices.size();
			totalIndices += indices.size();
			totalVertexBufferBytes += vertices.size() * sizeof(ModelVertex);
			totalIndexBufferBytes += indices.size() * sizeof(uint32_t);

			if (sourceMesh->mMaterialIndex < scene->mNumMaterials) {
				const aiMaterial* material = scene->mMaterials[sourceMesh->mMaterialIndex];
				aiString materialName;
				material->Get(AI_MATKEY_NAME, materialName);
				std::filesystem::path resolvedTexturePath = ResolveMaterialTexture(material, aiTextureType_BASE_COLOR, modelDirectory);
				if (resolvedTexturePath.empty()) {
					resolvedTexturePath = ResolveMaterialTexture(material, aiTextureType_DIFFUSE, modelDirectory);
				}
				if (resolvedTexturePath.empty()) {
					resolvedTexturePath = FindNamedMaterialTexturePath(modelDirectory, materialName.C_Str(), {}, L"_Base_color_1001.png");
				}
				if (resolvedTexturePath.empty()) {
					resolvedTexturePath = FindFallbackTexturePath(modelDirectory, materialName.C_Str());
				}
				if (!resolvedTexturePath.empty()) {
					mesh.diffuseTexture = Texture::LoadCached(device, commandList, resolvedTexturePath.wstring(), true, false, false, DXGI_FORMAT_BC3_UNORM_SRGB);
				}

				std::filesystem::path metallicPath = ResolveMaterialTexture(material, aiTextureType_METALNESS, modelDirectory);
				if (metallicPath.empty()) {
					metallicPath = FindNamedMaterialTexturePath(modelDirectory, materialName.C_Str(), resolvedTexturePath, L"_Metallic_1001.png");
				}
				if (!metallicPath.empty()) {
					mesh.metallicTexture = Texture::LoadCached(device, commandList, metallicPath.wstring(), false, false, false, DXGI_FORMAT_BC4_UNORM);
				}

				std::filesystem::path roughnessPath = ResolveMaterialTexture(material, aiTextureType_DIFFUSE_ROUGHNESS, modelDirectory);
				if (roughnessPath.empty()) {
					roughnessPath = FindNamedMaterialTexturePath(modelDirectory, materialName.C_Str(), resolvedTexturePath, L"_Roughness_1001.png");
				}
				if (!roughnessPath.empty()) {
					mesh.roughnessTexture = Texture::LoadCached(device, commandList, roughnessPath.wstring(), false, false, false, DXGI_FORMAT_BC4_UNORM);
				}

				std::filesystem::path normalPath = ResolveMaterialTexture(material, aiTextureType_NORMALS, modelDirectory);
				if (normalPath.empty()) {
					normalPath = FindNamedMaterialTexturePath(modelDirectory, materialName.C_Str(), resolvedTexturePath, L"_Normal_1001.png");
				}
				if (normalPath.empty()) {
					normalPath = FindNamedMaterialTexturePath(modelDirectory, materialName.C_Str(), resolvedTexturePath, L"_Normal_OpenGL_1001.png");
				}
				if (!normalPath.empty()) {
					std::wstring normalFilename = normalPath.filename().wstring();
					std::transform(normalFilename.begin(), normalFilename.end(), normalFilename.begin(), towlower);
					const bool invertNormalGreen = normalFilename.find(L"_opengl_") != std::wstring::npos;
					mesh.normalTexture = Texture::LoadCached(device, commandList, normalPath.wstring(), false, false, invertNormalGreen, DXGI_FORMAT_BC5_UNORM);
				}
			}

			auto descriptorHandle = renderDevice->AllocateSrvDescriptor(&mesh.textureSrvIndex);
			(mesh.diffuseTexture ? mesh.diffuseTexture.get() : m_fallbackTexture.get())->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			(mesh.metallicTexture ? mesh.metallicTexture.get() : m_fallbackMetallicTexture.get())->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			(mesh.roughnessTexture ? mesh.roughnessTexture.get() : m_fallbackRoughnessTexture.get())->CreateShaderResourceView(device, descriptorHandle);
			descriptorHandle = renderDevice->AllocateSrvDescriptor();
			(mesh.normalTexture ? mesh.normalTexture.get() : m_fallbackNormalTexture.get())->CreateShaderResourceView(device, descriptorHandle);
			m_meshes.push_back(std::move(mesh));
		}

		char message[512];
		sprintf_s(message, sizeof(message),
			"[Model Memory] Meshes: %zu, Vertices: %llu, Indices: %llu, Vertex Buffers: %.2f MiB, Index Buffers: %.2f MiB, Total: %.2f MiB\n",
			m_meshes.size(),
			static_cast<unsigned long long>(totalVertices),
			static_cast<unsigned long long>(totalIndices),
			static_cast<double>(totalVertexBufferBytes) / (1024.0 * 1024.0),
			static_cast<double>(totalIndexBufferBytes) / (1024.0 * 1024.0),
			static_cast<double>(totalVertexBufferBytes + totalIndexBufferBytes) / (1024.0 * 1024.0));
		OutputDebugStringA(message);

		return !m_meshes.empty();
	}

	void Model::Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice) const {
		for (const Mesh& mesh : m_meshes) {
			commandList->SetGraphicsRootDescriptorTable(0, renderDevice->GetSrvGpuHandle(mesh.textureSrvIndex));
			commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
			commandList->IASetIndexBuffer(&mesh.indexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
		}
	}

	void Model::ReleaseUploadResources() {
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
}
