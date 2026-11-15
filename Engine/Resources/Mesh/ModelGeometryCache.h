#pragma once
#include "ModelGeometry.h"
#include <array>
#include <filesystem>

namespace Engine::ModelProcessing {
	// マテリアル名と元の参照だけ。テクスチャの探索とデコードは毎回行う
	struct PreparedMaterial {
		std::string name;
		std::array<std::string, 5> textures; // ベースカラー、ディフューズ、メタルネス、ラフネス、法線の順
	};
	struct PreparedModelGeometry {
		bool loadedFromCache = false;
		std::array<std::vector<MeshGeometry>, 3> levels;
		std::vector<PreparedMaterial> materials;
		std::unordered_map<std::string, std::vector<DirectX::XMFLOAT3>> nodePositions;
		Bounds3 bounds{};
		float cullingRadius = 0;
	};

	PreparedModelGeometry PrepareModelGeometry(
		const std::filesystem::path& source, const ModelLoadOptions& options);
} // namespace Engine::ModelProcessing
