#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace Engine {
	class RedoxDocument;
	class JsonFileCache;
	class JsonDocumentCache;

	class JsonAssetReader {
	  public:
		JsonAssetReader() = default;
		explicit JsonAssetReader(JsonFileCache& files) : m_files(&files) {}
		explicit JsonAssetReader(JsonDocumentCache& documents) : m_documents(&documents) {}

		std::string ResolveString(const std::filesystem::path& jsonPath, const std::string& key) const;
		std::filesystem::path Resolve(const std::filesystem::path& jsonPath, const std::string& key) const;
		float ResolveFloat(const std::filesystem::path& jsonPath, const std::string& key) const;
		std::vector<float> ResolveFloatArray(
			const std::filesystem::path& jsonPath, const std::string& key) const;
		// 未指定のフィールドだけnulloptとし、型不一致や壊れたJSONは例外にする
		std::optional<std::vector<float>> TryResolveFloatArray(
			const std::filesystem::path& jsonPath, const std::string& key) const;
		std::optional<std::string> TryResolveString(
			const std::filesystem::path& jsonPath, const std::string& key) const;
		// keyはルート項目名、または /briefing/0/lines のようなJSON Pointer。
		std::optional<size_t> TryResolveArrayLength(
			const std::filesystem::path& jsonPath, const std::string& key) const;

	  private:
		const RedoxDocument& AcquireDocument(
			const std::filesystem::path& path, RedoxDocument& transient) const;
		JsonFileCache* m_files = nullptr;
		JsonDocumentCache* m_documents = nullptr;
	};
} // namespace Engine
