#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Engine {
	class AssetPathResolver {
	public:
		std::filesystem::path Resolve(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
		float ResolveFloat(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
		std::vector<float> ResolveFloatArray(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
	};
}
