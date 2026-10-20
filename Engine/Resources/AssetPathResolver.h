#pragma once

#include <filesystem>
#include <string>

namespace Engine {
	class AssetPathResolver {
	public:
		std::filesystem::path Resolve(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
	};
}
