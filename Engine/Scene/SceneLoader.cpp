#include "../Scene/SceneLoader.h"
#include "../Interop/Redox/JsonParametersLoader.h"
#include "OceanParameters.h"
#include "SkyParameters.h"
#include "../../External/REDoxWrapper/Generated/AircraftAIParametersLayout.h"
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
		JsonDocumentCache jsonDocuments;
		AssetPathResolver resolver(jsonDocuments);
		JsonParametersLoader parametersLoader(jsonDocuments);
		const std::filesystem::path skyJsonPath = resolver.Resolve(environmentJsonPath, "sky");
		const std::filesystem::path oceanJsonPath = resolver.Resolve(environmentJsonPath, "ocean");
		const std::filesystem::path retroFilmJsonPath = environmentJsonPath.parent_path() / L"retroFilm.json";

		const std::filesystem::path aircraftConfigJsonPath = aircraftJsonPath.parent_path() / L"N1K2JShidenKai/N1K2JShidenKai.json";
		const std::filesystem::path enemyAircraftConfigJsonPath = aircraftJsonPath.parent_path() / L"F4U4Corsair/F4U4Corsair.json";
		const float aircraftVisualScale = resolver.ResolveFloat(aircraftConfigJsonPath, "visualScale");
		const std::filesystem::path enemyAircraftModel = resolver.Resolve(enemyAircraftConfigJsonPath, "model");
		const float enemyAircraftVisualScale = resolver.ResolveFloat(enemyAircraftConfigJsonPath, "visualScale");
		const AircraftFlightParameters flightParameters = parametersLoader.Load<AircraftFlightParameters>(
			resolver.Resolve(aircraftConfigJsonPath, "flightParameters"),
			"MyRedoxGetAircraftFlightParameters");
		const AircraftFlightParameters enemyFlightParameters = parametersLoader.Load<AircraftFlightParameters>(
			resolver.Resolve(enemyAircraftConfigJsonPath, "flightParameters"),
			"MyRedoxGetAircraftFlightParameters");
		const std::filesystem::path enemyAIConfigPath = resolver.Resolve(enemyAircraftConfigJsonPath, "aiParameters");
		const AircraftAIParameters enemyAIParameters = parametersLoader.Load<AircraftAIParameters>(
			enemyAIConfigPath, "MyRedoxGetAircraftAIParameters");
		if (!std::isfinite(aircraftVisualScale) || aircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Aircraft visualScale must be a positive finite number.");
		}
		if (!std::isfinite(enemyAircraftVisualScale) || enemyAircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Enemy aircraft visualScale must be a positive finite number.");
		}
		if (!std::isfinite(enemyAIParameters.directionChangeSeconds) || enemyAIParameters.directionChangeSeconds <= 0.0f ||
			!std::isfinite(enemyAIParameters.altitudeChangeSeconds) || enemyAIParameters.altitudeChangeSeconds <= 0.0f ||
			!std::isfinite(enemyAIParameters.lowAltitude) || enemyAIParameters.lowAltitude < 0.0f ||
			!std::isfinite(enemyAIParameters.highAltitude) || enemyAIParameters.highAltitude <= enemyAIParameters.lowAltitude ||
			!std::isfinite(enemyAIParameters.pitchLookAheadDistance) || enemyAIParameters.pitchLookAheadDistance <= 0.0f ||
			!std::isfinite(enemyAIParameters.turnGain) || enemyAIParameters.turnGain <= 0.0f ||
			!std::isfinite(enemyAIParameters.pitchGain) || enemyAIParameters.pitchGain <= 0.0f ||
			!std::isfinite(enemyAIParameters.speedMatchGain) || enemyAIParameters.speedMatchGain <= 0.0f ||
			!std::isfinite(enemyAIParameters.highGManeuverIntervalSeconds) || enemyAIParameters.highGManeuverIntervalSeconds <= 0.0f ||
			!std::isfinite(enemyAIParameters.highGManeuverDurationSeconds) || enemyAIParameters.highGManeuverDurationSeconds <= 0.0f ||
			enemyAIParameters.highGManeuverDurationSeconds > enemyAIParameters.highGManeuverIntervalSeconds) {
			throw std::runtime_error("Enemy aircraft AI parameters are outside their valid ranges.");
		}
		const OceanParameters oceanParameters = parametersLoader.Load<OceanParameters>(
			oceanJsonPath, "MyRedoxGetOceanParameters");
		const SkyParameters skyParameters = parametersLoader.Load<SkyParameters>(
			skyJsonPath, "MyRedoxGetSkyParameters");
		const RetroFilmParameters retroFilmParameters = parametersLoader.Load<RetroFilmParameters>(
			retroFilmJsonPath, "MyRedoxGetRetroFilmParameters");
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
			enemyAircraftModel,
			aircraftVisualScale,
			enemyAircraftVisualScale,
			flightParameters,
			enemyFlightParameters,
			enemyAIParameters,
			ocean,
			sky,
			retroFilmParameters
		};
	}
}
