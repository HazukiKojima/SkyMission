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
#include <cstring>
#include <DirectXMath.h>
#include <filesystem>
#include <limits>
#include <vector>

namespace Engine {
	namespace {
		struct ModelVertex {
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT2 texcoord;
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
	}

	bool Model::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice, const std::wstring& filePath) {
		m_meshes.clear();
		m_fallbackTexture.reset();

		const std::filesystem::path modelPath(filePath);
		const auto utf8Path = modelPath.u8string();
		const std::string importerPath(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(importerPath,
			aiProcess_Triangulate |
			aiProcess_JoinIdenticalVertices |
			aiProcess_ImproveCacheLocality |
			aiProcess_ConvertToLeftHanded |
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
		const auto fallbackCpuHandle = renderDevice->AllocateSrvDescriptor(&m_fallbackTextureSrvIndex);
		m_fallbackTexture->CreateShaderResourceView(device, fallbackCpuHandle);

		const std::filesystem::path modelDirectory = modelPath.parent_path();
		m_meshes.reserve(scene->mNumMeshes);
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
				vertices.push_back({
					{ (position.x - (boundsMin.x + boundsMax.x) * 0.5f) * scale,
					  (position.y - boundsMin.y) * scale + 0.1f,
					  (position.z - (boundsMin.z + boundsMax.z) * 0.5f) * scale },
					{ texcoord.x, texcoord.y }
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

			if (sourceMesh->mMaterialIndex < scene->mNumMaterials) {
				const aiMaterial* material = scene->mMaterials[sourceMesh->mMaterialIndex];
				aiString texturePath;
				if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS && texturePath.length > 0 && texturePath.C_Str()[0] != '*') {
					const char8_t* texturePathUtf8 = reinterpret_cast<const char8_t*>(texturePath.C_Str());
					const std::filesystem::path relativeTexturePath(std::u8string(texturePathUtf8, texturePathUtf8 + texturePath.length));
					const std::filesystem::path resolvedTexturePath = relativeTexturePath.is_absolute() ? relativeTexturePath : modelDirectory / relativeTexturePath;
					mesh.diffuseTexture = std::make_unique<Texture>();
					if (mesh.diffuseTexture->LoadFromFile(device, commandList, resolvedTexturePath.wstring())) {
						const auto textureCpuHandle = renderDevice->AllocateSrvDescriptor(&mesh.textureSrvIndex);
						mesh.diffuseTexture->CreateShaderResourceView(device, textureCpuHandle);
					}
					else {
						mesh.diffuseTexture.reset();
					}
				}
			}
			m_meshes.push_back(std::move(mesh));
		}

		return !m_meshes.empty();
	}

	void Model::Draw(ID3D12GraphicsCommandList* commandList, RenderDevice* renderDevice) const {
		for (const Mesh& mesh : m_meshes) {
			const UINT textureIndex = mesh.diffuseTexture ? mesh.textureSrvIndex : m_fallbackTextureSrvIndex;
			commandList->SetGraphicsRootDescriptorTable(0, renderDevice->GetSrvGpuHandle(textureIndex));
			commandList->IASetVertexBuffers(0, 1, &mesh.vertexBufferView);
			commandList->IASetIndexBuffer(&mesh.indexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(mesh.indexCount, 1, 0, 0, 0);
		}
	}
}
