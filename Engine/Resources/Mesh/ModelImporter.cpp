#include "ModelImporter.h"
#include "Engine/Log.h"
#include <Windows.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <algorithm>
#include <cwctype>
#include <filesystem>
namespace Engine::ModelImport {
	namespace {
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

	} // namespace
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

} // namespace Engine::ModelImport
