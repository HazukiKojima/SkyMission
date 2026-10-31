#pragma once
#include "Game/Aircraft/AircraftAIController.h"
#include "Engine/Renderer/RendererDefinition.h"
#include "Game/Weapons/WeaponDefinition.h"
#include "Game/Weapons/RocketDefinition.h"
#include "Game/Combat/CombatTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
namespace Game {
	inline constexpr const char* PlayerModelId = "player";
	inline constexpr const char* EnemyModelId = "enemy";
	inline constexpr const char* AmmunitionModelId = "20mmAmmo";
	inline constexpr const char* RocketModelId = "rocket";
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
		RocketDefinition playerRocket;
		CombatSettings combat;
		Engine::HudSettings hud;
		Engine::RendererDefinition BuildRendererDefinition() const {
			return {environment,
				{{PlayerModelId, playerAircraft.model, playerAircraft.modelImport},
					{EnemyModelId, enemyAircraft.model, enemyAircraft.modelImport}, playerWeapon.ammunition,
					playerRocket.ammunition}};
		}
	};
	class FlightSceneLoader {
	  public:
		FlightSceneDefinition Load(const SceneSourcePaths& paths) const;
	};
} // namespace Game
