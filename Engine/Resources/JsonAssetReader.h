#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Engine {
	class RedoxDocument;
	class JsonFileCache;
	class JsonDocumentCache;

	class JsonAssetReader {
	  public:
		JsonAssetReader() = default;
		explicit JsonAssetReader(JsonFileCache& files) : m_files(&files) {
		}
		explicit JsonAssetReader(JsonDocumentCache& documents) : m_documents(&documents) {
		}

		std::string ResolveString(const std::filesystem::path& jsonPath, const std::string& key) const;
		std::filesystem::path Resolve(const std::filesystem::path& jsonPath, const std::string& key) const;
		float ResolveFloat(const std::filesystem::path& jsonPath, const std::string& key) const;
		std::vector<float> ResolveFloatArray(const std::filesystem::path& jsonPath, const std::string& key) const;

	  private:
		const RedoxDocument& AcquireDocument(const std::filesystem::path& path, RedoxDocument& transient) const;
		JsonFileCache* m_files = nullptr;
		JsonDocumentCache* m_documents = nullptr;
	};
} // namespace Engine
