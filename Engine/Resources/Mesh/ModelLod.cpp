#include "ModelGeometry.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace Engine::ModelProcessing {
	MeshGeometry SimplifyGeometry(const MeshGeometry& source, float extent, unsigned lod) {
		if (lod == 0 || source.isRotatingNode) return source;
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
		if (componentSize < extent * (lod == 1 ? 0.006f : 0.018f)) return {};
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
		result.materialIndex = source.materialIndex;
		std::unordered_map<Key, uint32_t, Hash> clusters;
		clusters.reserve(source.vertices.size());
		result.vertices.reserve(source.vertices.size());
		result.indices.reserve(source.indices.size());
		std::vector<uint32_t> remap(source.vertices.size()), counts;
		counts.reserve(source.vertices.size());
		for (size_t i = 0; i < source.vertices.size(); ++i) {
			const auto& v = source.vertices[i];
			const float n[] = {v.normal.x, v.normal.y, v.normal.z};
			int axis = 0;
			if (std::abs(n[1]) > std::abs(n[axis])) axis = 1;
			if (std::abs(n[2]) > std::abs(n[axis])) axis = 2;
			const Key key{{int(std::floor(v.position.x / cell)), int(std::floor(v.position.y / cell)),
				int(std::floor(v.position.z / cell)),
				int(std::floor(std::clamp(v.texcoord.x, -1e6f, 1e6f) * 32)),
				int(std::floor(std::clamp(v.texcoord.y, -1e6f, 1e6f) * 32)), axis * 2 + (n[axis] < 0 ? 1 : 0),
				v.tangent.w < 0 ? 1 : 0}};
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
			if (a != b && b != c && a != c) result.indices.insert(result.indices.end(), {a, b, c});
		}
		// Discard vertices no longer referenced by surviving triangles.
		std::vector<ModelVertex> compact;
		compact.reserve(result.vertices.size());
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

} // namespace Engine::ModelProcessing
