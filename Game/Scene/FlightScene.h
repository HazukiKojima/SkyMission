#include "Game/Settings/GameplaySettings.h"
#pragma once
#include <functional>
#include <map>
#include "FlightMission.h"
#include "Engine/Scene/Scene.h"
#include "Game/Aircraft/AircraftAIController.h"
#include "Game/Camera/AircraftCameraController.h"
#include "Game/Weapons/AircraftGun.h"
#include "Game/Weapons/ProjectileSystem.h"
#include "Game/Combat/CombatController.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Effects/CombatEffectSystem.h"
#include "Game/Weapons/CarrierMissileSystem.h"
#include "Game/Combat/EnemyGunBehavior.h"
#include "Game/Weapons/BombDefinition.h"
#include "Game/Cinematics/CinematicScene.h"

namespace Game {
	struct FlightSceneDefinition;
	// 飛行・AI・武器・任務・カメラを統合し、描画用Sceneと表示用スナップショットを更新する
	class FlightScene {
	  public:
		// 初期化・設定
		// 必須アセットと設定をまとめて渡し、完成したミッションだけを返す
		static FlightScene CreateMission(const FlightSceneDefinition& definition,
			const std::function<std::shared_ptr<Engine::Model>(const std::string&)>& resolveModel, UINT width,
			UINT height, float farZ);
		void Initialize(float aspect, float nearZ, float farZ);
		// 本番開始時に有効化する。個別構成のテストでは呼ばずに直接戦闘できる
		void EnableOpening(const CinematicSettings& settings, std::shared_ptr<Engine::Model> model,
			float visualScale, float farZ);
		void EnableOpeningBriefing();
		void UpdateCinematicBriefing(CinematicVoiceStatus voice, float deltaSeconds);
		struct CinematicModel {
			std::shared_ptr<Engine::Model> model;
			float scale = 1;
		};
		using CinematicModelResolver = std::function<CinematicModel(const std::string&)>;
		void ConfigureCinematics(const std::map<std::string, CinematicSettings>& definitions,
			CinematicModelResolver resolve, float farZ);
		bool PlayCinematic(const std::string& id);
		std::optional<CinematicCompletion> TakeCinematicCompletion() {
			return m_cinematic.TakeCompletion();
		}
		void ConfigureCombat(const CombatSettings& settings);
		void ConfigureMission(const MissionSettings& settings, std::shared_ptr<Engine::Model> enemyModel,
			float enemyScale, const AircraftFlightParameters& flight, const AircraftAIParameters& ai);
		void RegisterMissionShip(Engine::ObjectId id, bool carrier) {
			m_mission.RegisterShip(m_scene, id, carrier);
		}
		void ConfigurePlayerWeapon(
			const WeaponDefinition& definition, std::shared_ptr<Engine::Model> ammunition);
		void ConfigureEnemyWeapons(const WeaponDefinition& definition, const MissionSettings& settings,
			std::shared_ptr<Engine::Model> ammunition);
		void ConfigureCarrierMissileWeapon(
			const BombDefinition& definition, float reloadSeconds, std::shared_ptr<Engine::Model> model);
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

		// オブジェクトの追加・削除
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

		// フレーム更新
		// deltaSecondsは秒。1フレーム分の操作を渡し、更新後の状態を各Get関数から取得する
		void Update(const AircraftInput& input, float deltaSeconds,
			const Engine::InputSnapshot& cameraInput = {}, CombatInput combatInput = {});
		void OnResize(UINT width, UINT height);

		// 状態の取得
		// 内部状態への参照。次のUpdateで内容が変わるため、フレームを越える保存にはコピーする
		const CombatSnapshot& GetCombatSnapshot() const {
			return m_combat;
		}
		const CombatEffectSnapshot& GetEffectSnapshot() const {
			return m_effects.GetSnapshot();
		}
		const Engine::Scene& GetRenderScene() const {
			return m_cinematic.GetPresentation().active && m_cinematic.GetPresentation().formationShot
				&& m_cinematic.GetCurrentShot().target == "model"
				? m_cinematicScene.GetScene()
				: m_scene;
		}
		const std::vector<Engine::SceneObject>& GetObjects() const {
			return m_scene.GetObjects();
		}
		size_t GetProjectileCount() const {
			return m_projectiles.GetCount();
		}
		size_t GetCarrierMissileCount() const {
			return m_carrierMissiles.GetCount();
		}
		size_t GetHostileProjectileCount() const {
			return m_hostileProjectiles.GetCount();
		}

