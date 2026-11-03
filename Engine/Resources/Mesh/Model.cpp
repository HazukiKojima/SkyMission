#include "../../Renderer/RootBindings.h"
#include "Model.h"
#include "MaterialTexturePaths.h"
#include "../Buffer/BufferUtilities.h"
#include "../../Log.h"
#include "../../Renderer/Device/RenderDevice.h"
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>
#include <assimp/config.h>
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
#include <array>
#include <unordered_map>

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
			bool isRotatingNode;
		};

		// メッシュと補助ノードに同じ原点補正・縮尺を適用する
		struct ModelNormalization {
			aiVector3D minimum;
			aiVector3D maximum;
			float scale;
			float groundOffset;
			ModelOrigin origin;
			DirectX::XMFLOAT3 Transform(const aiVector3D& position) const {
				return {(position.x - (minimum.x + maximum.x) * 0.5f) * scale,
					origin == ModelOrigin::BoundsCenter
						? (position.y - (minimum.y + maximum.y) * 0.5f) * scale
						: (position.y - minimum.y) * scale + groundOffset,
					(position.z - (minimum.z + maximum.z) * 0.5f) * scale};
			}
		};
		struct MeshGeometry {
			const aiMesh* source = nullptr;
			std::vector<ModelVertex> vertices;
			std::vector<uint32_t> indices;
			bool isRotatingNode = false;
			DirectX::XMFLOAT3 nodeRotationPivot{};
			DirectX::XMFLOAT3 nodeRotationAxis{0.0f, 0.0f, 1.0f};
		};
		MeshGeometry BuildMeshGeometry(
			const MeshInstance& instance, const ModelNormalization& normalization) {
			MeshGeometry result;
			const aiMesh* sourceMesh = instance.mesh;
			if (!sourceMesh->HasPositions() || !sourceMesh->HasFaces())
				return result;
			result.source = sourceMesh;
			auto& vertices = result.vertices;
			vertices.reserve(sourceMesh->mNumVertices);
			const bool isRotatingNode = instance.isRotatingNode;
			const aiMatrix3x3 linearTransform(instance.transform);
			aiMatrix3x3 normalTransform(linearTransform);
			normalTransform.Inverse().Transpose();
			aiVector3D nodeRotationBoundsMin(std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
			aiVector3D nodeRotationBoundsMax(std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
			for (unsigned int vertexIndex = 0; vertexIndex < sourceMesh->mNumVertices; ++vertexIndex) {
				const aiVector3D position = instance.transform * sourceMesh->mVertices[vertexIndex];
				if (isRotatingNode) {
					nodeRotationBoundsMin.x = std::min(nodeRotationBoundsMin.x, position.x);
					nodeRotationBoundsMin.y = std::min(nodeRotationBoundsMin.y, position.y);
					nodeRotationBoundsMin.z = std::min(nodeRotationBoundsMin.z, position.z);
					nodeRotationBoundsMax.x = std::max(nodeRotationBoundsMax.x, position.x);
					nodeRotationBoundsMax.y = std::max(nodeRotationBoundsMax.y, position.y);
					nodeRotationBoundsMax.z = std::max(nodeRotationBoundsMax.z, position.z);
				}

				const aiVector3D texcoord = sourceMesh->HasTextureCoords(0)
					? sourceMesh->mTextureCoords[0][vertexIndex]
					: aiVector3D();
				const aiVector3D normal = normalTransform *
					(sourceMesh->HasNormals() ? sourceMesh->mNormals[vertexIndex]
											  : aiVector3D(0.0f, 1.0f, 0.0f));
				const aiVector3D tangent = linearTransform *
					(sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mTangents[vertexIndex]
															: aiVector3D(1.0f, 0.0f, 0.0f));
				const aiVector3D bitangent = linearTransform *
					(sourceMesh->HasTangentsAndBitangents() ? sourceMesh->mBitangents[vertexIndex]
															: aiVector3D(0.0f, 0.0f, 1.0f));
				const float handedness = normal.x * (tangent.y * bitangent.z - tangent.z * bitangent.y) +
					normal.y * (tangent.z * bitangent.x - tangent.x * bitangent.z) +
					normal.z * (tangent.x * bitangent.y - tangent.y * bitangent.x);
				const float tangentSign = handedness < 0 ? -1.0f : 1.0f;

				vertices.push_back({normalization.Transform(position), {texcoord.x, texcoord.y},
					{normal.x, normal.y, normal.z}, {tangent.x, tangent.y, tangent.z, tangentSign}});
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

			result.isRotatingNode = isRotatingNode;
			if (isRotatingNode) {
				const float extentX = nodeRotationBoundsMax.x - nodeRotationBoundsMin.x;
				const float extentY = nodeRotationBoundsMax.y - nodeRotationBoundsMin.y;
				const float extentZ = nodeRotationBoundsMax.z - nodeRotationBoundsMin.z;
				result.nodeRotationPivot =
					normalization.Transform((nodeRotationBoundsMin + nodeRotationBoundsMax) * 0.5f);
				if (extentX <= extentY && extentX <= extentZ) {
					result.nodeRotationAxis = {1.0f, 0.0f, 0.0f};
				} else if (extentY <= extentZ) {
					result.nodeRotationAxis = {0.0f, 1.0f, 0.0f};
				}
			}
			return result;
		}

		MeshGeometry SimplifyGeometry(const MeshGeometry& source, float extent, unsigned lod) {
			if (lod == 0 || source.isRotatingNode)
				return source;
			auto low = source.vertices.front().position, high = low;
			for (const auto& v : source.vertices) {
				low.x = std::min(low.x, v.position.x);
				low.y = std::min(low.y, v.position.y);
				low.z = std::min(low.z, v.position.z);
				high.x = std::max(high.x, v.position.x);
				high.y = std::max(high.y, v.position.y);
				high.z = std::max(high.z, v.position.z);
			}
			const float componentSize = std::max({high.x - low.x, high.y - low.y, high.z - low.z});
			if (componentSize < extent * (lod == 1 ? 0.006f : 0.018f))
				return {};
			struct Key {
				std::array<int, 7> values;
				bool operator==(const Key&) const = default;
			};
			struct Hash {
				size_t operator()(const Key& key) const {
					size_t hash = 14695981039346656037ULL;
					for (int value : key.values) {
						hash ^= static_cast<unsigned>(value);
						hash *= 1099511628211ULL;
					}
					return hash;
				}
			};
			const float cell = extent / (lod == 1 ? 128.0f : 48.0f);
			MeshGeometry result;
			result.source = source.source;
			std::unordered_map<Key, uint32_t, Hash> clusters;
			std::vector<uint32_t> remap(source.vertices.size()), counts;
			for (size_t i = 0; i < source.vertices.size(); ++i) {
				const auto& v = source.vertices[i];
				const float n[] = {v.normal.x, v.normal.y, v.normal.z};
				int axis = 0;
				if (std::abs(n[1]) > std::abs(n[axis]))
					axis = 1;
				if (std::abs(n[2]) > std::abs(n[axis]))
					axis = 2;
				const Key key{{int(std::floor(v.position.x / cell)), int(std::floor(v.position.y / cell)),
					int(std::floor(v.position.z / cell)),
					int(std::floor(std::clamp(v.texcoord.x, -1e6f, 1e6f) * 32)),
					int(std::floor(std::clamp(v.texcoord.y, -1e6f, 1e6f) * 32)),
					axis * 2 + (n[axis] < 0 ? 1 : 0), v.tangent.w < 0 ? 1 : 0}};
				const auto [entry, inserted] =
					clusters.emplace(key, static_cast<uint32_t>(result.vertices.size()));
				remap[i] = entry->second;
				if (inserted) {
					result.vertices.push_back(v);
					counts.push_back(1);
				} else {
					auto& target = result.vertices[entry->second];
					target.position.x += v.position.x;
					target.position.y += v.position.y;
					target.position.z += v.position.z;
					target.texcoord.x += v.texcoord.x;
					target.texcoord.y += v.texcoord.y;
					++counts[entry->second];
				}
			}
			for (size_t i = 0; i < result.vertices.size(); ++i) {
				const float scale = 1.0f / counts[i];
				auto& v = result.vertices[i];
				v.position.x *= scale;
				v.position.y *= scale;
				v.position.z *= scale;
				v.texcoord.x *= scale;
				v.texcoord.y *= scale;
			}
			for (size_t i = 0; i < source.indices.size(); i += 3) {
				const auto a = remap[source.indices[i]], b = remap[source.indices[i + 1]],
						   c = remap[source.indices[i + 2]];
				if (a != b && b != c && a != c)
					result.indices.insert(result.indices.end(), {a, b, c});
			}
			// Discard vertices no longer referenced by surviving triangles.
			std::vector<ModelVertex> compact;
			std::vector<uint32_t> used(result.vertices.size(), UINT32_MAX);
			for (auto& index : result.indices) {
				if (used[index] == UINT32_MAX) {
					used[index] = static_cast<uint32_t>(compact.size());
					compact.push_back(result.vertices[index]);
				}
				index = used[index];
			}
			result.vertices = std::move(compact);
			return result;
		}

		void CollectMeshInstances(const aiScene* scene, const aiNode* node,
			const aiMatrix4x4& parentTransform, bool parentIsNodeRotation,
			const std::string& rotatingNodeName, std::vector<MeshInstance>& instances) {
			const aiMatrix4x4 transform = parentTransform * node->mTransformation;
			const bool isRotatingNode = parentIsNodeRotation ||
				(!rotatingNodeName.empty() && node->mName.C_Str() == rotatingNodeName);
			for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
				const aiMesh* mesh = scene->mMeshes[node->mMeshes[meshIndex]];
				instances.push_back({mesh, transform,
					isRotatingNode ||
						(!rotatingNodeName.empty() && mesh->mName.C_Str() == rotatingNodeName)});
			}

			for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
				CollectMeshInstances(scene, node->mChildren[childIndex], transform, isRotatingNode,
					rotatingNodeName, instances);
			}
		}

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

		std::filesystem::path ModelSceneCachePath(const std::filesystem::path& source, unsigned flags) {
			auto extension = source.extension().wstring();
			std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
			// FBX contains its geometry and material definitions; OBJ can depend on an external MTL.
			if (extension != L".fbx")
				return {};
			std::error_code error;
			const auto canonical = std::filesystem::weakly_canonical(source, error);
			if (error)
				return {};
			const auto size = std::filesystem::file_size(source, error);
			if (error)
				return {};
			const auto modified = std::filesystem::last_write_time(source, error);
			if (error)
				return {};
			const auto pathText = canonical.u8string();
			const auto key = std::string(reinterpret_cast<const char*>(pathText.data()), pathText.size()) +
				"|scene-v2-static|" + std::to_string(size) + "|" +
				std::to_string(modified.time_since_epoch().count()) + "|" + std::to_string(flags) + "|" +
				std::to_string(aiGetVersionMajor()) + "." + std::to_string(aiGetVersionMinor()) + "." +
				std::to_string(aiGetVersionRevision());
			uint64_t hash = 14695981039346656037ULL;
			for (unsigned char value : key) {
				hash ^= value;
				hash *= 1099511628211ULL;
			}
			const auto temp = std::filesystem::temp_directory_path(error);
			return error ? std::filesystem::path{}
						 : temp / L"SkyMissionModelCache" / (std::to_wstring(hash) + L".assbin");
		}

		const aiScene* ImportScene(Assimp::Importer& importer, const std::string& importerPath) {
			// Animation is driven by FlightScene/node rotation; imported animation tracks are never used.
			importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_ANIMATIONS, false);
			importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_CAMERAS, false);
			importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_READ_LIGHTS, false);
			constexpr unsigned flags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
				aiProcess_ImproveCacheLocality | aiProcess_ConvertToLeftHanded | aiProcess_GenSmoothNormals |
				aiProcess_CalcTangentSpace;
			const auto source = std::filesystem::path(
				std::u8string(reinterpret_cast<const char8_t*>(importerPath.data()), importerPath.size()));
			const auto cache = ModelSceneCachePath(source, flags);
			const auto utf8 = [](const std::filesystem::path& path) {
				const auto text = path.u8string();
				return std::string(reinterpret_cast<const char*>(text.data()), text.size());
			};
			std::error_code error;
			if (!cache.empty() && std::filesystem::is_regular_file(cache, error)) {
				const auto* cached = importer.ReadFile(utf8(cache), 0);
				if (cached && cached->HasMeshes() && cached->mRootNode) {
					Log("Model scene cache hit: {}\n", importerPath);
					return cached;
				}
			}
			const aiScene* scene = importer.ReadFile(importerPath, flags);
			if (!scene || !scene->HasMeshes() || !scene->mRootNode) {
				Log("{}", importer.GetErrorString());
				Log("\n");
				return nullptr;
			}
			if (!cache.empty()) {
				std::filesystem::create_directories(cache.parent_path(), error);
				if (!error) {
					auto temporary = cache;
					temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
					Assimp::Exporter exporter;
					if (exporter.Export(scene, "assbin", utf8(temporary)) == AI_SUCCESS)
						MoveFileExW(temporary.c_str(), cache.c_str(), MOVEFILE_REPLACE_EXISTING);
					std::filesystem::remove(temporary, error);
				}
			}

			return scene;
		}

		void CollectNodePositions(const aiNode& node, const aiMatrix4x4& parent,
			const ModelNormalization& normalization,
			std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>>& positions) {
			const auto transform = parent * node.mTransformation;
			positions[node.mName.C_Str()].push_back(normalization.Transform(transform * aiVector3D()));
			for (unsigned i = 0; i < node.mNumChildren; ++i)
				CollectNodePositions(*node.mChildren[i], transform, normalization, positions);
		}

		std::vector<MeshGeometry> BuildModelGeometry(const aiScene& scene, const ModelLoadOptions& options,
			std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>>& positions) {
			std::vector<MeshInstance> meshInstances;
			CollectMeshInstances(
				&scene, scene.mRootNode, aiMatrix4x4(), false, options.rotatingNodeName, meshInstances);
			if (meshInstances.empty()) {
				Log("Model::Load - model has no node mesh instances\n");
				return {};
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
				return {};
			}
			const ModelNormalization normalization{boundsMin, boundsMax, options.normalizedExtent / extent,
				options.groundOffset, options.origin};
			CollectNodePositions(*scene.mRootNode, aiMatrix4x4(), normalization, positions);
			std::vector<MeshGeometry> geometries;
			for (const auto& instance : meshInstances) {
				auto geometry = BuildMeshGeometry(instance, normalization);
				if (!geometry.indices.empty())
					geometries.push_back(std::move(geometry));
			}

			if (geometries.empty())
				return {};

			return geometries;
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
		const auto utf8Path = modelPath.u8string();
		const std::string importerPath(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

		Assimp::Importer importer;
		const aiScene* scene = ImportScene(importer, importerPath);
		if (!scene)
			return false;
		auto geometries = BuildModelGeometry(*scene, options, m_nodePositions);
		m_sourceMeshCount = geometries.size();
		if (geometries.empty())
			return false;
		Bounds3 bounds{
			geometries.front().vertices.front().position, geometries.front().vertices.front().position};
		for (const auto& geometry : geometries)
			for (const auto& vertex : geometry.vertices) {
				bounds.minimum.x = std::min(bounds.minimum.x, vertex.position.x);
				bounds.minimum.y = std::min(bounds.minimum.y, vertex.position.y);
				bounds.minimum.z = std::min(bounds.minimum.z, vertex.position.z);
				bounds.maximum.x = std::max(bounds.maximum.x, vertex.position.x);
				bounds.maximum.y = std::max(bounds.maximum.y, vertex.position.y);
				bounds.maximum.z = std::max(bounds.maximum.z, vertex.position.z);
			}
		m_localBounds = bounds;

		const auto center = bounds.Center();
		const auto distance = [](const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) {
			return std::sqrt(
				(a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
		};
		for (const auto& geometry : geometries)
			for (const auto& vertex : geometry.vertices)
				m_cullingRadius = std::max(m_cullingRadius,
					geometry.isRotatingNode ? distance(center, geometry.nodeRotationPivot) +
							distance(vertex.position, geometry.nodeRotationPivot)
											: distance(center, vertex.position));
		InitializeFallbackTextures(device, commandList);
		const auto modelDirectory = modelPath.parent_path();
		struct Batch {
			Mesh mesh;
			std::vector<ModelVertex> vertices;
			std::vector<uint32_t> indices;
		};
		std::array<std::vector<Batch>, 3> batches;
		std::vector<TextureDescriptorSet> textureDescriptorSets;
		std::unordered_map<unsigned, Mesh> materials;
		const float extent = std::max({bounds.maximum.x - bounds.minimum.x,
			bounds.maximum.y - bounds.minimum.y, bounds.maximum.z - bounds.minimum.z});
		for (auto& geometry : geometries) {
			const auto materialIndex = geometry.source->mMaterialIndex;
			auto [material, inserted] = materials.try_emplace(materialIndex);
			if (inserted) {
				if (materialIndex < scene->mNumMaterials)
					LoadMaterialTextures(material->second, scene->mMaterials[materialIndex], modelDirectory,
						options, device, commandList);
				AssignMaterialDescriptors(material->second, device, renderDevice, textureDescriptorSets);
			}
			for (unsigned lod = 0; lod < m_lodCount; ++lod) {
				// Full detail is moved last, so simplification can still read its original topology.
				const unsigned level = m_lodCount - 1 - lod;
				auto reduced = level == 0 ? MeshGeometry{} : SimplifyGeometry(geometry, extent, level);
				auto& data = level == 0 ? geometry : reduced;
				if (data.indices.empty())
					continue;
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
			if (m_lodMeshes[level].empty() && level > 0)
				m_lodMeshes[level] = m_lodMeshes[level - 1];
		}
		if (!IsLoaded())
			return false;
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
		if (found == m_nodePositions.end() || found->second.size() != 1)
			return std::nullopt;
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

	void Model::LoadMaterialTextures(Mesh& mesh, const aiMaterial* material,
		const std::filesystem::path& modelDirectory, const ModelLoadOptions& options, ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList) {
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
			if (path.empty())
				return std::shared_ptr<Texture>{};
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
