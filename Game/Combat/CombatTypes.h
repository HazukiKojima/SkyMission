#pragma once
#include "Engine/Scene/Scene.h"
#include <array>
#include <optional>
#include <span>
#include "MissionState.h"

namespace Game {
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
	struct CombatTarget {
		Engine::ObjectId id = Engine::InvalidObjectId;
		DirectX::XMFLOAT3 center{}, velocity{};
		std::string name;
		float health = 100;
		DirectX::XMFLOAT3 heading{0, 0, -1};
	};
	enum class LockState { None, Selected, Acquiring, Locked };
	struct AimSolution {
		DirectX::XMFLOAT3 direction{}, aimPoint{};
		float seconds = 0;
	};
	struct AimAssistance {
		std::optional<CombatTarget> target;
		float angleDegrees = 3;
	};
	struct CollisionTarget {
		Engine::ObjectId id;
		Engine::Bounds3 bounds;
		DirectX::XMFLOAT3 previousPosition;
		DirectX::XMFLOAT4X4 world;
	};
	enum class ProjectileKind { Gun, Bomb };
	enum class AttackSource { Player, EnemyAircraft, AntiAircraft };
	struct HitEvent {
		Engine::ObjectId target;
		float frameSeconds;
		DirectX::XMFLOAT3 position{}, direction{0, 0, -1};
		ProjectileKind kind = ProjectileKind::Gun;
		AttackSource source = AttackSource::Player;
		float damage = 0;
	};
	enum class CombatResult { Active, Victory, Defeat };
	struct CombatSnapshot {
		MissionPhase missionPhase = MissionPhase::Patrol;
		bool deckDisabled = false;
		float bombCooldown = 0;
		std::optional<DirectX::XMFLOAT3> bombImpactPoint;
		DirectX::XMFLOAT3 objectivePosition{};
		float missionMessageSeconds = 0;
		float playerSpeedMetersPerSecond = 0;
		float playerAltitudeMeters = 0;
		DirectX::XMFLOAT3 playerPosition{}, playerHeading{0, 0, -1};
		float playerHealth = 100, playerMaxHealth = 100, playerHitSeconds = 0;
		DirectX::XMFLOAT3 playerHitDirection{};
		CombatResult result = CombatResult::Active;
		static constexpr float gunSightDistance = 1500.0f;
		std::vector<CombatTarget> enemies;
		unsigned firedVolleys = 0;
		unsigned hitCount = 0;
		Engine::ObjectId selected = Engine::InvalidObjectId;
		LockState lockState = LockState::None;
		float acquireProgress = 0;
		DirectX::XMFLOAT3 muzzle{}, forward{0, 0, -1};
		std::optional<AimSolution> aim;
		std::optional<DirectX::XMFLOAT3> gunImpactPoint;
		bool assistReady = false;
		float hitSeconds = 0;
		float destroyedSeconds = 0;
		float allDownSeconds = 0;
	};
} // namespace Game
