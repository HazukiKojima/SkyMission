#pragma once
#include <filesystem>
#include <string>

namespace Engine {
	// アセットの命名規則はゲーム側の設定から渡す
	struct ModelLoadOptions {
		std::string rotatingNodeName;
		std::filesystem::path textureDirectory = L"Textures";
		std::wstring baseColorStemSuffix;
		std::wstring baseColorSuffix;
		std::wstring metallicSuffix;
		std::wstring roughnessSuffix;
		std::wstring normalSuffix;
		std::wstring openGlNormalSuffix;
		std::wstring openGlMarker;
		std::string fallbackMaterialToken;
		std::filesystem::path matchingFallbackTexture;
		std::filesystem::path defaultFallbackTexture;
		float normalizedExtent = 10.0f;
		float groundOffset = 0.1f;
	};
} // namespace Engine
