#pragma once
#include "ModelLoadOptions.h"
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
struct aiScene;
namespace Engine::ModelProcessing {
	struct ModelVertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT2 texcoord;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT4 tangent;
	};

	struct MeshGeometry {
		unsigned materialIndex = 0;
		std::vector<ModelVertex> vertices;
		std::vector<uint32_t> indices;
		bool isRotatingNode = false;
		DirectX::XMFLOAT3 nodeRotationPivot{};
		DirectX::XMFLOAT3 nodeRotationAxis{0.0f, 0.0f, 1.0f};
	};

	std::vector<MeshGeometry> BuildModelGeometry(const aiScene& scene, const ModelLoadOptions& options,
		std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>>& positions);
	MeshGeometry SimplifyGeometry(const MeshGeometry& source, float extent, unsigned lod);
} // namespace Engine::ModelProcessing
