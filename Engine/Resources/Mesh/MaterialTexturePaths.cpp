#include "MaterialTexturePaths.h"
#include <algorithm>
#include <cctype>
#include <cwctype>

namespace Engine::MaterialTexturePaths {

	std::filesystem::path FindTexturePath(const std::filesystem::path& modelDirectory,
		const std::filesystem::path& texturePath, const ModelLoadOptions& options) {
		const std::filesystem::path texturesDirectory = modelDirectory / options.textureDirectory;
		const std::filesystem::path candidates[] = {
			texturePath.is_absolute() ? texturePath : modelDirectory / texturePath,
			texturesDirectory / texturePath, texturesDirectory / texturePath.filename()};

		for (const auto& candidate : candidates) {
			if (std::filesystem::exists(candidate)) {
				return candidate;
			}
		}

		return {};
	}

	std::filesystem::path FindFallbackTexturePath(const std::filesystem::path& modelDirectory,
		const std::string& materialName, const ModelLoadOptions& options) {
		const std::filesystem::path texturesDirectory = modelDirectory / options.textureDirectory;
		const std::filesystem::path fallbackPath = !options.fallbackMaterialToken.empty() &&
				materialName.find(options.fallbackMaterialToken) != std::string::npos
			? texturesDirectory / options.matchingFallbackTexture
			: texturesDirectory / options.defaultFallbackTexture;
		return std::filesystem::is_regular_file(fallbackPath) ? fallbackPath : std::filesystem::path();
	}

	std::filesystem::path FindNamedMaterialTexturePath(const std::filesystem::path& modelDirectory,
		const std::string& materialName, const std::filesystem::path& baseColorPath,
		const std::wstring& suffix, const ModelLoadOptions& options) {
		if (suffix.empty())
			return {};
		const std::filesystem::path texturesDirectory = modelDirectory / options.textureDirectory;
		const std::wstring& baseColorSuffix = options.baseColorStemSuffix;
		std::wstring baseName = baseColorPath.stem().wstring();
		std::wstring baseNameLower = baseName;
		std::transform(baseNameLower.begin(), baseNameLower.end(), baseNameLower.begin(), towlower);
		std::wstring baseSuffixLower = baseColorSuffix;
		std::transform(baseSuffixLower.begin(), baseSuffixLower.end(), baseSuffixLower.begin(), towlower);
		const size_t baseColorPosition = baseNameLower.find(baseSuffixLower);
		if (!baseColorSuffix.empty() && baseColorPosition != std::wstring::npos) {
			const auto candidate = texturesDirectory / (baseName.substr(0, baseColorPosition) + suffix);
			if (std::filesystem::exists(candidate)) {
				return candidate;
			}
		}

		std::string materialNameLower = materialName;
		std::transform(materialNameLower.begin(), materialNameLower.end(), materialNameLower.begin(),
			[](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(texturesDirectory, error)) {
			if (error || !entry.is_regular_file()) {
				continue;
			}
			std::wstring filename = entry.path().filename().wstring();
			std::wstring filenameLower = filename;
			std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), towlower);
			std::wstring suffixLower = suffix;
			std::transform(suffixLower.begin(), suffixLower.end(), suffixLower.begin(), towlower);
			if (filenameLower.size() < suffixLower.size() ||
				filenameLower.substr(filenameLower.size() - suffixLower.size()) != suffixLower) {
				continue;
			}

			const std::wstring prefix = filename.substr(0, filename.size() - suffix.size());
			const auto utf8 = std::filesystem::path(prefix).u8string();
			const std::string prefixUtf8(reinterpret_cast<const char*>(utf8.data()), utf8.size());
			std::string prefixLower = prefixUtf8;
			std::transform(prefixLower.begin(), prefixLower.end(), prefixLower.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			if (prefixLower == materialNameLower) {
				return entry.path();
			}
		}

		return {};
	}
} // namespace Engine::MaterialTexturePaths
