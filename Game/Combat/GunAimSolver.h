#pragma once
#include "CombatTypes.h"

namespace Game {
	std::optional<AimSolution> SolveGunAim(const DirectX::XMFLOAT3& muzzle,
		const DirectX::XMFLOAT3& shooterVelocity, const CombatTarget& target, float speed, float lifetime);
	bool IsWithinAngle(const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& direction, float degrees);
} // namespace Game
