#include "SceneLoader.h"
#include "AircraftFlightParametersLoader.h"
#include "../Resources/AssetPathResolver.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Engine {
	SceneAssetPaths SceneLoader::LoadAssets(
		const std::filesystem::path& environmentJsonPath,
		const std::filesystem::path& aircraftJsonPath) const
	{
		AssetPathResolver resolver;
		const std::filesystem::path skyJsonPath = resolver.Resolve(environmentJsonPath, "sky");
		const std::filesystem::path oceanJsonPath = resolver.Resolve(environmentJsonPath, "ocean");

		const std::filesystem::path f16JsonPath = aircraftJsonPath.parent_path() / L"F16/f16.json";
		const float aircraftVisualScale = resolver.ResolveFloat(f16JsonPath, "visualScale");
		const AircraftFlightParameters flightParameters = AircraftFlightParametersLoader().Load(
			aircraftJsonPath.parent_path() / L"F16/flightParameters.json");
		if (!std::isfinite(aircraftVisualScale) || aircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Aircraft visualScale must be a positive finite number.");
		}
		const OceanSettings ocean = {
			resolver.ResolveFloat(oceanJsonPath, "size"),
			resolver.ResolveFloat(oceanJsonPath, "waveLength"),
			resolver.ResolveFloat(oceanJsonPath, "waveSteepness"),
			resolver.ResolveFloat(oceanJsonPath, "normalUvScale"),
			resolver.ResolveFloat(oceanJsonPath, "wavePatternUvScale"),
			resolver.ResolveFloat(oceanJsonPath, "fogStartDistance"),
			resolver.ResolveFloat(oceanJsonPath, "fogEndDistance"),
			resolver.ResolveFloat(oceanJsonPath, "edgeFadeStart"),
			resolver.ResolveFloat(oceanJsonPath, "edgeFadeWidth")
		};
		const std::vector<float> sunDirection = resolver.ResolveFloatArray(skyJsonPath, "sunDirection");
		const std::vector<float> sunColor = resolver.ResolveFloatArray(skyJsonPath, "sunColor");
		const std::vector<float> ambientColor = resolver.ResolveFloatArray(skyJsonPath, "ambientColor");
		if (sunDirection.size() != 3 || sunColor.size() != 3 || ambientColor.size() != 3) {
			throw std::runtime_error("Sky environment vectors must have three elements.");
		}

		const SkySettings sky = {
			resolver.ResolveFloat(skyJsonPath, "radius"),
			{ sunDirection[0], sunDirection[1], sunDirection[2] },
			resolver.ResolveFloat(skyJsonPath, "sunIntensity"),
			{ sunColor[0], sunColor[1], sunColor[2] },
			resolver.ResolveFloat(skyJsonPath, "ambientIntensity"),
			{ ambientColor[0], ambientColor[1], ambientColor[2] }
		};

		return {
			resolver.Resolve(skyJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "normal"),
			resolver.Resolve(oceanJsonPath, "normal2"),
			resolver.Resolve(f16JsonPath, "model"),
			aircraftVisualScale,
			ocean,
			sky,
			flightParameters
		};
	}
}