#pragma once
#include "Engine/Resources/Mesh/ModelGeometryCache.h"

void TestPreparedModelCache(const fs::path& repository, const fs::path& fixture) {
	using namespace Engine::ModelProcessing;
	const auto source = fixture / (L"prepared-model-" + std::to_wstring(GetTickCount64()) + L".fbx");
	fs::copy_file(repository / L"Assets/Aircraft/N1K2JShidenKai/N1K2JShidenKai.fbx", source);
	const auto directory = fs::temp_directory_path() / L"SkyMissionModelCache";
	const auto files = [&] {
		std::vector<fs::path> paths;
		std::error_code error;
		for (const auto& entry : fs::directory_iterator(directory, error))
			if (entry.path().extension() == L".geometry") paths.push_back(entry.path());
		return paths;
	};
	const auto previous = files();
	ModelLoadOptions options;
	options.rotatingNodeName = "Propeller";
	options.generateLods = true;
	const auto first = PrepareModelGeometry(source, options);
	const auto second = PrepareModelGeometry(source, options);
	Require(!first.loadedFromCache && second.loadedFromCache && !first.levels[0].empty(),
		"Prepared model cache did not bypass source processing on reload");
	Require(first.cullingRadius == second.cullingRadius &&
			std::memcmp(&first.bounds, &second.bounds, sizeof(Bounds3)) == 0 &&
			first.nodePositions.size() == second.nodePositions.size() &&
			first.materials.size() == second.materials.size(),
		"Prepared cache changed bounds, radius, nodes or material count");
	for (const auto& [name, positions] : first.nodePositions) {
		const auto& cached = second.nodePositions.at(name);
		Require(positions.size() == cached.size() &&
				std::memcmp(positions.data(), cached.data(), positions.size() * sizeof(DirectX::XMFLOAT3)) ==
					0,
			"Prepared cache changed muzzle/node positions");
	}
	for (size_t i = 0; i < first.materials.size(); ++i)
		Require(first.materials[i].name == second.materials[i].name &&
				first.materials[i].textures == second.materials[i].textures,
			"Prepared cache changed material names or texture references");
	for (unsigned lod = 0; lod < 3; ++lod) {
		Require(
			first.levels[lod].size() == second.levels[lod].size(), "Prepared cache changed LOD mesh count");
		for (size_t mesh = 0; mesh < first.levels[lod].size(); ++mesh) {
			const auto& a = first.levels[lod][mesh];
			const auto& b = second.levels[lod][mesh];
			Require(a.materialIndex == b.materialIndex && a.isRotatingNode == b.isRotatingNode &&
					a.indices == b.indices && a.vertices.size() == b.vertices.size() &&
					(a.vertices.empty() ||
						std::memcmp(a.vertices.data(), b.vertices.data(),
							a.vertices.size() * sizeof(ModelVertex)) == 0) &&
					std::memcmp(&a.nodeRotationPivot, &b.nodeRotationPivot, sizeof(a.nodeRotationPivot)) ==
						0 &&
					std::memcmp(&a.nodeRotationAxis, &b.nodeRotationAxis, sizeof(a.nodeRotationAxis)) == 0,
				"Prepared cache changed vertices, topology, material assignment or propeller animation");
		}
	}
	const auto current = files();
	const auto created = std::find_if(current.begin(), current.end(), [&](const auto& path) {
		return std::find(previous.begin(), previous.end(), path) == previous.end();
	});
	Require(created != current.end(), "Prepared geometry was not written to disk");
	std::ofstream(*created, std::ios::binary | std::ios::trunc) << "interrupted write";
	Require(!PrepareModelGeometry(source, options).loadedFromCache &&
			PrepareModelGeometry(source, options).loadedFromCache,
		"Corrupt prepared cache did not recover from the source");
	auto changed = options;
	changed.normalizedExtent *= 2;
	const auto scaled = PrepareModelGeometry(source, changed);
	Require(!scaled.loadedFromCache && std::abs(scaled.cullingRadius - first.cullingRadius * 2) < 0.001f,
		"Geometry cache ignored normalization settings");
	changed = options;
	changed.generateLods = false;
	const auto noLods = PrepareModelGeometry(source, changed);
	Require(!noLods.loadedFromCache && noLods.levels[1].empty(), "Geometry cache ignored LOD settings");
	changed = options;
	changed.rotatingNodeName = "no-such-node";
	Require(!PrepareModelGeometry(source, changed).loadedFromCache,
		"Geometry cache ignored rotating-node settings");
	fs::last_write_time(source, fs::last_write_time(source) + std::chrono::seconds(1));
	Require(!PrepareModelGeometry(source, options).loadedFromCache, "Geometry cache ignored source updates");
	fs::remove(source);
	Require(PrepareModelGeometry(source, options).levels[0].empty(),
		"Geometry cache hid a missing required model");
	std::cout << "PASS: prepared FBX/LOD cache, exact geometry/material/node reload, corruption recovery and "
				 "source/options invalidation\n";
}
