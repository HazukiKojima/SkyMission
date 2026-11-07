#pragma once
#include "ModelGeometry.h"
#include <array>
#include <filesystem>

namespace Engine::ModelProcessing {
	// Material names and original references only; texture lookup/decoding still runs on every load.
	struct PreparedMaterial {
		std::string name;
		std::array<std::string, 5> textures; // base color, diffuse, metalness, roughness, normals
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
