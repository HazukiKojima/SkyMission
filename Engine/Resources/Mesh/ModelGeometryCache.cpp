#include "ModelGeometryCache.h"
#include "Engine/Log.h"
#include "Engine/StartupTiming.h"
#include "ModelImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include <Windows.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cwctype>
#include <fstream>
#include <stdexcept>
#include <type_traits>

namespace Engine::ModelProcessing {
	namespace {
		constexpr uint64_t Magic =
			0x534d47454f4d3032ULL; // Change when import/geometry/LOD processing changes.

		std::filesystem::path CachePath(
			const std::filesystem::path& source, const ModelLoadOptions& options) {
			auto extension = source.extension().wstring();
			std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
			// Other formats can depend on external geometry/material definitions.
			if (extension != L".fbx") return {};
			std::error_code error;
			auto canonical = std::filesystem::weakly_canonical(source, error);
			if (error) {
				canonical = std::filesystem::absolute(source, error).lexically_normal();
				if (error) return {};
			}
			const auto size = std::filesystem::file_size(source, error);
			if (error) return {};
			const auto modified = std::filesystem::last_write_time(source, error);
			if (error) return {};
			const auto text = canonical.u8string();
			std::string key(reinterpret_cast<const char*>(text.data()), text.size());
			key += "|" + std::to_string(Magic) + "|" + std::to_string(size) + "|" +
				std::to_string(modified.time_since_epoch().count()) + "|" + options.rotatingNodeName + "|" +
				std::to_string(std::bit_cast<uint32_t>(options.normalizedExtent)) + "|" +
				std::to_string(std::bit_cast<uint32_t>(options.groundOffset)) + "|" +
				std::to_string(static_cast<unsigned>(options.origin)) + "|" +
				std::to_string(options.generateLods) + "|" + std::to_string(aiGetVersionMajor()) + "." +
				std::to_string(aiGetVersionMinor()) + "." + std::to_string(aiGetVersionRevision());
			uint64_t hash = 14695981039346656037ULL;
			for (unsigned char value : key) {
				hash ^= value;
				hash *= 1099511628211ULL;
			}
			const auto temporary = std::filesystem::temp_directory_path(error);
			return error ? std::filesystem::path{}
						 : temporary / L"SkyMissionModelCache" / (std::to_wstring(hash) + L".geometry");
		}

		class Reader {
		  public:
			explicit Reader(const std::filesystem::path& path)
				: input(path, std::ios::binary | std::ios::ate) {
				const auto size = input.tellg();
				if (size < 0 || size > 512LL * 1024 * 1024)
					throw std::runtime_error("Invalid geometry cache size");
				remaining = static_cast<uint64_t>(size);
				input.seekg(0);
			}
			template <class T> T Value() {
				static_assert(std::is_trivially_copyable_v<T>);
				T value{};
				Read(&value, sizeof(T));
				return value;
			}
			template <class T> std::vector<T> Vector() {
				static_assert(std::is_trivially_copyable_v<T>);
				const auto count = Value<uint64_t>();
				if (count > remaining / sizeof(T)) throw std::runtime_error("Truncated geometry cache");
				std::vector<T> values(static_cast<size_t>(count));
				if (count) Read(values.data(), count * sizeof(T));
				return values;
			}
			void Finish() const {
				if (remaining) throw std::runtime_error("Unexpected geometry cache data");
			}

		  private:
			void Read(void* data, uint64_t size) {
				if (size > remaining ||
					!input.read(static_cast<char*>(data), static_cast<std::streamsize>(size)))
					throw std::runtime_error("Truncated geometry cache");
				remaining -= size;
			}
			std::ifstream input;
			uint64_t remaining = 0;
		};

		PreparedModelGeometry ReadCache(const std::filesystem::path& path, unsigned lodCount) {
			Reader input(path);
			if (input.Value<uint64_t>() != Magic || input.Value<unsigned>() != lodCount)
				throw std::runtime_error("Outdated geometry cache");
			PreparedModelGeometry data;
			data.bounds = input.Value<Bounds3>();
			data.cullingRadius = input.Value<float>();
			if (!std::isfinite(data.cullingRadius) || data.cullingRadius < 0)
				throw std::runtime_error("Invalid cached bounds");
			const auto materials = input.Value<uint32_t>();
			if (materials > 1000000) throw std::runtime_error("Invalid cached material count");
			for (uint32_t material = 0; material < materials; ++material) {
				PreparedMaterial source;
				const auto name = input.Vector<char>();
				source.name.assign(name.begin(), name.end());
				for (auto& texture : source.textures) {
					const auto reference = input.Vector<char>();
					texture.assign(reference.begin(), reference.end());
				}
				data.materials.push_back(std::move(source));
			}
			const auto nodes = input.Value<uint32_t>();
			if (nodes > 1000000) throw std::runtime_error("Invalid cached node count");
			for (uint32_t node = 0; node < nodes; ++node) {
				const auto name = input.Vector<char>();
				const auto positions = input.Vector<DirectX::XMFLOAT3>();
				data.nodePositions.emplace(std::string(name.begin(), name.end()), positions);
			}
			for (unsigned level = 0; level < lodCount; ++level) {
				const auto count = input.Value<uint32_t>();
				if (!count || count > 1000000 || (level && count != data.levels[0].size()))
					throw std::runtime_error("Invalid cached mesh count");
				for (uint32_t mesh = 0; mesh < count; ++mesh) {
					MeshGeometry geometry;
					geometry.materialIndex = input.Value<unsigned>();
					geometry.isRotatingNode = input.Value<uint8_t>() != 0;
					geometry.nodeRotationPivot = input.Value<DirectX::XMFLOAT3>();
					geometry.nodeRotationAxis = input.Value<DirectX::XMFLOAT3>();
					geometry.vertices = input.Vector<ModelVertex>();
					geometry.indices = input.Vector<uint32_t>();
					if ((!level && geometry.indices.empty()) || geometry.indices.size() % 3 ||
						std::any_of(geometry.indices.begin(), geometry.indices.end(), [&](uint32_t index) {
							return index >= geometry.vertices.size();
						}))
						throw std::runtime_error("Invalid cached topology");
					data.levels[level].push_back(std::move(geometry));
				}
			}
			input.Finish();
			data.loadedFromCache = true;
			return data;
		}

