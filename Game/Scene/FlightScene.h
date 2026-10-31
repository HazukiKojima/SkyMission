#pragma once
#include "Engine/Scene/Scene.h"
#include "Game/Aircraft/AircraftAIController.h"
#include "Game/Camera/AircraftCameraController.h"
#include "Game/Weapons/AircraftGun.h"
#include "Game/Weapons/ProjectileSystem.h"
#include "Game/Weapons/RocketLauncher.h"
#include "Game/Weapons/RocketSystem.h"
#include "Game/Combat/CombatController.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Effects/CombatEffectSystem.h"

namespace Game {
	class FlightScene {
	  public:
		void Initialize(float aspect, float nearZ, float farZ);
		void Update(const AircraftInput& input, float deltaSeconds,
			const Engine::InputSnapshot& cameraInput = {}, CombatInput combatInput = {});
		void ConfigureCombat(const CombatSettings& settings);
		void ConfigureEnemyWeapon(const GunSettings& settings,
			const std::array<DirectX::XMFLOAT3, 4>& muzzles, std::shared_ptr<Engine::Model> ammunition,
			const DirectX::XMFLOAT3& modelForward);
		void ConfigurePlayerRocket(const RocketDefinition& definition, std::shared_ptr<Engine::Model> model);
		size_t GetRocketCount() const {
			return m_rockets.GetCount();
		}
		const CombatSnapshot& GetCombatSnapshot() const {
			return m_combat;
		}
		const CombatEffectSnapshot& GetEffectSnapshot() const {
			return m_effects.GetSnapshot();
		}
		void ConfigurePlayerWeapon(
			const WeaponDefinition& definition, std::shared_ptr<Engine::Model> ammunition);
		size_t GetProjectileCount() const {
			return m_projectiles.GetCount();
		}
		void OnResize(UINT width, UINT height);
		const Engine::Scene& GetRenderScene() const {
			return m_scene;
		}
		const std::vector<Engine::SceneObject>& GetObjects() const {
			return m_scene.GetObjects();
		}
		void SetEnvironment(const Engine::SceneEnvironment& environment) {
			m_scene.SetEnvironment(environment);
		}
		void SetPlayerAircraftPosition(const DirectX::XMFLOAT3& position) {
			m_player.SetPosition(position);
		}
		void SetPlayerAircraftVisualScale(float scale) {
			m_playerVisualScale = scale;
		}
		void SetPlayerAircraftFlightParameters(const AircraftFlightParameters& parameters) {
			m_player.SetFlightParameters(parameters);
		}
		Engine::ObjectId AddPlayerObject(std::shared_ptr<Engine::Model> model);
		Engine::ObjectId AddEnemyAircraft(std::shared_ptr<Engine::Model> model,
			const DirectX::XMFLOAT3& position, float scale, const AircraftFlightParameters& flight,
			const AircraftAIParameters& ai, float health = 100, std::string name = "");
		Engine::ObjectId AddObject(std::shared_ptr<Engine::Model> model) {
			return m_scene.AddObject(std::move(model));
		}
		void RemoveObject(Engine::ObjectId id);

	  private:
		struct EnemyAircraft {
			AircraftFlightModel flightModel;
			AircraftAIController controller;
			AircraftAIParameters aiParameters;
			Engine::ObjectId objectId = Engine::InvalidObjectId;
			float visualScale = 1;
			float propellerAngle = 0;
			float health = 100;
			size_t slot = 0;
			bool fire = false;
			float gunCooldown = 0;
			std::string name;
			DirectX::XMFLOAT3 previousPosition{};
		};
		void UpdatePlayer(const AircraftInput& input, float dt);
		void UpdateEnemies(float dt);
		void UpdateCamera(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput);
		void ApplyRenderStates();
		void RefreshCombatSnapshot();
		std::vector<CollisionTarget> BuildCollisionTargets();
		void ApplyHits();
		void UpdateGunEffects();
		void UpdateRockets(const CombatInput& input, float dt, std::span<const CollisionTarget> targets);
		void UpdateEnemyWeapons(float dt);
		// 初回更新直前の状態を保持し、アセットを共有したまま再挑戦する。
		std::shared_ptr<const FlightScene> m_initialState;
		UINT m_width = 800, m_height = 600;
		float m_battleSeconds = 0;
		DirectX::XMFLOAT3 m_previousPlayerPosition{};
		ProjectileSystem m_enemyProjectiles;
		GunSettings m_enemyGunSettings;
		std::array<DirectX::XMFLOAT3, 4> m_enemyMuzzles{};
		bool m_enemyWeaponConfigured = false;
		Engine::Scene m_scene;
		AircraftCameraController m_cameraController;
		AircraftFlightModel m_player;
		Engine::ObjectId m_playerId = Engine::InvalidObjectId;
		float m_playerVisualScale = 1;
		float m_playerPropellerAngle = 0;
		std::vector<EnemyAircraft> m_enemies;
		AircraftGun m_gun;
		ProjectileSystem m_projectiles;
		RocketLauncher m_rocketLauncher;
		RocketSystem m_rockets;
		RocketSettings m_rocketSettings;
		bool m_rocketConfigured = false;
		bool m_weaponConfigured = false;
		GunSettings m_gunSettings;
		CombatSettings m_combatSettings;
		TargetingSystem m_targeting;
		CombatSnapshot m_combat;
		CombatEffectSystem m_effects;
		size_t m_enemySequence = 0;
	};
} // namespace Game
