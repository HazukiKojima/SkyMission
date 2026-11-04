#include "ModelGeometry.h"
#include "Engine/Log.h"
#include <assimp/scene.h>
#include <assimp/matrix3x3.h>
#include <algorithm>
#include <climits>
#include <cmath>
#include <limits>
namespace Engine::ModelProcessing {
	namespace {
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
		MeshGeometry BuildMeshGeometry(
			const MeshInstance& instance, const ModelNormalization& normalization) {
			MeshGeometry result;
			const aiMesh* sourceMesh = instance.mesh;
			if (!sourceMesh->HasPositions() || !sourceMesh->HasFaces())
				return result;
			result.materialIndex = sourceMesh->mMaterialIndex;
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

		void CollectNodePositions(const aiNode& node, const aiMatrix4x4& parent,
			const ModelNormalization& normalization,
			std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>>& positions) {
			const auto transform = parent * node.mTransformation;
			positions[node.mName.C_Str()].push_back(normalization.Transform(transform * aiVector3D()));
			for (unsigned i = 0; i < node.mNumChildren; ++i)
				CollectNodePositions(*node.mChildren[i], transform, normalization, positions);
		}

	} // namespace
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
		const ModelNormalization normalization{
			boundsMin, boundsMax, options.normalizedExtent / extent, options.groundOffset, options.origin};
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
} // namespace Engine::ModelProcessing
