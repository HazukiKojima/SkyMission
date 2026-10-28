#include "FlightSceneLoader.h"
#include "Game/Aircraft/ValidateFlightParameters.h"
#include "Engine/Interop/Redox/JsonParametersLoader.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Engine/Scene/OceanParameters.h"
#include "Engine/Scene/SkyParameters.h"
#include "External/REDoxWrapper/Generated/AircraftAIParametersLayout.h"
#include "External/REDoxWrapper/Generated/OceanParametersLayout.h"
#include "External/REDoxWrapper/Generated/SkyParametersLayout.h"
#include <cmath>
#include <stdexcept>

namespace Game {
	using namespace Engine;
	namespace {

		ModelLoadOptions LoadModelOptions(
			const JsonAssetReader& reader, const std::filesystem::path& aircraftPath) {
			const auto path = reader.Resolve(aircraftPath, "modelImport");
			const auto wide = [&](const char* key) {
				const auto value = reader.ResolveString(path, key);
				return std::filesystem::path(
					std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()))
					.wstring();
			};
			ModelLoadOptions options{.rotatingNodeName = reader.ResolveString(path, "propellerName"),
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

		AircraftDefinition LoadAircraftDefinition(const JsonAssetReader& reader,
			const JsonParametersLoader& loader, const std::filesystem::path& path) {
			AircraftDefinition aircraft{.model = reader.Resolve(path, "model"),
				.visualScale = reader.ResolveFloat(path, "visualScale"),
				.flightParameters = loader.Load<AircraftFlightParameters>(
					reader.Resolve(path, "flightParameters"), "MyRedoxGetAircraftFlightParameters"),
				.modelImport = LoadModelOptions(reader, path)};
			if (!std::isfinite(aircraft.visualScale) || aircraft.visualScale <= 0)
				throw std::runtime_error("Aircraft visualScale must be a positive finite number.");
			ValidateFlightParameters(aircraft.flightParameters);
			return aircraft;
		}

		void ValidateAIParameters(const AircraftAIParameters& parameters) {
			if (!std::isfinite(parameters.directionChangeSeconds) ||
				parameters.directionChangeSeconds <= 0.0f ||
				!std::isfinite(parameters.altitudeChangeSeconds) ||
				parameters.altitudeChangeSeconds <= 0.0f || !std::isfinite(parameters.lowAltitude) ||
				parameters.lowAltitude < 0.0f || !std::isfinite(parameters.highAltitude) ||
				parameters.highAltitude <= parameters.lowAltitude ||
				!std::isfinite(parameters.pitchLookAheadDistance) ||
				parameters.pitchLookAheadDistance <= 0.0f || !std::isfinite(parameters.turnGain) ||
				parameters.turnGain <= 0.0f || !std::isfinite(parameters.pitchGain) ||
				parameters.pitchGain <= 0.0f || !std::isfinite(parameters.speedMatchGain) ||
				parameters.speedMatchGain <= 0.0f ||
				!std::isfinite(parameters.highGManeuverIntervalSeconds) ||
				parameters.highGManeuverIntervalSeconds <= 0.0f ||
				!std::isfinite(parameters.highGManeuverDurationSeconds) ||
				parameters.highGManeuverDurationSeconds <= 0.0f ||
				parameters.highGManeuverDurationSeconds > parameters.highGManeuverIntervalSeconds) {
				throw std::runtime_error("Enemy aircraft AI parameters are outside their valid ranges.");
			}
		}

		AircraftAIParameters LoadAIParameters(const JsonAssetReader& reader,
			const JsonParametersLoader& loader, const std::filesystem::path& aircraftPath) {
			const auto path = reader.Resolve(aircraftPath, "aiParameters");
			const auto parameters = loader.Load<AircraftAIParameters>(path, "MyRedoxGetAircraftAIParameters");
			ValidateAIParameters(parameters);
			return parameters;
		}

		RenderEnvironmentDefinition LoadEnvironment(const SceneSourcePaths& paths,
			const JsonAssetReader& reader, const JsonParametersLoader& parametersLoader) {
			const auto skyJsonPath = reader.Resolve(paths.environment, "sky");
			const auto oceanJsonPath = reader.Resolve(paths.environment, "ocean");

			const OceanParameters oceanParameters =
				parametersLoader.Load<OceanParameters>(oceanJsonPath, "MyRedoxGetOceanParameters");
			const SkyParameters skyParameters =
				parametersLoader.Load<SkyParameters>(skyJsonPath, "MyRedoxGetSkyParameters");
			const RetroFilmParameters retroFilmParameters =
				parametersLoader.Load<RetroFilmParameters>(paths.retroFilm, "MyRedoxGetRetroFilmParameters");
			const OceanSettings ocean = {.size = oceanParameters.size,
				.waveLength = oceanParameters.waveLength,
				.waveSteepness = oceanParameters.waveSteepness,
				.normalUvScale = oceanParameters.normalUvScale,
				.wavePatternUvScale = oceanParameters.wavePatternUvScale,
				.fogStartDistance = oceanParameters.fogStartDistance,
				.fogEndDistance = oceanParameters.fogEndDistance,
				.edgeFadeStart = oceanParameters.edgeFadeStart,
				.edgeFadeWidth = oceanParameters.edgeFadeWidth,
				.fogTop = reader.ResolveFloat(oceanJsonPath, "fogTop"),
				.fogDensity = reader.ResolveFloat(oceanJsonPath, "fogDensity")};
			if (!std::isfinite(ocean.fogTop) || ocean.fogTop <= 0.0f || !std::isfinite(ocean.fogDensity) ||
				ocean.fogDensity < 0.0f)
				throw std::runtime_error("Invalid ocean fog settings.");
			const SkySettings sky = {.radius = skyParameters.radius,
				.sunDirection = {skyParameters.sunDirection[0], skyParameters.sunDirection[1],
					skyParameters.sunDirection[2]},
				.sunIntensity = skyParameters.sunIntensity,
				.sunColor = {skyParameters.sunColor[0], skyParameters.sunColor[1], skyParameters.sunColor[2]},
				.ambientIntensity = skyParameters.ambientIntensity,
				.ambientColor = {skyParameters.ambientColor[0], skyParameters.ambientColor[1],
					skyParameters.ambientColor[2]}};

			return {reader.Resolve(skyJsonPath, "texture"), reader.Resolve(oceanJsonPath, "texture"),
				reader.Resolve(oceanJsonPath, "normal"), reader.Resolve(oceanJsonPath, "normal2"), ocean, sky,
				retroFilmParameters};
		}
	} // namespace
	FlightSceneDefinition FlightSceneLoader::Load(const SceneSourcePaths& paths) const {
		JsonDocumentCache documents;
		JsonAssetReader reader(documents);
		JsonParametersLoader parameters(documents);
		return {.environment = LoadEnvironment(paths, reader, parameters),
			.playerAircraft = LoadAircraftDefinition(reader, parameters, paths.playerAircraft),
			.enemyAircraft = LoadAircraftDefinition(reader, parameters, paths.enemyAircraft),
			.enemyAIParameters = LoadAIParameters(reader, parameters, paths.enemyAircraft)};
	}
} // namespace Game