	  private:
		UiTextSettings m_text;
		EnemyGunTuning m_enemyGunTuning;
		EnemyAITuning m_enemyAITuning;
		struct RestartState;

		struct CombatGeometry {
			std::vector<CombatTarget> enemies;
			DirectX::XMFLOAT3 muzzle{};
			DirectX::XMFLOAT3 forward{};
		};

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

		using SquadronKind = FlightMission::SquadronKind;

		// 更新処理
		void UpdatePlayer(const AircraftInput& input, float dt);
		void UpdateEnemies(float dt);
		void UpdateCombatTimers(float dt);
		void UpdateMission(float dt);
		void UpdateWeapons(const CombatInput& input, float dt);
		void FirePlayerGun(const CombatInput& input, float dt, const CombatGeometry& geometry,
			std::span<const CollisionTarget> targets);
		void UpdateCarrierMissiles(const CombatInput& input, float dt);
		void UpdateHostileWeapons(float dt);
		void UpdateEnemyGuns();
		void UpdateTargeting(const CombatGeometry& geometry, bool next, float dt);
		void UpdateCamera(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput);
		void ApplyHits();
		void ApplySeaCollision();
		void UpdateEnemyRadio(float dt);
		EnemyRadioController m_enemyRadio;
		void UpdateGunEffects();
		void ApplyRenderStates();
		void RefreshCombatSnapshot();
		void RefreshDebugCollisions();
		bool m_debugCollisionsEnabled = false;

		// 戦闘対象・増援の構築
		CombatGeometry BuildCombatGeometry();
		std::vector<CollisionTarget> BuildCollisionTargets();
		void SpawnMissionAircraft(size_t count, SquadronKind kind);
		std::optional<CollisionTarget> GetDeckTarget() {
			return m_mission.GetDeckTarget(m_scene);
		}

		// 再挑戦
		void SaveInitialState();
		void Restart(float dt);
		void StartOpening();
		void UpdateCinematic(float deltaSeconds, bool skipRequested);
		void BeginCinematic(const CinematicSettings& settings, const std::string& id, bool briefing);
		void InitializeCinematicShot();
		void UpdateCarrierCinematicCamera();
		void CheckMissionCinematics();

		bool m_briefingEnabled = false;
		bool m_openingEnabled = false;
		CinematicSettings m_openingSettings;
		CinematicController m_cinematic;
		CinematicScene m_cinematicScene;
		std::map<std::string, CinematicSettings> m_cinematics;
		CinematicModelResolver m_resolveCinematicModel;
		CinematicSettings m_activeCinematicSettings;
		DirectX::XMFLOAT3 m_cinematicOrigin{};
		float m_cinematicFarZ = 100000;
		bool m_midMissionPlayed = false;
		float m_carrierLookSeconds = 0;
		bool m_endingPlayed = false;
		bool m_reinforcementsPlayed = false;

		// 任務と敵側の武器
		FlightMission m_mission;
		CarrierMissileSystem m_carrierMissiles;
		ProjectileSystem m_hostileProjectiles;
		GunSettings m_enemyGunSettings;
		float m_commanderGunInterval = 0.1f;

		// CreateMission完了時の状態を保持する。個別構成時は初回更新前に保存する
		std::shared_ptr<const RestartState> m_initialState;
		UINT m_width = 800;
		UINT m_height = 600;
		float m_battleSeconds = 0;
		DirectX::XMFLOAT3 m_previousPlayerPosition{};

		// 描画シーンと機体
		Engine::Scene m_scene;
		AircraftCameraController m_cameraController;
		AircraftFlightModel m_player;
		Engine::ObjectId m_playerId = Engine::InvalidObjectId;
		AircraftAnimationSettings m_playerAnimation, m_enemyAnimation;
		float m_playerVisualScale = 1;
		float m_playerPropellerAngle = 0;
		std::vector<EnemyAircraft> m_enemies;

		// 自機の武器と戦闘・表示状態
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
