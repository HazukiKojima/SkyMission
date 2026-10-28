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
	SceneDefinition SceneLoader::Load(const SceneSourcePaths& paths) const
	{
		JsonFileCache jsonFiles;
		AssetPathResolver resolver(jsonFiles);
		JsonParametersLoader parametersLoader(jsonFiles);
		const std::filesystem::path skyJsonPath = resolver.Resolve(paths.environment, "sky");
		const std::filesystem::path oceanJsonPath = resolver.Resolve(paths.environment, "ocean");

		const std::filesystem::path& aircraftConfigJsonPath = paths.playerAircraft;
		const std::filesystem::path& enemyAircraftConfigJsonPath = paths.enemyAircraft;
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
			paths.retroFilm, "MyRedoxGetRetroFilmParameters");
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

		SceneDefinition scene;
		scene.skyTexture = resolver.Resolve(skyJsonPath, "texture");
		scene.oceanTexture = resolver.Resolve(oceanJsonPath, "texture");
		scene.oceanNormalTexture = resolver.Resolve(oceanJsonPath, "normal");
		scene.oceanNormalTexture2 = resolver.Resolve(oceanJsonPath, "normal2");
		scene.playerAircraft = { resolver.Resolve(aircraftConfigJsonPath, "model"), aircraftVisualScale, flightParameters };
		scene.enemyAircraft = { enemyAircraftModel, enemyAircraftVisualScale, enemyFlightParameters };
		scene.enemyAIParameters = enemyAIParameters;
		scene.ocean = ocean;
		scene.sky = sky;
		scene.retroFilm = retroFilmParameters;
		return scene;
	}
}