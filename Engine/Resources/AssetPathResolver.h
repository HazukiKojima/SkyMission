#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Engine {
	class JsonFileCache;
	class JsonDocumentCache;

	class AssetPathResolver {
	public:
		AssetPathResolver() = default;
		explicit AssetPathResolver(JsonFileCache& files) : m_files(&files) {}
		explicit AssetPathResolver(JsonDocumentCache& documents) : m_documents(&documents) {}

		std::filesystem::path Resolve(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
		float ResolveFloat(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;
		std::vector<float> ResolveFloatArray(
			const std::filesystem::path& jsonPath,
			const std::string& key) const;

	private:
		JsonFileCache* m_files = nullptr;
		JsonDocumentCache* m_documents = nullptr;
	};
}
