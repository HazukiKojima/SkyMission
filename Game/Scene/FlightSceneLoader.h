#pragma once
#include "Game/Aircraft/AircraftAIController.h"
#include "Engine/Renderer/RendererDefinition.h"
#include "Game/Weapons/WeaponDefinition.h"
namespace Game {
	inline constexpr const char* PlayerModelId = "player";
	inline constexpr const char* EnemyModelId = "enemy";
	inline constexpr const char* AmmunitionModelId = "20mmAmmo";
	struct SceneSourcePaths {
		std::filesystem::path environment, playerAircraft, enemyAircraft, retroFilm;
	};
	struct AircraftDefinition {
		std::filesystem::path model;
		float visualScale = 1;
		AircraftFlightParameters flightParameters;
		Engine::ModelLoadOptions modelImport;
	};
	struct FlightSceneDefinition {
		Engine::RenderEnvironmentDefinition environment;
		AircraftDefinition playerAircraft, enemyAircraft;
		AircraftAIParameters enemyAIParameters;
		WeaponDefinition playerWeapon;
		Engine::RendererDefinition BuildRendererDefinition() const {
			return {environment,
				{{PlayerModelId, playerAircraft.model, playerAircraft.modelImport},
					{EnemyModelId, enemyAircraft.model, enemyAircraft.modelImport}, playerWeapon.ammunition}};
		}
	};
	class FlightSceneLoader {
	  public:
		FlightSceneDefinition Load(const SceneSourcePaths& paths) const;
	};
} // namespace Game
