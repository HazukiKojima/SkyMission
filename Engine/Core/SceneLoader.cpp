#include "SceneLoader.h"
#include "../Resources/AssetPathResolver.h"

namespace Engine {
	SceneAssetPaths SceneLoader::LoadAssets(
		const std::filesystem::path& environmentJsonPath,
		const std::filesystem::path& aircraftJsonPath) const
	{
		AssetPathResolver resolver;
		const std::filesystem::path skyJsonPath = resolver.Resolve(environmentJsonPath, "sky");
		const std::filesystem::path oceanJsonPath = resolver.Resolve(environmentJsonPath, "ocean");

		const std::filesystem::path f16JsonPath = aircraftJsonPath.parent_path() / L"F16/f16.json";

		return {
			resolver.Resolve(skyJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "normal"),
			resolver.Resolve(f16JsonPath, "model")
		};
	}
}