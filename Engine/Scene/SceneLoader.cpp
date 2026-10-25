#include "../Scene/SceneLoader.h"
#include "../Interop/Redox/JsonParametersLoader.h"
#include "OceanParameters.h"
#include "SkyParameters.h"
#include "../../External/REDoxWrapper/Generated/OceanParametersLayout.h"
#include "../../External/REDoxWrapper/Generated/SkyParametersLayout.h"
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

		const std::filesystem::path aircraftConfigJsonPath = aircraftJsonPath.parent_path() / L"N1K2JShidenKai/N1K2JShidenKai.json";
		const float aircraftVisualScale = resolver.ResolveFloat(aircraftConfigJsonPath, "visualScale");
		const AircraftFlightParameters flightParameters = JsonParametersLoader().Load<AircraftFlightParameters>(
			aircraftConfigJsonPath.parent_path() / L"flightParameters.json",
			"MyRedoxGetAircraftFlightParameters");
		if (!std::isfinite(aircraftVisualScale) || aircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Aircraft visualScale must be a positive finite number.");
		}
		const OceanParameters oceanParameters = JsonParametersLoader().Load<OceanParameters>(
			oceanJsonPath, "MyRedoxGetOceanParameters");
		const SkyParameters skyParameters = JsonParametersLoader().Load<SkyParameters>(
			skyJsonPath, "MyRedoxGetSkyParameters");
		const OceanSettings ocean = {
			oceanParameters.size, oceanParameters.waveLength, oceanParameters.waveSteepness,
			oceanParameters.normalUvScale, oceanParameters.wavePatternUvScale,
			oceanParameters.fogStartDistance, oceanParameters.fogEndDistance,
			oceanParameters.edgeFadeStart, oceanParameters.edgeFadeWidth
		};
		const SkySettings sky = {
			skyParameters.radius,
			{ skyParameters.sunDirection[0], skyParameters.sunDirection[1], skyParameters.sunDirection[2] },
			skyParameters.sunIntensity,
			{ skyParameters.sunColor[0], skyParameters.sunColor[1], skyParameters.sunColor[2] },
			skyParameters.ambientIntensity,
			{ skyParameters.ambientColor[0], skyParameters.ambientColor[1], skyParameters.ambientColor[2] }
		};

		return {
			resolver.Resolve(skyJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "texture"),
			resolver.Resolve(oceanJsonPath, "normal"),
			resolver.Resolve(oceanJsonPath, "normal2"),
			resolver.Resolve(aircraftConfigJsonPath, "model"),
			aircraftVisualScale,
			ocean,
			sky,
			flightParameters
		};
	}
}