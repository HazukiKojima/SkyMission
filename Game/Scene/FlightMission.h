#pragma once
#include "Game/Combat/MissionState.h"
#include "Game/Aircraft/AircraftAIController.h"
#include "Game/Weapons/ProjectileSystem.h"
#include "Game/Effects/CombatEffectSystem.h"
namespace Game {
	// ミッション規則、増援配置、艦隊の状態と攻撃を所有する。
	class FlightMission {
	  public:
		enum class SquadronKind { Patrol, Fighters, Commander };
		struct AircraftSpawn {
			std::shared_ptr<Engine::Model> model;
			DirectX::XMFLOAT3 position;
			float scale;
			AircraftFlightParameters flight;
			AircraftAIParameters ai;
			float health;
			std::string name;
			bool facePlayer;
			std::optional<DirectX::XMFLOAT3> heading;
		};
		void Configure(const MissionSettings& settings, std::shared_ptr<Engine::Model> enemyModel,
			float enemyScale, const AircraftFlightParameters& flight, const AircraftAIParameters& ai);
		void RegisterShip(Engine::Scene& scene, Engine::ObjectId id, bool carrier);
		std::optional<CollisionTarget> GetDeckTarget(Engine::Scene& scene) const;
		std::vector<AircraftSpawn> BuildWave(Engine::Scene& scene, const DirectX::XMFLOAT3& player,
			size_t count, SquadronKind kind, float enemyHealth,
			const DirectX::XMFLOAT3& forward = {0, 0, -1}, size_t slotBase = 0);
		bool Advance(size_t remaining, bool alive, float dt);
		void UpdateDeckSmoke(Engine::Scene& scene, CombatEffectSystem& effects, float dt);
		void UpdateAntiAircraftGuns(Engine::Scene& scene, const AircraftFlightModel& player,
			ProjectileSystem& projectiles, float battleSeconds, float dt);
		MissionPhase GetPhase() const {
			return m_mission.GetPhase();
		}
		bool IsDeckDisabled() const {
			return m_mission.IsDeckDisabled();
		}
		std::optional<DirectX::XMFLOAT3> GetCarrierCenter(Engine::Scene& scene) const;
		std::optional<DirectX::XMFLOAT3> GetDefenseCenter(Engine::Scene& scene) {
			if (auto center = GetCarrierCenter(scene)) m_defenseCenter = center;
			return m_defenseCenter;
		}
		float GetCarrierHealth() const { return m_mission.GetCarrierHealth(); }
		float GetCarrierMaxHealth() const { return m_mission.GetCarrierMaxHealth(); }
		bool ApplyCarrierDamage(float damage) { return m_mission.ApplyCarrierDamage(damage); }
		void ConfirmDeckHit() {
			m_mission.ConfirmDeckHit();
		}
		const MissionSettings& GetSettings() const {
			return m_missionSettings;
		}
		Engine::ObjectId GetCarrierId() const {
			return m_carrierId;
		}
		float GetMessageSeconds() const {
			return m_missionMessageSeconds;
		}
		const DirectX::XMFLOAT3& GetAirCombatPosition() const {
			return m_airCombatPosition;
		}

	  private:
		MissionState m_mission;
		MissionSettings m_missionSettings;
		std::shared_ptr<Engine::Model> m_missionEnemyModel;
		AircraftFlightParameters m_missionEnemyFlight;
		AircraftAIParameters m_missionEnemyAI;
		float m_missionEnemyScale = 1;
		Engine::ObjectId m_carrierId = Engine::InvalidObjectId;
		struct EscortState {
			Engine::ObjectId objectId;
			float cooldownSeconds = 0;
		};
		std::vector<EscortState> m_escorts;
		float m_missionMessageSeconds = 8, m_deckSmokeSeconds = 0;
		DirectX::XMFLOAT3 m_airCombatPosition{};
		std::optional<DirectX::XMFLOAT3> m_defenseCenter;
	};
} // namespace Game
