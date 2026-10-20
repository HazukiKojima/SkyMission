#pragma once

#include <filesystem>

namespace Engine {
	struct SceneAssetPaths {
		std::filesystem::path skyTexture;
		std::filesystem::path oceanTexture;
		std::filesystem::path oceanNormalTexture;
		std::filesystem::path aircraftModel;
	};

	class SceneLoader {
	public:
		SceneAssetPaths LoadAssets(
			const std::filesystem::path& environmentJsonPath,
			const std::filesystem::path& aircraftJsonPath) const;
	};
}
