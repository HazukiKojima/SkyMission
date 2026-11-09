#pragma once
#include "Engine/Scene/Scene.h"
#include <array>
#include <optional>
#include <span>
#include "MissionState.h"
#include "Game/Cinematics/OpeningCinematicTypes.h"

namespace Game {
	// 距離はワールド単位、時間は秒、角度は度。捕捉条件と維持条件を分けて揺れを抑える
	struct TargetingSettings {
		float selectionDistance = 6000;
		float acquireDistance = 2500;
		float acquireAngleDegrees = 25;
		float acquireSeconds = 0.5f;
		float maintainDistance = 2750;
		float maintainAngleDegrees = 35;
		float lossGraceSeconds = 0.25f;
		float assistAngleDegrees = 3;
	};
	struct CombatSettings {
		TargetingSettings targeting;
		float enemyHealth = 120;
		float playerHealth = 100;
		float bulletDamage = 10;
	};
	// 戦闘処理へ渡す対象の値コピー。centerとvelocityはワールド座標系
	struct CombatTarget {
		Engine::ObjectId id = Engine::InvalidObjectId;
		DirectX::XMFLOAT3 center{};
		DirectX::XMFLOAT3 velocity{};
		std::string name;
		float health = 100;
		DirectX::XMFLOAT3 heading{0, 0, -1};
	};
	enum class LockState { None, Selected, Acquiring, Locked };
	// directionは射撃方向、aimPointは自機速度を補正した照準点、secondsは迎撃までの秒数
	struct AimSolution {
		DirectX::XMFLOAT3 direction{};
		DirectX::XMFLOAT3 aimPoint{};
		float seconds = 0;
	};
	struct AimAssistance {
		std::optional<CombatTarget> target;
		float angleDegrees = 3;
	};
	// boundsはモデル空間、worldは現フレームの変換、previousPositionは前フレームの位置
	struct CollisionTarget {
		Engine::ObjectId id;
		Engine::Bounds3 bounds;
		DirectX::XMFLOAT3 previousPosition;
		DirectX::XMFLOAT4X4 world;
	};
	enum class ProjectileKind { Gun, Rocket, Bomb };
	enum class AttackSource { Player, EnemyAircraft, AntiAircraft };
	// 命中処理と演出に渡す通知。position・directionはワールド座標系
	struct HitEvent {
		Engine::ObjectId target;
		float frameSeconds;
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{0, 0, -1};
		ProjectileKind kind = ProjectileKind::Gun;
		AttackSource source = AttackSource::Player;
		float damage = 0;
	};
	enum class CombatResult { Active, Victory, Defeat };
	struct CombatAudioState {
		bool flying = false;
		bool gunFiring = false;
		bool restarted = false;
		unsigned missileLaunches = 0;
		unsigned gunHits = 0;
		unsigned missileHits = 0;
		unsigned aircraftExplosions = 0;
		unsigned shipExplosions = 0;
	};
	// シミュレーションからHUD・音声へ渡す表示用状態。表示側からゲーム状態を書き換えない
	struct CombatSnapshot {
		OpeningCinematicPresentation opening;
		float elapsedSeconds = 0; // HUDの明滅に使う戦闘開始からの経過秒数
		CombatAudioState audio;
		bool missionEnabled = true;
		bool carrierPresent = false;
		float carrierHealth = 0;
		float carrierMaxHealth = 0;
		unsigned carrierMissileAmmo = 2;
		MissionPhase missionPhase = MissionPhase::Patrol;
		bool deckDisabled = false;
		float missileCooldownSeconds = 0;
		std::optional<DirectX::XMFLOAT3> missileImpactPoint;
		DirectX::XMFLOAT3 objectivePosition{};
		float missionMessageSeconds = 0;
		float playerSpeedMetersPerSecond = 0;
		float playerAltitudeMeters = 0;
		DirectX::XMFLOAT3 playerPosition{};
		DirectX::XMFLOAT3 playerHeading{0, 0, -1};
		float playerHealth = 100;
		float playerMaxHealth = 100;
		float playerHitSeconds = 0;
		float enemyRearFireWarningSeconds = 0;
		float enemyFireWarningSeconds = 0; // 敵機の実際の発射から警告を表示する残り時間
		DirectX::XMFLOAT3 playerHitDirection{};
		CombatResult result = CombatResult::Active;
		static constexpr float gunSightDistance = 1500.0f;
		std::vector<CombatTarget> enemies;
		unsigned firedVolleys = 0;
		unsigned hitCount = 0;
		Engine::ObjectId selected = Engine::InvalidObjectId;
		LockState lockState = LockState::None;
		float acquireProgress = 0; // ロック捕捉の進行率（0～1）
		DirectX::XMFLOAT3 muzzle{};
		DirectX::XMFLOAT3 forward{0, 0, -1};
		std::optional<AimSolution> aim; // 寿命内に迎撃解がない場合などは未設定
		std::optional<DirectX::XMFLOAT3> gunImpactPoint;
		bool assistReady = false;
		float hitSeconds = 0;
		float destroyedSeconds = 0;
		float allDownSeconds = 0;
	};
} // namespace Game
