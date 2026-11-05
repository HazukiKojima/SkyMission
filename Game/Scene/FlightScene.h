#pragma once
#include <functional>
#include "FlightMission.h"
#include "Engine/Scene/Scene.h"
#include "Game/Aircraft/AircraftAIController.h"
#include "Game/Camera/AircraftCameraController.h"
#include "Game/Weapons/AircraftGun.h"
#include "Game/Weapons/ProjectileSystem.h"
#include "Game/Combat/CombatController.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Effects/CombatEffectSystem.h"
#include "Game/Weapons/BombSystem.h"
#include "Game/Combat/EnemyGunBehavior.h"
#include "Game/Weapons/BombDefinition.h"

namespace Game {
	struct FlightSceneDefinition;
	// 飛行・AI・武器・任務・カメラを統合し、描画用Sceneと表示用スナップショットを更新する
	class FlightScene {
	  public:
		// 必須アセットと設定をまとめて渡し、完成したミッションだけを返す
		static FlightScene CreateMission(const FlightSceneDefinition& definition,
			const std::function<std::shared_ptr<Engine::Model>(const std::string&)>& resolveModel, UINT width,
			UINT height, float farZ);
		void Initialize(float aspect, float nearZ, float farZ);
		// deltaSecondsは秒。1フレーム分の操作を渡し、更新後の状態を各Get関数から取得する
		void Update(const AircraftInput& input, float deltaSeconds,
			const Engine::InputSnapshot& cameraInput = {}, CombatInput combatInput = {});
		void ConfigureCombat(const CombatSettings& settings);
		void ConfigureMission(const MissionSettings& settings, std::shared_ptr<Engine::Model> enemyModel,
			float enemyScale, const AircraftFlightParameters& flight, const AircraftAIParameters& ai);
		void RegisterMissionShip(Engine::ObjectId id, bool carrier) {
			m_mission.RegisterShip(m_scene, id, carrier);
		}
		// 内部状態への参照。次のUpdateで内容が変わるため、フレームを越える保存にはコピーする
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
		void ConfigureEnemyWeapons(const WeaponDefinition& definition, const MissionSettings& settings,
			std::shared_ptr<Engine::Model> ammunition);
		void ConfigureBombWeapon(
			const BombDefinition& definition, float reloadSeconds, std::shared_ptr<Engine::Model> model);
		size_t GetBombCount() const {
			return m_bombs.GetCount();
		}
		size_t GetHostileProjectileCount() const {
			return m_hostileProjectiles.GetCount();
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
			const AircraftAIParameters& ai, float health = 100, std::string name = "",
			bool facePlayer = false);
		Engine::ObjectId AddObject(std::shared_ptr<Engine::Model> model) {
			return m_scene.AddObject(std::move(model));
		}
		Engine::ObjectId AddObject(
			std::shared_ptr<Engine::Model> model, const DirectX::XMFLOAT4X4& transform) {
			return m_scene.AddObject(std::move(model), transform);
		}
		void RemoveObject(Engine::ObjectId id);

	  private:
		struct RestartState;
		void SaveInitialState();
		void Restart(float dt);
		void UpdateCombatTimers(float dt);
		void UpdateWeapons(const CombatInput& input, float dt);
		struct CombatGeometry {
			std::vector<CombatTarget> enemies;
			DirectX::XMFLOAT3 muzzle{}, forward{};
		};
		CombatGeometry BuildCombatGeometry();
		void UpdateMission(float dt);
		using SquadronKind = FlightMission::SquadronKind;
		void SpawnMissionAircraft(size_t count, SquadronKind kind);
		void UpdateBombs(const CombatInput& input, float dt);
		void UpdateHostileWeapons(float dt);
		void UpdateEnemyGuns();
		std::optional<CollisionTarget> GetDeckTarget() {
			return m_mission.GetDeckTarget(m_scene);
		}
		FlightMission m_mission;
		BombSystem m_bombs;
		ProjectileSystem m_hostileProjectiles;
		GunSettings m_enemyGunSettings;
		float m_commanderGunInterval = 0.1f;
		struct EnemyAircraft {
			AircraftFlightModel flightModel;
			AircraftAIController controller;
			EnemyGunPattern gunPattern;
			AircraftAIParameters aiParameters;
			Engine::ObjectId objectId = Engine::InvalidObjectId;
			float visualScale = 1;
			float propellerAngle = 0;
			float health = 100;
			size_t slot = 0;
			bool fire = false;
			float weaponCooldownSeconds = 0;
			std::string name;
			DirectX::XMFLOAT3 previousPosition{};
		};
		void UpdatePlayer(const AircraftInput& input, float dt);
		void UpdateEnemies(float dt);
		void UpdateTargeting(const CombatGeometry& geometry, bool next, float dt);
		void UpdateCamera(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput);
		void ApplyRenderStates();
		void RefreshCombatSnapshot();
		std::vector<CollisionTarget> BuildCollisionTargets();
		void ApplyHits();
		void UpdateGunEffects();
		// CreateMission完了時の状態を保持する。個別構成時は初回更新前に保存する
		std::shared_ptr<const RestartState> m_initialState;
		UINT m_width = 800, m_height = 600;
		float m_battleSeconds = 0;
		DirectX::XMFLOAT3 m_previousPlayerPosition{};
		Engine::Scene m_scene;
		AircraftCameraController m_cameraController;
		AircraftFlightModel m_player;
		Engine::ObjectId m_playerId = Engine::InvalidObjectId;
		float m_playerVisualScale = 1;
		float m_playerPropellerAngle = 0;
		std::vector<EnemyAircraft> m_enemies;
		AircraftGun m_gun;
		ProjectileSystem m_projectiles;
		bool m_weaponConfigured = false;
		GunSettings m_gunSettings;
		CombatSettings m_combatSettings;
		TargetingSystem m_targeting;
		CombatSnapshot m_combat;
		CombatEffectSystem m_effects;
		size_t m_enemySequence = 0;
	};
} // namespace Game
