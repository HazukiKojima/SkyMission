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
#include <limits>
#include "Game/Combat/CombatSettingsLoader.h"

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
			if (!std::isfinite(options.normalizedExtent) || options.normalizedExtent <= 0.0f)
				throw std::runtime_error("normalizedExtent must be positive and finite");
			if (!std::isfinite(options.groundOffset))
				throw std::runtime_error("groundOffset must be finite");
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

		WeaponDefinition LoadWeapon(const JsonAssetReader& reader, const std::filesystem::path& aircraftPath) {
			const auto weaponPath = reader.Resolve(aircraftPath, "weapon");
			const auto pathText = [](const std::filesystem::path& path) {
				const auto text = path.u8string();
				return std::string(reinterpret_cast<const char*>(text.data()), text.size());
			};
			WeaponDefinition definition;
			definition.sourcePath = weaponPath;
			try {
				const char* muzzleKeys[] = {"muzzleL1", "muzzleL2", "muzzleR1", "muzzleR2"};
				for (size_t i = 0; i < 4; ++i)
					definition.gun.muzzleNames[i] = reader.ResolveString(weaponPath, muzzleKeys[i]);
				definition.gun.fireInterval = reader.ResolveFloat(weaponPath, "fireInterval");
				definition.gun.muzzleSpeed = reader.ResolveFloat(weaponPath, "muzzleSpeed");
				definition.gun.lifetime = reader.ResolveFloat(weaponPath, "lifetime");
				const float capacity = reader.ResolveFloat(weaponPath, "maxProjectiles");
				if (!std::isfinite(capacity) || capacity < 4 || std::floor(capacity) != capacity ||
					capacity > static_cast<float>(std::numeric_limits<int>::max() / 2))
					throw std::invalid_argument("maxProjectiles must be an integer of at least 4 within supported range");
				definition.gun.maxProjectiles = static_cast<size_t>(capacity);
				ValidateGunSettings(definition.gun);
			} catch (const std::exception& error) {
				throw std::runtime_error(pathText(weaponPath) + ": " + error.what());
			}
			const auto ammoPath = reader.Resolve(weaponPath, "ammunition");
			try {
				definition.ammunition = {AmmunitionModelId, reader.Resolve(ammoPath, "model"), LoadModelOptions(reader, ammoPath)};
				definition.ammunition.options.origin = ModelOrigin::BoundsCenter;
				const auto axis = reader.ResolveString(ammoPath, "modelForward");
				if (axis == "+Y") definition.modelForward = {0, 1, 0};
				else if (axis == "-Y") definition.modelForward = {0, -1, 0};
				else if (axis == "+X") definition.modelForward = {1, 0, 0};
				else if (axis == "-X") definition.modelForward = {-1, 0, 0};
				else if (axis == "+Z") definition.modelForward = {0, 0, 1};
				else if (axis == "-Z") definition.modelForward = {0, 0, -1};
				else throw std::invalid_argument("modelForward must be +X, -X, +Y, -Y, +Z or -Z");
			} catch (const std::exception& error) {
				throw std::runtime_error(pathText(ammoPath) + ": " + error.what());
			}
			return definition;
		}

		RocketDefinition LoadRocket(const JsonAssetReader& reader, const std::filesystem::path& aircraftPath) {
			const auto weaponPath=reader.Resolve(aircraftPath,"rocketWeapon");
			RocketDefinition definition; definition.sourcePath=weaponPath;
			auto source=weaponPath;
			try {
				auto& settings=definition.settings;
				settings.muzzleNames = {reader.ResolveString(weaponPath, "muzzleL1"),
					reader.ResolveString(weaponPath, "muzzleR1")};
				settings.cooldownSeconds = reader.ResolveFloat(weaponPath, "cooldownSeconds");
				settings.speed = reader.ResolveFloat(weaponPath, "speed");
				settings.turnDegreesPerSecond = reader.ResolveFloat(weaponPath, "turnDegreesPerSecond");
				settings.lifetimeSeconds = reader.ResolveFloat(weaponPath, "lifetimeSeconds");
				settings.damage = reader.ResolveFloat(weaponPath, "damage");
				const float capacity = reader.ResolveFloat(weaponPath, "maxRockets");
				if (!std::isfinite(capacity) || capacity < 1 || capacity > 1024 ||
					std::floor(capacity) != capacity)
					throw std::invalid_argument("maxRockets must be an integer between 1 and 1024");
				settings.maxRockets = static_cast<size_t>(capacity);
				ValidateRocketSettings(settings);
				source = reader.Resolve(weaponPath, "ammunition");
				definition.ammunition = {
					RocketModelId, reader.Resolve(source, "model"), LoadModelOptions(reader, source)};
				definition.ammunition.options.origin = ModelOrigin::BoundsCenter;
				const auto axis = reader.ResolveString(source, "modelForward");
				if (axis == "+X")
					definition.modelForward = {1, 0, 0};
				else if (axis == "-X")
					definition.modelForward = {-1, 0, 0};
				else if (axis == "+Y")
					definition.modelForward = {0, 1, 0};
				else if (axis == "-Y")
					definition.modelForward = {0, -1, 0};
				else if (axis == "+Z")
					definition.modelForward = {0, 0, 1};
				else if (axis == "-Z")
					definition.modelForward = {0, 0, -1};
				else
					throw std::invalid_argument("modelForward must be +X, -X, +Y, -Y, +Z or -Z");
			} catch (const std::exception& error) {
				const auto path = source.u8string();
				throw std::runtime_error(
					std::string(reinterpret_cast<const char*>(path.data()), path.size()) + ": " +
					error.what());
			}
			return definition;
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

		void RequireEnvironment(const std::filesystem::path& path, const std::string& field,
			bool valid, const char* reason) {
			if (valid) return;
			const auto text=path.u8string();
			throw std::runtime_error("JSON '"+std::string(reinterpret_cast<const char*>(text.data()),text.size())+
				"', field '"+field+"': "+reason);
		}

		void ValidateOceanSettings(const OceanSettings& ocean, const std::filesystem::path& path) {
			// 不正値をGPUへ渡さず、CPUの波高計算とシェーダーで同じ条件を保証する
			for (const auto& [field,value] : {std::pair{"size",ocean.size},std::pair{"waveLength",ocean.waveLength},
				std::pair{"normalUvScale",ocean.normalUvScale},std::pair{"wavePatternUvScale",ocean.wavePatternUvScale},
				std::pair{"edgeFadeWidth",ocean.edgeFadeWidth},std::pair{"fogTop",ocean.fogTop}})
				RequireEnvironment(path,field,std::isfinite(value) && value>0,"must be positive and finite");
			for (const auto& [field,value] : {std::pair{"waveSteepness",ocean.waveSteepness},
				std::pair{"fogStartDistance",ocean.fogStartDistance},std::pair{"fogDensity",ocean.fogDensity},
				std::pair{"edgeFadeStart",ocean.edgeFadeStart}})
				RequireEnvironment(path,field,std::isfinite(value) && value>=0,"must be nonnegative and finite");
			RequireEnvironment(path,"fogEndDistance",std::isfinite(ocean.fogEndDistance) && ocean.fogEndDistance>ocean.fogStartDistance,
				"must be finite and greater than fogStartDistance");
			RequireEnvironment(path,"edgeFadeStart",ocean.edgeFadeStart<1,"must be less than 1");
			RequireEnvironment(path,"edgeFadeWidth",ocean.edgeFadeWidth<=1,"must be at most 1");
		}

		void ValidateSkySettings(const SkySettings& sky, const std::filesystem::path& path) {
			RequireEnvironment(path,"radius",std::isfinite(sky.radius) && sky.radius>0,"must be positive and finite");
			for (const auto& [field,value] : {std::pair{"sunIntensity",sky.sunIntensity},std::pair{"ambientIntensity",sky.ambientIntensity}})
				RequireEnvironment(path,field,std::isfinite(value) && value>=0,"must be nonnegative and finite");
			for (const auto& [field,value] : {std::pair{"sunColor",sky.sunColor},std::pair{"ambientColor",sky.ambientColor}})
				RequireEnvironment(path,field,std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
					value.x>=0 && value.y>=0 && value.z>=0,"must contain three nonnegative finite components");
			const auto& direction=sky.sunDirection;
			const double lengthSquared=double(direction.x)*direction.x+double(direction.y)*direction.y+double(direction.z)*direction.z;
			RequireEnvironment(path,"sunDirection",std::isfinite(lengthSquared) && lengthSquared>1e-12,"must be finite and nonzero");
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
			ValidateOceanSettings(ocean, oceanJsonPath);
			const SkySettings sky = {.radius = skyParameters.radius,
				.sunDirection = {skyParameters.sunDirection[0], skyParameters.sunDirection[1],
					skyParameters.sunDirection[2]},
				.sunIntensity = skyParameters.sunIntensity,
				.sunColor = {skyParameters.sunColor[0], skyParameters.sunColor[1], skyParameters.sunColor[2]},
				.ambientIntensity = skyParameters.ambientIntensity,
				.ambientColor = {skyParameters.ambientColor[0], skyParameters.ambientColor[1],
					skyParameters.ambientColor[2]}};

			ValidateSkySettings(sky, skyJsonPath);

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
			.enemyAIParameters = LoadAIParameters(reader, parameters, paths.enemyAircraft),
			.playerWeapon = LoadWeapon(reader, paths.playerAircraft),
			.playerRocket = LoadRocket(reader, paths.playerAircraft),
			.combat = LoadCombatSettings(reader, paths.environment.parent_path().parent_path()/L"Combat/targeting.json"),
			.hud = LoadHudSettings(reader, paths.environment.parent_path().parent_path()/L"UI/combatHud.json")};
	}
} // namespace Game
