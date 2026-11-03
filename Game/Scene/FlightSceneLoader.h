#pragma once
#include "Game/Aircraft/AircraftAIController.h"
#include "Engine/Renderer/RendererDefinition.h"
#include "Game/Weapons/WeaponDefinition.h"
#include "Game/Weapons/BombDefinition.h"
#include "Game/Combat/CombatTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
namespace Engine {
	class JsonAssetReader;
}
namespace Game {
	inline constexpr const char* PlayerModelId = "player";
	inline constexpr const char* EnemyModelId = "enemy";
	inline constexpr const char* AmmunitionModelId = "20mmAmmo";
	inline constexpr const char* BombModelId = "bomb";
	struct SceneSourcePaths {
		std::filesystem::path environment, playerAircraft, enemyAircraft, retroFilm;
		std::filesystem::path ships;
		std::filesystem::path mission;
		std::filesystem::path bomb;
	};
	struct AircraftDefinition {
		std::filesystem::path model;
		float visualScale = 1;
		AircraftFlightParameters flightParameters;
		Engine::ModelLoadOptions modelImport;
	};
	struct ShipDefinition {
		Engine::ModelAssetDefinition asset;
		DirectX::XMFLOAT3 position{}, rotationDegrees{};
		float size = 1;
		float waterlineOffset = 0;
		DirectX::XMFLOAT4X4 BuildTransform() const;
	};
	std::vector<ShipDefinition> LoadShipDefinitions(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	struct FlightSceneDefinition {
		Engine::RenderEnvironmentDefinition environment;
		AircraftDefinition playerAircraft, enemyAircraft;
		AircraftAIParameters enemyAIParameters;
		WeaponDefinition playerWeapon;
		BombDefinition bomb;
		CombatSettings combat;
		Engine::HudSettings hud;
		std::vector<ShipDefinition> ships;
		MissionSettings mission;
		Engine::RendererDefinition BuildRendererDefinition() const {
			Engine::RendererDefinition result{environment,
				{{PlayerModelId, playerAircraft.model, playerAircraft.modelImport},
					{EnemyModelId, enemyAircraft.model, enemyAircraft.modelImport}, playerWeapon.ammunition,
					bomb.ammunition}};
			for (const auto& ship : ships)
				result.models.push_back(ship.asset);
			return result;
		}
	};
	class FlightSceneLoader {
	  public:
		FlightSceneDefinition Load(const SceneSourcePaths& paths) const;
	};
} // namespace Game
