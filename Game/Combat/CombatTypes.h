#pragma once
#include "Engine/Scene/Scene.h"
#include <array>
#include <optional>
#include <span>

namespace Game {
	struct TargetingSettings {
		float selectionDistance = 6000, acquireDistance = 2500, acquireAngleDegrees = 25;
		float acquireSeconds = 0.5f, maintainDistance = 2750, maintainAngleDegrees = 35;
		float lossGraceSeconds = 0.25f, assistAngleDegrees = 3;
	};
	struct CombatSettings {
		TargetingSettings targeting;
		float enemyHealth = 100, bulletDamage = 10;
		std::array<DirectX::XMFLOAT3, 3> enemyPositions{{{0,300,-1200}, {-450,350,-1700}, {500,400,-2000}}};
	};
	struct CombatTarget {
		Engine::ObjectId id = Engine::InvalidObjectId;
		DirectX::XMFLOAT3 center{}, velocity{};
		std::string name;
		float health = 100;
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
	enum class ProjectileKind { Gun, Rocket };
	struct HitEvent { Engine::ObjectId target; float frameSeconds; DirectX::XMFLOAT3 position{}, direction{0,0,-1}; ProjectileKind kind = ProjectileKind::Gun; };
	struct RocketHudState {
		bool configured = false;
		std::array<float, 2> cooldowns{};
		size_t nextMuzzle = 0;
	};
	struct CombatSnapshot {
		static constexpr float gunSightDistance = 500.0f;
		std::vector<CombatTarget> enemies;
		unsigned firedVolleys = 0, hitCount = 0;
		RocketHudState rocket;
		Engine::ObjectId selected = Engine::InvalidObjectId;
		LockState lockState = LockState::None;
		float acquireProgress = 0;
		DirectX::XMFLOAT3 muzzle{}, forward{0,0,-1};
		std::optional<AimSolution> aim;
		std::optional<DirectX::XMFLOAT3> gunImpactPoint;
		bool assistReady = false;
		float hitSeconds = 0, destroyedSeconds = 0, allDownSeconds = 0;
	};
}
