#include "../Scene/SceneLoader.h"
#include "../Interop/Redox/JsonParametersLoader.h"
#include "OceanParameters.h"
#include "SkyParameters.h"
#include "External/REDoxWrapper/Generated/AircraftAIParametersLayout.h"
#include "External/REDoxWrapper/Generated/OceanParametersLayout.h"
#include "External/REDoxWrapper/Generated/SkyParametersLayout.h"
#include "../Resources/JsonAssetReader.h"
#include "../Aircraft/ValidateFlightParameters.h"
#include <cmath>
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		ModelLoadOptions LoadModelOptions(const JsonAssetReader& reader, const std::filesystem::path& aircraftPath) {
			const auto path = reader.Resolve(aircraftPath, "modelImport");
			const auto wide = [&](const char* key) {
				const auto value = reader.ResolveString(path, key);
				return std::filesystem::path(
						   std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()))
					.wstring();
			};
			ModelLoadOptions options{.propellerName = reader.ResolveString(path, "propellerName"),
									 .textureDirectory = wide("textureDirectory"),
									 .baseColorStemSuffix = wide("baseColorStemSuffix"),
									 .baseColorSuffix = wide("baseColorSuffix"),
									 .metallicSuffix = wide("metallicSuffix"),
									 .roughnessSuffix = wide("roughnessSuffix"),
									 .normalSuffix = wide("normalSuffix"),
									 .openGlNormalSuffix = wide("openGlNormalSuffix"),
									 .openGlMarker = wide("openGlMarker"),
									 .fallbackMaterialToken = reader.ResolveString(path, "fallbackMaterialToken"),
									 .matchingFallbackTexture = wide("matchingFallbackTexture"),
									 .defaultFallbackTexture = wide("defaultFallbackTexture"),
									 .normalizedExtent = reader.ResolveFloat(path, "normalizedExtent"),
									 .groundOffset = reader.ResolveFloat(path, "groundOffset")};
			if (!std::isfinite(options.normalizedExtent) || options.normalizedExtent <= 0.0f ||
				!std::isfinite(options.groundOffset))
				throw std::runtime_error("Invalid model normalization settings.");
			return options;
		}
	} // namespace

	SceneDefinition SceneLoader::Load(const SceneSourcePaths& paths) const {
		JsonDocumentCache jsonDocuments;
		JsonAssetReader resolver(jsonDocuments);
		JsonParametersLoader parametersLoader(jsonDocuments);
		const std::filesystem::path skyJsonPath = resolver.Resolve(paths.environment, "sky");
		const std::filesystem::path oceanJsonPath = resolver.Resolve(paths.environment, "ocean");
		const std::filesystem::path retroFilmJsonPath = paths.retroFilm;

		const std::filesystem::path aircraftConfigJsonPath = paths.playerAircraft;
		const std::filesystem::path enemyAircraftConfigJsonPath = paths.enemyAircraft;
		const float aircraftVisualScale = resolver.ResolveFloat(aircraftConfigJsonPath, "visualScale");
		const std::filesystem::path enemyAircraftModel = resolver.Resolve(enemyAircraftConfigJsonPath, "model");
		const float enemyAircraftVisualScale = resolver.ResolveFloat(enemyAircraftConfigJsonPath, "visualScale");
		const AircraftFlightParameters flightParameters = parametersLoader.Load<AircraftFlightParameters>(
			resolver.Resolve(aircraftConfigJsonPath, "flightParameters"), "MyRedoxGetAircraftFlightParameters");
		const AircraftFlightParameters enemyFlightParameters = parametersLoader.Load<AircraftFlightParameters>(
			resolver.Resolve(enemyAircraftConfigJsonPath, "flightParameters"), "MyRedoxGetAircraftFlightParameters");
		const std::filesystem::path enemyAIConfigPath = resolver.Resolve(enemyAircraftConfigJsonPath, "aiParameters");
		const AircraftAIParameters enemyAIParameters =
			parametersLoader.Load<AircraftAIParameters>(enemyAIConfigPath, "MyRedoxGetAircraftAIParameters");
		ValidateFlightParameters(flightParameters);
		ValidateFlightParameters(enemyFlightParameters);
		if (!std::isfinite(aircraftVisualScale) || aircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Aircraft visualScale must be a positive finite number.");
		}
		if (!std::isfinite(enemyAircraftVisualScale) || enemyAircraftVisualScale <= 0.0f) {
			throw std::runtime_error("Enemy aircraft visualScale must be a positive finite number.");
		}
		if (!std::isfinite(enemyAIParameters.directionChangeSeconds) ||
			enemyAIParameters.directionChangeSeconds <= 0.0f ||
			!std::isfinite(enemyAIParameters.altitudeChangeSeconds) ||
			enemyAIParameters.altitudeChangeSeconds <= 0.0f || !std::isfinite(enemyAIParameters.lowAltitude) ||
			enemyAIParameters.lowAltitude < 0.0f || !std::isfinite(enemyAIParameters.highAltitude) ||
			enemyAIParameters.highAltitude <= enemyAIParameters.lowAltitude ||
			!std::isfinite(enemyAIParameters.pitchLookAheadDistance) ||
			enemyAIParameters.pitchLookAheadDistance <= 0.0f || !std::isfinite(enemyAIParameters.turnGain) ||
			enemyAIParameters.turnGain <= 0.0f || !std::isfinite(enemyAIParameters.pitchGain) ||
			enemyAIParameters.pitchGain <= 0.0f || !std::isfinite(enemyAIParameters.speedMatchGain) ||
			enemyAIParameters.speedMatchGain <= 0.0f ||
			!std::isfinite(enemyAIParameters.highGManeuverIntervalSeconds) ||
			enemyAIParameters.highGManeuverIntervalSeconds <= 0.0f ||
			!std::isfinite(enemyAIParameters.highGManeuverDurationSeconds) ||
			enemyAIParameters.highGManeuverDurationSeconds <= 0.0f ||
			enemyAIParameters.highGManeuverDurationSeconds > enemyAIParameters.highGManeuverIntervalSeconds) {
			throw std::runtime_error("Enemy aircraft AI parameters are outside their valid ranges.");
		}
		const OceanParameters oceanParameters =
			parametersLoader.Load<OceanParameters>(oceanJsonPath, "MyRedoxGetOceanParameters");
		const SkyParameters skyParameters =
			parametersLoader.Load<SkyParameters>(skyJsonPath, "MyRedoxGetSkyParameters");
		const RetroFilmParameters retroFilmParameters =
			parametersLoader.Load<RetroFilmParameters>(retroFilmJsonPath, "MyRedoxGetRetroFilmParameters");
		const OceanSettings ocean = {.size = oceanParameters.size,
									 .waveLength = oceanParameters.waveLength,
									 .waveSteepness = oceanParameters.waveSteepness,
									 .normalUvScale = oceanParameters.normalUvScale,
									 .wavePatternUvScale = oceanParameters.wavePatternUvScale,
									 .fogStartDistance = oceanParameters.fogStartDistance,
									 .fogEndDistance = oceanParameters.fogEndDistance,
									 .edgeFadeStart = oceanParameters.edgeFadeStart,
									 .edgeFadeWidth = oceanParameters.edgeFadeWidth,
									 .fogTop = resolver.ResolveFloat(oceanJsonPath, "fogTop"),
									 .fogDensity = resolver.ResolveFloat(oceanJsonPath, "fogDensity")};
		if (!std::isfinite(ocean.fogTop) || ocean.fogTop <= 0.0f || !std::isfinite(ocean.fogDensity) ||
			ocean.fogDensity < 0.0f)
			throw std::runtime_error("Invalid ocean fog settings.");
		const SkySettings sky = {
			.radius = skyParameters.radius,
			.sunDirection = {skyParameters.sunDirection[0], skyParameters.sunDirection[1],
							 skyParameters.sunDirection[2]},
			.sunIntensity = skyParameters.sunIntensity,
			.sunColor = {skyParameters.sunColor[0], skyParameters.sunColor[1], skyParameters.sunColor[2]},
			.ambientIntensity = skyParameters.ambientIntensity,
			.ambientColor = {skyParameters.ambientColor[0], skyParameters.ambientColor[1],
							 skyParameters.ambientColor[2]}};

		return {resolver.Resolve(skyJsonPath, "texture"),
				resolver.Resolve(oceanJsonPath, "texture"),
				resolver.Resolve(oceanJsonPath, "normal"),
				resolver.Resolve(oceanJsonPath, "normal2"),
				{.model = resolver.Resolve(aircraftConfigJsonPath, "model"),
				 .visualScale = aircraftVisualScale,
				 .flightParameters = flightParameters,
				 .modelImport = LoadModelOptions(resolver, paths.playerAircraft)},
				{.model = enemyAircraftModel,
				 .visualScale = enemyAircraftVisualScale,
				 .flightParameters = enemyFlightParameters,
				 .modelImport = LoadModelOptions(resolver, paths.enemyAircraft)},
				enemyAIParameters,
				ocean,
				sky,
				retroFilmParameters};
	}
} // namespace Engine