		template <class T> void WriteValue(std::ostream& output, const T& value) {
			static_assert(std::is_trivially_copyable_v<T>);
			output.write(reinterpret_cast<const char*>(&value), sizeof(T));
		}
		template <class T> void WriteVector(std::ostream& output, const std::vector<T>& values) {
			static_assert(std::is_trivially_copyable_v<T>);
			WriteValue(output, static_cast<uint64_t>(values.size()));
			if (!values.empty())
				output.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(T));
		}
		void SaveCache(
			const std::filesystem::path& path, const PreparedModelGeometry& data, unsigned lodCount) {
			if (path.empty()) return;
			std::error_code error;
			std::filesystem::create_directories(path.parent_path(), error);
			if (error) return;
			auto temporary = path;
			temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L"." +
				std::to_wstring(GetCurrentThreadId()) + L".tmp";
			{
				std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
				WriteValue(output, Magic);
				WriteValue(output, lodCount);
				WriteValue(output, data.bounds);
				WriteValue(output, data.cullingRadius);
				WriteValue(output, static_cast<uint32_t>(data.materials.size()));
				for (const auto& material : data.materials) {
					WriteVector(output, std::vector<char>(material.name.begin(), material.name.end()));
					for (const auto& texture : material.textures)
						WriteVector(output, std::vector<char>(texture.begin(), texture.end()));
				}
				WriteValue(output, static_cast<uint32_t>(data.nodePositions.size()));
				for (const auto& [name, positions] : data.nodePositions) {
					WriteVector(output, std::vector<char>(name.begin(), name.end()));
					WriteVector(output, positions);
				}
				for (unsigned level = 0; level < lodCount; ++level) {
					WriteValue(output, static_cast<uint32_t>(data.levels[level].size()));
					for (const auto& geometry : data.levels[level]) {
						WriteValue(output, geometry.materialIndex);
						WriteValue(output, static_cast<uint8_t>(geometry.isRotatingNode));
						WriteValue(output, geometry.nodeRotationPivot);
						WriteValue(output, geometry.nodeRotationAxis);
						WriteVector(output, geometry.vertices);
						WriteVector(output, geometry.indices);
					}
				}
				output.close();
				if (output) MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
			}
			std::filesystem::remove(temporary, error);
		}
	} // namespace

	PreparedModelGeometry PrepareModelGeometry(
		const std::filesystem::path& source, const ModelLoadOptions& options) {
		StartupTiming timing("model.prepared_cache");
		const auto cache = CachePath(source, options);
		const unsigned lodCount = options.generateLods ? 3 : 1;
		if (!cache.empty()) {
			try {
				auto data = ReadCache(cache, lodCount);
				Log(L"Prepared geometry cache hit: {}\n", source.wstring());
				return data;
			} catch (const std::runtime_error& error) {
				const std::string reason = std::string("model.cache_miss: ") + error.what();
				StartupTiming diagnostic(reason);
				// Missing, old or interrupted cache writes fall back to source processing.
			}
		}
		PreparedModelGeometry data;
		timing.Mark("model.import");
		const auto text = source.u8string();
		Assimp::Importer importer;
		const auto* scene = ModelImport::ImportScene(
			importer, std::string(reinterpret_cast<const char*>(text.data()), text.size()));
		if (!scene) return data;
		timing.Mark("model.geometry_and_lods");
		constexpr aiTextureType types[] = {aiTextureType_BASE_COLOR, aiTextureType_DIFFUSE,
			aiTextureType_METALNESS, aiTextureType_DIFFUSE_ROUGHNESS, aiTextureType_NORMALS};
		for (unsigned i = 0; i < scene->mNumMaterials; ++i) {
			PreparedMaterial material;
			aiString name;
			scene->mMaterials[i]->Get(AI_MATKEY_NAME, name);
			material.name = name.C_Str();
			for (size_t type = 0; type < std::size(types); ++type) {
				aiString texture;
				if (scene->mMaterials[i]->GetTexture(types[type], 0, &texture) == AI_SUCCESS)
					material.textures[type] = texture.C_Str();
			}
			data.materials.push_back(std::move(material));
		}
		data.levels[0] = BuildModelGeometry(*scene, options, data.nodePositions);
		if (data.levels[0].empty()) return data;
		data.bounds = CalculateModelBounds(data.levels[0]);
		data.cullingRadius = CalculateCullingRadius(data.levels[0], data.bounds);
		const float extent = std::max({data.bounds.maximum.x - data.bounds.minimum.x,
			data.bounds.maximum.y - data.bounds.minimum.y, data.bounds.maximum.z - data.bounds.minimum.z});
		for (unsigned level = 1; level < lodCount; ++level) {
			data.levels[level].reserve(data.levels[0].size());
			for (const auto& geometry : data.levels[0])
				data.levels[level].push_back(SimplifyGeometry(geometry, extent, level));
		}
		SaveCache(cache, data, lodCount);
		return data;
	}
} // namespace Engine::ModelProcessing
