#pragma once
#include "Game/Aircraft/AircraftAIController.h"
#include "Engine/Renderer/RendererDefinition.h"
namespace Game {
	inline constexpr const char* PlayerModelId = "player";
	inline constexpr const char* EnemyModelId = "enemy";
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
		Engine::RendererDefinition BuildRendererDefinition() const {
			return {environment,
				{{PlayerModelId, playerAircraft.model, playerAircraft.modelImport},
					{EnemyModelId, enemyAircraft.model, enemyAircraft.modelImport}}};
		}
	};
	class FlightSceneLoader {
	  public:
		FlightSceneDefinition Load(const SceneSourcePaths& paths) const;
	};
} // namespace Game
